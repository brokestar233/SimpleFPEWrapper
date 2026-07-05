// SimpleFPEWrapper - SimpleFPEWrapper/init.h
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#pragma once
#include <string>
#include <cstring>
#include <stdexcept>
#include <cstdarg>
#include "backend/loader.h"

#define SFPEW_APIENTRY __attribute__((visibility("default"))) extern "C"
extern SFPEW::External::EGLFunctionsTable g_eglFuncs;
extern SFPEW::External::BackendGLFunctionsTable g_glFuncs;
extern bool g_sfpewCompatMode;
extern bool g_sfpewDebugLogging;
extern bool g_sfpewBackendAlphaTestAvailable;

const char* glEnumToString(GLenum e);
bool SFPEWIsDebugLoggingEnabled();
void SFPEWDebugLog(const char* fmt, ...);
void SFPEWDrainBackendErrors(const char* stage);

SFPEW_APIENTRY const GLubyte* glGetString(GLenum name);
SFPEW_APIENTRY const GLubyte* glGetStringi(GLenum name, GLuint index);
SFPEW_APIENTRY void glGetIntegerv(GLenum pname, GLint* params);
SFPEW_APIENTRY GLboolean glIsEnabled(GLenum cap);
SFPEW_APIENTRY void glGetTexEnvfv(GLenum target, GLenum pname, GLfloat* params);
SFPEW_APIENTRY void glGetTexEnviv(GLenum target, GLenum pname, GLint* params);
SFPEW_APIENTRY void glPushAttrib(GLbitfield mask);
SFPEW_APIENTRY void glPopAttrib(void);
SFPEW_APIENTRY void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params);
SFPEW_APIENTRY void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params);
SFPEW_APIENTRY void glTexEnvf(GLenum target, GLenum pname, GLfloat param);
SFPEW_APIENTRY void glTexEnvi(GLenum target, GLenum pname, GLint param);
SFPEW_APIENTRY void glTexEnvfv(GLenum target, GLenum pname, const GLfloat* params);
SFPEW_APIENTRY void glTexEnviv(GLenum target, GLenum pname, const GLint* params);
SFPEW_APIENTRY void glUseProgram(GLuint program);
SFPEW_APIENTRY void glBindVertexArray(GLuint array);
SFPEW_APIENTRY void glBindBuffer(GLenum target, GLuint buffer);
SFPEW_APIENTRY void glBindBufferARB(GLenum target, GLuint buffer);
SFPEW_APIENTRY void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                 GLint border, GLenum format, GLenum type, const void* pixels);
SFPEW_APIENTRY void glDrawArrays(GLenum mode, GLint first, GLsizei count);
SFPEW_APIENTRY void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
SFPEW_APIENTRY void glGetFloatv(GLenum pname, GLfloat* params);
SFPEW_APIENTRY void glBindFramebuffer(GLenum target, GLuint framebuffer);
SFPEW_APIENTRY void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
SFPEW_APIENTRY void glGenFramebuffers(GLsizei n, GLuint* framebuffers);
SFPEW_APIENTRY GLenum glCheckFramebufferStatus(GLenum target);
SFPEW_APIENTRY GLboolean glIsFramebuffer(GLuint framebuffer);
SFPEW_APIENTRY void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
SFPEW_APIENTRY void glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
SFPEW_APIENTRY void glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
SFPEW_APIENTRY void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
SFPEW_APIENTRY void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params);
SFPEW_APIENTRY void glBindRenderbuffer(GLenum target, GLuint renderbuffer);
SFPEW_APIENTRY void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
SFPEW_APIENTRY void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers);
SFPEW_APIENTRY GLboolean glIsRenderbuffer(GLuint renderbuffer);
SFPEW_APIENTRY void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
SFPEW_APIENTRY void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params);
SFPEW_APIENTRY void glDrawBuffers(GLsizei n, const GLenum* bufs);
SFPEW_APIENTRY void glDrawBuffer(GLenum buf);
SFPEW_APIENTRY void glReadBuffer(GLenum src);
SFPEW_APIENTRY void glGenerateMipmap(GLenum target);
SFPEW_APIENTRY void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
SFPEW_APIENTRY void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                      GLbitfield mask, GLenum filter);

SFPEW_APIENTRY void glBindFramebufferEXT(GLenum target, GLuint framebuffer);
SFPEW_APIENTRY void glDeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers);
SFPEW_APIENTRY void glGenFramebuffersEXT(GLsizei n, GLuint* framebuffers);
SFPEW_APIENTRY GLenum glCheckFramebufferStatusEXT(GLenum target);
SFPEW_APIENTRY GLboolean glIsFramebufferEXT(GLuint framebuffer);
SFPEW_APIENTRY void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
SFPEW_APIENTRY void glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
SFPEW_APIENTRY void glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
SFPEW_APIENTRY void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
SFPEW_APIENTRY void glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint* params);
SFPEW_APIENTRY void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer);
SFPEW_APIENTRY void glDeleteRenderbuffersEXT(GLsizei n, const GLuint* renderbuffers);
SFPEW_APIENTRY void glGenRenderbuffersEXT(GLsizei n, GLuint* renderbuffers);
SFPEW_APIENTRY GLboolean glIsRenderbufferEXT(GLuint renderbuffer);
SFPEW_APIENTRY void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
SFPEW_APIENTRY void glGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint* params);
SFPEW_APIENTRY void glGenerateMipmapEXT(GLenum target);
SFPEW_APIENTRY void glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
SFPEW_APIENTRY void glBlitFramebufferEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                         GLbitfield mask, GLenum filter);

SFPEW_APIENTRY void glBindFramebufferARB(GLenum target, GLuint framebuffer);
SFPEW_APIENTRY void glDeleteFramebuffersARB(GLsizei n, const GLuint* framebuffers);
SFPEW_APIENTRY void glGenFramebuffersARB(GLsizei n, GLuint* framebuffers);
SFPEW_APIENTRY GLenum glCheckFramebufferStatusARB(GLenum target);
SFPEW_APIENTRY GLboolean glIsFramebufferARB(GLuint framebuffer);
SFPEW_APIENTRY void glFramebufferTexture2DARB(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
SFPEW_APIENTRY void glFramebufferTexture1DARB(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
SFPEW_APIENTRY void glFramebufferTexture3DARB(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
SFPEW_APIENTRY void glFramebufferRenderbufferARB(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
SFPEW_APIENTRY void glGetFramebufferAttachmentParameterivARB(GLenum target, GLenum attachment, GLenum pname, GLint* params);
SFPEW_APIENTRY void glBindRenderbufferARB(GLenum target, GLuint renderbuffer);
SFPEW_APIENTRY void glDeleteRenderbuffersARB(GLsizei n, const GLuint* renderbuffers);
SFPEW_APIENTRY void glGenRenderbuffersARB(GLsizei n, GLuint* renderbuffers);
SFPEW_APIENTRY GLboolean glIsRenderbufferARB(GLuint renderbuffer);
SFPEW_APIENTRY void glRenderbufferStorageARB(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
SFPEW_APIENTRY void glGetRenderbufferParameterivARB(GLenum target, GLenum pname, GLint* params);
SFPEW_APIENTRY void glRenderbufferStorageMultisampleARB(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
SFPEW_APIENTRY void glDrawBuffersARB(GLsizei n, const GLenum* bufs);
