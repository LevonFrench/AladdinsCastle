/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ACN22_NATIVE_TARGET_STATE_H
#define ACN22_NATIVE_TARGET_STATE_H
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include "acvr.h"
typedef struct {
    void (APIENTRY *integer)(GLenum,GLint *);
    void (APIENTRY *draw_buffers)(GLsizei,const GLenum *);
    void (APIENTRY *attachment)(GLenum,GLenum,GLenum,GLint *);
    GLenum (APIENTRY *error)(void);
    void (APIENTRY *texture)(GLenum,GLuint);
    void (APIENTRY *texture_parameter)(GLenum,GLint,GLenum,GLint *);
    void (APIENTRY *renderbuffer)(GLenum,GLuint);
    void (APIENTRY *renderbuffer_parameter)(GLenum,GLenum,GLint *);
} acn22_target_ops;
typedef struct {
    GLenum selections[32];
    GLsizei count;
    int restore_pending;
} acn22_target_selection;
/* Caller has the borrowed target bound. Values only, no retained FBO handle. */
acvr_result acn22_target_colour_begin(const acn22_target_ops *,acn22_target_selection *);
acvr_result acn22_target_colour_restore(const acn22_target_ops *,acn22_target_selection *);
acvr_result acn22_target_level_zero(const acn22_target_ops *);
/* Single colour0 plus optional declared depth. Query bindings are restored;
 * attachments are never changed. Other colour/stencil layouts are unsupported. */
acvr_result acn22_target_images(const acn22_target_ops *,const acvr_render_target *);
#endif
