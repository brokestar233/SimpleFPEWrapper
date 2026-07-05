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

namespace {
    bool HasConstantAttributes(const vertex_pointer_array_t& va, const fixed_function_draw_size_t& sizes) {
        for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
            const bool enabled = ((va.enabled_pointers >> i) & 1) != 0;
            if (!enabled && size_for_attribute_index(sizes, i) > 0) {
                return true;
            }
        }
        return false;
    }

    uint64_t ComputeArrayBindingHash(const vertex_pointer_array_t& va) {
        XXHash64 hash(glstate_t::s_hash_seed);
        hash.add(&va.enabled_pointers, sizeof(va.enabled_pointers));
        hash.add(&va.stride, sizeof(va.stride));
        for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
            const bool enabled = ((va.enabled_pointers >> i) & 1) != 0;
            if (!enabled) {
                continue;
            }

            const auto& vp = va.attributes[i];
            const GLuint cidx = va.cidx(i);
            const uint64_t pointerValue = reinterpret_cast<uint64_t>(vp.pointer);
            hash.add(&i, sizeof(i));
            hash.add(&cidx, sizeof(cidx));
            hash.add(&vp.size, sizeof(vp.size));
            hash.add(&vp.type, sizeof(vp.type));
            hash.add(&vp.normalized, sizeof(vp.normalized));
            hash.add(&vp.stride, sizeof(vp.stride));
            hash.add(&pointerValue, sizeof(pointerValue));
        }
        return hash.hash();
    }
}

void glstate_t::send_uniforms(const program_t& program) {
    // LOG()

    const int programId = program.id();
    if (last_uniform_program_id == programId &&
        last_uniform_state_version == uniform_state_version) {
        return;
    }

    const auto& uniforms = program.uniform_locations();
    const auto& mv = fpe_uniform.transformation.matrices[matrix_idx(GL_MODELVIEW)];
    const auto& proj = fpe_uniform.transformation.matrices[matrix_idx(GL_PROJECTION)];
    // LOG_D("GL_MODELVIEW: ")
    print_matrix(mv);
    // LOG_D("GL_PROJECTION: ")
    print_matrix(proj);

    // TODO: detect change and only set dirty bits here
    g_glFuncs.glBindVertexArray(fpe_state.fpe_vao);
    backend_vertex_array_binding = static_cast<GLint>(fpe_state.fpe_vao);

    auto drainUniformStage = [&](const char* uniformName) {
        if (!SFPEWIsDebugLoggingEnabled()) {
            return;
        }
        char stage[128];
        std::snprintf(stage, sizeof(stage), "fpe.uniform.program=%d.%s", programId, uniformName);
        SFPEWDrainBackendErrors(stage);
    };

    const auto mat = proj * mv;
    if (uniforms.ModelViewMat >= 0) {
        g_glFuncs.glUniformMatrix4fv(uniforms.ModelViewMat, 1, GL_FALSE,
                                     glm::value_ptr(fpe_uniform.transformation.matrices[matrix_idx(GL_MODELVIEW)]));
        drainUniformStage("ModelViewMat");
    }

    if (uniforms.ModelViewProjMat >= 0) {
        g_glFuncs.glUniformMatrix4fv(uniforms.ModelViewProjMat, 1, GL_FALSE, glm::value_ptr(mat));
        drainUniformStage("ModelViewProjMat");
    }

    for (int i = 0; i < MAX_TEX; ++i) {
        if (uniforms.Samplers[i] >= 0) {
            g_glFuncs.glUniform1i(uniforms.Samplers[i], i);
            char name[32];
            std::snprintf(name, sizeof(name), "Sampler%d", i);
            drainUniformStage(name);
        }

        if (uniforms.TextureMatrices[i] >= 0) {
            g_glFuncs.glUniformMatrix4fv(uniforms.TextureMatrices[i],
                                         1,
                                         GL_FALSE,
                                         glm::value_ptr(fpe_uniform.transformation.texture_matrices[i]));
            char name[32];
            std::snprintf(name, sizeof(name), "TextureMat%d", i);
            drainUniformStage(name);
        }

        if (uniforms.TextureEnvColors[i] >= 0) {
            g_glFuncs.glUniform4fv(uniforms.TextureEnvColors[i], 1, glm::value_ptr(fpe_uniform.texture_env[i].color));
            char name[32];
            std::snprintf(name, sizeof(name), "TextureEnvColor%d", i);
            drainUniformStage(name);
        }

        if (uniforms.TextureEnvScales[i] >= 0) {
            const GLfloat scale[2] = {
                fpe_uniform.texture_env[i].rgb_scale,
                fpe_uniform.texture_env[i].alpha_scale,
            };
            g_glFuncs.glUniform2fv(uniforms.TextureEnvScales[i], 1, scale);
            char name[32];
            std::snprintf(name, sizeof(name), "TextureEnvScale%d", i);
            drainUniformStage(name);
        }
    }
    SFPEWDebugLog("FPE uniforms program=%d mv_loc=%d mvp_loc=%d alpha_test=%d fog=%d",
                  programId,
                  uniforms.ModelViewMat,
                  uniforms.ModelViewProjMat,
                  fpe_state.fpe_bools.alpha_test_enable ? 1 : 0,
                  fpe_state.fpe_bools.fog_enable ? 1 : 0);

    if (fpe_state.fpe_bools.fog_enable) {
        if (uniforms.FogColor >= 0) {
            g_glFuncs.glUniform4fv(uniforms.FogColor, 1, glm::value_ptr(fpe_uniform.fog_color));
            drainUniformStage("FogColor");
        }

        if (uniforms.FogDensity >= 0) {
            g_glFuncs.glUniform1f(uniforms.FogDensity, fpe_uniform.fog_density);
            drainUniformStage("FogDensity");
        }

        if (uniforms.FogStart >= 0) {
            g_glFuncs.glUniform1f(uniforms.FogStart, fpe_uniform.fog_start);
            drainUniformStage("FogStart");
        }

        if (uniforms.FogEnd >= 0) {
            g_glFuncs.glUniform1f(uniforms.FogEnd, fpe_uniform.fog_end);
            drainUniformStage("FogEnd");
        }
    }

    if (fpe_state.fpe_bools.lighting_enable) {
        if (uniforms.LightModelAmbient >= 0) {
            g_glFuncs.glUniform4fv(uniforms.LightModelAmbient, 1, glm::value_ptr(fpe_uniform.light_model_ambient));
            drainUniformStage("LightModelAmbient");
        }

        for (int i = 0; i < MAX_LIGHTS; ++i) {
            if (!fpe_state.fpe_bools.light_enable[i]) {
                continue;
            }

            if (uniforms.LightAmbient[i] >= 0) {
                g_glFuncs.glUniform4fv(uniforms.LightAmbient[i], 1, glm::value_ptr(fpe_uniform.lights[i].ambient));
                char name[32];
                std::snprintf(name, sizeof(name), "LightAmbient%d", i);
                drainUniformStage(name);
            }

            if (uniforms.LightDiffuse[i] >= 0) {
                g_glFuncs.glUniform4fv(uniforms.LightDiffuse[i], 1, glm::value_ptr(fpe_uniform.lights[i].diffuse));
                char name[32];
                std::snprintf(name, sizeof(name), "LightDiffuse%d", i);
                drainUniformStage(name);
            }

            if (uniforms.LightPositions[i] >= 0) {
                g_glFuncs.glUniform4fv(uniforms.LightPositions[i], 1, glm::value_ptr(fpe_uniform.lights[i].position));
                char name[32];
                std::snprintf(name, sizeof(name), "LightPosition%d", i);
                drainUniformStage(name);
            }
        }
    }

    if (fpe_state.fpe_bools.alpha_test_enable) {
        if (uniforms.AlphaRef >= 0) {
            g_glFuncs.glUniform1f(uniforms.AlphaRef, fpe_uniform.alpha_ref);
            drainUniformStage("alpharef");
        }
    }

    if (uniforms.CurrentColor >= 0) {
        g_glFuncs.glUniform4fv(uniforms.CurrentColor, 1, glm::value_ptr(fpe_state.fpe_draw.current_data.color));
        drainUniformStage("CurrentColor");
    }

    last_uniform_program_id = programId;
    last_uniform_state_version = uniform_state_version;
}

uint64_t glstate_t::program_hash(bool reset) {
    if (reset) {
        p_hash = XXHash64(s_hash_seed);
    }

    vertex_attrib_hash(true);

    auto& hash = p_hash;

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
        p_hash = XXHash64(s_hash_seed);
    }

    auto& hash = p_hash;

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

bool glstate_t::send_vertex_attributes(const vertex_pointer_array_t& va) {
    // LOG()

    //    auto& va = fpe_state.vertexpointer_array;
    if (!va.dirty) return false;

    const bool hasConstantAttributes = HasConstantAttributes(va, fpe_state.fpe_draw.current_data.sizes);
    if (!hasConstantAttributes) {
        const uint64_t arrayBindingHash = ComputeArrayBindingHash(va);
        if (last_array_binding_hash_valid &&
            last_array_binding_vao == fpe_state.fpe_vao &&
            last_array_binding_hash == arrayBindingHash) {
            return false;
        }
        last_array_binding_hash = arrayBindingHash;
        last_array_binding_vao = fpe_state.fpe_vao;
        last_array_binding_hash_valid = true;
    } else {
        last_array_binding_hash_valid = false;
    }

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
