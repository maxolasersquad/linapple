#ifndef STRETCH_H
#define STRETCH_H

#include <cstdint>
#include "apple2/Video.h"

int VideoSoftStretch(VideoSurface *src, VideoRect *srcrect, VideoSurface *dst, VideoRect *dstrect);
int VideoSoftStretchOr(VideoSurface *src, VideoRect *srcrect, VideoSurface *dst, VideoRect *dstrect);
int VideoSoftStretchMono8(VideoSurface *src, VideoRect *srcrect, VideoSurface *dst, VideoRect *dstrect, uint32_t fgbrush, uint32_t bgbrush);

// Font Routines
#define FONT_SIZE_X  6
#define FONT_SIZE_Y  8
#define CHARS_IN_ROW  45
extern VideoSurface *font_sfc;

bool fonts_initialization(void);
void fonts_termination(void);
void font_print(int x, int y, const char *text, VideoSurface *surface, double kx, double ky);
void font_print_right(int x, int y, const char *text, VideoSurface *surface, double kx, double ky);
void font_print_centered(int x, int y, const char *text, VideoSurface *surface, double kx, double ky);

// Some auxiliary functions
void surface_fader(VideoSurface *surface, float r_factor, float g_factor, float b_factor, float a_factor, VideoRect *r);
void putpixel(VideoSurface *surface, int x, int y, uint32_t pixel);
void rectangle(VideoSurface *surface, int x, int y, int w, int h, uint32_t pixel);

#endif
