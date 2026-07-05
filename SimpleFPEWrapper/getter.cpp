// SimpleFPEWrapper - SimpleFPEWrapper/getter.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "GL/gl.h"
#include "init.h"
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <vector>
#include <glm/gtc/type_ptr.hpp>
#include "fpe/fpe.hpp"

inline bool containsMobileGLDev(const std::string& str) {
    return str.find("MobileGL-Dev") != std::string::npos;
}

namespace {
    constexpr const char* kCompatExtensions[] = {
            "GL_ARB_compatibility",
            "OpenGL21",
            "OpenGL11",
            "OpenGL12",
            "OpenGL13",
            "OpenGL14",
            "OpenGL15",
            "OpenGL20",
    };
    constexpr GLint kCompatReportedMaxTextureSize = 16384;
    constexpr GLint kCompatReportedMaxRenderbufferSize = 16384;
    constexpr GLint kCompatMinUsableTextureSize = 1024;
    constexpr GLint kProxyTexture2DMaxSize = 16384;

    GLint g_proxyTexture2DWidth = 0;
    GLint g_proxyTexture2DHeight = 0;
    GLint g_proxyTexture2DInternalFormat = GL_RGBA;

    GLint proxyLevelSize(GLint size, GLint level) {
        if (size <= 0) {
            return 0;
        }
        size >>= level;
        return size > 0 ? size : 1;
    }

    bool proxyTexture2DSupported(GLsizei width, GLsizei height, GLint level) {
        if (width < 0 || height < 0 || level < 0) {
            return false;
        }
        const auto widthAtBaseLevel = static_cast<long long>(width) << level;
        const auto heightAtBaseLevel = static_cast<long long>(height) << level;
        return widthAtBaseLevel <= kProxyTexture2DMaxSize && heightAtBaseLevel <= kProxyTexture2DMaxSize;
    }

    GLint getLegacyTextureUnitCount() {
        GLint textureUnits = 0;
        g_glFuncs.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureUnits);
        if (textureUnits <= 0) {
            textureUnits = 16;
        }
        return textureUnits;
    }

    bool isBlockedCompatExtension(const char* extension) {
        return extension != nullptr &&
               (std::strcmp(extension, "GL_KHR_debug") == 0 ||
                std::strcmp(extension, "GL_ARB_debug_output") == 0 ||
                std::strcmp(extension, "GL_AMD_debug_output") == 0);
    }

    const std::vector<std::string>& getFilteredBackendExtensions() {
        static std::vector<std::string> cachedExtensions;
        static bool initialized = false;
        if (initialized) {
            return cachedExtensions;
        }
        initialized = true;

        GLint extCount = 0;
        g_glFuncs.glGetIntegerv(GL_NUM_EXTENSIONS, &extCount);
        if (extCount > 0 && g_glFuncs.glGetStringi) {
            cachedExtensions.reserve(static_cast<size_t>(extCount));
            for (GLint i = 0; i < extCount; ++i) {
                const auto* extension = reinterpret_cast<const char*>(g_glFuncs.glGetStringi(GL_EXTENSIONS, i));
                if (!extension || extension[0] == '\0' || isBlockedCompatExtension(extension)) {
                    continue;
                }
                cachedExtensions.emplace_back(extension);
            }
            return cachedExtensions;
        }

        const auto* legacyExtensions = reinterpret_cast<const char*>(g_glFuncs.glGetString(GL_EXTENSIONS));
        if (!legacyExtensions || legacyExtensions[0] == '\0') {
            return cachedExtensions;
        }

        std::string token;
        for (const char* cursor = legacyExtensions;; ++cursor) {
            const char ch = *cursor;
            if (ch == ' ' || ch == '\0') {
                if (!token.empty() && !isBlockedCompatExtension(token.c_str())) {
                    cachedExtensions.push_back(token);
                }
                token.clear();
                if (ch == '\0') {
                    break;
                }
                continue;
            }
            token.push_back(ch);
        }
        return cachedExtensions;
    }

    GLint getCompatFixedTextureLimit(GLenum pname, GLint reportedValue) {
        GLint backendValue = 0;
        g_glFuncs.glGetIntegerv(pname, &backendValue);

        static bool loggedTextureSize = false;
        static bool loggedCubeMapSize = false;
        static bool loggedRenderbufferSize = false;
        bool* logged = nullptr;
        switch (pname) {
        case GL_MAX_TEXTURE_SIZE:
            logged = &loggedTextureSize;
            break;
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
            logged = &loggedCubeMapSize;
            break;
        case GL_MAX_RENDERBUFFER_SIZE:
            logged = &loggedRenderbufferSize;
            break;
        default:
            break;
        }

        if (logged && !*logged) {
            *logged = true;
            std::fprintf(stderr,
                         "SFPEW_COMPAT_CAP pname=0x%X backend=%d reported=%d minimum=%d\n",
                         pname,
                         backendValue,
                         reportedValue,
                         kCompatMinUsableTextureSize);
        }
        return reportedValue;
    }

    bool isMostlyPrintable(const char* value) {
        if (!value || value[0] == '\0') {
            return false;
        }
        for (size_t i = 0; value[i] != '\0' && i < 256; ++i) {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            if (!(std::isprint(ch) || std::isspace(ch))) {
                return false;
            }
        }
        return true;
    }

    std::string readBackendString(GLenum name, const char* fallback) {
        const auto* raw = reinterpret_cast<const char*>(g_glFuncs.glGetString(name));
        if (!isMostlyPrintable(raw)) {
            return fallback;
        }
        return raw;
    }

    std::string buildExtensionString() {
        std::string extensions;
        for (const char* ext : kCompatExtensions) {
            if (!extensions.empty()) {
                extensions.push_back(' ');
            }
            extensions += ext;
        }

        for (const auto& extension : getFilteredBackendExtensions()) {
            extensions.push_back(' ');
            extensions += extension;
        }
        return extensions;
    }

    int getActiveTextureIndex() {
        const GLint textureIndex = static_cast<GLint>(g_glstate.fpe_state.active_texture - GL_TEXTURE0);
        if (textureIndex >= 0 && textureIndex < MAX_TEX) {
            return textureIndex;
        }
        return 0;
    }

    bool tryGetHijackedEnabled(GLenum cap, GLboolean* enabled) {
        if (!enabled) {
            return false;
        }

        switch (cap) {
        case GL_FOG:
            *enabled = g_glstate.fpe_state.fpe_bools.fog_enable ? GL_TRUE : GL_FALSE;
            return true;
        case GL_LIGHTING:
            *enabled = g_glstate.fpe_state.fpe_bools.lighting_enable ? GL_TRUE : GL_FALSE;
            return true;
        case GL_ALPHA_TEST:
            *enabled = g_glstate.fpe_state.fpe_bools.alpha_test_enable ? GL_TRUE : GL_FALSE;
            return true;
        case GL_COLOR_MATERIAL:
            *enabled = g_glstate.fpe_state.fpe_bools.color_material_enable ? GL_TRUE : GL_FALSE;
            return true;
        case GL_RESCALE_NORMAL:
            *enabled = g_glstate.fpe_state.fpe_bools.rescale_normal_enable ? GL_TRUE : GL_FALSE;
            return true;
        case GL_TEXTURE_2D:
            *enabled = g_glstate.fpe_state.fpe_bools.texture_2d_enable[getActiveTextureIndex()] ? GL_TRUE : GL_FALSE;
            return true;
        case GL_LIGHT0:
        case GL_LIGHT1:
        case GL_LIGHT2:
        case GL_LIGHT3:
        case GL_LIGHT4:
        case GL_LIGHT5:
        case GL_LIGHT6:
        case GL_LIGHT7: {
            const GLint lightIndex = static_cast<GLint>(cap - GL_LIGHT0);
            if (lightIndex >= 0 && lightIndex < MAX_LIGHTS) {
                *enabled = g_glstate.fpe_state.fpe_bools.light_enable[lightIndex] ? GL_TRUE : GL_FALSE;
                return true;
            }
            break;
        }
        default:
            break;
        }
        return false;
    }

    bool tryGetTexEnvFloat(GLenum pname, GLfloat* params) {
        if (!params) {
            return false;
        }

        const int textureIndex = getActiveTextureIndex();
        const auto& envState = g_glstate.fpe_state.texture_env[textureIndex];
        const auto& envUniform = g_glstate.fpe_uniform.texture_env[textureIndex];

        switch (pname) {
        case GL_TEXTURE_ENV_MODE:
            params[0] = static_cast<GLfloat>(envState.mode);
            return true;
        case GL_TEXTURE_ENV_COLOR:
            std::memcpy(params, glm::value_ptr(envUniform.color), sizeof(GLfloat) * 4);
            return true;
        case GL_RGB_SCALE:
            params[0] = envUniform.rgb_scale;
            return true;
        case GL_ALPHA_SCALE:
            params[0] = envUniform.alpha_scale;
            return true;
        case GL_COMBINE_RGB:
            params[0] = static_cast<GLfloat>(envState.combine_rgb);
            return true;
        case GL_COMBINE_ALPHA:
            params[0] = static_cast<GLfloat>(envState.combine_alpha);
            return true;
        case GL_SOURCE0_RGB:
        case GL_SOURCE1_RGB:
        case GL_SOURCE2_RGB:
            params[0] = static_cast<GLfloat>(envState.source_rgb[pname - GL_SOURCE0_RGB]);
            return true;
        case GL_SOURCE0_ALPHA:
        case GL_SOURCE1_ALPHA:
        case GL_SOURCE2_ALPHA:
            params[0] = static_cast<GLfloat>(envState.source_alpha[pname - GL_SOURCE0_ALPHA]);
            return true;
        case GL_OPERAND0_RGB:
        case GL_OPERAND1_RGB:
        case GL_OPERAND2_RGB:
            params[0] = static_cast<GLfloat>(envState.operand_rgb[pname - GL_OPERAND0_RGB]);
            return true;
        case GL_OPERAND0_ALPHA:
        case GL_OPERAND1_ALPHA:
        case GL_OPERAND2_ALPHA:
            params[0] = static_cast<GLfloat>(envState.operand_alpha[pname - GL_OPERAND0_ALPHA]);
            return true;
        default:
            return false;
        }
    }

    bool tryGetTexEnvInt(GLenum pname, GLint* params) {
        if (!params) {
            return false;
        }

        const int textureIndex = getActiveTextureIndex();
        const auto& envState = g_glstate.fpe_state.texture_env[textureIndex];
        const auto& envUniform = g_glstate.fpe_uniform.texture_env[textureIndex];

        switch (pname) {
        case GL_TEXTURE_ENV_MODE:
            params[0] = static_cast<GLint>(envState.mode);
            return true;
        case GL_TEXTURE_ENV_COLOR:
            params[0] = static_cast<GLint>(envUniform.color[0] * static_cast<GLfloat>(INT32_MAX));
            params[1] = static_cast<GLint>(envUniform.color[1] * static_cast<GLfloat>(INT32_MAX));
            params[2] = static_cast<GLint>(envUniform.color[2] * static_cast<GLfloat>(INT32_MAX));
            params[3] = static_cast<GLint>(envUniform.color[3] * static_cast<GLfloat>(INT32_MAX));
            return true;
        case GL_RGB_SCALE:
            params[0] = static_cast<GLint>(envUniform.rgb_scale);
            return true;
        case GL_ALPHA_SCALE:
            params[0] = static_cast<GLint>(envUniform.alpha_scale);
            return true;
        case GL_COMBINE_RGB:
            params[0] = static_cast<GLint>(envState.combine_rgb);
            return true;
        case GL_COMBINE_ALPHA:
            params[0] = static_cast<GLint>(envState.combine_alpha);
            return true;
        case GL_SOURCE0_RGB:
        case GL_SOURCE1_RGB:
        case GL_SOURCE2_RGB:
            params[0] = static_cast<GLint>(envState.source_rgb[pname - GL_SOURCE0_RGB]);
            return true;
        case GL_SOURCE0_ALPHA:
        case GL_SOURCE1_ALPHA:
        case GL_SOURCE2_ALPHA:
            params[0] = static_cast<GLint>(envState.source_alpha[pname - GL_SOURCE0_ALPHA]);
            return true;
        case GL_OPERAND0_RGB:
        case GL_OPERAND1_RGB:
        case GL_OPERAND2_RGB:
            params[0] = static_cast<GLint>(envState.operand_rgb[pname - GL_OPERAND0_RGB]);
            return true;
        case GL_OPERAND0_ALPHA:
        case GL_OPERAND1_ALPHA:
        case GL_OPERAND2_ALPHA:
            params[0] = static_cast<GLint>(envState.operand_alpha[pname - GL_OPERAND0_ALPHA]);
            return true;
        default:
            return false;
        }
    }
}

const GLubyte* glGetString(GLenum name) {
    if (!g_sfpewCompatMode) {
        if (const auto* value = g_glFuncs.glGetString(name)) {
            return value;
        }
        return reinterpret_cast<const GLubyte*>("");
    }

    // we only wrap GL_VERSION GL_RENDERER GL_VENDOR
    switch (name) {
    case GL_VERSION:
        static std::string cachedVersionString;
        if (cachedVersionString.empty()) {
            cachedVersionString = readBackendString(GL_VERSION, "3.3") + " (with Simple FPE Wrapper)";
        }
        return (const GLubyte*)cachedVersionString.c_str();
    case GL_RENDERER:
        static std::string cachedRendererString;
        if (cachedRendererString.empty()) {
            cachedRendererString = readBackendString(GL_RENDERER, "MobileGL Compat") + " (SFPEW)";
        }
        return (const GLubyte*)cachedRendererString.c_str();
    case GL_VENDOR:
        static std::string cachedVendorString;
        if (cachedVendorString.empty()) {
            cachedVendorString = readBackendString(GL_VENDOR, "MobileGL-Dev");
            if (!containsMobileGLDev(cachedVendorString)) {
                cachedVendorString += " (SFPEW: MobileGL-Dev)";
            }
        }
        return (const GLubyte*)cachedVendorString.c_str();
    case GL_EXTENSIONS:
        static std::string cachedExtensionsString;
        if (cachedExtensionsString.empty()) {
            cachedExtensionsString = buildExtensionString();
        }
        return reinterpret_cast<const GLubyte*>(cachedExtensionsString.c_str());
    case GL_SHADING_LANGUAGE_VERSION:
        static std::string cachedShadingLanguageVersion;
        if (cachedShadingLanguageVersion.empty()) {
            cachedShadingLanguageVersion = readBackendString(GL_SHADING_LANGUAGE_VERSION, "1.50");
        }
        return reinterpret_cast<const GLubyte*>(cachedShadingLanguageVersion.c_str());
    default:
        if (const auto* value = g_glFuncs.glGetString(name)) {
            return value;
        }
        return reinterpret_cast<const GLubyte*>("");
    }
}

const GLubyte* glGetStringi(GLenum name, GLuint index) {
    if (!g_sfpewCompatMode) {
        if (!g_glFuncs.glGetStringi) {
            return reinterpret_cast<const GLubyte*>("");
        }
        if (const auto* extension = g_glFuncs.glGetStringi(name, index)) {
            return extension;
        }
        return reinterpret_cast<const GLubyte*>("");
    }

    if (name != GL_EXTENSIONS) {
        return g_glFuncs.glGetStringi(name, index);
    }

    if (index < std::size(kCompatExtensions)) {
        return reinterpret_cast<const GLubyte*>(kCompatExtensions[index]);
    }

    const auto& filteredExtensions = getFilteredBackendExtensions();
    const size_t backendIndex = index - std::size(kCompatExtensions);
    if (backendIndex < filteredExtensions.size()) {
        return reinterpret_cast<const GLubyte*>(filteredExtensions[backendIndex].c_str());
    }
    return reinterpret_cast<const GLubyte*>("");
}

void glGetIntegerv(GLenum pname, GLint* params) {
    if (!params) {
        throw std::invalid_argument("params pointer cannot be null");
    }

    switch (pname) {
    case GL_CURRENT_PROGRAM:
        *params = g_glstate.backend_current_program;
        return;
    case GL_VERTEX_ARRAY_BINDING:
        *params = g_glstate.backend_vertex_array_binding;
        return;
    case GL_ARRAY_BUFFER_BINDING:
        *params = g_glstate.backend_array_buffer_binding;
        return;
    default:
        break;
    }

    if (!g_sfpewCompatMode) {
        g_glFuncs.glGetIntegerv(pname, params);
        return;
    }

    switch (pname) {
    case GL_CONTEXT_PROFILE_MASK:
        *params = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
        break;
    case GL_CONTEXT_FLAGS:
        *params = 0;
        break;
    case GL_NUM_EXTENSIONS:
        *params = static_cast<GLint>(std::size(kCompatExtensions) + getFilteredBackendExtensions().size());
        break;
    case GL_MAX_ATTRIB_STACK_DEPTH:
        *params = 16;
        break;
    case GL_MAX_MODELVIEW_STACK_DEPTH:
        *params = 32;
        break;
    case GL_MAX_PROJECTION_STACK_DEPTH:
        *params = 4;
        break;
    case GL_MAX_TEXTURE_STACK_DEPTH:
        *params = 4;
        break;
    case GL_MAX_TEXTURE_SIZE:
        *params = getCompatFixedTextureLimit(GL_MAX_TEXTURE_SIZE, kCompatReportedMaxTextureSize);
        break;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        *params = getCompatFixedTextureLimit(GL_MAX_CUBE_MAP_TEXTURE_SIZE, kCompatReportedMaxTextureSize);
        break;
    case GL_MAX_RENDERBUFFER_SIZE:
        *params = getCompatFixedTextureLimit(GL_MAX_RENDERBUFFER_SIZE, kCompatReportedMaxRenderbufferSize);
        break;
    case GL_MAX_TEXTURE_UNITS:
#if defined(GL_MAX_TEXTURE_UNITS_ARB) && GL_MAX_TEXTURE_UNITS_ARB != GL_MAX_TEXTURE_UNITS
    case GL_MAX_TEXTURE_UNITS_ARB:
#endif
    case GL_MAX_TEXTURE_COORDS:
#if defined(GL_MAX_TEXTURE_COORDS_ARB) && GL_MAX_TEXTURE_COORDS_ARB != GL_MAX_TEXTURE_COORDS
    case GL_MAX_TEXTURE_COORDS_ARB:
#endif
        *params = getLegacyTextureUnitCount();
        break;
    case GL_MAX_TEXTURE_IMAGE_UNITS_ARB:
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB:
        g_glFuncs.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, params);
        if (*params <= 0) {
            *params = getLegacyTextureUnitCount();
        }
        break;
#if defined(GL_MAX_TEXTURE_IMAGE_UNITS) && GL_MAX_TEXTURE_IMAGE_UNITS != GL_MAX_TEXTURE_IMAGE_UNITS_ARB
    case GL_MAX_TEXTURE_IMAGE_UNITS:
#endif
#if defined(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) && GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS != GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
#endif
        g_glFuncs.glGetIntegerv(pname, params);
        if (*params <= 0) {
            *params = getLegacyTextureUnitCount();
        }
        break;
    default:
        g_glFuncs.glGetIntegerv(pname, params);
        break;
    }
}

GLboolean glIsEnabled(GLenum cap) {
    GLboolean enabled = GL_FALSE;
    if (tryGetHijackedEnabled(cap, &enabled)) {
        return enabled;
    }

    if (g_glFuncs.glIsEnabled) {
        return g_glFuncs.glIsEnabled(cap);
    }
    return GL_FALSE;
}

void glGetTexEnvfv(GLenum target, GLenum pname, GLfloat* params) {
    if (!params) {
        throw std::invalid_argument("params pointer cannot be null");
    }

    if (target == GL_TEXTURE_ENV && tryGetTexEnvFloat(pname, params)) {
        return;
    }

    std::memset(params, 0, sizeof(GLfloat) * ((pname == GL_TEXTURE_ENV_COLOR) ? 4 : 1));
}

void glGetTexEnviv(GLenum target, GLenum pname, GLint* params) {
    if (!params) {
        throw std::invalid_argument("params pointer cannot be null");
    }

    if (target == GL_TEXTURE_ENV && tryGetTexEnvInt(pname, params)) {
        return;
    }

    std::memset(params, 0, sizeof(GLint) * ((pname == GL_TEXTURE_ENV_COLOR) ? 4 : 1));
}

void glGetFloatv(GLenum pname, GLfloat* params) {
    switch (pname) {
    case GL_MODELVIEW_MATRIX: {
        auto* ptr = glm::value_ptr(g_glstate.fpe_uniform.transformation.matrices[matrix_idx(GL_MODELVIEW)]);
        memcpy(params, ptr, sizeof(GLfloat) * 16);
        break;
    }
    case GL_PROJECTION_MATRIX: {
        auto* ptr = glm::value_ptr(g_glstate.fpe_uniform.transformation.matrices[matrix_idx(GL_PROJECTION)]);
        memcpy(params, ptr, sizeof(GLfloat) * 16);
        break;
    }
    default:
        g_glFuncs.glGetFloatv(pname, params);
        break;
    }
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const void* pixels) {
    if (target == GL_PROXY_TEXTURE_2D) {
        const bool supported = proxyTexture2DSupported(width, height, level);
        g_proxyTexture2DWidth = supported ? width : 0;
        g_proxyTexture2DHeight = supported ? height : 0;
        g_proxyTexture2DInternalFormat = internalformat;
        std::fprintf(stderr,
                     "SFPEW_PROXY_TEXIMAGE2D level=%d width=%d height=%d supported=%s max=%d\n",
                     level,
                     width,
                     height,
                     supported ? "true" : "false",
                     kProxyTexture2DMaxSize);
        return;
    }

    g_glFuncs.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) {
    if (!params) {
        throw std::invalid_argument("params pointer cannot be null");
    }

    if (target == GL_PROXY_TEXTURE_2D) {
        switch (pname) {
        case GL_TEXTURE_WIDTH:
            *params = proxyLevelSize(g_proxyTexture2DWidth, level);
            return;
        case GL_TEXTURE_HEIGHT:
            *params = proxyLevelSize(g_proxyTexture2DHeight, level);
            return;
        case GL_TEXTURE_INTERNAL_FORMAT:
            *params = g_proxyTexture2DInternalFormat;
            return;
        default:
            *params = 0;
            return;
        }
    }

    if (g_glFuncs.glGetTexLevelParameteriv) {
        g_glFuncs.glGetTexLevelParameteriv(target, level, pname, params);
        return;
    }
    *params = 0;
}

void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params) {
    if (!params) {
        throw std::invalid_argument("params pointer cannot be null");
    }

    GLint value = 0;
    glGetTexLevelParameteriv(target, level, pname, &value);
    *params = static_cast<GLfloat>(value);
}
