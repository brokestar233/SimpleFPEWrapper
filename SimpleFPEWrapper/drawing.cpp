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

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    LIST_RECORD(glDrawArrays, {}, mode, first, count)

    // TODO: deal with draw in list later
    if (DisplayListManager::isCalling()) {
        return;
    }

    SFPEWDebugLog("DRAWARRAYS enter mode=%s first=%d count=%d compat=%d",
                  glEnumToString(mode),
                  first,
                  count,
                  g_sfpewCompatMode ? 1 : 0);
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
    } else
        g_glFuncs.glDrawArrays(mode, first, count);

    SFPEWDrainBackendErrors("glDrawArrays.draw");
    g_glFuncs.glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    g_glFuncs.glBindVertexArray(prev_vao);
    SET_PREV_PROGRAM
    SFPEWDrainBackendErrors("glDrawArrays.restore");
}
