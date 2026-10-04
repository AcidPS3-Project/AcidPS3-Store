#ifndef DRAW_H
#define DRAW_H

#include "global.h"

#define PAD_TRIANGLE 0
#define PAD_CIRCLE 1
#define PAD_CROSS 2
#define PAD_SQUARE 3
#define PAD_START 4
#define PAD_SELECT 5
#define PAD_L1 6
#define PAD_R1 7
#define PAD_L2 8
#define PAD_R2 9
#define PAD_L3 10
#define PAD_R3 11

pngData DefaultIcon;
u32 DefaultOff;
pngData Buttons;
u32 ButtonsOff;

u32 colorsa[16];
u32 colorsb[16];
u32 colorsc[16];
u32 colorsd[16];
u32 colorse[16];
u32 colorsf[16];

extern int cols;

int LoadIconPNG(const char *path, pngData *tex, u32 *icon_off);
void LoadTexture();
void drawScene();

#endif