// SimpleFPEWrapper - SimpleFPEWrapper/framebuffer.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "init.h"

#include <cstring>
#include <vector>

namespace {
    struct AttachmentInfo {
        GLenum textarget = GL_TEXTURE_2D;
        GLuint texture = 0;
        GLint level = 0;
    };

    struct BoundFramebufferState {
        GLenum currentTarget = GL_FRAMEBUFFER;
        std::vector<AttachmentInfo> drawAttachments;
        std::vector<AttachmentInfo> readAttachments;
    };

    BoundFramebufferState g_boundFramebufferState;
    GLint g_maxDrawBuffers = 0;

    GLint getMaxDrawBuffers() {
        if (g_maxDrawBuffers <= 0) {
            g_glFuncs.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &g_maxDrawBuffers);
            if (g_maxDrawBuffers <= 0) {
                g_maxDrawBuffers = 1;
            }
        }
        return g_maxDrawBuffers;
    }

    void ensureAttachmentStorage(GLenum target) {
        const size_t size = static_cast<size_t>(getMaxDrawBuffers());
        if (target == GL_DRAW_FRAMEBUFFER || target == GL_FRAMEBUFFER) {
            g_boundFramebufferState.drawAttachments.assign(size, AttachmentInfo{});
        }
        if (target == GL_READ_FRAMEBUFFER || target == GL_FRAMEBUFFER) {
            g_boundFramebufferState.readAttachments.assign(size, AttachmentInfo{});
        }
    }

    std::vector<AttachmentInfo>& getAttachmentVector(GLenum target) {
        return target == GL_READ_FRAMEBUFFER
                ? g_boundFramebufferState.readAttachments
                : g_boundFramebufferState.drawAttachments;
    }

    void remapFramebufferAttachment(GLenum oldAttachment, GLenum targetAttachment) {
        auto& attachments = getAttachmentVector(g_boundFramebufferState.currentTarget);
        const auto index = static_cast<size_t>(oldAttachment - GL_COLOR_ATTACHMENT0);
        if (index >= attachments.size()) {
            return;
        }
        const auto& attachment = attachments[index];
        g_glFuncs.glFramebufferTexture2D(
                g_boundFramebufferState.currentTarget,
                targetAttachment,
                attachment.textarget,
                attachment.texture,
                attachment.level
        );
    }
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    g_glFuncs.glBindFramebuffer(target, framebuffer);
    g_boundFramebufferState.currentTarget = target;
    if (g_sfpewCompatMode) {
        ensureAttachmentStorage(target);
    }
}

void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    g_glFuncs.glDeleteFramebuffers(n, framebuffers);
}

void glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    g_glFuncs.glGenFramebuffers(n, framebuffers);
}

GLenum glCheckFramebufferStatus(GLenum target) {
    const GLenum status = g_glFuncs.glCheckFramebufferStatus(target);
    if (g_sfpewCompatMode && status == 0) {
        return GL_FRAMEBUFFER_COMPLETE;
    }
    return status;
}

GLboolean glIsFramebuffer(GLuint framebuffer) {
    return g_glFuncs.glIsFramebuffer(framebuffer);
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    if (g_sfpewCompatMode && attachment >= GL_COLOR_ATTACHMENT0) {
        auto& attachments = getAttachmentVector(target);
        const auto index = static_cast<size_t>(attachment - GL_COLOR_ATTACHMENT0);
        if (index < attachments.size()) {
            attachments[index].textarget = textarget;
            attachments[index].texture = texture;
            attachments[index].level = level;
            g_boundFramebufferState.currentTarget = target;
        }
    }
    g_glFuncs.glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, texture, level);
}

void glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level,
                            GLint zoffset) {
    if (g_glFuncs.glFramebufferTextureLayer) {
        g_glFuncs.glFramebufferTextureLayer(target, attachment, texture, level, zoffset);
        return;
    }
    glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, texture, level);
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    g_glFuncs.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params) {
    g_glFuncs.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    g_glFuncs.glBindRenderbuffer(target, renderbuffer);
}

void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
    g_glFuncs.glDeleteRenderbuffers(n, renderbuffers);
}

void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    g_glFuncs.glGenRenderbuffers(n, renderbuffers);
}

GLboolean glIsRenderbuffer(GLuint renderbuffer) {
    return g_glFuncs.glIsRenderbuffer(renderbuffer);
}

void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    g_glFuncs.glRenderbufferStorage(target, internalformat, width, height);
}

void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    g_glFuncs.glGetRenderbufferParameteriv(target, pname, params);
}

void glDrawBuffer(GLenum buf) {
    if (!g_sfpewCompatMode) {
        if (g_glFuncs.glDrawBuffers) {
            g_glFuncs.glDrawBuffers(1, &buf);
        }
        return;
    }

    GLint currentFbo = 0;
    g_glFuncs.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);
    if (currentFbo == 0) {
        GLenum buffer = GL_NONE;
        switch (buf) {
            case GL_FRONT:
            case GL_BACK:
            case GL_NONE:
                buffer = buf;
                break;
            default:
                break;
        }
        if (g_glFuncs.glDrawBuffers) {
            g_glFuncs.glDrawBuffers(1, &buffer);
        }
        return;
    }

    const GLint maxAttachments = getMaxDrawBuffers();
    std::vector<GLenum> buffers(static_cast<size_t>(maxAttachments), GL_NONE);
    if (buf >= GL_COLOR_ATTACHMENT0 && buf < GL_COLOR_ATTACHMENT0 + maxAttachments) {
        buffers[static_cast<size_t>(buf - GL_COLOR_ATTACHMENT0)] = buf;
    }
    g_glFuncs.glDrawBuffers(maxAttachments, buffers.data());
}

void glDrawBuffers(GLsizei n, const GLenum* bufs) {
    if (!g_sfpewCompatMode) {
        g_glFuncs.glDrawBuffers(n, bufs);
        return;
    }

    std::vector<GLenum> mapped(static_cast<size_t>(n), GL_NONE);
    for (GLsizei i = 0; i < n; ++i) {
        const GLenum attachment = bufs[i];
        if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + getMaxDrawBuffers()) {
            const GLenum targetAttachment = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
            mapped[static_cast<size_t>(i)] = targetAttachment;
            remapFramebufferAttachment(attachment, targetAttachment);
        } else {
            mapped[static_cast<size_t>(i)] = attachment;
        }
    }
    g_glFuncs.glDrawBuffers(n, mapped.data());
}

void glReadBuffer(GLenum src) {
    g_glFuncs.glReadBuffer(src);
}

void glGenerateMipmap(GLenum target) {
    g_glFuncs.glGenerateMipmap(target);
}

void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                      GLsizei height) {
    g_glFuncs.glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter) {
    g_glFuncs.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void glBindFramebufferEXT(GLenum target, GLuint framebuffer) { glBindFramebuffer(target, framebuffer); }
void glDeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers) { glDeleteFramebuffers(n, framebuffers); }
void glGenFramebuffersEXT(GLsizei n, GLuint* framebuffers) { glGenFramebuffers(n, framebuffers); }
GLenum glCheckFramebufferStatusEXT(GLenum target) { return glCheckFramebufferStatus(target); }
GLboolean glIsFramebufferEXT(GLuint framebuffer) { return glIsFramebuffer(framebuffer); }
void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
void glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    glFramebufferTexture1D(target, attachment, textarget, texture, level);
}
void glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level,
                               GLint zoffset) {
    glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
}
void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}
void glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint* params) {
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}
void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer) { glBindRenderbuffer(target, renderbuffer); }
void glDeleteRenderbuffersEXT(GLsizei n, const GLuint* renderbuffers) { glDeleteRenderbuffers(n, renderbuffers); }
void glGenRenderbuffersEXT(GLsizei n, GLuint* renderbuffers) { glGenRenderbuffers(n, renderbuffers); }
GLboolean glIsRenderbufferEXT(GLuint renderbuffer) { return glIsRenderbuffer(renderbuffer); }
void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    glRenderbufferStorage(target, internalformat, width, height);
}
void glGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint* params) {
    glGetRenderbufferParameteriv(target, pname, params);
}
void glGenerateMipmapEXT(GLenum target) { glGenerateMipmap(target); }
void glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                         GLsizei height) {
    glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}
void glBlitFramebufferEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                          GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                          GLbitfield mask, GLenum filter) {
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void glBindFramebufferARB(GLenum target, GLuint framebuffer) { glBindFramebuffer(target, framebuffer); }
void glDeleteFramebuffersARB(GLsizei n, const GLuint* framebuffers) { glDeleteFramebuffers(n, framebuffers); }
void glGenFramebuffersARB(GLsizei n, GLuint* framebuffers) { glGenFramebuffers(n, framebuffers); }
GLenum glCheckFramebufferStatusARB(GLenum target) { return glCheckFramebufferStatus(target); }
GLboolean glIsFramebufferARB(GLuint framebuffer) { return glIsFramebuffer(framebuffer); }
void glFramebufferTexture2DARB(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
void glFramebufferTexture1DARB(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    glFramebufferTexture1D(target, attachment, textarget, texture, level);
}
void glFramebufferTexture3DARB(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level,
                               GLint zoffset) {
    glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset);
}
void glFramebufferRenderbufferARB(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}
void glGetFramebufferAttachmentParameterivARB(GLenum target, GLenum attachment, GLenum pname, GLint* params) {
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}
void glBindRenderbufferARB(GLenum target, GLuint renderbuffer) { glBindRenderbuffer(target, renderbuffer); }
void glDeleteRenderbuffersARB(GLsizei n, const GLuint* renderbuffers) { glDeleteRenderbuffers(n, renderbuffers); }
void glGenRenderbuffersARB(GLsizei n, GLuint* renderbuffers) { glGenRenderbuffers(n, renderbuffers); }
GLboolean glIsRenderbufferARB(GLuint renderbuffer) { return glIsRenderbuffer(renderbuffer); }
void glRenderbufferStorageARB(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    glRenderbufferStorage(target, internalformat, width, height);
}
void glGetRenderbufferParameterivARB(GLenum target, GLenum pname, GLint* params) {
    glGetRenderbufferParameteriv(target, pname, params);
}
void glRenderbufferStorageMultisampleARB(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                         GLsizei height) {
    glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}
void glDrawBuffersARB(GLsizei n, const GLenum* bufs) {
    glDrawBuffers(n, bufs);
}
