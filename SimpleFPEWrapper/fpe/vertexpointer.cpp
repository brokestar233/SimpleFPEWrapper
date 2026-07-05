// SimpleFPEWrapper - SimpleFPEWrapper/fpe/vertexpointer.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "vertexpointer.h"
#include "list.h"
#include "fpe.hpp"

#define DEBUG 0

namespace {

int recording_texcoord_index(GLenum client_active_texture) {
    if (client_active_texture < GL_TEXTURE0 || client_active_texture >= GL_TEXTURE0 + MAX_TEX) {
        return 7;
    }
    return 7 + static_cast<int>(client_active_texture - GL_TEXTURE0);
}

uint32_t recording_vp_mask(GLenum cap, GLenum client_active_texture) {
    switch (cap) {
    case GL_VERTEX_ARRAY:
        return (1u << 0);
    case GL_NORMAL_ARRAY:
        return (1u << 1);
    case GL_COLOR_ARRAY:
        return (1u << 2);
    case GL_INDEX_ARRAY:
        return (1u << 3);
    case GL_EDGE_FLAG_ARRAY:
        return (1u << 4);
    case GL_FOG_COORD_ARRAY:
        return (1u << 5);
    case GL_SECONDARY_COLOR_ARRAY:
        return (1u << 6);
    case GL_TEXTURE_COORD_ARRAY:
        return (1u << recording_texcoord_index(client_active_texture));
    default:
        return 0;
    }
}

DisplayListManager::RecordingState& recording_state() {
    return DisplayListManager::recordingStateRef();
}

void sfpew_list_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    auto& attr = g_glstate.fpe_state.vertexpointer_array.attributes[vp2idx(GL_VERTEX_ARRAY)];
    attr.size = size;
    attr.usage = GL_VERTEX_ARRAY;
    attr.type = type;
    attr.normalized = GL_FALSE;
    attr.stride = stride;
    attr.pointer = pointer;
    g_glstate.fpe_state.vertexpointer_array.dirty = true;
    g_glstate.fpe_state.vertexpointer_array.buffer_based = true;
    SFPEWDebugLog("VP vertex size=%d type=%s stride=%d ptr=%p", size, glEnumToString(type), stride, pointer);
}

void sfpew_list_glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    g_glstate.fpe_state.vertexpointer_array.attributes[vp2idx(GL_COLOR_ARRAY)] = {
        .size = size,
        .usage = GL_COLOR_ARRAY,
        .type = type,
        .normalized = GL_TRUE,
        .stride = stride,
        .pointer = pointer,
    };
    g_glstate.fpe_state.vertexpointer_array.dirty = true;
    SFPEWDebugLog("VP color size=%d type=%s stride=%d ptr=%p normalized=1",
                  size, glEnumToString(type), stride, pointer);
}

void sfpew_list_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    const GLenum usage = GL_TEXTURE_COORD_ARRAY + (g_glstate.fpe_state.client_active_texture - GL_TEXTURE0);
    g_glstate.fpe_state.vertexpointer_array.attributes[vp2idx(GL_TEXTURE_COORD_ARRAY)] = {
        .size = size,
        .usage = usage,
        .type = type,
        .normalized = GL_FALSE,
        .stride = stride,
        .pointer = pointer,
    };
    g_glstate.fpe_state.vertexpointer_array.dirty = true;
    SFPEWDebugLog("VP texcoord size=%d type=%s stride=%d ptr=%p active=%s",
                  size, glEnumToString(type), stride, pointer,
                  glEnumToString(g_glstate.fpe_state.client_active_texture));
}

void sfpew_list_glEnableClientState(GLenum cap) {
    auto mask = vp_mask(cap);
    g_glstate.fpe_state.vertexpointer_array.enabled_pointers |= mask;
    g_glstate.fpe_state.vertexpointer_array.dirty = true;
    SFPEWDebugLog("VP enable_client cap=%s enabled_mask=0x%x", glEnumToString(cap),
                  g_glstate.fpe_state.vertexpointer_array.enabled_pointers);
}

void sfpew_list_glDisableClientState(GLenum cap) {
    auto mask = vp_mask(cap);
    g_glstate.fpe_state.vertexpointer_array.enabled_pointers &= (~mask);
    g_glstate.fpe_state.vertexpointer_array.dirty = true;
    SFPEWDebugLog("VP disable_client cap=%s enabled_mask=0x%x", glEnumToString(cap),
                  g_glstate.fpe_state.vertexpointer_array.enabled_pointers);
}

}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    // LOG_D("glVertexPointer, size = %d, type = %s, stride = %d, pointer = 0x%x", size, glEnumToString(type), stride,
    // pointer)
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        displayListManager.record<sfpew_list_glVertexPointer>({}, size, type, stride, pointer);
        auto& attr = recording_state().vertexpointer_array.attributes[0];
        attr.size = size;
        attr.usage = GL_VERTEX_ARRAY;
        attr.type = type;
        attr.normalized = GL_FALSE;
        attr.stride = stride;
        attr.pointer = pointer;
        recording_state().vertexpointer_array.dirty = true;
        recording_state().vertexpointer_array.buffer_based = true;
        if (DisplayListManager::shouldFinish()) return;
    }

    sfpew_list_glVertexPointer(size, type, stride, pointer);
}

void glNormalPointer(GLenum type, GLsizei stride, const GLvoid* pointer) {
    // LOG_D("glNormalPointer, type = %s, stride = %d, pointer = 0x%x", glEnumToString(type), stride, pointer)
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        displayListManager.record<glNormalPointer>({}, type, stride, pointer);
        recording_state().vertexpointer_array.attributes[1] = {
            .size = 3,
            .usage = GL_NORMAL_ARRAY,
            .type = type,
            .normalized = GL_FALSE,
            .stride = stride,
            .pointer = pointer,
        };
        recording_state().vertexpointer_array.dirty = true;
        if (DisplayListManager::shouldFinish()) return;
    }

    g_glstate.fpe_state.vertexpointer_array.attributes[vp2idx(GL_NORMAL_ARRAY)] = {
        .size = 3,
        .usage = GL_NORMAL_ARRAY,
        .type = type,
        .normalized = GL_FALSE,
        .stride = stride,
        .pointer = pointer,
        //            .varying = true
    };
    g_glstate.fpe_state.vertexpointer_array.dirty = true;
    SFPEWDebugLog("VP normal type=%s stride=%d ptr=%p", glEnumToString(type), stride, pointer);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    // LOG_D("glColorPointer, size = %d, type = %s, stride = %d, pointer = 0x%x", size, glEnumToString(type), stride,
    // pointer)
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        displayListManager.record<sfpew_list_glColorPointer>({}, size, type, stride, pointer);
        recording_state().vertexpointer_array.attributes[2] = {
            .size = size,
            .usage = GL_COLOR_ARRAY,
            .type = type,
            .normalized = GL_TRUE,
            .stride = stride,
            .pointer = pointer,
        };
        recording_state().vertexpointer_array.dirty = true;
        if (DisplayListManager::shouldFinish()) return;
    }

    sfpew_list_glColorPointer(size, type, stride, pointer);
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    // LOG_D("glTexCoordPointer, size = %d, type = %s, stride = %d, pointer = 0x%x", size, glEnumToString(type), stride,
    // pointer) LOG_D("Active texture: %s", glEnumToString(g_glstate.fpe_state.client_active_texture))
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        displayListManager.record<sfpew_list_glTexCoordPointer>({}, size, type, stride, pointer);
        const GLenum recordingTexture = recording_state().client_active_texture;
        const int index = recording_texcoord_index(recordingTexture);
        const GLenum usage = GL_TEXTURE_COORD_ARRAY + (recordingTexture - GL_TEXTURE0);
        recording_state().vertexpointer_array.attributes[index] = {
            .size = size,
            .usage = usage,
            .type = type,
            .normalized = GL_FALSE,
            .stride = stride,
            .pointer = pointer,
        };
        recording_state().vertexpointer_array.dirty = true;
        if (DisplayListManager::shouldFinish()) return;
    }

    sfpew_list_glTexCoordPointer(size, type, stride, pointer);
}

void glIndexPointer(GLenum type, GLsizei stride, const GLvoid* pointer) {
    // LOG_D("glIndexPointer, size = %d, type = %s, stride = %d, pointer = 0x%x", glEnumToString(type), stride, pointer)
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        displayListManager.record<glIndexPointer>({}, type, stride, pointer);
        recording_state().vertexpointer_array.attributes[3] = {
            .size = 1,
            .usage = GL_INDEX_ARRAY,
            .type = type,
            .normalized = GL_FALSE,
            .stride = stride,
            .pointer = pointer,
        };
        recording_state().vertexpointer_array.dirty = true;
        if (DisplayListManager::shouldFinish()) return;
    }

    g_glstate.fpe_state.vertexpointer_array.attributes[vp2idx(GL_INDEX_ARRAY)] = {
        .size = 1,
        .usage = GL_INDEX_ARRAY,
        .type = type,
        .normalized = GL_FALSE,
        .stride = stride,
        .pointer = pointer,
        //            .varying = true
    };
    g_glstate.fpe_state.vertexpointer_array.dirty = true;
    SFPEWDebugLog("VP index type=%s stride=%d ptr=%p", glEnumToString(type), stride, pointer);
}

void glEnableClientState(GLenum cap) {
    // LOG_D("glEnableClientState, cap = %s", glEnumToString(cap))
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        displayListManager.record<sfpew_list_glEnableClientState>({}, cap);
        auto mask = recording_vp_mask(cap, recording_state().client_active_texture);
        recording_state().vertexpointer_array.enabled_pointers |= mask;
        recording_state().vertexpointer_array.dirty = true;
        if (DisplayListManager::shouldFinish()) return;
    }

    sfpew_list_glEnableClientState(cap);
}

void glDisableClientState(GLenum cap) {
    // LOG_D("glDisableClientState, cap = %s", glEnumToString(cap))
    if (!disableRecording && DisplayListManager::shouldRecord()) {
        displayListManager.record<sfpew_list_glDisableClientState>({}, cap);
        auto mask = recording_vp_mask(cap, recording_state().client_active_texture);
        recording_state().vertexpointer_array.enabled_pointers &= (~mask);
        recording_state().vertexpointer_array.dirty = true;
        if (DisplayListManager::shouldFinish()) return;
    }

    sfpew_list_glDisableClientState(cap);
}
