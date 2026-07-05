// SimpleFPEWrapper - SimpleFPEWrapper/fpe/vertexpointerarray.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "types.h"

void vertex_pointer_array_t::reset() {
    starting_pointer = NULL;
    stride = 0;
    enabled_pointers = 0;
    dirty = false;
    buffer_based = false;
    memset(&attributes, 0, sizeof(attributes));
}

// Split into starting pointer & offset into buffer per pointer
vertex_pointer_array_t vertex_pointer_array_t::normalize() {
    vertex_pointer_array_t that = *this;
    int first_va_idx = -1;

    // Find starting pointer
    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        bool enabled = ((enabled_pointers >> i) & 1);
        if (!enabled) continue;

        first_va_idx = i;
        break;
    }

    if (stride == 0) that.stride = attributes[first_va_idx].stride;

    // If the pointers are real client-memory addresses, use the lowest enabled attribute pointer as the
    // interleaved vertex base. Picking the first enabled attribute is wrong for common layouts like
    // color/uv/vertex, where position data appears later in the struct than color or UV.
    if (!(that.stride != 0 && that.starting_pointer != 0 && that.starting_pointer > (void*)that.stride)) {
        if (that.stride > 0) {
            uint64_t min_pointer = UINT64_MAX;
            bool found_client_pointer = false;
            for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
                bool enabled = ((enabled_pointers >> i) & 1);
                if (!enabled) continue;

                const uint64_t pointer_value = reinterpret_cast<uint64_t>(attributes[i].pointer);
                if (pointer_value > static_cast<uint64_t>(that.stride) && pointer_value < min_pointer) {
                    min_pointer = pointer_value;
                    found_client_pointer = true;
                }
            }

            if (found_client_pointer) {
                that.starting_pointer = reinterpret_cast<const void*>(min_pointer);
            } else {
                that.starting_pointer = attributes[first_va_idx].pointer;
            }
        } else {
            that.starting_pointer = attributes[first_va_idx].pointer;
        }
    }

    // stride==0 && stride in pointer == 0
    // => tightly packed, infer stride from offset below
    bool do_calc_stride = (that.stride == 0);

    // Adjust pointer offsets according to starting pointer
    // Getting actual stride if stride==0
    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        bool enabled = ((enabled_pointers >> i) & 1);
        if (!enabled) continue;

        auto& vp = that.attributes[i];

        // check if pointer is a pointer rather than an offset
        if (that.stride > 0 && (uint64_t)vp.pointer > (uint64_t)that.stride &&
            (uint64_t)vp.pointer >= (uint64_t)that.starting_pointer)
            vp.pointer = (const void*)((const uint64_t)vp.pointer - (const uint64_t)that.starting_pointer);

        if (do_calc_stride)
            that.stride = std::max((uint64_t)stride, (uint64_t)vp.pointer + vp.size * type_size(vp.type));
    }

    // Overwrite `stride` in pointers
    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        bool enabled = ((enabled_pointers >> i) & 1);
        if (!enabled) continue;

        auto& vp = that.attributes[i];
        vp.stride = that.stride;
    }
    return that;
}

void vertex_pointer_array_t::generate_compressed_index(const GLint* constant_sizes) {
    // Should set the array to -1, or ~0u
    memset(compressed_index, -1, sizeof(compressed_index));

    GLuint index_count = 0;
    // Populate `compressed_index`
    // Save attribute index space for some devices
    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        bool enabled = ((enabled_pointers >> i) & 1);
        if (enabled || constant_sizes[i] > 0) {
            // An attribute is in use,
            // should generate an index entry
            compressed_index[i] = index_count;
            ++index_count;
        }
    }
}
