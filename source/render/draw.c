#include "draw.h"
#include "text.h"
#include "libs.h"
#include "lang.h"
#include "download.h"
#include "rsx_renderer.h"

#include "default_icon_bin.h"
#include "buttons_bin.h"

#define RSX_DEFAULT_TEXTURE 0
#define RSX_BUTTONS_TEXTURE 1
#define RSX_FIRST_ICON_TEXTURE 3
#define RSX_ICON_TEXTURE_COUNT (RSX_MAX_TEXTURES - RSX_FIRST_ICON_TEXTURE - 3) /* reserve 61 JP, 62 base font, 63 white */

int screen_w = 848 / 2;
int screen_h = 512 / 2;
int offset_x = -34;
int offset_y = -22;

static int active_texture = RSX_DEFAULT_TEXTURE;
static int icon_owner[RSX_ICON_TEXTURE_COUNT];
static unsigned int icon_last_used[RSX_ICON_TEXTURE_COUNT];
static unsigned int icon_last_frame[RSX_ICON_TEXTURE_COUNT];
static unsigned int icon_use_clock = 1;
static unsigned int icon_frame_clock = 1;

static int UploadPNGMemory(int texture_id, const void *png_buffer, size_t png_size)
{
    pngData decoded;
    u32 *pixels;
    u32 row_bytes;
    u32 y;
    size_t pixel_bytes;
    int result;

    memset(&decoded, 0, sizeof(decoded));
    if (!png_buffer || png_size == 0) return 0;
    if (pngLoadFromBuffer((void *)png_buffer, (u32)png_size, &decoded) != 0) return 0;
    if (!decoded.bmp_out || !decoded.width || !decoded.height || decoded.width > 65535 || decoded.height > 65535)
    {
        if (decoded.bmp_out) free(decoded.bmp_out);
        return 0;
    }

    row_bytes = decoded.width * sizeof(u32);
    if (decoded.pitch < row_bytes || (size_t)row_bytes > ((size_t)-1) / decoded.height)
    {
        free(decoded.bmp_out);
        return 0;
    }

    pixel_bytes = (size_t)row_bytes * decoded.height;
    pixels = (u32 *)malloc(pixel_bytes);
    if (!pixels)
    {
        free(decoded.bmp_out);
        return 0;
    }

    for (y = 0; y < decoded.height; y++)
		memcpy((u8 *)pixels + y * row_bytes, (u8 *)decoded.bmp_out + y * decoded.pitch, row_bytes);

    result = RSX_LoadTexture(texture_id, pixels, (u16)decoded.width, (u16)decoded.height);
    free(pixels);
    free(decoded.bmp_out);
    return result;
}

static int UploadPNGFile(int texture_id, const char *path)
{
    FILE *fp;
    long file_size;
    void *buffer;
    size_t bytes_read;
    int result;

    fp = fopen(path, "rb");
    if (!fp) return 0;

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return 0;
    }
    file_size = ftell(fp);
    if (file_size <= 0 || fseek(fp, 0, SEEK_SET) != 0)
    {
        fclose(fp);
        return 0;
    }

    buffer = memalign(128, (size_t)file_size);
    if (!buffer)
    {
        fclose(fp);
        return 0;
    }

    bytes_read = fread(buffer, 1, (size_t)file_size, fp);
    fclose(fp);
    if (bytes_read != (size_t)file_size)
    {
        free(buffer);
        return 0;
    }

    result = UploadPNGMemory(texture_id, buffer, (size_t)file_size);
    free(buffer);
    return result;
}

static void SetDefaultTexture(void)
{
    active_texture = RSX_DEFAULT_TEXTURE;
}

static void SetButtonsTexture(void)
{
    active_texture = RSX_BUTTONS_TEXTURE;
}

static void SetAppTexture(int app_index)
{
    int i;
    int slot = -1;
    int victim = 0;
    unsigned int oldest = 0xFFFFFFFFU;
    char path[256];
    int texture_id;

    if (app_index < 0 || app_index >= gAppCount || !gIcons[app_index].loaded)
    {
        SetDefaultTexture();
        return;
    }

    for (i = 0; i < RSX_ICON_TEXTURE_COUNT; i++)
    {
        if (icon_owner[i] == app_index)
        {
            icon_last_used[i] = ++icon_use_clock;
            icon_last_frame[i] = icon_frame_clock;
            active_texture = RSX_FIRST_ICON_TEXTURE + i;
            return;
        }

        if (icon_owner[i] < 0)
        {
            if (slot < 0) slot = i;
            continue;
        }

        if (icon_last_frame[i] != icon_frame_clock && icon_last_used[i] < oldest)
        {
            oldest = icon_last_used[i];
            victim = i;
        }
    }

    if (slot < 0)
    {
        if (oldest == 0xFFFFFFFFU)
        {
            SetDefaultTexture();
            return;
        }
        slot = victim;
    }
    texture_id = RSX_FIRST_ICON_TEXTURE + slot;
    snprintf(path, sizeof(path), "/dev_hdd0/game/ACIDSTORE/USRDIR/cache/icons/%s.png", gApps[app_index].id);

    if (!UploadPNGFile(texture_id, path))
    {
        RSX_UnloadTexture(texture_id);
        icon_owner[slot] = -1;
        icon_last_used[slot] = 0;
        icon_last_frame[slot] = 0;
        gIcons[app_index].loaded = 0;
        SetDefaultTexture();
        return;
    }

    icon_owner[slot] = app_index;
    icon_last_used[slot] = ++icon_use_clock;
    icon_last_frame[slot] = icon_frame_clock;
    active_texture = texture_id;
}

void DrawSprite2D(int id, float x, float y, int wid2, int hei2, int wid1, int hei1, int wid0, int hei0, float layer, u32 color)
{
    int sprite_cols;
    int col;
    int row;
    float u0, v0, u1, v1;

    if (wid1 <= 0 || hei1 <= 0 || wid0 <= 0 || hei0 <= 0 || wid2 <= 0 || hei2 <= 0) return;
    sprite_cols = wid0 / wid1;
    if (sprite_cols <= 0) return;
    if (id < 0 || id >= sprite_cols * (hei0 / hei1)) return;
    col = id % sprite_cols;
    row = id / sprite_cols;
    u0 = (float)(col * wid1) / (float)wid0;
    v0 = (float)(row * hei1) / (float)hei0;
    u1 = (float)((col + 1) * wid1) / (float)wid0;
    v1 = (float)((row + 1) * hei1) / (float)hei0;

    RSX_SetLayer(layer);
    RSX_DrawSprite2D(active_texture, x, y, (float)wid2, (float)hei2, u0, v0, u1 - u0, v1 - v0, color);
}

void DrawIcon(float x, float y, float layer, float dx, float dy, u32 color)
{
    RSX_SetLayer(layer);
    RSX_DrawSprite2D(active_texture, x, y, dx, dy, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

void DrawRect4(float x, float y, float w, float h, float z, u32 c_tl, u32 c_tr, u32 c_br, u32 c_bl)
{
    RSX_SetLayer(z);
    RSX_DrawGradientRectangle2D(x, y, x + w, y + h, c_tl, c_tr, c_br, c_bl);
}

void DrawWave(float baseY, float amp, float freq, float phase, float thickness, float z, u32 topColor, u32 bottomColor)
{
    int x;
    const int step = 16;

    RSX_SetLayer(z);
    for (x = -99; x < 1280; x += step)
    {
        float y1 = baseY + sinf((x * freq) + phase) * amp;
        float y2 = baseY + sinf(((x + step) * freq) + phase) * amp;
        RSX_DrawGradientQuad2D((float)x, y1, (float)(x + step), y2,
            (float)(x + step), y2 + thickness, (float)x, y1 + thickness,
            topColor, topColor, bottomColor, bottomColor);
    }
}

void DrawRect(float x, float y, float dx, float dy, float layer, u32 color)
{
    RSX_SetLayer(layer);
    RSX_DrawRectangle2D(x, y, dx, dy, color);
}

void DrawRectOutline(float x, float y, float w, float h, float thickness, float z, u32 color)
{
    DrawRect(x, y, w, thickness, z, color);
    DrawRect(x, y + h - thickness, w, thickness, z, color);
    DrawRect(x, y, thickness, h, z, color);
    DrawRect(x + w - thickness, y, thickness, h, z, color);
}

void DrawSprite2DR(int id, float x, float y, int wid2, int hei2, int wid1, int hei1, int wid0, int hei0, float angle, float layer, u32 color)
{
    RSX_SetLayer(layer);
    RSX_DrawSprite2DR(active_texture, id, x, y, wid2, hei2, wid1, hei1, wid0, hei0, angle, color);
}

void LoadTexture(void)
{
    int i;
    for (i = 0; i < RSX_ICON_TEXTURE_COUNT; i++)
    {
        icon_owner[i] = -1;
        icon_last_used[i] = 0;
        icon_last_frame[i] = 0;
    }
    icon_use_clock = 1;
    icon_frame_clock = 1;

    if (!UploadPNGMemory(RSX_DEFAULT_TEXTURE, default_icon_bin, default_icon_bin_size)) printf("RSX: failed to load default icon texture\n");
    if (!UploadPNGMemory(RSX_BUTTONS_TEXTURE, buttons_bin, buttons_bin_size)) printf("RSX: failed to load button texture\n");

    RSX_SetTextureFilter(RSX_DEFAULT_TEXTURE, 0);
    RSX_SetTextureFilter(RSX_BUTTONS_TEXTURE, 0);

    if (!Text_Init()) printf("RSX: failed to initialise font atlas\n");
}

void DrawButton(int x, int y, int button, int z)
{
    SetButtonsTexture();
    DrawSprite2D(button, x, y + 4, 32, 32, 20, 20, 120, 40, (float)z, 0xFFFFFFFF);
}

void drawScene()
{
	icon_frame_clock++;
	u32 color = 0xffffffff;
	int i = 0;
	int off_x = -20+40;
	int base_x = 454;
	int off_y = 60+22;
	int off_y2 = 400;
	static float wave_t = 0.0f;
	SetFontSize(28, 32);
    SetFontColor(0xffffffff, 0x00000000);
	
	wave_t += 0.015f;
	
	menu_index_i = lerp(menu_index_i, menu_index2, 0.1f);
	
	DrawRect4(-99, -99, 1280, 720, 6, colorsa[m_color], colorsa[m_color], colorsc[m_color], colorsc[m_color]);
	
	DrawWave(240.0f, 32.0f, 0.015f, wave_t * 0.7f, 70.0f, 5, colorse[m_color], 0x00000000);
	DrawWave(299.0f, 48.0f, 0.012f, wave_t, 80.0f, 4, colorsf[m_color], 0x00000000);
	
	if(!show_screen_idk)
	{
		DrawFString(off_x, 64, langStr[0]);
		DrawFString(off_x, 96, langStr[1]);
		if(ui_type == 1) DrawRect(-66, off_y2+off_y, 1280, 720, 0, colorsa[m_color]);
	}
	else if(need_update)
	{
		if(ui_type == 1) DrawRect(-66, off_y2+off_y, 1280, 720, 0, colorsa[m_color]);
		DrawFString(off_x, 96, langStr[2]);
		DrawFString(off_x, 160, "%s: %s", langStr[3], manifestVersion);
		DrawFString(off_x, 192, langStr[4]);
		DrawFString(off_x, 256, langStr[5]);
		DrawFString(off_x, 288, "/dev_hdd0/packages/");
		DrawFString(off_x, 352, langStr[6]);
		
		if(!file_downloaded)
		{
			SetFontSize(28, 32);
			float percent = 0.0f;
			if(g_download_total > 0) percent = (float)g_downloaded_bytes * 100.0f / (float)g_download_total;
			
			const char *display_name = strrchr(g_download_name, '/');
			if(display_name) display_name++;
			else display_name = g_download_name;
			
			DrawFString(off_x, 344+off_y, langStr[7], display_name);
			DrawFString(off_x, 384+off_y, "%llu / %llu %s (%.1f%%)", (unsigned long long)g_downloaded_bytes, (unsigned long long)g_download_total, langStr[8], percent);
            if(g_download_active && (download_kind == DOWNLOAD_GAME || download_kind == DOWNLOAD_UPDATE))
            {
                DrawButton(off_x, 424+off_y, PAD_CIRCLE, -2);
                DrawFString(off_x+36, 424+off_y, langStr[27]);
            }
		}
		else
		{
			DrawFString(off_x+36, 344+off_y, langStr[9]);
			DrawButton(off_x, 344+off_y, PAD_CROSS, -2);
		}
	}
	else
	{
		if(in_settings)
		{
			DrawFString(off_x, off_y, langStr[10]);
			switch(language)
			{
				case 0: DrawFString(off_x, off_y+64, "Language: English"); break;
				case 1: DrawFString(off_x, off_y+64, "Язык: Русский"); break;
				case 2: DrawFString(off_x, off_y+64, "Тілі: Қазақ"); break;
				case 3: DrawFString(off_x, off_y+64, "Idioma: Español"); break;
				case 4: DrawFString(off_x, off_y+64, "Мова: Українська"); break;
				case 5: DrawFString(off_x, off_y+64, "言語：日本語"); break;
			}
			DrawFString(off_x, off_y+96, "%s: %d", langStr[11], m_color);
			DrawFString(off_x, off_y+128, "%s: %dX%d", langStr[25], cols, cols);
			DrawFString(off_x, off_y+160, "%s: %d", langStr[26], ui_type);
			DrawFString(off_x-20, off_y+64+(menu_setting*32), ">");
			
			if(ui_type == 1) DrawRect(-66, off_y2+off_y, 1280, 720, 0, colorsa[m_color]);
			
			DrawFString(off_x+36, off_y+424, langStr[12]);
			DrawButton(off_x, off_y+424, PAD_CIRCLE, -2);
		}
		else if(!menu_selected)
		{
			if(ui_type==0)
			{
				int iconW = 400 / cols;
				int iconH = 400 / cols;
				int rowsVisible = cols;
				
				DrawRect4(off_x-5, off_y-5, 392, 392, 3, colorsb[m_color], colorsb[m_color], colorsd[m_color], colorsd[m_color]);

				int selectedRow = menu_index / cols;
				if(selectedRow < scrollRow) scrollRow = selectedRow;
				if(selectedRow >= scrollRow + rowsVisible) scrollRow = selectedRow - rowsVisible + 1;

				for(i = 0; i < visibleCount; i++)
				{
					int app = visibleApps[i];
					int row = i / cols;
					int col = i % cols;
					int app_x_pos = base_x + col * iconW;
					int app_y_pos = off_y + (row - scrollRow) * iconH;
					if(row < scrollRow || row >= scrollRow + rowsVisible) continue;

					if(gIcons[app].loaded) SetAppTexture(app);
					else SetDefaultTexture();
					
					u32 viscolor = 0x888888ff;
					float sizey = 0.89f;
					if(i == menu_index)
					{
						viscolor = 0xffffffff;
						sizey = 1.09f;
					}
					
					float actualW = iconW * sizey;
					float actualH = iconH * sizey;

					int centered_x = app_x_pos + (iconW - actualW) / 2;
					int centered_y = app_y_pos + (iconH - actualH) / 2;
					
					DrawIcon(centered_x, centered_y, 1, actualW, actualH, viscolor);
				}
				SetFontSize(20, 22);
				int selected_app = visibleApps[menu_index];
				
				if(gIcons[selected_app].loaded) SetAppTexture(selected_app); else SetDefaultTexture();
				
				DrawIcon(off_x, off_y, 1, 156, 156, color);
				DrawWrappedText(off_x+160, off_y, 20, 24, gApps[selected_app].name);
				DrawWrappedText(off_x, off_y+162, 18, 44, gApps[selected_app].short_desc);
				SetFontSize(16, 16);
				DrawFString(off_x, off_y+360, "%s: %s", langStr[17], gApps[selected_app].author);
			}
			else if(ui_type==1)
			{
				for(i = 0; i < visibleCount; i++)
				{
					int space_y = 100;
					int app = visibleApps[i];
					int app_y_pos = off_y+(i*space_y)+((-menu_index_i)*space_y);
					
					if(app_y_pos > -100 && app_y_pos < 582)
					{
						if(gIcons[app].loaded) SetAppTexture(app);
						else SetDefaultTexture();
						DrawIcon(off_x, app_y_pos, 1, 88, 88, 0xffffffff);
						SetFontSize(28, 32);
						DrawFString(off_x+92, app_y_pos, "%s %s", gApps[app].name, gApps[app].version);
						SetFontSize(16, 16);
						DrawFString(off_x+92, 36+app_y_pos, "%s: %s", langStr[17], gApps[app].author);
						if(i==menu_index2) DrawRectOutline(off_x-5, app_y_pos-5, 1000, 95, 3, 0, 0x00000055);
					}
				}
				DrawRect(-66, off_y2+off_y, 1280, 720, 0, colorsa[m_color]);
			}
			
			SetFontSize(28, 32);
			DrawFString(off_x+36, off_y+384+24, langStr[13]);
			DrawButton(off_x, off_y+384+24, PAD_CROSS, -2);
			
			if(menu_type==0) DrawFString(off_x+36, off_y+424+24, langStr[15]);
			else DrawFString(off_x+36, off_y+424+24, langStr[16]);
			DrawButton(off_x, off_y+424+24, PAD_TRIANGLE, -2);
			
			DrawFString(off_x+400+36, off_y+384+24, langStr[14]);
			DrawButton(off_x+400, off_y+384+24, PAD_SELECT, -2);
			DrawFString(off_x+400+36, off_y+424+24, langStr[10]);
			DrawButton(off_x+400, off_y+424+24, PAD_START, -2);
		}
		else
		{
			int selected_app = visibleApps[menu_index];
			
			if(gIcons[selected_app].loaded) SetAppTexture(selected_app); else SetDefaultTexture();
			
			DrawIcon(off_x, off_y, 1, 128, 128, color);
			SetFontSize(28, 32);
			DrawFString(off_x+148, off_y, "%s %s", gApps[selected_app].name, gApps[selected_app].version);
			SetFontSize(16, 16);
			DrawFString(off_x+148, 36+off_y, "%s: %s", langStr[17], gApps[selected_app].author);
			DrawWrappedText(off_x+148, 56+off_y, 16, 100, gApps[selected_app].description);
			
			if(ui_type == 1) DrawRect(-66, off_y2+off_y-45, 1280, 720, 0, colorsa[m_color]);
			
			SetFontSize(28, 32);
			if(file_downloaded)
			{
				DrawFString(off_x+36, off_y+384, langStr[18]);
				DrawButton(off_x, off_y+384, PAD_CROSS, -2);
				DrawFString(off_x+36, off_y+424, langStr[12]);
				DrawButton(off_x, off_y+424, PAD_CIRCLE, -2);
			}
		}
		
		SetFontSize(28, 32);
		if(!file_downloaded)
		{
			SetFontSize(28, 32);
			float percent = 0.0f;
			if(g_download_total > 0) percent = (float)g_downloaded_bytes * 100.0f / (float)g_download_total;
			
			const char *display_name = strrchr(g_download_name, '/');
			if(display_name) display_name++;
			else display_name = g_download_name;
			
			DrawFString(off_x, 344+off_y, "%s: %s", langStr[19], display_name);
			DrawFString(off_x, 384+off_y, "%llu / %llu %s (%.1f%%)", (unsigned long long)g_downloaded_bytes, (unsigned long long)g_download_total, langStr[8], percent);
            if(g_download_active && (download_kind == DOWNLOAD_GAME || download_kind == DOWNLOAD_UPDATE))
            {
                DrawButton(off_x, 424+off_y, PAD_CIRCLE, -2);
                DrawFString(off_x+36, 424+off_y, langStr[27]);
            }
		}
	}
	
	if(ui_type == 1) DrawRect(-66, -66, 1280, 135, 0, colorsa[m_color]);
	
	SetFontSize(28, 32);
	DrawFString(off_x,-4+22, "%s [%s]", global_name, global_version);
}
