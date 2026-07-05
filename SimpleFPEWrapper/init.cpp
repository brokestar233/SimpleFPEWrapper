// SimpleFPEWrapper - SimpleFPEWrapper/init.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "init.h"
#include "fpe/fpe.hpp"
#include <fstream>
#include <string>

SFPEW::External::EGLFunctionsTable g_eglFuncs;
SFPEW::External::BackendGLFunctionsTable g_glFuncs;
bool g_sfpewCompatMode = true;

namespace {
    constexpr const char* kPluginConfigPath = "/storage/emulated/0/FCL/mobilegl-plugin.cfg";

    std::string Trim(std::string value) {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return {};
        }
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
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
            }
        }
    }
}

void Init() {
    LoadPluginConfig();

    std::string eglLibName;
    const char* envEglLib = std::getenv("SFPEW_EGL");
    if (envEglLib) {
        eglLibName = envEglLib;
    } else {
        eglLibName = "libEGL.so";
    }

    if (!SFPEW::Utils::BackendLoader::AcquireEGLFunctions(g_eglFuncs, eglLibName) ||
        g_eglFuncs.eglGetProcAddress == nullptr) {
        throw std::runtime_error("Failed to acquire EGL functions");
    }

    if (!SFPEW::Utils::BackendLoader::AcquireBackendGLFunctions(g_glFuncs, g_eglFuncs.eglGetProcAddress) ||
        g_glFuncs.glGetString == nullptr) {
        throw std::runtime_error("Failed to acquire BackendGL functions");
    } // FIXME: actually we should acquire gl functions after egl initialization

    init_fpe();
}

struct InitClass {
    InitClass() { Init(); }
};

static InitClass staticInitObject;
