#ifndef _DOOM_SHIM_MATH_H
#define _DOOM_SHIM_MATH_H

// Only r_main.c's one-time R_InitTextureMapping setup (sin/tan, building
// the viewangletox lookup table) and v_video.c's mouse-acceleration check
// (fabs, on a branch this port never reaches with usemouse always 0) touch
// this at all -- see doom_libc_shim.c for the x87 FPU-backed
// implementations (kernel_init() runs one `fninit` at boot for this).
double sin(double x);
double cos(double x);
double tan(double x);
double fabs(double x);

#endif
