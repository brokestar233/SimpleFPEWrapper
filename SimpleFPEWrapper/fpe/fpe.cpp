// SimpleFPEWrapper - SimpleFPEWrapper/fpe/fpe.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "fpe.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include "pointer_utils.h"

#define DEBUG 0

namespace {
    EGLContext g_fpe_context = EGL_NO_CONTEXT;

    bool AttributeUsesClientMemory(const vertexattribute_t& attr) {
        if (attr.pointer == nullptr || attr.size <= 0) {
            return false;
        }

        const int typeBytes = PointerUtils::type_to_bytes(attr.type);
        if (typeBytes <= 0) {
            return false;
        }

        const std::uintptr_t pointerValue = reinterpret_cast<std::uintptr_t>(attr.pointer);
        const std::uintptr_t elementSize = static_cast<std::uintptr_t>(typeBytes * attr.size);
        const std::uintptr_t effectiveStride =
            attr.stride > 0 ? static_cast<std::uintptr_t>(attr.stride) : elementSize;
        return pointerValue > effectiveStride;
    }

    bool AnyEnabledAttributeUsesClientMemory(const vertex_pointer_array_t& vpa) {
        for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
            const bool enabled = ((vpa.enabled_pointers >> i) & 1) != 0;
            if (!enabled) {
                continue;
            }

            if (AttributeUsesClientMemory(vpa.attributes[i])) {
                return true;
            }
        }
        return false;
    }

    bool BuildPackedClientArrayLayout(const vertex_pointer_array_t& source,
                                      GLint first,
                                      GLsizei count,
                                      const GLint* constant_sizes,
                                      vertex_pointer_array_t& packedLayout,
                                      std::vector<std::uint8_t>& packedData) {
        struct CopyInfo {
            const std::uint8_t* sourceBase = nullptr;
            std::size_t sourceStride = 0;
            std::size_t elementSize = 0;
            std::size_t destOffset = 0;
        };

        packedLayout.reset();
        packedLayout.enabled_pointers = source.enabled_pointers;
        packedLayout.dirty = true;
        packedLayout.buffer_based = true;

        std::vector<CopyInfo> copies(VERTEX_POINTER_COUNT);
        std::size_t packedStride = 0;

        for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
            const bool enabled = ((source.enabled_pointers >> i) & 1) != 0;
            if (!enabled) {
                continue;
            }

            const auto& srcAttr = source.attributes[i];
            const int typeBytes = PointerUtils::type_to_bytes(srcAttr.type);
            if (srcAttr.pointer == nullptr || srcAttr.size <= 0 || typeBytes <= 0) {
                return false;
            }

            const std::size_t elementSize = static_cast<std::size_t>(srcAttr.size) * static_cast<std::size_t>(typeBytes);
            const std::size_t sourceStride =
                srcAttr.stride > 0 ? static_cast<std::size_t>(srcAttr.stride) : elementSize;

            auto& dstAttr = packedLayout.attributes[i];
            dstAttr = srcAttr;
            dstAttr.pointer = reinterpret_cast<const void*>(packedStride);
            dstAttr.stride = 0;

            copies[i] = {
                .sourceBase = reinterpret_cast<const std::uint8_t*>(srcAttr.pointer),
                .sourceStride = sourceStride,
                .elementSize = elementSize,
                .destOffset = packedStride,
            };
            packedStride += elementSize;
        }

        if (packedStride == 0) {
            return false;
        }

        packedLayout.stride = static_cast<GLsizei>(packedStride);
        packedLayout.starting_pointer = nullptr;
        for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
            const bool enabled = ((packedLayout.enabled_pointers >> i) & 1) != 0;
            if (!enabled) {
                continue;
            }
            packedLayout.attributes[i].stride = packedLayout.stride;
        }
        packedLayout.generate_compressed_index(constant_sizes);

        packedData.resize(static_cast<std::size_t>(count) * packedStride);
        for (GLsizei vertexIndex = 0; vertexIndex < count; ++vertexIndex) {
            const std::size_t destVertexBase = static_cast<std::size_t>(vertexIndex) * packedStride;
            const std::size_t sourceVertexIndex = static_cast<std::size_t>(first + vertexIndex);
            for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
                const bool enabled = ((packedLayout.enabled_pointers >> i) & 1) != 0;
                if (!enabled) {
                    continue;
                }

                const auto& copy = copies[i];
                std::memcpy(packedData.data() + destVertexBase + copy.destOffset,
                            copy.sourceBase + sourceVertexIndex * copy.sourceStride,
                            copy.elementSize);
            }
        }
        return true;
    }
} // namespace

glstate_t& glstate_t::get_instance() {
    static glstate_t s_glstate;
    return s_glstate;
}

GLsizei type_size(GLenum type) {
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        return 1;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT:
        return 2;
    case GL_3_BYTES:
        return 3;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
    case GL_FIXED:
        return 4;
    case GL_DOUBLE:
        return 8;
    default:
        // LOG_D("%s: unknown type: %s", __FUNCTION__, glEnumToString(type))
        return 0;
    }
}

void fill_quad_to_triangle_indices(std::vector<uint32_t>& indices, int vertexCount) {
    const int quadCount = vertexCount / 4;
    const int indexCount = quadCount * 6;
    indices.resize(static_cast<std::size_t>(indexCount));

    for (int i = 0; i < quadCount; ++i) {
        const int baseIndex = i * 4;
        indices[i * 6 + 0] = static_cast<uint32_t>(baseIndex + 0);
        indices[i * 6 + 1] = static_cast<uint32_t>(baseIndex + 1);
        indices[i * 6 + 2] = static_cast<uint32_t>(baseIndex + 2);
        indices[i * 6 + 3] = static_cast<uint32_t>(baseIndex + 2);
        indices[i * 6 + 4] = static_cast<uint32_t>(baseIndex + 3);
        indices[i * 6 + 5] = static_cast<uint32_t>(baseIndex + 0);
    }
}

#if DEBUG || GLOBAL_DEBUG
void log_vtx_attrib_data(const void* ptr, GLenum type, int size, int stride, int offset, int idx) {
    const char* p = (const char*)ptr + idx * stride + offset;
    switch (type) {
    case GL_FLOAT: {
        // LOG_D_N("(GL_FLOAT): (")
        const GLfloat* p_data = (const GLfloat*)p;
        for (int i = 0; i < size; ++i) {
            // LOG_D_N("%.2f, ", p_data[i]);
        }
        // LOG_D_N(") ")
        break;
    }
    case GL_UNSIGNED_BYTE: {
        // LOG_D_N("(GL_UNSIGNED_BYTE): (")
        const GLubyte* p_data = (const GLubyte*)p;
        for (int i = 0; i < size; ++i) {
            // LOG_D_N("%hhu, ", p_data[i]);
        }
        // LOG_D_N(") ")
    }
    }
}
#endif

bool fpe_inited = false;
int init_fpe() {
    const EGLContext currentContext =
        g_eglFuncs.eglGetCurrentContext ? g_eglFuncs.eglGetCurrentContext() : EGL_NO_CONTEXT;
    if (currentContext == EGL_NO_CONTEXT) {
        return 0;
    }

    // LOG_I("Initializing fixed-function pipeline...")

    g_glstate.fpe_programs.clear();
    g_glstate.fpe_vaos.clear();
    g_glstate.last_array_binding_hash_valid = false;
    g_glstate.last_array_binding_vao = 0;
    g_glstate.last_array_binding_hash = 0;
    g_glstate.uniform_state_version = 1;
    g_glstate.last_uniform_state_version = 0;
    g_glstate.last_uniform_program_id = -1;
    g_glstate.shader_state_version = 1;
    g_glstate.last_program_hash_state_version = 0;
    g_glstate.last_program_hash = 0;
    g_glstate.backend_current_program = 0;
    g_glstate.backend_vertex_array_binding = 0;
    g_glstate.backend_array_buffer_binding = 0;
    g_glstate.fpe_state.fpe_vao = 0;
    g_glstate.fpe_state.fpe_vbo = 0;
    g_glstate.fpe_state.fpe_ibo = 0;
    g_glstate.fpe_state.fpe_ib.clear();

    g_glFuncs.glGenVertexArrays(1, &g_glstate.fpe_state.fpe_vao);

    g_glFuncs.glGenBuffers(1, &g_glstate.fpe_state.fpe_vbo);

    g_glFuncs.glGenBuffers(1, &g_glstate.fpe_state.fpe_ibo);

    // LOG_D("fpe_vao: %d", g_glstate.fpe_state.fpe_vao)
    // LOG_D("fpe_vbo: %d", g_glstate.fpe_state.fpe_vbo)
    // LOG_D("fpe_ibo: %d", g_glstate.fpe_state.fpe_ibo)

    g_glFuncs.glBindVertexArray(g_glstate.fpe_state.fpe_vao);
    g_glstate.backend_vertex_array_binding = static_cast<GLint>(g_glstate.fpe_state.fpe_vao);

    g_glFuncs.glBindVertexArray(0);
    g_glstate.backend_vertex_array_binding = 0;
    g_fpe_context = currentContext;
    SFPEWDebugLog("FPE init vao=%u vbo=%u ibo=%u", g_glstate.fpe_state.fpe_vao, g_glstate.fpe_state.fpe_vbo,
                  g_glstate.fpe_state.fpe_ibo);

    return 0;
}

bool ensure_fpe_ready() {
    const EGLContext currentContext =
        g_eglFuncs.eglGetCurrentContext ? g_eglFuncs.eglGetCurrentContext() : EGL_NO_CONTEXT;
    if (currentContext == EGL_NO_CONTEXT) {
        return false;
    }

    if (!fpe_inited || g_fpe_context != currentContext) {
        if (init_fpe() != 0) {
            return false;
        }
        fpe_inited = true;
    }

    return true;
}

int commit_fpe_state_on_draw(GLenum* mode, GLint* first, GLsizei* count) {
    // LOG()

    if (!ensure_fpe_ready()) {
        return 0;
    }

    // Need to generate_compressed_index first (shadergen will use that)
    auto& gls = g_glstate;
    auto& fpeState = gls.fpe_state;
    auto& raw_vpa = fpeState.vertexpointer_array;
    auto& vpa = fpeState.normalized_vpa;
    std::vector<std::uint8_t> packedClientData;
    const auto attributeSizes = build_attribute_size_table(fpeState.fpe_draw.current_data.sizes);
    const bool usesClientMemory = AnyEnabledAttributeUsesClientMemory(raw_vpa);
    if (usesClientMemory) {
        if (!BuildPackedClientArrayLayout(raw_vpa, *first, *count, attributeSizes.data(),
                                          vpa, packedClientData)) {
            SFPEWDebugLog("FPE failed to build packed client-array layout");
            return 0;
        }
        *first = 0;
    } else {
        vpa = raw_vpa.normalize();
        vpa.generate_compressed_index(attributeSizes.data());
    }
    // kinda cursed...
    raw_vpa.generate_compressed_index(attributeSizes.data());
    //    g_glFuncs.glGenVertexArrays(1, &vpa.fpe_vao);
    // LOG_D("fpe_vao: %d", g_glstate.fpe_state.fpe_vao)
    g_glFuncs.glBindVertexArray(fpeState.fpe_vao);
    gls.backend_vertex_array_binding = static_cast<GLint>(fpeState.fpe_vao);
    SFPEWDrainBackendErrors("fpe.bind_vertex_array");

    auto key = gls.program_hash(false);
    // LOG_D("%s: key=0x%x", __func__, key)
    auto& prog = gls.get_or_generate_program(key);
    int prog_id = prog.get_program();
    // if (prog_id < 0) LOG_D("Error: FPE shader link failed!")
    g_glFuncs.glUseProgram(prog_id);
    gls.backend_current_program = prog_id;
    SFPEWDrainBackendErrors("fpe.use_program");

    SFPEWDebugLog("FPE commit mode=%s first=%d count=%d key=0x%llx program=%d vao=%u stride=%d start=%p enabled=0x%x client_mem=%d",
                  glEnumToString(*mode),
                  *first,
                  *count,
                  static_cast<unsigned long long>(key),
                  prog_id,
                  fpeState.fpe_vao,
                  vpa.stride,
                  vpa.starting_pointer,
                  vpa.enabled_pointers,
                  usesClientMemory ? 1 : 0);

    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        const bool enabled = ((vpa.enabled_pointers >> i) & 1) != 0;
        const GLint constantSize = size_for_attribute_index(fpeState.fpe_draw.current_data.sizes, i);
        if (!enabled && constantSize <= 0) {
            continue;
        }
        const auto& vp = vpa.attributes[i];
        SFPEWDebugLog(
            "FPE attr idx=%d cidx=%u enabled=%d usage=%s type=%s size=%d stride=%d normalized=%d ptr=%p constant_size=%d",
            i,
            vpa.cidx(i),
            enabled ? 1 : 0,
            glEnumToString(vp.usage),
            glEnumToString(vp.type),
            enabled ? vp.size : constantSize,
            vp.stride,
            vp.normalized,
            vp.pointer,
            constantSize);
    }

    if (usesClientMemory) {
        g_glFuncs.glBindBuffer(GL_ARRAY_BUFFER, fpeState.fpe_vbo);
        gls.backend_array_buffer_binding = static_cast<GLint>(fpeState.fpe_vbo);
        SFPEWDrainBackendErrors("fpe.bind_array_buffer");
    }

    // LOG_D("starting_ptr = %p", vpa.starting_pointer)
    // LOG_D("stride = %d", vpa.stride)

    gls.send_vertex_attributes(vpa);
    vpa.dirty = false;
    SFPEWDrainBackendErrors("fpe.send_vertex_attributes");

    int ret = 0;

    // Making sure it is a valid pointer rather than an offset into the buffer
    if (usesClientMemory) {
        g_glFuncs.glBufferData(GL_ARRAY_BUFFER,
                               static_cast<GLsizeiptr>(packedClientData.size()),
                               packedClientData.data(),
                               GL_DYNAMIC_DRAW);
        SFPEWDebugLog("FPE uploaded packed client memory bytes=%zu stride=%d",
                      packedClientData.size(),
                      vpa.stride);

    } else {
        SFPEWDebugLog("FPE using bound vertex buffer path");
    }

    if (*mode == GL_QUADS) {
        fill_quad_to_triangle_indices(fpeState.fpe_ib, *count);

        // LOG_D("glBufferData: size = %d, data = 0x%x -> GL_ELEMENT_ARRAY_BUFFER (%d)",
        //      g_glstate.fpe_state.fpe_ib.size() * sizeof(uint32_t), g_glstate.fpe_state.fpe_ib.data(),
        //      g_glstate.fpe_state.fpe_ibo)

        g_glFuncs.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fpeState.fpe_ibo);

        g_glFuncs.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                               static_cast<GLsizeiptr>(fpeState.fpe_ib.size() * sizeof(uint32_t)),
                               fpeState.fpe_ib.data(),
                               GL_DYNAMIC_DRAW);

        *count = fpeState.fpe_ib.size();

        *mode = GL_TRIANGLES;
        ret = 1;
        SFPEWDebugLog("FPE converted quads to triangles new_count=%d", *count);
    }

    gls.send_uniforms(prog);
    SFPEWDrainBackendErrors("fpe.send_uniforms");
    vpa.reset();
    //    vpa.starting_pointer = 0;
    //    vpa.stride = 0;
    return ret;
}
