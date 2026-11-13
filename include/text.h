#ifndef TEXT_H
#define TEXT_H
#include <ppu-types.h>
#include <stddef.h>
int Text_Init(void);
void Text_Flush(void); /* Uploads newly rasterized Japanese glyphs before RSX_FlushDraws. */
void Text_Shutdown(void);
void SetFontSize(float sx, float sy);
void SetFontColor(u32 rgba, u32 bk_rgba);
void SetFontAutoCenter(int on_off);
float WidthFromStr(const char *str);
float DrawString(float x, float y, const char *str);
float DrawFormatString(float x, float y, const char *format, ...);
void DrawFString(float x, float y, const char *format, ...);
void DrawFXString(float x, float y, const char *format, ...);
void DrawWrappedText(float x, float y, float line_h, int max_chars, const char *text);
void utf8_to_cp1251(const char *src, char *dest, size_t max_len);
#endif
