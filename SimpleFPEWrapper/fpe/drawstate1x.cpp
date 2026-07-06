// SimpleFPEWrapper - SimpleFPEWrapper/fpe/drawstate1x.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "types.h"
#include <cstring>

#define DEBUG 0

namespace {
    std::size_t ComputeVertexStrideBytes(const fixed_function_draw_size_t& sizes) {
        std::size_t strideBytes = 0;

        if (sizes.vertex_size > 0) {
            strideBytes += sizeof(GLfloat) * static_cast<std::size_t>(sizes.vertex_size);
        }
        if (sizes.normal_size > 0) {
            strideBytes += sizeof(GLfloat) * static_cast<std::size_t>(sizes.normal_size);
        }
        if (sizes.color_size > 0) {
            strideBytes += sizeof(GLfloat) * static_cast<std::size_t>(sizes.color_size);
        }
        for (GLint i = 0; i < MAX_TEX; ++i) {
            if (sizes.texcoord_size[i] > 0) {
                strideBytes += sizeof(GLfloat) * static_cast<std::size_t>(sizes.texcoord_size[i]);
            }
        }

        return strideBytes;
    }

    bool IsVertex3Tex2OnlyLayout(const fixed_function_draw_size_t& sizes) {
        if (sizes.vertex_size != 3 ||
            sizes.normal_size != 0 ||
            sizes.color_size != 0 ||
            sizes.index_size != 0 ||
            sizes.edge_size != 0 ||
            sizes.fog_size != 0 ||
            sizes.secondary_color_size != 0 ||
            sizes.texcoord_size[0] != 2) {
            return false;
        }

        for (GLint i = 1; i < MAX_TEX; ++i) {
            if (sizes.texcoord_size[i] != 0) {
                return false;
            }
        }
        return true;
    }
}

void fixed_function_draw_state_t::reset() {
    primitive = GL_NONE;
    vertex_count = 0;
    vb.clear();
}

void fixed_function_draw_state_t::advance() {
    ++vertex_count;

    const auto& sizes = current_data.sizes;
    if (!cached_vertex_stride_valid ||
        std::memcmp(&cached_vertex_stride_sizes, &sizes, sizeof(sizes)) != 0) {
        cached_vertex_stride_sizes = sizes;
        cached_vertex_stride_bytes = ComputeVertexStrideBytes(sizes);
        cached_vertex_stride_valid = true;
    }

    const std::size_t vertexStrideBytes = cached_vertex_stride_bytes;
    if (vertexStrideBytes == 0) {
        return;
    }

    const std::size_t offset = vb.size();
    vb.resize(offset + vertexStrideBytes);
    auto* cursor = vb.data() + offset;

    if (IsVertex3Tex2OnlyLayout(sizes)) {
        auto* dst = reinterpret_cast<GLfloat*>(cursor);
        const auto* position = glm::value_ptr(current_data.vertex);
        const auto* texcoord0 = glm::value_ptr(current_data.texcoord[0]);
        dst[0] = position[0];
        dst[1] = position[1];
        dst[2] = position[2];
        dst[3] = texcoord0[0];
        dst[4] = texcoord0[1];
        return;
    }

    // vertex
    if (sizes.vertex_size > 0) {
        const std::size_t byteCount = sizeof(GLfloat) * static_cast<std::size_t>(sizes.vertex_size);
        std::memcpy(cursor, glm::value_ptr(current_data.vertex), byteCount);
        cursor += byteCount;
    }

    // normal
    if (sizes.normal_size > 0) {
        const std::size_t byteCount = sizeof(GLfloat) * static_cast<std::size_t>(sizes.normal_size);
        std::memcpy(cursor, glm::value_ptr(current_data.normal), byteCount);
        cursor += byteCount;
    }

    // color
    if (sizes.color_size > 0) {
        const std::size_t byteCount = sizeof(GLfloat) * static_cast<std::size_t>(sizes.color_size);
        std::memcpy(cursor, glm::value_ptr(current_data.color), byteCount);
        cursor += byteCount;
    }

    // texcoord
    for (GLint i = 0; i < MAX_TEX; ++i) {
        if (sizes.texcoord_size[i] > 0) {
            const std::size_t byteCount = sizeof(GLfloat) * static_cast<std::size_t>(sizes.texcoord_size[i]);
            std::memcpy(cursor, glm::value_ptr(current_data.texcoord[i]), byteCount);
            cursor += byteCount;
        }
    }

    // LOG_D("advance(): vertexcount = %d, vbsize = %d", vertex_count, vb.str().size())
}

void fixed_function_draw_state_t::compile_vertexattrib(vertex_pointer_array_t& va) const {
    va.reset();

    va.dirty = true;
    va.buffer_based = false;

    const auto& sizes = current_data.sizes;
    GLsizei offset = 0;

    // vertex
    if (sizes.vertex_size > 0) {
        va.enabled_pointers |= vp_mask(GL_VERTEX_ARRAY);

        va.attributes[vp2idx(GL_VERTEX_ARRAY)] = {
            .size = sizes.vertex_size,
            .usage = GL_VERTEX_ARRAY,
            .type = GL_FLOAT,
            .normalized = GL_FALSE,
            .stride = 0,
            .pointer = (const void*)offset,
            //                .varying = true
        };
        offset += sizes.vertex_size * sizeof(GLfloat);
    }

    // normal
    if (sizes.normal_size > 0) {
        va.enabled_pointers |= vp_mask(GL_NORMAL_ARRAY);

        va.attributes[vp2idx(GL_NORMAL_ARRAY)] = {
            .size = sizes.normal_size,
            .usage = GL_NORMAL_ARRAY,
            .type = GL_FLOAT,
            .normalized = GL_FALSE,
            .stride = 0,
            .pointer = (const void*)offset,
            //                .varying = true
        };
        offset += sizes.normal_size * sizeof(GLfloat);
    }

    // color
    if (sizes.color_size > 0) {
        va.enabled_pointers |= vp_mask(GL_COLOR_ARRAY);
        va.attributes[vp2idx(GL_COLOR_ARRAY)] = {
            .size = sizes.color_size,
            .usage = GL_COLOR_ARRAY,
            .type = GL_FLOAT,
            .normalized = GL_FALSE,
            .stride = 0,
            .pointer = (const void*)offset,
            //                .varying = true
        };
        offset += sizes.color_size * sizeof(GLfloat);
    }

    // texcoord
    for (GLint i = 0; i < MAX_TEX; ++i) {
        if (sizes.texcoord_size[i] > 0) {
            // TODO: fix vp_mask()/vp2idx(), make it adapt to here
            va.enabled_pointers |= (1 << (7 + i));
            va.attributes[7 + i] = {
                .size = sizes.texcoord_size[i],
                .usage = GL_TEXTURE_COORD_ARRAY,
                .type = GL_FLOAT,
                .normalized = GL_FALSE,
                .stride = 0,
                .pointer = (const void*)offset,
                //                    .varying = true
            };
            offset += sizes.texcoord_size[i] * sizeof(GLfloat);
        }
    }

    va.stride = offset;
}
