// SimpleFPEWrapper - SimpleFPEWrapper/fpe/list.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "list.h"
#include "pointer_utils.h"
#include "fpe.hpp"
#include <limits>
#include <vector>

#define DEBUG 0

GLuint currentListBase = 0;

namespace {

bool try_get_calllists_bytes(GLsizei n, GLenum type, size_t& outBytes) {
    outBytes = 0;
    if (n <= 0) {
        return true;
    }

    const int typeBytes = PointerUtils::type_to_bytes(type);
    if (typeBytes <= 0) {
        return false;
    }

    const size_t count = static_cast<size_t>(n);
    const size_t stride = static_cast<size_t>(typeBytes);
    if (count > std::numeric_limits<size_t>::max() / stride) {
        return false;
    }

    outBytes = count * stride;
    return true;
}

bool decode_calllist_offset(GLenum type, const uint8_t*& ptr, GLuint& offset) {
    switch (type) {
    case GL_BYTE: {
        GLbyte value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        offset = static_cast<GLuint>(value);
        ptr += sizeof(value);
        return true;
    }
    case GL_UNSIGNED_BYTE: {
        GLubyte value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        offset = static_cast<GLuint>(value);
        ptr += sizeof(value);
        return true;
    }
    case GL_SHORT: {
        GLshort value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        offset = static_cast<GLuint>(value);
        ptr += sizeof(value);
        return true;
    }
    case GL_UNSIGNED_SHORT: {
        GLushort value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        offset = static_cast<GLuint>(value);
        ptr += sizeof(value);
        return true;
    }
    case GL_INT: {
        GLint value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        offset = static_cast<GLuint>(value);
        ptr += sizeof(value);
        return true;
    }
    case GL_UNSIGNED_INT: {
        GLuint value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        offset = value;
        ptr += sizeof(value);
        return true;
    }
    case GL_FLOAT: {
        GLfloat value = 0.0f;
        std::memcpy(&value, ptr, sizeof(value));
        offset = static_cast<GLuint>(value);
        ptr += sizeof(value);
        return true;
    }
    case GL_2_BYTES:
        offset = (static_cast<GLuint>(ptr[0]) << 8) | ptr[1];
        ptr += 2;
        return true;
    case GL_3_BYTES:
        offset =
            (static_cast<GLuint>(ptr[0]) << 16) | (static_cast<GLuint>(ptr[1]) << 8) | static_cast<GLuint>(ptr[2]);
        ptr += 3;
        return true;
    case GL_4_BYTES:
        offset = (static_cast<GLuint>(ptr[0]) << 24) | (static_cast<GLuint>(ptr[1]) << 16) |
                 (static_cast<GLuint>(ptr[2]) << 8) | static_cast<GLuint>(ptr[3]);
        ptr += 4;
        return true;
    default:
        return false;
    }
}

void execute_calllists(GLsizei n, GLenum type, const void* lists) {
    if (n <= 0 || lists == nullptr) {
        return;
    }

    const auto* ptr = static_cast<const uint8_t*>(lists);
    for (GLsizei i = 0; i < n; ++i) {
        GLuint offset = 0;
        if (!decode_calllist_offset(type, ptr, offset)) {
            return;
        }
        DisplayListManager::callList(currentListBase + offset);
    }
}

class GLCallListsCmd final : public GLCmd {
public:
    GLCallListsCmd(GLsizei n, GLenum type, std::vector<uint8_t>&& data)
        : n_(n), type_(type), data_(std::move(data)) {}

    void execute() const override { execute_calllists(n_, type_, data_.data()); }

private:
    GLsizei n_;
    GLenum type_;
    std::vector<uint8_t> data_;
};

}

GLuint glGenLists(GLsizei range) {
    // LOG()
    // LOG_D("glGenLists(%i)", range)
    GLuint first = DisplayListManager::genDisplayList(range);
    // LOG_D("-> ", first)
    return first;
}

void glDeleteLists(GLuint list, GLsizei range) {
    // LOG()
    // LOG_D("glDeleteLists(%d, %i)", list, range)
    DisplayListManager::deleteDisplayList(list, range);
}

GLboolean glIsList(GLuint list) {
    // LOG()
    // LOG_D("glIsList(%d)", list)
    return DisplayListManager::isDisplayList(list);
}

void glNewList(GLuint list, GLenum mode) {
    // LOG()
    // LOG_D("glNewList(%d, %s)", list, glEnumToString(mode))
    DisplayListManager::startRecord(list, mode);
    DisplayListManager::primeRecordingState(g_glstate.fpe_state.client_active_texture,
                                            g_glstate.fpe_state.vertexpointer_array,
                                            g_glstate.fpe_state.fpe_draw.current_data);
}

void glEndList() {
    // LOG()
    // LOG_D("glEndLists()")
    DisplayListManager::endRecord();
}

void glCallList(GLuint list) {
    // LOG()
    // LOG_D("glCallList(%d)", list)

    if (DisplayListManager::shouldRecord()) {
        displayListManager.record<glCallList>({}, list);
        if (DisplayListManager::isRecording()) return;
    }
    GET_PREV_PROGRAM
    DisplayListManager::callList(list);
    SET_PREV_PROGRAM
}

void glCallLists(GLsizei n, GLenum type, const GLvoid* lists) {
    // LOG()
    // LOG_D("glCallLists(%i, %s, %p)", n, glEnumToString(type), lists)

    size_t listBytes = 0;
    if (!try_get_calllists_bytes(n, type, listBytes)) {
        SFPEWDebugLog("glCallLists ignored invalid args n=%d type=%s", n, glEnumToString(type));
        return;
    }

    if (DisplayListManager::shouldRecord()) {
        std::vector<uint8_t> encodedLists;
        if (listBytes > 0 && lists != nullptr) {
            encodedLists.resize(listBytes);
            std::memcpy(encodedLists.data(), lists, listBytes);
        }
        DisplayListManager::recordCustom(std::make_unique<GLCallListsCmd>(n, type, std::move(encodedLists)));
        if (DisplayListManager::isRecording()) return;
    }
    GET_PREV_PROGRAM
    execute_calllists(n, type, lists);
    SET_PREV_PROGRAM
}

void glListBase(GLuint base) {
    // LOG()
    // LOG_D("glGenLists(%d)", base)

    if (DisplayListManager::shouldRecord()) {
        displayListManager.record<glListBase>({}, base);
        if (DisplayListManager::shouldFinish()) return;
    }

    currentListBase = base;
}
