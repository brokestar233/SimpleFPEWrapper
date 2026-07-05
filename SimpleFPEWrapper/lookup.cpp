// SimpleFPEWrapper - SimpleFPEWrapper/lookup.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "EGL/egl.h"
#include "init.h"
#include "fpe/state.h"
#include "fpe/transformation.h"

#define GETPROC(name, var)                                                                                             \
    if (std::strcmp(#name, var) == 0) {                                                                                \
        return (__eglMustCastToProperFunctionPointerType)name;                                                         \
    }

namespace {
    bool isBlockedDebugProc(const char* name) {
        return std::strcmp(name, "glDebugMessageControl") == 0 ||
               std::strcmp(name, "glDebugMessageInsert") == 0 ||
               std::strcmp(name, "glDebugMessageCallback") == 0 ||
               std::strcmp(name, "glGetDebugMessageLog") == 0 ||
               std::strcmp(name, "glPushDebugGroup") == 0 ||
               std::strcmp(name, "glPopDebugGroup") == 0 ||
               std::strcmp(name, "glObjectLabel") == 0 ||
               std::strcmp(name, "glGetObjectLabel") == 0 ||
               std::strcmp(name, "glObjectPtrLabel") == 0 ||
               std::strcmp(name, "glGetObjectPtrLabel") == 0 ||
               std::strcmp(name, "glDebugMessageControlARB") == 0 ||
               std::strcmp(name, "glDebugMessageInsertARB") == 0 ||
               std::strcmp(name, "glDebugMessageCallbackARB") == 0 ||
               std::strcmp(name, "glGetDebugMessageLogARB") == 0 ||
               std::strcmp(name, "glDebugMessageEnableAMD") == 0 ||
               std::strcmp(name, "glDebugMessageInsertAMD") == 0 ||
               std::strcmp(name, "glDebugMessageCallbackAMD") == 0;
    }
}

SFPEW_APIENTRY __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* name) {
    if (g_sfpewCompatMode && isBlockedDebugProc(name)) {
        return nullptr;
    }

    GETPROC(glGetString, name)
    GETPROC(glGetStringi, name)
    GETPROC(glGetIntegerv, name)
    GETPROC(glGetTexLevelParameteriv, name)
    GETPROC(glGetTexLevelParameterfv, name)
    GETPROC(glTexImage2D, name)
    GETPROC(glDrawArrays, name)
    GETPROC(glBindFramebuffer, name)
    GETPROC(glDeleteFramebuffers, name)
    GETPROC(glGenFramebuffers, name)
    GETPROC(glCheckFramebufferStatus, name)
    GETPROC(glIsFramebuffer, name)
    GETPROC(glFramebufferTexture2D, name)
    GETPROC(glFramebufferTexture1D, name)
    GETPROC(glFramebufferTexture3D, name)
    GETPROC(glFramebufferRenderbuffer, name)
    GETPROC(glGetFramebufferAttachmentParameteriv, name)
    GETPROC(glBindRenderbuffer, name)
    GETPROC(glDeleteRenderbuffers, name)
    GETPROC(glGenRenderbuffers, name)
    GETPROC(glIsRenderbuffer, name)
    GETPROC(glRenderbufferStorage, name)
    GETPROC(glGetRenderbufferParameteriv, name)
    GETPROC(glDrawBuffer, name)
    GETPROC(glDrawBuffers, name)
    GETPROC(glReadBuffer, name)
    GETPROC(glGenerateMipmap, name)
    GETPROC(glRenderbufferStorageMultisample, name)
    GETPROC(glBlitFramebuffer, name)
    GETPROC(glBindFramebufferEXT, name)
    GETPROC(glDeleteFramebuffersEXT, name)
    GETPROC(glGenFramebuffersEXT, name)
    GETPROC(glCheckFramebufferStatusEXT, name)
    GETPROC(glIsFramebufferEXT, name)
    GETPROC(glFramebufferTexture2DEXT, name)
    GETPROC(glFramebufferTexture1DEXT, name)
    GETPROC(glFramebufferTexture3DEXT, name)
    GETPROC(glFramebufferRenderbufferEXT, name)
    GETPROC(glGetFramebufferAttachmentParameterivEXT, name)
    GETPROC(glBindRenderbufferEXT, name)
    GETPROC(glDeleteRenderbuffersEXT, name)
    GETPROC(glGenRenderbuffersEXT, name)
    GETPROC(glIsRenderbufferEXT, name)
    GETPROC(glRenderbufferStorageEXT, name)
    GETPROC(glGetRenderbufferParameterivEXT, name)
    GETPROC(glGenerateMipmapEXT, name)
    GETPROC(glRenderbufferStorageMultisampleEXT, name)
    GETPROC(glBlitFramebufferEXT, name)
    GETPROC(glBindFramebufferARB, name)
    GETPROC(glDeleteFramebuffersARB, name)
    GETPROC(glGenFramebuffersARB, name)
    GETPROC(glCheckFramebufferStatusARB, name)
    GETPROC(glIsFramebufferARB, name)
    GETPROC(glFramebufferTexture2DARB, name)
    GETPROC(glFramebufferTexture1DARB, name)
    GETPROC(glFramebufferTexture3DARB, name)
    GETPROC(glFramebufferRenderbufferARB, name)
    GETPROC(glGetFramebufferAttachmentParameterivARB, name)
    GETPROC(glBindRenderbufferARB, name)
    GETPROC(glDeleteRenderbuffersARB, name)
    GETPROC(glGenRenderbuffersARB, name)
    GETPROC(glIsRenderbufferARB, name)
    GETPROC(glRenderbufferStorageARB, name)
    GETPROC(glGetRenderbufferParameterivARB, name)
    GETPROC(glRenderbufferStorageMultisampleARB, name)
    GETPROC(glDrawBuffersARB, name)

    GETPROC(glBegin, name)
    GETPROC(glEnd, name)
    // GETPROC(glVertex2d, name)
    GETPROC(glVertex2f, name)
    // GETPROC(glVertex2i, name)
    // GETPROC(glVertex2s, name)
    // GETPROC(glVertex3d, name)
    GETPROC(glVertex3f, name)
    // GETPROC(glVertex3i, name)
    // GETPROC(glVertex3s, name)
    // GETPROC(glVertex4d, name)
    GETPROC(glVertex4f, name)
    // GETPROC(glVertex4i, name)
    // GETPROC(glVertex4s, name)
    // GETPROC(glVertex2dv, name)
    GETPROC(glVertex2fv, name)
    // GETPROC(glVertex2iv, name)
    // GETPROC(glVertex2sv, name)
    // GETPROC(glVertex3dv, name)
    GETPROC(glVertex3fv, name)
    // GETPROC(glVertex3iv, name)
    // GETPROC(glVertex3sv, name)
    // GETPROC(glVertex4dv, name)
    GETPROC(glVertex4fv, name)
    // GETPROC(glVertex4iv, name)
    // GETPROC(glVertex4sv, name)
    // GETPROC(glNormal3b, name)
    // GETPROC(glNormal3d, name)
    GETPROC(glNormal3f, name)
    // GETPROC(glNormal3i, name)
    // GETPROC(glNormal3s, name)
    // GETPROC(glNormal3bv, name)
    // GETPROC(glNormal3dv, name)
    GETPROC(glNormal3fv, name)
    // GETPROC(glNormal3iv, name)
    // GETPROC(glNormal3sv, name)
    GETPROC(glTexCoord2f, name)
    GETPROC(glTexCoord2fv, name)
    GETPROC(glTexCoord4f, name)
    GETPROC(glTexCoord4fv, name)
    GETPROC(glMultiTexCoord2f, name)
    GETPROC(glMultiTexCoord2fv, name)
    GETPROC(glMultiTexCoord4f, name)
    GETPROC(glMultiTexCoord4fv, name)
    GETPROC(glColor3b, name)
    GETPROC(glColor3d, name)
    GETPROC(glColor3f, name)
    GETPROC(glColor3i, name)
    GETPROC(glColor3s, name)
    GETPROC(glColor3ub, name)
    GETPROC(glColor3ui, name)
    GETPROC(glColor3us, name)
    GETPROC(glColor3bv, name)
    GETPROC(glColor3dv, name)
    GETPROC(glColor3fv, name)
    GETPROC(glColor3iv, name)
    GETPROC(glColor3sv, name)
    GETPROC(glColor3ubv, name)
    GETPROC(glColor3uiv, name)
    GETPROC(glColor3usv, name)
    GETPROC(glGenLists, name)
    GETPROC(glDeleteLists, name)
    GETPROC(glIsList, name)
    GETPROC(glNewList, name)
    GETPROC(glEndList, name)
    GETPROC(glCallList, name)
    GETPROC(glCallLists, name)
    GETPROC(glListBase, name)
    GETPROC(glEnable, name)
    GETPROC(glDisable, name)
    GETPROC(glBlendFunc, name)
    GETPROC(glBlendFuncSeparate, name)
    GETPROC(glBlendEquation, name)
    GETPROC(glBlendEquationSeparate, name)
    GETPROC(glBlendColor, name)
    GETPROC(glColorMask, name)
    GETPROC(glCullFace, name)
    GETPROC(glDepthFunc, name)
    GETPROC(glDepthMask, name)
    GETPROC(glFrontFace, name)
    GETPROC(glActiveTexture, name)
    GETPROC(glClientActiveTexture, name)
    GETPROC(glTexEnvf, name)
    GETPROC(glTexEnvi, name)
    GETPROC(glTexEnvfv, name)
    GETPROC(glTexEnviv, name)
    GETPROC(glAlphaFunc, name)
    GETPROC(glFogf, name)
    GETPROC(glFogi, name)
    GETPROC(glFogfv, name)
    GETPROC(glFogiv, name)
    GETPROC(glShadeModel, name)
    GETPROC(glLightf, name)
    GETPROC(glLighti, name)
    GETPROC(glLightfv, name)
    GETPROC(glLightiv, name)
    GETPROC(glLightModelf, name)
    GETPROC(glLightModeli, name)
    GETPROC(glLightModelfv, name)
    GETPROC(glLightModeliv, name)
    GETPROC(glColor4b, name)
    GETPROC(glColor4d, name)
    GETPROC(glColor4f, name)
    GETPROC(glColor4i, name)
    GETPROC(glColor4s, name)
    GETPROC(glColor4ub, name)
    GETPROC(glColor4ui, name)
    GETPROC(glColor4us, name)
    GETPROC(glColor4bv, name)
    GETPROC(glColor4dv, name)
    GETPROC(glColor4fv, name)
    GETPROC(glColor4iv, name)
    GETPROC(glColor4sv, name)
    GETPROC(glColor4ubv, name)
    GETPROC(glColor4uiv, name)
    GETPROC(glColor4usv, name)
    GETPROC(glMatrixMode, name)
    GETPROC(glLoadIdentity, name)
    GETPROC(glOrtho, name)
    GETPROC(glOrthof, name)
    GETPROC(glScalef, name)
    GETPROC(glTranslatef, name)
    GETPROC(glRotatef, name)
    GETPROC(glMultMatrixf, name)
    GETPROC(glRotated, name)
    GETPROC(glScaled, name)
    GETPROC(glTranslated, name)
    GETPROC(glPushMatrix, name)
    GETPROC(glPopMatrix, name)
    GETPROC(glVertexPointer, name)
    GETPROC(glNormalPointer, name)
    GETPROC(glColorPointer, name)
    GETPROC(glTexCoordPointer, name)
    GETPROC(glIndexPointer, name)
    GETPROC(glEnableClientState, name)
    GETPROC(glDisableClientState, name)
    GETPROC(glGetFloatv, name)

    __eglMustCastToProperFunctionPointerType ptr = g_eglFuncs.eglGetProcAddress(name);
    if (!ptr) {
        printf("eglGetProcAddress: eglGetProcAddress also failed to find '%s'\n", name);
    }
    return ptr;
}

SFPEW_APIENTRY void* glXGetProcAddress(const char* name) {
    return (void*)eglGetProcAddress(name);
}

SFPEW_APIENTRY void* glXGetProcAddressARB(const char* name) {
    return glXGetProcAddress(name);
}
