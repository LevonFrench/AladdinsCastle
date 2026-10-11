#ifndef ACN22_EYE_H
#define ACN22_EYE_H
#include "acvr.h"
#include "geo_hw.h"
#include "ss22_gl.h"
/* Slice admission only; no backend capability or production ABI claim. */
extern const acvr_eye *acn22_eye;
acvr_result acn22_begin_eye(const acvr_eye *eye);
void acn22_end_eye(void);
void acn22_latch_regs(const ss22_regs *regs);
int acn22_project(const geo_quad *source, const acvr_eye *eye, geo_quad *copy);
const geo_quad *acn22_prepared(int *count);
void acn22_draw_quad(const geo_quad *source, const void *cfg);
#endif
