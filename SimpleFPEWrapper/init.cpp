// SimpleFPEWrapper - SimpleFPEWrapper/init.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "init.h"
#include "fpe/fpe.hpp"
#include <dlfcn.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

SFPEW::External::EGLFunctionsTable g_eglFuncs;
SFPEW::External::BackendGLFunctionsTable g_glFuncs;
bool g_sfpewCompatMode = true;
bool g_sfpewDebugLogging = false;
bool g_sfpewBackendAlphaTestAvailable = false;

namespace {
    constexpr const char* kPluginConfigPath = "/storage/emulated/0/FCL/mobilegl-plugin.cfg";
    constexpr const char* kLoaderLogPath = "/storage/emulated/0/FCL/mobilegl-loader.log";

    std::string Trim(std::string value) {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return {};
        }
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    bool ParseBoolString(const std::string& value, bool defaultValue = false) {
        if (value.empty()) {
            return defaultValue;
        }

        std::string normalized;
        normalized.reserve(value.size());
        for (char ch : value) {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }

        if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
            return true;
        }
        if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
            return false;
        }
        return defaultValue;
    }

    void LoadPluginConfig() {
        std::ifstream configStream(kPluginConfigPath);
        if (!configStream.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(configStream, line)) {
            const auto commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line.erase(commentPos);
            }

            const auto eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                continue;
            }

            const auto key = Trim(line.substr(0, eqPos));
            const auto value = Trim(line.substr(eqPos + 1));
            if (key == "gl_mode") {
                g_sfpewCompatMode = value != "direct";
            } else if (key == "debug_log") {
                g_sfpewDebugLogging = ParseBoolString(value, false);
            }
        }
    }

    void AppendLoaderLogLine(const std::string& line) {
        std::ofstream stream(kLoaderLogPath, std::ios::app);
        if (!stream.is_open()) {
            return;
        }
        stream << line << '\n';
    }

    void ResetLoaderLog() {
        std::ofstream stream(kLoaderLogPath, std::ios::trunc);
        if (!stream.is_open()) {
            return;
        }
        stream << "SFPEW loader log\n";
    }

    void AppendLoaderSymbolInfo(const char* label, const void* ptr) {
        if (label == nullptr) {
            return;
        }

        if (ptr == nullptr) {
            AppendLoaderLogLine(std::string("symbol=") + label + " ptr=<null>");
            return;
        }

        Dl_info info{};
        if (dladdr(ptr, &info) && info.dli_fname != nullptr) {
            std::string line = std::string("symbol=") + label + " ptr=";
            char ptrBuf[32];
            std::snprintf(ptrBuf, sizeof(ptrBuf), "%p", ptr);
            line += ptrBuf;
            line += " so=";
            line += info.dli_fname;
            line += " sym=";
            line += info.dli_sname ? info.dli_sname : "<unknown>";
            AppendLoaderLogLine(line);
            return;
        }

        char ptrBuf[32];
        std::snprintf(ptrBuf, sizeof(ptrBuf), "%p", ptr);
        AppendLoaderLogLine(std::string("symbol=") + label + " ptr=" + ptrBuf + " so=<unknown>");
    }
}

bool SFPEWIsDebugLoggingEnabled() {
    return g_sfpewDebugLogging;
}

void SFPEWDebugLog(const char* fmt, ...) {
    if (!g_sfpewDebugLogging || fmt == nullptr) {
        return;
    }

    std::fputs("SFPEW_DEBUG ", stderr);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
}

void SFPEWDrainBackendErrors(const char* stage) {
    if (!g_sfpewDebugLogging || g_glFuncs.glGetError == nullptr) {
        return;
    }

    GLenum error = GL_NO_ERROR;
    int count = 0;
    while ((error = g_glFuncs.glGetError()) != GL_NO_ERROR) {
        std::fprintf(stderr, "SFPEW_GL_ERROR stage=%s error=%s(0x%04X)\n",
                     stage ? stage : "<null>",
                     glEnumToString(error),
                     error);
        if (++count >= 16) {
            std::fprintf(stderr, "SFPEW_GL_ERROR stage=%s additional_errors_suppressed=1\n",
                         stage ? stage : "<null>");
            break;
        }
    }
}

void Init() {
    ResetLoaderLog();
    LoadPluginConfig();
    if (const char* envDebugLog = std::getenv("SFPEW_DEBUG_LOG")) {
        g_sfpewDebugLogging = ParseBoolString(envDebugLog, g_sfpewDebugLogging);
    }

    SFPEWDebugLog("Init compat_mode=%d config_path=%s", g_sfpewCompatMode ? 1 : 0, kPluginConfigPath);

    std::string eglLibName;
    const char* envEglLib = std::getenv("SFPEW_EGL");
    AppendLoaderLogLine(std::string("env.SFPEW_EGL=") + (envEglLib ? envEglLib : "<unset>"));
    if (envEglLib) {
        eglLibName = envEglLib;
    } else {
        eglLibName = g_sfpewCompatMode ? "libMobileGL.so" : "libEGL.so";
    }
    AppendLoaderLogLine(std::string("requested_egl_lib=") + eglLibName);
    AppendLoaderLogLine(std::string("compat_mode=") + (g_sfpewCompatMode ? "1" : "0"));

    if (!SFPEW::Utils::BackendLoader::AcquireEGLFunctions(g_eglFuncs, eglLibName) ||
        g_eglFuncs.eglGetProcAddress == nullptr) {
        throw std::runtime_error("Failed to acquire EGL functions");
    }

    if (!SFPEW::Utils::BackendLoader::AcquireBackendGLFunctions(g_glFuncs, g_eglFuncs.eglGetProcAddress) ||
        g_glFuncs.glGetString == nullptr) {
        throw std::runtime_error("Failed to acquire BackendGL functions");
    } // FIXME: actually we should acquire gl functions after egl initialization

    AppendLoaderSymbolInfo("eglGetProcAddress", reinterpret_cast<const void*>(g_eglFuncs.eglGetProcAddress));
    AppendLoaderSymbolInfo("glGetError", reinterpret_cast<const void*>(g_glFuncs.glGetError));
    AppendLoaderSymbolInfo("glEnable", reinterpret_cast<const void*>(g_glFuncs.glEnable));
    AppendLoaderSymbolInfo("glBlendFunc", reinterpret_cast<const void*>(g_glFuncs.glBlendFunc));
    AppendLoaderSymbolInfo("glBindVertexArray", reinterpret_cast<const void*>(g_glFuncs.glBindVertexArray));

    g_sfpewBackendAlphaTestAvailable = (g_glFuncs.glAlphaFunc != nullptr);
    SFPEWDebugLog("Backend alpha_test_available=%d", g_sfpewBackendAlphaTestAvailable ? 1 : 0);

    SFPEWDebugLog("Backend acquired egl=%s compat_mode=%d", eglLibName.c_str(), g_sfpewCompatMode ? 1 : 0);
    init_fpe();
}

struct InitClass {
    InitClass() { Init(); }
};

static InitClass staticInitObject;
