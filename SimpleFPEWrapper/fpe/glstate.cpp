// SimpleFPEWrapper - SimpleFPEWrapper/fpe/glstate.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "types.h"
#include "transformation.h"
#include "../init.h"
#include <cstdio>
#define DEBUG 0

void glstate_t::send_uniforms(int program) {
    // LOG()

    const auto& mv = fpe_uniform.transformation.matrices[matrix_idx(GL_MODELVIEW)];
    const auto& proj = fpe_uniform.transformation.matrices[matrix_idx(GL_PROJECTION)];
    // LOG_D("GL_MODELVIEW: ")
    print_matrix(mv);
    // LOG_D("GL_PROJECTION: ")
    print_matrix(proj);

    // TODO: detect change and only set dirty bits here
    g_glFuncs.glBindVertexArray(fpe_state.fpe_vao);

    auto drainUniformStage = [](const std::string& stage) {
        SFPEWDrainBackendErrors(stage.c_str());
    };

    GLint mvmat = g_glFuncs.glGetUniformLocation(program, "ModelViewMat");

    //    GLint projmat = g_glFuncs.glGetUniformLocation(program, "ProjMat");
    //
    GLint mat_id = g_glFuncs.glGetUniformLocation(program, "ModelViewProjMat");

    const auto mat = proj * mv;
    if (mvmat >= 0) {
        g_glFuncs.glUniformMatrix4fv(mvmat, 1, GL_FALSE,
                                     glm::value_ptr(fpe_uniform.transformation.matrices[matrix_idx(GL_MODELVIEW)]));
        drainUniformStage(std::format("fpe.uniform.program={}.ModelViewMat", program));
    }

    //    g_glFuncs.glUniformMatrix4fv(projmat, 1, GL_FALSE,
    //    glm::value_ptr(fpe_uniform.transformation.matrices[matrix_idx(GL_PROJECTION)]));
    if (mat_id >= 0) {
        g_glFuncs.glUniformMatrix4fv(mat_id, 1, GL_FALSE, glm::value_ptr(mat));
        drainUniformStage(std::format("fpe.uniform.program={}.ModelViewProjMat", program));
    }

    for (int i = 0; i < MAX_TEX; ++i) {
        const std::string uniformName = std::format("Sampler{}", i);
        const GLint samplerLocation = g_glFuncs.glGetUniformLocation(program, uniformName.c_str());
        if (samplerLocation >= 0) {
            g_glFuncs.glUniform1i(samplerLocation, i);
            drainUniformStage(std::format("fpe.uniform.program={}.{}", program, uniformName));
        }

        const std::string textureMatrixName = std::format("TextureMat{}", i);
        const GLint textureMatrixLocation = g_glFuncs.glGetUniformLocation(program, textureMatrixName.c_str());
        if (textureMatrixLocation >= 0) {
            g_glFuncs.glUniformMatrix4fv(textureMatrixLocation,
                                         1,
                                         GL_FALSE,
                                         glm::value_ptr(fpe_uniform.transformation.texture_matrices[i]));
            drainUniformStage(std::format("fpe.uniform.program={}.{}", program, textureMatrixName));
        }

        const std::string textureEnvColorName = std::format("TextureEnvColor{}", i);
        const GLint textureEnvColorLocation = g_glFuncs.glGetUniformLocation(program, textureEnvColorName.c_str());
        if (textureEnvColorLocation >= 0) {
            g_glFuncs.glUniform4fv(textureEnvColorLocation, 1, glm::value_ptr(fpe_uniform.texture_env[i].color));
            drainUniformStage(std::format("fpe.uniform.program={}.{}", program, textureEnvColorName));
        }

        const std::string textureEnvScaleName = std::format("TextureEnvScale{}", i);
        const GLint textureEnvScaleLocation = g_glFuncs.glGetUniformLocation(program, textureEnvScaleName.c_str());
        if (textureEnvScaleLocation >= 0) {
            const GLfloat scale[2] = {
                fpe_uniform.texture_env[i].rgb_scale,
                fpe_uniform.texture_env[i].alpha_scale,
            };
            g_glFuncs.glUniform2fv(textureEnvScaleLocation, 1, scale);
            drainUniformStage(std::format("fpe.uniform.program={}.{}", program, textureEnvScaleName));
        }
    }
    SFPEWDebugLog("FPE uniforms program=%d mv_loc=%d mvp_loc=%d alpha_test=%d fog=%d",
                  program,
                  mvmat,
                  mat_id,
                  fpe_state.fpe_bools.alpha_test_enable ? 1 : 0,
                  fpe_state.fpe_bools.fog_enable ? 1 : 0);

    if (fpe_state.fpe_bools.fog_enable) {
        GLint fogcolor_id = g_glFuncs.glGetUniformLocation(program, "FogColor");

        // LOG_D("fogcolor_id = %d", fogcolor_id)
        if (fogcolor_id >= 0) {
            g_glFuncs.glUniform4fv(fogcolor_id, 1, glm::value_ptr(fpe_uniform.fog_color));
            drainUniformStage(std::format("fpe.uniform.program={}.FogColor", program));
        }

        GLint fogdensity_id = g_glFuncs.glGetUniformLocation(program, "FogDensity");

        if (fogdensity_id >= 0) {
            g_glFuncs.glUniform1f(fogdensity_id, fpe_uniform.fog_density);
            drainUniformStage(std::format("fpe.uniform.program={}.FogDensity", program));
        }

        GLint fogstart_id = g_glFuncs.glGetUniformLocation(program, "FogStart");

        if (fogstart_id >= 0) {
            g_glFuncs.glUniform1f(fogstart_id, fpe_uniform.fog_start);
            drainUniformStage(std::format("fpe.uniform.program={}.FogStart", program));
        }

        GLint fogend_id = g_glFuncs.glGetUniformLocation(program, "FogEnd");

        if (fogend_id >= 0) {
            g_glFuncs.glUniform1f(fogend_id, fpe_uniform.fog_end);
            drainUniformStage(std::format("fpe.uniform.program={}.FogEnd", program));
        }
    }

    if (fpe_state.fpe_bools.lighting_enable) {
        GLint lightModelAmbientId = g_glFuncs.glGetUniformLocation(program, "LightModelAmbient");
        if (lightModelAmbientId >= 0) {
            g_glFuncs.glUniform4fv(lightModelAmbientId, 1, glm::value_ptr(fpe_uniform.light_model_ambient));
            drainUniformStage(std::format("fpe.uniform.program={}.LightModelAmbient", program));
        }

        for (int i = 0; i < MAX_LIGHTS; ++i) {
            if (!fpe_state.fpe_bools.light_enable[i]) {
                continue;
            }

            const std::string ambientName = std::format("LightAmbient{}", i);
            const GLint ambientLocation = g_glFuncs.glGetUniformLocation(program, ambientName.c_str());
            if (ambientLocation >= 0) {
                g_glFuncs.glUniform4fv(ambientLocation, 1, glm::value_ptr(fpe_uniform.lights[i].ambient));
                drainUniformStage(std::format("fpe.uniform.program={}.{}", program, ambientName));
            }

            const std::string diffuseName = std::format("LightDiffuse{}", i);
            const GLint diffuseLocation = g_glFuncs.glGetUniformLocation(program, diffuseName.c_str());
            if (diffuseLocation >= 0) {
                g_glFuncs.glUniform4fv(diffuseLocation, 1, glm::value_ptr(fpe_uniform.lights[i].diffuse));
                drainUniformStage(std::format("fpe.uniform.program={}.{}", program, diffuseName));
            }

            const std::string positionName = std::format("LightPosition{}", i);
            const GLint positionLocation = g_glFuncs.glGetUniformLocation(program, positionName.c_str());
            if (positionLocation >= 0) {
                g_glFuncs.glUniform4fv(positionLocation, 1, glm::value_ptr(fpe_uniform.lights[i].position));
                drainUniformStage(std::format("fpe.uniform.program={}.{}", program, positionName));
            }
        }
    }

    if (fpe_state.fpe_bools.alpha_test_enable) {
        GLint alpharef_id = g_glFuncs.glGetUniformLocation(program, "alpharef");

        if (alpharef_id >= 0) {
            g_glFuncs.glUniform1f(alpharef_id, fpe_uniform.alpha_ref);
            drainUniformStage(std::format("fpe.uniform.program={}.alpharef", program));
        }
    }

    GLint currentColorId = g_glFuncs.glGetUniformLocation(program, "CurrentColor");
    if (currentColorId >= 0) {
        g_glFuncs.glUniform4fv(currentColorId, 1, glm::value_ptr(fpe_state.fpe_draw.current_data.color));
        drainUniformStage(std::format("fpe.uniform.program={}.CurrentColor", program));
    }
}

uint64_t glstate_t::program_hash(bool reset) {
    if (reset) {
        p_hash.reset();
        p_hash = std::make_unique<XXHash64>(s_hash_seed);
    }

    vertex_attrib_hash(true);

    auto& hash = *p_hash;

    hash.add(&fpe_state.client_active_texture, sizeof(fpe_state.client_active_texture));
    hash.add(&fpe_state.alpha_func, sizeof(fpe_state.alpha_func));
    hash.add(&fpe_state.fog_mode, sizeof(fpe_state.fog_mode));
    hash.add(&fpe_state.fog_index, sizeof(fpe_state.fog_index));
    hash.add(&fpe_state.fog_coord_src, sizeof(fpe_state.fog_coord_src));
    hash.add(&fpe_state.shade_model, sizeof(fpe_state.shade_model));
    hash.add(&fpe_state.light_model_color_ctrl, sizeof(fpe_state.light_model_color_ctrl));
    hash.add(&fpe_state.light_model_local_viewer, sizeof(fpe_state.light_model_local_viewer));
    hash.add(&fpe_state.light_model_two_side, sizeof(fpe_state.light_model_two_side));
    hash.add(&fpe_state.texture_env, sizeof(fpe_state.texture_env));

    hash.add(&fpe_state.fpe_bools, sizeof(fpe_state.fpe_bools));

    uint64_t key = hash.hash();

    return key;
}

uint64_t glstate_t::vertex_attrib_hash(bool reset) {
    if (reset) {
        p_hash.reset();
        p_hash = std::make_unique<XXHash64>(s_hash_seed);
    }

    auto& hash = *p_hash;

    auto va = fpe_state.vertexpointer_array.normalize();

    //    hash.add(&va.starting_pointer, sizeof(va.starting_pointer));
    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        bool enabled = ((va.enabled_pointers >> i) & 1);
        const GLint constantSize = size_for_attribute_index(fpe_state.fpe_draw.current_data.sizes, i);
        if (enabled || constantSize > 0) {
            hash.add(&i, sizeof(i));
            hash.add(&enabled, sizeof(enabled));
            auto& attr = va.attributes[i];

            if (enabled)
                hash.add(&attr.size, sizeof(attr.size));
            else
                hash.add(&constantSize, sizeof(constantSize));

            hash.add(&attr.usage, sizeof(attr.usage));

            if (enabled) {
                hash.add(&attr.type, sizeof(attr.type));
                hash.add(&attr.normalized, sizeof(attr.normalized));
                // Program variants depend on attribute semantics, not transient client-memory addresses.
                // Hashing `pointer` makes the cache miss on nearly every immediate-mode draw.
            } else {
                const GLenum t = GL_FLOAT;
                hash.add(&t, sizeof(t));
            }
        }
    }

    uint64_t result = hash.hash();
    return result;
}

program_t& glstate_t::get_or_generate_program(const uint64_t key) {
    // LOG()
    if (fpe_programs.find(key) == fpe_programs.end()) {
        // LOG_D("Generating new shader: 0x%x", key)
        fpe_shader_generator gen(fpe_state);
        program_t program = gen.generate_program();
        SFPEWDebugLog("FPE program generate key=0x%llx\nVS:\n%s\nFS:\n%s",
                      static_cast<unsigned long long>(key),
                      program.vs.c_str(),
                      program.fs.c_str());
        int prog_id = program.get_program();
        if (prog_id > 0)
            fpe_programs[key] = program;
        else {
            // LOG_D("Error: FPE shader link failed!")
            // reserve key==0 as null program for failure
            return fpe_programs[0];
        }
    } else {
        // LOG_D("Using existing shader: 0x%x", key)
    }

    auto& prog = fpe_programs[key];
    return prog;
}

bool glstate_t::get_vao(const uint64_t key, GLuint* vao) {
    // LOG()
    if (fpe_vaos.find(key) == fpe_vaos.end()) {
        return false;
    }

    if (vao) *vao = fpe_vaos[key];
    return true;
}

void glstate_t::save_vao(const uint64_t key, const GLuint vao) {
    // LOG()
    fpe_vaos[key] = vao;
}

bool glstate_t::send_vertex_attributes(const vertex_pointer_array_t& va) const {
    // LOG()

    //    auto& va = fpe_state.vertexpointer_array;
    if (!va.dirty) return false;

    auto drainAttrStage = [](const char* op, GLuint cidx, GLenum usage) {
        if (!SFPEWIsDebugLoggingEnabled()) {
            return;
        }

        char stage[96];
        std::snprintf(stage, sizeof(stage), "fpe.%s.cidx=%u.usage=%s", op, cidx, glEnumToString(usage));
        SFPEWDrainBackendErrors(stage);
    };

    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        bool enabled = ((va.enabled_pointers >> i) & 1);

        auto& vp = va.attributes[i];
        if (enabled) {
            SFPEWDebugLog("FPE attrib pointer idx=%d cidx=%u usage=%s type=%s size=%d stride=%d normalized=%d ptr=%p",
                          i,
                          va.cidx(i),
                          glEnumToString(vp.usage),
                          glEnumToString(vp.type),
                          vp.size,
                          vp.stride,
                          vp.normalized,
                          vp.pointer);
            g_glFuncs.glVertexAttribPointer(va.cidx(i), vp.size, vp.type, vp.normalized, vp.stride, vp.pointer);
            drainAttrStage("vertex_attrib_pointer", va.cidx(i), vp.usage);

            g_glFuncs.glEnableVertexAttribArray(va.cidx(i));
            drainAttrStage("enable_vertex_attrib_array", va.cidx(i), vp.usage);

            // LOG_D("attrib #%d, cidx #%u: type = %s, size = %d, stride = %d, usage = %s, ptr = %p", i, va.cidx(i),
            //      glEnumToString(vp.type), vp.size, vp.stride, glEnumToString(vp.usage), vp.pointer)
        } else {
            switch (idx2vp(i)) {
            case GL_COLOR_ARRAY:
                if (fpe_state.fpe_draw.current_data.sizes.color_size > 0) {
                    const auto& v = fpe_state.fpe_draw.current_data.color;
                    SFPEWDebugLog("FPE attrib const color cidx=%u value=(%.3f, %.3f, %.3f, %.3f)",
                                  va.cidx(i), v[0], v[1], v[2], v[3]);
                    // LOG_D("attrib #%d, cidx #%u: type = %s, usage = %s, value = (%.2f, %.2f, %.2f, %.2f) (disabled)",
                    // i,
                    //      va.cidx(i), glEnumToString(vp.type), glEnumToString(vp.usage), v[0], v[1], v[2], v[3])

                    g_glFuncs.glVertexAttrib4fv(va.cidx(i), glm::value_ptr(v));
                    drainAttrStage("vertex_attrib4fv", va.cidx(i), vp.usage);
                }
                break;
            case GL_NORMAL_ARRAY:
                if (fpe_state.fpe_draw.current_data.sizes.normal_size > 0) {
                    const auto& v = fpe_state.fpe_draw.current_data.normal;
                    SFPEWDebugLog("FPE attrib const normal cidx=%u value=(%.3f, %.3f, %.3f)",
                                  va.cidx(i), v[0], v[1], v[2]);
                    // LOG_D("attrib #%d, cidx #%u: type = %s, usage = %s, value = (%.2f, %.2f, %.2f) (disabled)", i,
                    //      va.cidx(i), glEnumToString(vp.type), glEnumToString(vp.usage), v[0], v[1], v[2])

                    g_glFuncs.glVertexAttrib3fv(va.cidx(i), glm::value_ptr(v));
                    drainAttrStage("vertex_attrib3fv", va.cidx(i), vp.usage);
                }
                break;
            case GL_TEXTURE_COORD_ARRAY: {
                const int texIndex = i - 7;
                if (texIndex >= 0 && texIndex < MAX_TEX &&
                    fpe_state.fpe_draw.current_data.sizes.texcoord_size[texIndex] > 0) {
                    const auto& v = fpe_state.fpe_draw.current_data.texcoord[texIndex];
                    g_glFuncs.glVertexAttrib4fv(va.cidx(i), glm::value_ptr(v));
                    drainAttrStage("vertex_attrib4fv", va.cidx(i), vp.usage);
                }
                break;
            }
            default:
                // LOG_D("attrib #%d: (disabled)", i)
                break;
            }

            if (va.cidx(i) != ~0u) {
                g_glFuncs.glDisableVertexAttribArray(va.cidx(i));
                drainAttrStage("disable_vertex_attrib_array", va.cidx(i), vp.usage);
            }
        }
    }

    return true;
}
