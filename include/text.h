#ifndef TEXT_H
#define TEXT_H

#include <libfont.h>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <stdint.h>

FT_Library freetype;
FT_Face face;

int TTFLoadFont(char * path, void * from_memory, int size_from_memory);
void LoadFontIDK();
void TTFUnloadFont();
void TTF_to_Bitmap(u8 chr, u8 * bitmap, short *w, short *h, short *y_correction);
void DrawWrappedText(float x, float y, float line_h, int max_chars, const char *text);
void utf8_to_cp1251(const char* src, char* dest, size_t max_len);
void DrawFString(float x, float y, const char* format, ...);

#endif