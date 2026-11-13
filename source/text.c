#include "libs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "rsx_renderer.h"
#include "text.h"
#include "asimov_ttf_bin.h"
#include "noto_sans_jp_ttf_bin.h"

#define FONT_FIRST       32
#define FONT_LAST        255
#define FONT_CELL        64
#define FONT_PAD         2
#define FONT_STRIDE      (FONT_CELL + FONT_PAD * 2)
#define FONT_COLS        16
#define FONT_ROWS        (((FONT_LAST - FONT_FIRST + 1) + FONT_COLS - 1) / FONT_COLS)
#define FONT_ATLAS_W     (FONT_COLS * FONT_STRIDE)
#define FONT_ATLAS_H     (FONT_ROWS * FONT_STRIDE)

#define JP_CELL          64
#define JP_PAD           2
#define JP_STRIDE        (JP_CELL + JP_PAD * 2)
#define JP_COLS          16
#define JP_CACHE_SLOTS   256
#define JP_ROWS          (JP_CACHE_SLOTS / JP_COLS)
#define JP_ATLAS_W       (JP_COLS * JP_STRIDE)
#define JP_ATLAS_H       (JP_ROWS * JP_STRIDE)

/* Texture 63 is owned by the renderer's white texture. */
#define JP_TEX_ID        (RSX_MAX_TEXTURES - 3) /* 61 */
#define FONT_TEX_ID      (RSX_MAX_TEXTURES - 2) /* 62 */
#define FONT_SCREEN_W    916.0f

typedef struct
{
    uint32_t codepoint;
    float advance;
    short rows;
    short y_corr;
    short bearing_x;
    int valid;
} FontGlyph;

static FontGlyph base_glyphs[256];
static FontGlyph jp_glyphs[JP_CACHE_SLOTS];
static int jp_glyph_count = 0;
static int text_ready = 0;
static int jp_ready = 0;
static int jp_atlas_dirty = 0;
static u32 *jp_atlas = NULL;
static FT_Library freetype;
static FT_Face base_face = NULL;
static FT_Face jp_face = NULL;
static float font_w = 32.0f, font_h = 32.0f;
static u32 font_color = 0xFFFFFFFF;
static int font_autocenter = 0;

static uint32_t cp1251_to_unicode(unsigned char c)
{
    switch(c)
    {
        case 0x80: return 0x00A1; case 0x81: return 0x00BF;
        case 0x82: return 0x00E1; case 0x83: return 0x00E9;
        case 0x84: return 0x00ED; case 0x85: return 0x00F3;
        case 0x86: return 0x00FA; case 0x87: return 0x00FC;
        case 0x88: return 0x00F1; case 0x89: return 0x00C1;
        case 0x8A: return 0x00C9; case 0x8B: return 0x00CD;
        case 0x8C: return 0x00D3; case 0x8D: return 0x00DA;
        case 0x8E: return 0x00DC; case 0x8F: return 0x00D1;
        case 0x92: return 0x0404; case 0x93: return 0x0454;
        case 0x94: return 0x0407; case 0x95: return 0x0457;
        case 0x96: return 0x0490; case 0x97: return 0x0491;
        case 0xA1: return 0x04D8; case 0xA2: return 0x04D9;
        case 0xA3: return 0x0492; case 0xA4: return 0x0493;
        case 0xA5: return 0x049A; case 0xA6: return 0x049B;
        case 0xA7: return 0x04A2; case 0xAA: return 0x04A3;
        case 0xAB: return 0x04E8; case 0xAC: return 0x04E9;
        case 0xAD: return 0x04AE; case 0xAE: return 0x04AF;
        case 0xAF: return 0x04B0; case 0xB0: return 0x04B1;
        case 0xB1: return 0x04BA; case 0xB2: return 0x04BB;
        case 0xA8: return 0x0401; case 0xB8: return 0x0451;
        default:
            if(c >= 0xC0) return (uint32_t)c + 0x0350;
            return c;
    }
}

static int unicode_to_base_byte(uint32_t cp)
{
    if(cp >= 0x20 && cp <= 0x7E) return (int)cp;
    if(cp >= 0x0410 && cp <= 0x044F) return (int)(cp - 0x0350);
    if(cp == 0x0401) return 0xA8;
    if(cp == 0x0451) return 0xB8;
    if(cp == 0x0404) return 0x92;
    if(cp == 0x0454) return 0x93;
    if(cp == 0x0407) return 0x94;
    if(cp == 0x0457) return 0x95;
    if(cp == 0x0490) return 0x96;
    if(cp == 0x0491) return 0x97;
    if(cp == 0x0406) return 'I';
    if(cp == 0x0456) return 'i';
    if(cp == 0x04D8) return 0xA1;
    if(cp == 0x04D9) return 0xA2;
    if(cp == 0x0492) return 0xA3;
    if(cp == 0x0493) return 0xA4;
    if(cp == 0x049A) return 0xA5;
    if(cp == 0x049B) return 0xA6;
    if(cp == 0x04A2) return 0xA7;
    if(cp == 0x04A3) return 0xAA;
    if(cp == 0x04E8) return 0xAB;
    if(cp == 0x04E9) return 0xAC;
    if(cp == 0x04AE) return 0xAD;
    if(cp == 0x04AF) return 0xAE;
    if(cp == 0x04B0) return 0xAF;
    if(cp == 0x04B1) return 0xB0;
    if(cp == 0x04BA) return 0xB1;
    if(cp == 0x04BB) return 0xB2;
    if(cp == 0x00A1) return 0x80;
    if(cp == 0x00BF) return 0x81;
    if(cp == 0x00E1) return 0x82;
    if(cp == 0x00E9) return 0x83;
    if(cp == 0x00ED) return 0x84;
    if(cp == 0x00F3) return 0x85;
    if(cp == 0x00FA) return 0x86;
    if(cp == 0x00FC) return 0x87;
    if(cp == 0x00F1) return 0x88;
    if(cp == 0x00C1) return 0x89;
    if(cp == 0x00C9) return 0x8A;
    if(cp == 0x00CD) return 0x8B;
    if(cp == 0x00D3) return 0x8C;
    if(cp == 0x00DA) return 0x8D;
    if(cp == 0x00DC) return 0x8E;
    if(cp == 0x00D1) return 0x8F;
    return -1;
}

static int is_japanese_codepoint(uint32_t cp)
{
    return (cp >= 0x3000 && cp <= 0x30FF) ||
           (cp >= 0x31F0 && cp <= 0x31FF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFF01 && cp <= 0xFFEF);
}

/* Returns a Unicode codepoint. Invalid UTF-8 bytes are treated as legacy CP1251. */
static uint32_t next_codepoint(const unsigned char *s, int *used, int *is_utf8)
{
    unsigned char a = s[0], b, c, d;
    *used = 1;
    *is_utf8 = 0;
    if(a < 0x80) return a;
    if(a >= 0xC2 && a <= 0xDF && s[1] != 0 && (s[1] & 0xC0) == 0x80)
    {
        *used = 2; *is_utf8 = 1;
        return ((uint32_t)(a & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if(a >= 0xE0 && a <= 0xEF)
    {
        b=s[1]; c=(b != 0) ? s[2] : 0;
        if(b != 0 && c != 0 && (b & 0xC0)==0x80 && (c & 0xC0)==0x80 &&
           !(a==0xE0 && b<0xA0) && !(a==0xED && b>=0xA0))
        {
            *used=3; *is_utf8=1;
            return ((uint32_t)(a&0x0F)<<12) | ((uint32_t)(b&0x3F)<<6) | (c&0x3F);
        }
    }
    if(a >= 0xF0 && a <= 0xF4)
    {
        b=s[1]; c=(b != 0) ? s[2] : 0; d=(c != 0) ? s[3] : 0;
        if(b != 0 && c != 0 && d != 0 && (b&0xC0)==0x80 && (c&0xC0)==0x80 && (d&0xC0)==0x80 &&
           !(a==0xF0 && b<0x90) && !(a==0xF4 && b>=0x90))
        {
            *used=4; *is_utf8=1;
            return ((uint32_t)(a&7)<<18) | ((uint32_t)(b&0x3F)<<12) |
                   ((uint32_t)(c&0x3F)<<6) | (d&0x3F);
        }
    }
    return cp1251_to_unicode(a);
}

static void copy_ft_bitmap(FT_GlyphSlot slot, u32 *atlas, int atlas_w,
                           int dst_x, int dst_y, int cell_w, int cell_h)
{
    int x, y;
    FT_Bitmap *bm = &slot->bitmap;
    for(y = 0; y < (int)bm->rows && y < cell_h; y++)
    {
        const unsigned char *row;
        if(bm->pitch >= 0) row = bm->buffer + y * bm->pitch;
        else row = bm->buffer + (bm->rows - 1 - y) * (-bm->pitch);
        for(x = 0; x < (int)bm->width && x < cell_w; x++)
        {
            unsigned char alpha = 0;
            if(bm->pixel_mode == FT_PIXEL_MODE_GRAY) alpha = row[x];
            else if(bm->pixel_mode == FT_PIXEL_MODE_MONO) alpha = (row[x >> 3] & (0x80 >> (x & 7))) ? 255 : 0;
            atlas[(size_t)(dst_y + y) * atlas_w + dst_x + x] = ((u32)alpha << 24) | 0x00FFFFFF;
        }
    }
}

static int build_base_atlas(void)
{
    u32 *atlas = (u32*)calloc((size_t)FONT_ATLAS_W * FONT_ATLAS_H, sizeof(u32));
    int c;
    if(!atlas) return 0;
    memset(base_glyphs, 0, sizeof(base_glyphs));
    for(c = FONT_FIRST; c <= FONT_LAST; c++)
    {
        int idx = c - FONT_FIRST;
        int x = (idx % FONT_COLS) * FONT_STRIDE + FONT_PAD;
        int y = (idx / FONT_COLS) * FONT_STRIDE + FONT_PAD;
        FT_GlyphSlot slot;
        uint32_t cp = cp1251_to_unicode((unsigned char)c);
        if(FT_Set_Pixel_Sizes(base_face, FONT_CELL, FONT_CELL) != 0 ||
           FT_Load_Char(base_face, cp, FT_LOAD_RENDER) != 0)
            continue;
        slot = base_face->glyph;
        copy_ft_bitmap(slot, atlas, FONT_ATLAS_W, x, y, FONT_CELL, FONT_CELL);
        /* Preserve the small descender correction used for Kazakh Қ/қ. */
        if(c == 0xA5 || c == 0xA6)
        {
            int base_row = (int)slot->bitmap.rows - 1;
            int right_edge = (int)slot->bitmap.width - 1;
            if(right_edge > FONT_CELL - 4) right_edge = FONT_CELL - 4;
            if(base_row > 0 && right_edge > 0)
            {
                atlas[(size_t)(y + base_row) * FONT_ATLAS_W + x + right_edge + 1] = 0xFFFFFFFF;
                atlas[(size_t)(y + base_row) * FONT_ATLAS_W + x + right_edge + 2] = 0xFFFFFFFF;
                if(base_row + 1 < FONT_CELL)
                    atlas[(size_t)(y + base_row + 1) * FONT_ATLAS_W + x + right_edge + 2] = 0xFFFFFFFF;
            }
        }
        base_glyphs[c].codepoint = cp;
        base_glyphs[c].advance = (float)((slot->advance.x + 32) >> 6);
        base_glyphs[c].rows = (short)((slot->bitmap.rows > FONT_CELL) ? FONT_CELL : slot->bitmap.rows);
        base_glyphs[c].y_corr = (short)(FONT_CELL - 1 - slot->bitmap_top);
        base_glyphs[c].bearing_x = (short)slot->bitmap_left;
        base_glyphs[c].valid = 1;
    }
    if(!RSX_LoadTexture(FONT_TEX_ID, atlas, FONT_ATLAS_W, FONT_ATLAS_H))
    {
        free(atlas);
        return 0;
    }
    free(atlas);
    RSX_SetTextureFilter(FONT_TEX_ID, 1);
    return 1;
}

static FontGlyph *get_jp_glyph(uint32_t cp)
{
    int i, slot_index;
    FT_GlyphSlot slot;
    FontGlyph *g;
    if(!jp_ready || !jp_face) return NULL;
    for(i = 0; i < jp_glyph_count; i++)
        if(jp_glyphs[i].codepoint == cp) return &jp_glyphs[i];
    if(jp_glyph_count >= JP_CACHE_SLOTS) return NULL;
    if(FT_Get_Char_Index(jp_face, cp) == 0 ||
       FT_Set_Pixel_Sizes(jp_face, JP_CELL, JP_CELL) != 0 ||
       FT_Load_Char(jp_face, cp, FT_LOAD_RENDER) != 0)
        return NULL;
    slot = jp_face->glyph;
    slot_index = jp_glyph_count++;
    g = &jp_glyphs[slot_index];
    memset(g, 0, sizeof(*g));
    g->codepoint = cp;
    g->advance = (float)((slot->advance.x + 32) >> 6);
    if(g->advance < 1.0f) g->advance = (float)JP_CELL;
    g->rows = (short)((slot->bitmap.rows > JP_CELL) ? JP_CELL : slot->bitmap.rows);
    g->y_corr = (short)(JP_CELL - 1 - slot->bitmap_top);
    g->bearing_x = (short)slot->bitmap_left;
    g->valid = 1;
    {
        int col = slot_index % JP_COLS;
        int row = slot_index / JP_COLS;
        int px = col * JP_STRIDE + JP_PAD;
        int py = row * JP_STRIDE + JP_PAD;
        int yy;
        /* Clear the whole cell before drawing this glyph into its atlas slot. */
        for(yy = 0; yy < JP_STRIDE; yy++)
            memset(jp_atlas + (size_t)(row * JP_STRIDE + yy) * JP_ATLAS_W + col * JP_STRIDE,
                   0, JP_STRIDE * sizeof(u32));
        copy_ft_bitmap(slot, jp_atlas, JP_ATLAS_W, px, py, JP_CELL, JP_CELL);
        jp_atlas_dirty = 1;
    }
    return g;
}

int Text_Init(void)
{
    if(text_ready) return 1;
    if(FT_Init_FreeType(&freetype) != 0) return 0;
    if(FT_New_Memory_Face(freetype, (const FT_Byte*)asimov_ttf_bin, (FT_Long)asimov_ttf_bin_size, 0, &base_face) != 0)
    {
        FT_Done_FreeType(freetype); return 0;
    }
    if(!build_base_atlas())
    {
        FT_Done_Face(base_face); base_face=NULL;
        FT_Done_FreeType(freetype); return 0;
    }
    jp_atlas = (u32*)calloc((size_t)JP_ATLAS_W * JP_ATLAS_H, sizeof(u32));
    if(jp_atlas && RSX_LoadTexture(JP_TEX_ID, jp_atlas, JP_ATLAS_W, JP_ATLAS_H))
    {
        RSX_SetTextureFilter(JP_TEX_ID, 1);
        if(FT_New_Memory_Face(freetype, (const FT_Byte*)noto_sans_jp_ttf_bin, (FT_Long)noto_sans_jp_ttf_bin_size, 0, &jp_face) == 0)
        {
            if(FT_Get_Char_Index(jp_face, 0x3042) && FT_Get_Char_Index(jp_face, 0x30A2) && FT_Get_Char_Index(jp_face, 0x65E5))
				jp_ready = 1;
            else
            {
                printf("TEXT: Noto Sans JP file lacks basic kana/kanji coverage\n");
                FT_Done_Face(jp_face);
                jp_face = NULL;
            }
        }
        else printf("TEXT: Noto Sans JP could not be opened; Japanese glyphs disabled\n");
    }
    else
    {
        free(jp_atlas); jp_atlas=NULL;
        printf("TEXT: Japanese atlas allocation/upload failed\n");
    }
    text_ready = 1;
    printf("TEXT: base atlas ready; Japanese font %s\n", jp_ready ? "ready" : "unavailable");
    return 1;
}

void Text_Flush(void)
{
    if(jp_ready && jp_atlas_dirty)
    {
        if(RSX_UpdateTexture(JP_TEX_ID, jp_atlas, JP_ATLAS_W, JP_ATLAS_H)) jp_atlas_dirty = 0;
    }
}

void Text_Shutdown(void)
{
    if(base_face) { FT_Done_Face(base_face); base_face=NULL; }
    if(jp_face) { FT_Done_Face(jp_face); jp_face=NULL; }
    if(freetype) { FT_Done_FreeType(freetype); freetype=NULL; }
    RSX_UnloadTexture(FONT_TEX_ID);
    RSX_UnloadTexture(JP_TEX_ID);
    free(jp_atlas); jp_atlas=NULL;
    jp_glyph_count=0; jp_ready=0; jp_atlas_dirty=0; text_ready=0;
}

void SetFontSize(float sx, float sy) { font_w=sx; font_h=sy; }
void SetFontColor(u32 rgba, u32 bk_rgba) { (void)bk_rgba; font_color=rgba; }
void SetFontAutoCenter(int on_off) { font_autocenter=on_off; }

static float glyph_advance(uint32_t cp)
{
    int b = unicode_to_base_byte(cp);
    if(b >= FONT_FIRST && b <= FONT_LAST && base_glyphs[b].valid)
        return base_glyphs[b].advance;
    if(is_japanese_codepoint(cp))
    {
        FontGlyph *g = get_jp_glyph(cp);
        if(g) return g->advance;
        return base_glyphs['?'].advance;
    }
    return base_glyphs['?'].advance;
}

float WidthFromStr(const char *str)
{
    float width=0.0f, sx=font_w/FONT_CELL;
    const unsigned char *p=(const unsigned char*)str;
    if(!str) return 0.0f;
    while(*p)
    {
        int used, valid_utf8;
        uint32_t cp=next_codepoint(p,&used,&valid_utf8);
        (void)valid_utf8;
        if(cp=='\n') { p+=used; continue; }
        width += glyph_advance(cp) * sx;
        p += used;
    }
    return width;
}

float DrawString(float x, float y, const char *str)
{
    float sx=font_w/FONT_CELL, sy=font_h/FONT_CELL, start_x;
    const unsigned char *p=(const unsigned char*)str;
    if(!text_ready || !str) return x;
    if(font_autocenter) x=(FONT_SCREEN_W-WidthFromStr(str))*0.5f;
    start_x=x;
    while(*p)
    {
        int used, valid_utf8;
        uint32_t cp=next_codepoint(p,&used,&valid_utf8);
        FontGlyph *g=NULL;
        int b=-1, tex=FONT_TEX_ID, idx=0, col, row;
        float u,v,uw,vh,draw_w;
        (void)valid_utf8;
        if(cp=='\n') { x=start_x; y+=font_h; p+=used; continue; }
        b=unicode_to_base_byte(cp);
        if(b>=FONT_FIRST && b<=FONT_LAST && base_glyphs[b].valid)
        {
            g=&base_glyphs[b]; idx=b-FONT_FIRST;
            col=idx%FONT_COLS; row=idx/FONT_COLS;
            u=(float)(col*FONT_STRIDE+FONT_PAD)/FONT_ATLAS_W;
            v=(float)(row*FONT_STRIDE+FONT_PAD)/FONT_ATLAS_H;
            uw=(float)FONT_CELL/FONT_ATLAS_W;
            vh=(float)((g->rows>0)?g->rows:1)/FONT_ATLAS_H;
            draw_w=FONT_CELL*sx;
        }
        else if(is_japanese_codepoint(cp) && (g=get_jp_glyph(cp))!=NULL)
        {
            tex=JP_TEX_ID;
            idx=(int)(g-jp_glyphs);
            col=idx%JP_COLS; row=idx/JP_COLS;
            u=(float)(col*JP_STRIDE+JP_PAD)/JP_ATLAS_W;
            v=(float)(row*JP_STRIDE+JP_PAD)/JP_ATLAS_H;
            uw=(float)JP_CELL/JP_ATLAS_W;
            vh=(float)((g->rows>0)?g->rows:1)/JP_ATLAS_H;
            draw_w=JP_CELL*sx;
        }
        else
        {
            g=&base_glyphs['?']; idx='?'-FONT_FIRST;
            col=idx%FONT_COLS; row=idx/FONT_COLS;
            u=(float)(col*FONT_STRIDE+FONT_PAD)/FONT_ATLAS_W;
            v=(float)(row*FONT_STRIDE+FONT_PAD)/FONT_ATLAS_H;
            uw=(float)FONT_CELL/FONT_ATLAS_W;
            vh=(float)((g->rows>0)?g->rows:1)/FONT_ATLAS_H;
            draw_w=FONT_CELL*sx;
        }
        if(g && g->valid && g->rows>0)
            RSX_DrawSprite2D(tex, x+g->bearing_x*sx, y+g->y_corr*sy,
                             draw_w, g->rows*sy, u,v,uw,vh,font_color);
        x += (g && g->valid) ? g->advance*sx : font_w;
        p += used;
    }
    return x;
}

float DrawFormatString(float x, float y, const char *format, ...)
{
    char buffer[2048]; va_list args;
    va_start(args,format); vsnprintf(buffer,sizeof(buffer),format,args); va_end(args);
    return DrawString(x,y,buffer);
}

void DrawFXString(float x, float y, const char *format, ...)
{
    char buffer[2048]; va_list args;
    va_start(args,format); vsnprintf(buffer,sizeof(buffer),format,args); va_end(args);
    DrawString(x,y,buffer);
}

void DrawFString(float x, float y, const char *format, ...)
{
    char buffer[2048]; va_list args;
    va_start(args,format); vsnprintf(buffer,sizeof(buffer),format,args); va_end(args);
    DrawString(x,y,buffer);
}

void utf8_to_cp1251(const char *src, char *dest, size_t max_len)
{
    size_t i=0,j=0;
    if(!src || !dest || max_len==0) return;
    while(src[i] && j<max_len-1)
    {
        unsigned char c1=(unsigned char)src[i], c2=(unsigned char)src[i+1];
        if(c1==0xD3 && c2==0x98) { dest[j++]=(char)0xA1; i+=2; continue; }
        if(c1==0xD3 && c2==0x99) { dest[j++]=(char)0xA2; i+=2; continue; }
        if(c1==0xD2 && c2==0x92) { dest[j++]=(char)0xA3; i+=2; continue; }
        if(c1==0xD2 && c2==0x93) { dest[j++]=(char)0xA4; i+=2; continue; }
        if(c1==0xD2 && c2==0x9A) { dest[j++]=(char)0xA5; i+=2; continue; }
        if(c1==0xD2 && c2==0x9B) { dest[j++]=(char)0xA6; i+=2; continue; }
        if(c1==0xD2 && c2==0xA2) { dest[j++]=(char)0xA7; i+=2; continue; }
        if(c1==0xD2 && c2==0xA3) { dest[j++]=(char)0xAA; i+=2; continue; }
        if(c1==0xD3 && c2==0xA8) { dest[j++]=(char)0xAB; i+=2; continue; }
        if(c1==0xD3 && c2==0xA9) { dest[j++]=(char)0xAC; i+=2; continue; }
        if(c1==0xD2 && c2==0xAE) { dest[j++]=(char)0xAD; i+=2; continue; }
        if(c1==0xD2 && c2==0xAF) { dest[j++]=(char)0xAE; i+=2; continue; }
        if(c1==0xD2 && c2==0xB0) { dest[j++]=(char)0xAF; i+=2; continue; }
        if(c1==0xD2 && c2==0xB1) { dest[j++]=(char)0xB0; i+=2; continue; }
        if(c1==0xD2 && c2==0xBA) { dest[j++]=(char)0xB1; i+=2; continue; }
        if(c1==0xD2 && c2==0xBB) { dest[j++]=(char)0xB2; i+=2; continue; }
        if(c1==0xD0 && c2==0x81) { dest[j++]=(char)0xA8; i+=2; continue; }
        if(c1==0xD1 && c2==0x91) { dest[j++]=(char)0xB8; i+=2; continue; }
        if(c1==0xD0 && c2==0x86) { dest[j++]='I'; i+=2; continue; }
        if(c1==0xD1 && c2==0x96) { dest[j++]='i'; i+=2; continue; }
        if(c1==0xD0 && c2==0x84) { dest[j++]=(char)0x92; i+=2; continue; }
        if(c1==0xD1 && c2==0x94) { dest[j++]=(char)0x93; i+=2; continue; }
        if(c1==0xD0 && c2==0x87) { dest[j++]=(char)0x94; i+=2; continue; }
        if(c1==0xD1 && c2==0x97) { dest[j++]=(char)0x95; i+=2; continue; }
        if(c1==0xD2 && c2==0x90) { dest[j++]=(char)0x96; i+=2; continue; }
        if(c1==0xD2 && c2==0x91) { dest[j++]=(char)0x97; i+=2; continue; }
        if(c1==0xD0 && c2>=0x90 && c2<=0xBF) { dest[j++]=(char)(c2+0x30); i+=2; continue; }
        if(c1==0xD1 && c2>=0x80 && c2<=0x8F) { dest[j++]=(char)(c2+0x70); i+=2; continue; }
        if(c1==0xC3)
        {
            switch(c2)
            {
                case 0xA1: dest[j++]=(char)0x82; i+=2; continue;
                case 0xA9: dest[j++]=(char)0x83; i+=2; continue;
                case 0xAD: dest[j++]=(char)0x84; i+=2; continue;
                case 0xB3: dest[j++]=(char)0x85; i+=2; continue;
                case 0xBA: dest[j++]=(char)0x86; i+=2; continue;
                case 0xBC: dest[j++]=(char)0x87; i+=2; continue;
                case 0xB1: dest[j++]=(char)0x88; i+=2; continue;
                case 0x81: dest[j++]=(char)0x89; i+=2; continue;
                case 0x89: dest[j++]=(char)0x8A; i+=2; continue;
                case 0x8D: dest[j++]=(char)0x8B; i+=2; continue;
                case 0x93: dest[j++]=(char)0x8C; i+=2; continue;
                case 0x9A: dest[j++]=(char)0x8D; i+=2; continue;
                case 0x9C: dest[j++]=(char)0x8E; i+=2; continue;
                case 0x91: dest[j++]=(char)0x8F; i+=2; continue;
            }
        }
        if(c1==0xC2 && c2==0xA1) { dest[j++]=(char)0x80; i+=2; continue; }
        if(c1==0xC2 && c2==0xBF) { dest[j++]=(char)0x81; i+=2; continue; }
        dest[j++]=src[i++];
    }
    dest[j]='\0';
}

void DrawWrappedText(float x, float y, float line_h, int max_chars, const char *text)
{
    char line[1024];
    int line_bytes=0, line_chars=0;
    const unsigned char *p=(const unsigned char*)text;
    if(!text || max_chars<=0) return;
    line[0]='\0';
    while(*p)
    {
        int used, valid;
        uint32_t cp=next_codepoint(p,&used,&valid);
        (void)valid;
        if(cp=='\n' || cp==' ' || cp=='\t' || cp==0)
        {
            if(cp==' ' || cp=='\t')
            {
                if(line_chars < max_chars && line_bytes+used < (int)sizeof(line)-1)
                { memcpy(line+line_bytes,p,used); line_bytes+=used; line[line_bytes]='\0'; line_chars++; }
            }
            else
            {
                if(line_bytes) DrawString(x,y,line);
                y+=line_h; line_bytes=0; line_chars=0; line[0]='\0';
            }
            p+=used;
            continue;
        }
        if(line_chars >= max_chars)
        {
            DrawString(x,y,line); y+=line_h; line_bytes=0; line_chars=0; line[0]='\0';
        }
        if(line_bytes+used < (int)sizeof(line)-1)
        {
            memcpy(line+line_bytes,p,used); line_bytes+=used; line[line_bytes]='\0'; line_chars++;
        }
        p+=used;
    }
    if(line_bytes) DrawString(x,y,line);
}
