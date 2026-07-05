// SimpleFPEWrapper - SimpleFPEWrapper/drawing.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "init.h"

#include "fpe/fpe.hpp"
#include "fpe/list.h"
#include "fpe/pointer_utils.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace {

bool ShouldUseFpeDrawArrays() {
    if (!g_sfpewCompatMode) {
        return false;
    }

    const auto& vpa = g_glstate.fpe_state.vertexpointer_array;
    const bool hasVertexArray = (vpa.enabled_pointers & 0x1u) != 0;
    if (hasVertexArray) {
        return true;
    }

    // If there is no fixed-function position array, this draw is much more likely to belong to the
    // programmable pipeline path that uses generic vertex attributes. Feeding it into FPE generates
    // a shader that references `Position` without declaring it and blows up on newer versions.
    return false;
}

bool AttributeUsesClientMemoryForDisplayList(const vertexattribute_t& attr) {
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

bool AnyEnabledAttributeUsesClientMemoryForDisplayList(const vertex_pointer_array_t& vpa) {
    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        const bool enabled = ((vpa.enabled_pointers >> i) & 1) != 0;
        if (!enabled) {
            continue;
        }

        if (AttributeUsesClientMemoryForDisplayList(vpa.attributes[i])) {
            return true;
        }
    }
    return false;
}

bool BuildPackedClientArrayLayoutForDisplayList(const vertex_pointer_array_t& source,
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

void sfpew_list_glDrawArrays(GLenum mode, GLint first, GLsizei count);

class DisplayListDrawArraysSnapshotCmd final : public GLCmd {
public:
    DisplayListDrawArraysSnapshotCmd(GLenum mode,
                                     GLsizei count,
                                     vertex_pointer_array_t layout,
                                     fixed_function_draw_data_t currentData,
                                     std::vector<std::uint8_t> data)
        : mode_(mode), count_(count), layout_(std::move(layout)), currentData_(std::move(currentData)),
          data_(std::move(data)) {}

    void execute() const override {
        auto saved = g_glstate.fpe_state.vertexpointer_array;
        auto savedCurrentData = g_glstate.fpe_state.fpe_draw.current_data;
        auto snapshot = layout_;
        const auto* base = data_.data();
        for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
            const bool enabled = ((snapshot.enabled_pointers >> i) & 1) != 0;
            if (!enabled) {
                continue;
            }

            const std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(snapshot.attributes[i].pointer);
            snapshot.attributes[i].pointer = base + offset;
        }
        snapshot.starting_pointer = base;
        snapshot.dirty = true;
        snapshot.buffer_based = true;

        g_glstate.fpe_state.vertexpointer_array = snapshot;
        g_glstate.fpe_state.fpe_draw.current_data = currentData_;
        sfpew_list_glDrawArrays(mode_, 0, count_);
        g_glstate.fpe_state.fpe_draw.current_data = savedCurrentData;
        g_glstate.fpe_state.vertexpointer_array = saved;
    }

private:
    GLenum mode_;
    GLsizei count_;
    vertex_pointer_array_t layout_;
    fixed_function_draw_data_t currentData_;
    std::vector<std::uint8_t> data_;
};

bool TryRecordDisplayListDrawArraysSnapshot(GLenum mode, GLint first, GLsizei count) {
    const auto& recordingState = DisplayListManager::recordingStateRef();
    const auto& rawVpa = recordingState.vertexpointer_array;
    if (!AnyEnabledAttributeUsesClientMemoryForDisplayList(rawVpa)) {
        return false;
    }

    const auto attributeSizes = build_attribute_size_table(recordingState.current_data.sizes);
    vertex_pointer_array_t packedLayout;
    std::vector<std::uint8_t> packedData;
    if (!BuildPackedClientArrayLayoutForDisplayList(rawVpa, first, count, attributeSizes.data(), packedLayout, packedData)) {
        return false;
    }

    DisplayListManager::recordCustom(
        std::make_unique<DisplayListDrawArraysSnapshotCmd>(
            mode, count, std::move(packedLayout), recordingState.current_data, std::move(packedData)));
    SFPEWDebugLog("DISPLAY_LIST snapshotted glDrawArrays mode=%s first=%d count=%d bytes=%zu",
                  glEnumToString(mode),
                  first,
                  count,
                  packedData.size());
    return true;
}

bool ReadElementIndex(GLenum type, const void* indices, GLsizei elementIndex, GLuint& outIndex) {
    if (indices == nullptr || elementIndex < 0) {
        return false;
    }

    switch (type) {
    case GL_UNSIGNED_BYTE:
        outIndex = reinterpret_cast<const GLubyte*>(indices)[elementIndex];
        return true;
    case GL_UNSIGNED_SHORT:
        outIndex = reinterpret_cast<const GLushort*>(indices)[elementIndex];
        return true;
    case GL_UNSIGNED_INT:
        outIndex = reinterpret_cast<const GLuint*>(indices)[elementIndex];
        return true;
    default:
        return false;
    }
}

bool BuildPackedIndexedClientArrayLayout(const vertex_pointer_array_t& source,
                                         GLenum indexType,
                                         const void* indices,
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
        GLuint sourceVertexIndex = 0;
        if (!ReadElementIndex(indexType, indices, vertexIndex, sourceVertexIndex)) {
            return false;
        }

        const std::size_t destVertexBase = static_cast<std::size_t>(vertexIndex) * packedStride;
        for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
            const bool enabled = ((packedLayout.enabled_pointers >> i) & 1) != 0;
            if (!enabled) {
                continue;
            }

            const auto& copy = copies[i];
            std::memcpy(packedData.data() + destVertexBase + copy.destOffset,
                        copy.sourceBase + static_cast<std::size_t>(sourceVertexIndex) * copy.sourceStride,
                        copy.elementSize);
        }
    }
    return true;
}

bool TryRecordDisplayListDrawElementsSnapshot(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    const auto& recordingState = DisplayListManager::recordingStateRef();
    const auto& rawVpa = recordingState.vertexpointer_array;
    if (!AnyEnabledAttributeUsesClientMemoryForDisplayList(rawVpa)) {
        return false;
    }

    const auto attributeSizes = build_attribute_size_table(recordingState.current_data.sizes);
    vertex_pointer_array_t packedLayout;
    std::vector<std::uint8_t> packedData;
    if (!BuildPackedIndexedClientArrayLayout(rawVpa, type, indices, count, attributeSizes.data(), packedLayout,
                                             packedData)) {
        return false;
    }

    DisplayListManager::recordCustom(
        std::make_unique<DisplayListDrawArraysSnapshotCmd>(
            mode, count, std::move(packedLayout), recordingState.current_data, std::move(packedData)));
    return true;
}

void sfpew_list_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    SFPEWDebugLog("DRAWARRAYS enter mode=%s first=%d count=%d compat=%d",
                  glEnumToString(mode),
                  first,
                  count,
                  g_sfpewCompatMode ? 1 : 0);
    if (!ShouldUseFpeDrawArrays()) {
        SFPEWDebugLog("DRAWARRAYS bypass_fpe mode=%s first=%d count=%d enabled=0x%x",
                      glEnumToString(mode),
                      first,
                      count,
                      g_glstate.fpe_state.vertexpointer_array.enabled_pointers);
        g_glFuncs.glDrawArrays(mode, first, count);
        SFPEWDrainBackendErrors("glDrawArrays.bypass");
        return;
    }
    GET_PREV_PROGRAM
    GLint prev_vao = 0;
    GLint prev_vbo = 0;
    g_glFuncs.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    g_glFuncs.glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    int do_draw_element = commit_fpe_state_on_draw(&mode, &first, &count);
    SFPEWDebugLog("DRAWARRAYS prepared mode=%s first=%d count=%d draw_elements=%d prev_vao=%d prev_vbo=%d prev_program=%d",
                  glEnumToString(mode),
                  first,
                  count,
                  do_draw_element,
                  prev_vao,
                  prev_vbo,
                  m_prev_program);
    SFPEWDrainBackendErrors("glDrawArrays.prepare");
    if (do_draw_element) {
        g_glFuncs.glDrawElements(mode, count, GL_UNSIGNED_INT, (void*)0);
    } else {
        g_glFuncs.glDrawArrays(mode, first, count);
    }

    SFPEWDrainBackendErrors("glDrawArrays.draw");
    g_glFuncs.glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    g_glFuncs.glBindVertexArray(prev_vao);
    SET_PREV_PROGRAM
    SFPEWDrainBackendErrors("glDrawArrays.restore");
}

}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        if (!TryRecordDisplayListDrawArraysSnapshot(mode, first, count)) {
            displayListManager.record<sfpew_list_glDrawArrays>({}, mode, first, count);
        }
        if (DisplayListManager::shouldFinish()) return;
    }

    sfpew_list_glDrawArrays(mode, first, count);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        TryRecordDisplayListDrawElementsSnapshot(mode, count, type, indices);
        if (DisplayListManager::shouldFinish()) return;
    }

    if (!ShouldUseFpeDrawArrays()) {
        g_glFuncs.glDrawElements(mode, count, type, indices);
        SFPEWDrainBackendErrors("glDrawElements.bypass");
        return;
    }

    const auto& rawVpa = g_glstate.fpe_state.vertexpointer_array;
    if (!AnyEnabledAttributeUsesClientMemoryForDisplayList(rawVpa)) {
        g_glFuncs.glDrawElements(mode, count, type, indices);
        SFPEWDrainBackendErrors("glDrawElements.no_client_memory");
        return;
    }

    const auto attributeSizes = build_attribute_size_table(g_glstate.fpe_state.fpe_draw.current_data.sizes);
    vertex_pointer_array_t packedLayout;
    std::vector<std::uint8_t> packedData;
    if (!BuildPackedIndexedClientArrayLayout(rawVpa, type, indices, count, attributeSizes.data(), packedLayout,
                                             packedData)) {
        g_glFuncs.glDrawElements(mode, count, type, indices);
        SFPEWDrainBackendErrors("glDrawElements.build_failed");
        return;
    }

    auto savedVpa = g_glstate.fpe_state.vertexpointer_array;
    auto savedNormalizedVpa = g_glstate.fpe_state.normalized_vpa;
    const auto* base = packedData.data();
    for (int i = 0; i < VERTEX_POINTER_COUNT; ++i) {
        const bool enabled = ((packedLayout.enabled_pointers >> i) & 1) != 0;
        if (!enabled) {
            continue;
        }

        const std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(packedLayout.attributes[i].pointer);
        packedLayout.attributes[i].pointer = base + offset;
    }
    packedLayout.starting_pointer = base;
    packedLayout.dirty = true;
    packedLayout.buffer_based = true;

    g_glstate.fpe_state.vertexpointer_array = packedLayout;
    sfpew_list_glDrawArrays(mode, 0, count);
    g_glstate.fpe_state.normalized_vpa = savedNormalizedVpa;
    g_glstate.fpe_state.vertexpointer_array = savedVpa;
}
