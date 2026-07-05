// SimpleFPEWrapper - SimpleFPEWrapper/fpe/drawing1x.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <glm/gtc/type_ptr.hpp>
#include "drawing1x.h"
#include "list.h"
#include <bit>
#include <cstring>

#define DEBUG 0

void glBegin(GLenum mode) {
    LIST_RECORD(glBegin, {}, mode)

    if (!fpe_inited) {
        if (init_fpe() != 0)
            abort();
        else
            fpe_inited = true;
    }

    auto& s = g_glstate.fpe_state.fpe_draw;

    if (s.primitive != GL_NONE) {
        return;
    }

    s.primitive = mode;
    SFPEWDebugLog("IMMEDIATE begin mode=%s", glEnumToString(mode));
}

void glEnd() {
    LIST_RECORD(glEnd, {})

    auto& s = g_glstate.fpe_state.fpe_draw;
    auto& raw_va = g_glstate.fpe_state.vertexpointer_array;
    auto& vb = g_glstate.fpe_state.fpe_draw.vb;

    if (s.primitive == GL_NONE) {
        return;
    }

    GET_PREV_PROGRAM
    const GLint prev_vao = g_glstate.backend_vertex_array_binding;
    const GLint prev_vbo = g_glstate.backend_array_buffer_binding;

    // actual assembly work, and draw!
    {
        g_glstate.fpe_state.fpe_draw.compile_vertexattrib(raw_va);

        auto& va = g_glstate.fpe_state.normalized_vpa;
        va = raw_va.normalize();
        // Need to generate_compressed_index first (shadergen will use that)
        const auto attributeSizes = build_attribute_size_table(g_glstate.fpe_state.fpe_draw.current_data.sizes);
        va.generate_compressed_index(attributeSizes.data());

        auto key = g_glstate.program_hash();

        // Program
        auto& prog = g_glstate.get_or_generate_program(key);

        int prog_id = prog.get_program();
        if (prog_id < 0) {}
        g_glFuncs.glUseProgram(prog_id);
        g_glstate.backend_current_program = prog_id;
        SFPEWDrainBackendErrors("immediate.use_program");

        // VAO, VB
        g_glFuncs.glBindVertexArray(g_glstate.fpe_state.fpe_vao);
        g_glstate.backend_vertex_array_binding = static_cast<GLint>(g_glstate.fpe_state.fpe_vao);
        SFPEWDrainBackendErrors("immediate.bind_vertex_array");

        g_glFuncs.glBindBuffer(GL_ARRAY_BUFFER, g_glstate.fpe_state.fpe_vbo);
        g_glstate.backend_array_buffer_binding = static_cast<GLint>(g_glstate.fpe_state.fpe_vbo);
        SFPEWDrainBackendErrors("immediate.bind_array_buffer");

        const auto& vbbuf = vb;
        if (SFPEWIsDebugLoggingEnabled() && s.vertex_count > 0) {
            auto logVertex = [&](std::size_t vertexIndex, const char* label) {
                const std::size_t vertexStride = static_cast<std::size_t>(raw_va.stride);
                const std::size_t baseOffset = vertexIndex * vertexStride;
                if (baseOffset >= vbbuf.size()) {
                    return;
                }

                std::size_t offset = baseOffset;
                float pos[4] = {0.f, 0.f, 0.f, 1.f};
                float color[4] = {1.f, 1.f, 1.f, 1.f};
                float uv0[4] = {0.f, 0.f, 0.f, 1.f};

                if (s.current_data.sizes.vertex_size > 0 &&
                    vbbuf.size() >= offset + sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.vertex_size)) {
                    std::memcpy(pos, vbbuf.data() + offset,
                                sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.vertex_size));
                    offset += sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.vertex_size);
                }
                if (s.current_data.sizes.normal_size > 0) {
                    offset += sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.normal_size);
                }
                if (s.current_data.sizes.color_size > 0 &&
                    vbbuf.size() >= offset + sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.color_size)) {
                    std::memcpy(color, vbbuf.data() + offset,
                                sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.color_size));
                    offset += sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.color_size);
                }
                if (s.current_data.sizes.texcoord_size[0] > 0 &&
                    vbbuf.size() >= offset + sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.texcoord_size[0])) {
                    std::memcpy(uv0, vbbuf.data() + offset,
                                sizeof(float) * static_cast<std::size_t>(s.current_data.sizes.texcoord_size[0]));
                }

                SFPEWDebugLog("IMMEDIATE %s pos=(%.3f, %.3f, %.3f, %.3f) color=(%.3f, %.3f, %.3f, %.3f) uv0=(%.3f, %.3f, %.3f, %.3f)",
                              label,
                              pos[0], pos[1], pos[2], pos[3],
                              color[0], color[1], color[2], color[3],
                              uv0[0], uv0[1], uv0[2], uv0[3]);
            };

            logVertex(0, "vertex0");
            if (s.vertex_count > 1) {
                logVertex(1, "vertex1");
            }
        }
        g_glFuncs.glBufferData(GL_ARRAY_BUFFER,
                               static_cast<GLsizeiptr>(vbbuf.size()),
                               vbbuf.empty() ? nullptr : vbbuf.data(),
                               GL_DYNAMIC_DRAW);
        SFPEWDrainBackendErrors("immediate.buffer_data");

        // Vertex Pointer to ES
        g_glstate.send_vertex_attributes(va);
        SFPEWDrainBackendErrors("immediate.send_vertex_attributes");

        // Uniform
        {
            g_glstate.send_uniforms(prog);
            SFPEWDrainBackendErrors("immediate.send_uniforms");
        }

        SFPEWDebugLog("IMMEDIATE end mode=%s vertex_count=%zu vb_bytes=%zu program=%d",
                      glEnumToString(s.primitive),
                      s.vertex_count,
                      vbbuf.size(),
                      prog_id);

        if (s.primitive == GL_QUADS) {
            auto& ib = g_glstate.fpe_state.fpe_ib;
            fill_quad_to_triangle_indices(ib, static_cast<int>(s.vertex_count));
            g_glFuncs.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_glstate.fpe_state.fpe_ibo);
            SFPEWDrainBackendErrors("immediate.bind_element_array_buffer");
            g_glFuncs.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                   static_cast<GLsizeiptr>(ib.size() * sizeof(uint32_t)),
                                   ib.data(),
                                   GL_DYNAMIC_DRAW);
            SFPEWDrainBackendErrors("immediate.index_buffer_data");
            SFPEWDebugLog("IMMEDIATE converted quads to triangles new_count=%zu", ib.size());
            g_glFuncs.glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(ib.size()), GL_UNSIGNED_INT, (void*)0);
        } else {
            g_glFuncs.glDrawArrays(s.primitive, 0, static_cast<GLsizei>(s.vertex_count));
        }
        SFPEWDrainBackendErrors("immediate.draw_arrays");
    }

    SFPEWDrainBackendErrors("immediate.draw");
    g_glFuncs.glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    g_glstate.backend_array_buffer_binding = prev_vbo;
    g_glFuncs.glBindVertexArray(prev_vao);
    g_glstate.backend_vertex_array_binding = prev_vao;
    SET_PREV_PROGRAM
    g_glstate.backend_current_program = m_prev_program;

    // resetting draw state
    s.reset();
    raw_va.reset();
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    // LOG()
    //  LOG_D("glNormal3f(%.2f, %.2f, %.2f)", nx, ny, nz)

    LIST_RECORD(glNormal3f, {}, nx, ny, nz)

    mglNormal<GLfloat, 3>({nx, ny, nz});
}

void glTexCoord2d(GLdouble s, GLdouble t) {
    LIST_RECORD(glTexCoord2d, {}, s, t)

    mglTexCoord<GLdouble, 2>({s, t}, 0);
}

void glTexCoord2dv(const GLdouble* v) {
    if (v) {
        glTexCoord2d(v[0], v[1]);
    }
}

void glTexCoord2f(GLfloat s, GLfloat t) {
    // LOG()
    //  LOG_D("glTexCoord2f(%.2f, %.2f)", s, t)

    LIST_RECORD(glTexCoord2f, {}, s, t)

    mglTexCoord<GLfloat, 2>({s, t}, 0);
}

void glTexCoord2fv(const GLfloat* v) {
    if (v) {
        glTexCoord2f(v[0], v[1]);
    }
}

void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    // LOG()
    //  LOG_D("glTexCoord4f(%.2f, %.2f, %.2f, %.2f)", s, t, r, q)

    LIST_RECORD(glTexCoord4f, {}, s, t, r, q)

    mglTexCoord<GLfloat, 4>({s, t, r, q}, 0);
}

void glTexCoord4fv(const GLfloat* v) {
    if (v) {
        glTexCoord4f(v[0], v[1], v[2], v[3]);
    }
}

void glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) {
    // LOG()
    //  LOG_D("glMultiTexCoord2f(%s, %.2f, %.2f)", glEnumToString(target), s, t)

    LIST_RECORD(glMultiTexCoord2f, {}, target, s, t)

    assert(target - GL_TEXTURE0 < MAX_TEX);
    mglTexCoord<GLfloat, 2>({s, t}, target - GL_TEXTURE0);
}

void glMultiTexCoord2fv(GLenum target, const GLfloat* v) {
    if (v) {
        glMultiTexCoord2f(target, v[0], v[1]);
    }
}

void glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    // LOG()
    //  LOG_D("glMultiTexCoord4f(%s, %.2f, %.2f, %.2f, %.2f)", glEnumToString(target), s, t, r, q)

    LIST_RECORD(glMultiTexCoord4f, {}, target, s, t, r, q)

    assert(target - GL_TEXTURE0 < MAX_TEX);
    mglTexCoord<GLfloat, 4>({s, t, r, q}, target - GL_TEXTURE0);
}

void glMultiTexCoord4fv(GLenum target, const GLfloat* v) {
    if (v) {
        glMultiTexCoord4f(target, v[0], v[1], v[2], v[3]);
    }
}

void glVertex2f(GLfloat x, GLfloat y) {
    LIST_RECORD(glVertex2f, {}, x, y)

    mglVertex<GLfloat, 2>({x, y});
}

void glVertex2fv(const GLfloat* v) {
    if (v) {
        glVertex2f(v[0], v[1]);
    }
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    // LOG()
    //  LOG_D("glVertex3f(%.2f, %.2f, %.2f)", x, y, z)

    LIST_RECORD(glVertex3f, {}, x, y, z)

    mglVertex<GLfloat, 3>({x, y, z});
}

void glVertex3fv(const GLfloat* v) {
    if (v) {
        glVertex3f(v[0], v[1], v[2]);
    }
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    // LOG()
    //  LOG_D("glVertex4f(%.2f, %.2f, %.2f, %.2f)", x, y, z, w)

    LIST_RECORD(glVertex4f, {}, x, y, z, w)

    mglVertex<GLfloat, 4>({x, y, z, w});
}

void glVertex4fv(const GLfloat* v) {
    if (v) {
        glVertex4f(v[0], v[1], v[2], v[3]);
    }
}

void glNormal3fv(const GLfloat* v) {
    if (v) {
        glNormal3f(v[0], v[1], v[2]);
    }
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue) {
    // LOG()
    //  LOG_D("glColor3f(%f, %f, %f)", red, green, blue)

    LIST_RECORD(glColor3f, {}, red, green, blue)

    mglColor<GLfloat, 3>({red, green, blue});
}

void glColor3b(GLbyte red, GLbyte green, GLbyte blue) {
    LIST_RECORD(glColor3b, {}, red, green, blue)
    mglColor<GLbyte, 3>({red, green, blue});
}

void glColor3d(GLdouble red, GLdouble green, GLdouble blue) {
    LIST_RECORD(glColor3d, {}, red, green, blue)
    mglColor<GLdouble, 3>({red, green, blue});
}

void glColor3i(GLint red, GLint green, GLint blue) {
    LIST_RECORD(glColor3i, {}, red, green, blue)
    mglColor<GLint, 3>({red, green, blue});
}

void glColor3s(GLshort red, GLshort green, GLshort blue) {
    LIST_RECORD(glColor3s, {}, red, green, blue)
    mglColor<GLshort, 3>({red, green, blue});
}

void glColor3fv(const GLfloat* v) {
    if (v) {
        glColor3f(v[0], v[1], v[2]);
    }
}

void glColor3ub(GLubyte red, GLubyte green, GLubyte blue) {
    LIST_RECORD(glColor3ub, {}, red, green, blue)

    SFPEWDebugLog("STATE color3ub r=%u g=%u b=%u", red, green, blue);

    mglColor<GLubyte, 3>({red, green, blue});
}

void glColor3ui(GLuint red, GLuint green, GLuint blue) {
    LIST_RECORD(glColor3ui, {}, red, green, blue)
    mglColor<GLuint, 3>({red, green, blue});
}

void glColor3us(GLushort red, GLushort green, GLushort blue) {
    LIST_RECORD(glColor3us, {}, red, green, blue)
    mglColor<GLushort, 3>({red, green, blue});
}

void glColor3bv(const GLbyte* v) {
    if (v) {
        glColor3b(v[0], v[1], v[2]);
    }
}

void glColor3dv(const GLdouble* v) {
    if (v) {
        glColor3d(v[0], v[1], v[2]);
    }
}

void glColor3ubv(const GLubyte* v) {
    if (v) {
        glColor3ub(v[0], v[1], v[2]);
    }
}

void glColor3iv(const GLint* v) {
    if (v) {
        glColor3i(v[0], v[1], v[2]);
    }
}

void glColor3sv(const GLshort* v) {
    if (v) {
        glColor3s(v[0], v[1], v[2]);
    }
}

void glColor3uiv(const GLuint* v) {
    if (v) {
        glColor3ui(v[0], v[1], v[2]);
    }
}

void glColor3usv(const GLushort* v) {
    if (v) {
        glColor3us(v[0], v[1], v[2]);
    }
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    // LOG()
    // LOG_D("glColor4f(%f, %f, %f, %f)", red, green, blue, alpha)

    LIST_RECORD(glColor4f, {}, red, green, blue, alpha)

    SFPEWDebugLog("STATE color4f r=%.3f g=%.3f b=%.3f a=%.3f", red, green, blue, alpha);

    //    auto& attr = g_glstate.fpe_state.vertexpointer_array.attributes[vp2idx(GL_COLOR_ARRAY)];
    //    auto& vpa = g_glstate.fpe_state.vertexpointer_array;
    //    if (vpa.buffer_based) {
    //        attr.size = 4;
    //        attr.usage = GL_COLOR_ARRAY;
    //        attr.type = GL_FLOAT;
    //        attr.normalized = GL_FALSE;
    //        attr.stride = 0;
    //        attr.pointer = 0;
    //        attr.value = glm::vec4(red, green, blue, alpha);
    //        attr.varying = false;
    //    }

    mglColor<GLfloat, 4>({red, green, blue, alpha});
}

void glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha) {
    LIST_RECORD(glColor4b, {}, red, green, blue, alpha)
    mglColor<GLbyte, 4>({red, green, blue, alpha});
}

void glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha) {
    LIST_RECORD(glColor4d, {}, red, green, blue, alpha)
    mglColor<GLdouble, 4>({red, green, blue, alpha});
}

void glColor4i(GLint red, GLint green, GLint blue, GLint alpha) {
    LIST_RECORD(glColor4i, {}, red, green, blue, alpha)
    mglColor<GLint, 4>({red, green, blue, alpha});
}

void glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha) {
    LIST_RECORD(glColor4s, {}, red, green, blue, alpha)
    mglColor<GLshort, 4>({red, green, blue, alpha});
}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    LIST_RECORD(glColor4ub, {}, red, green, blue, alpha)
    mglColor<GLubyte, 4>({red, green, blue, alpha});
}

void glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha) {
    LIST_RECORD(glColor4ui, {}, red, green, blue, alpha)
    mglColor<GLuint, 4>({red, green, blue, alpha});
}

void glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha) {
    LIST_RECORD(glColor4us, {}, red, green, blue, alpha)
    mglColor<GLushort, 4>({red, green, blue, alpha});
}

void glColor4fv(const GLfloat* v) {
    if (v) {
        glColor4f(v[0], v[1], v[2], v[3]);
    }
}

void glColor4bv(const GLbyte* v) {
    if (v) {
        glColor4b(v[0], v[1], v[2], v[3]);
    }
}

void glColor4dv(const GLdouble* v) {
    if (v) {
        glColor4d(v[0], v[1], v[2], v[3]);
    }
}

void glColor4iv(const GLint* v) {
    if (v) {
        glColor4i(v[0], v[1], v[2], v[3]);
    }
}

void glColor4sv(const GLshort* v) {
    if (v) {
        glColor4s(v[0], v[1], v[2], v[3]);
    }
}

void glColor4ubv(const GLubyte* v) {
    if (v) {
        glColor4ub(v[0], v[1], v[2], v[3]);
    }
}

void glColor4uiv(const GLuint* v) {
    if (v) {
        glColor4ui(v[0], v[1], v[2], v[3]);
    }
}

void glColor4usv(const GLushort* v) {
    if (v) {
        glColor4us(v[0], v[1], v[2], v[3]);
    }
}
