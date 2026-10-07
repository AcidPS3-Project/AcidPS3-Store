#include "draw.h"
#include "text.h"
#include "libs.h"
#include "lang.h"
#include "download.h"

#include "default_icon_bin.h"
#include "buttons_bin.h"

int screen_w = 848 / 2;
int screen_h = 512 / 2;
int offset_x = -34;
int offset_y = -22;

int LoadIconPNG(const char *path, pngData *tex, u32 *icon_off)
{
    FILE *fp = fopen(path, "rb");
    if(fp)
	{
		fseek(fp, 0, SEEK_END);
		size_t size = ftell(fp);
		rewind(fp);

		void *buf = memalign(128, size);

		if(!buf)
		{
			fclose(fp);
			return 1;
		}

		if(fread(buf, 1, size, fp) != size)
		{
			free(buf);
			fclose(fp);
			return 1;
		}

		fclose(fp);

		if(pngLoadFromBuffer(buf, size, tex) != 0)
		{
			free(buf);
			return 1;
		}

		u32 *texture = tiny3d_AllocTexture(tex->pitch * tex->height);

		if(!texture)
		{
			free(tex->bmp_out);
			tex->bmp_out = NULL;
			free(buf);
			return 1;
		}
		
		memcpy(texture, tex->bmp_out, tex->pitch * tex->height);
		
		*icon_off = tiny3d_TextureOffset(texture);

		free(tex->bmp_out);
		tex->bmp_out = NULL;
		
		free(buf);
		return 0;
	}
	return 1;
}

void DrawSprite2D(int id, float x, float y, int wid2, int hei2, int wid1, int hei1, int wid0, int hei0, float layer, u32 color)
{
	int cols = wid0 / wid1;
    int col = id % cols;
    int row = id / cols;
    float u0 = (float)(col * wid1) / wid0;
    float v0 = (float)(row * hei1) / hei0;
    float u1 = (float)((col + 1) * wid1) / wid0;
    float v1 = (float)((row + 1) * hei1) / hei0;
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, layer);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u0, v0);
    tiny3d_VertexPos(x+wid2, y, layer);
    tiny3d_VertexTexture(u1, v0);
    tiny3d_VertexPos(x+wid2, y+hei2, layer);
    tiny3d_VertexTexture(u1, v1);
    tiny3d_VertexPos(x, y+hei2, layer);
    tiny3d_VertexTexture(u0, v1);
    tiny3d_End();
}

void DrawIcon(float x, float y, float layer, float dx, float dy, u32 color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, layer);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(0.0f, 0.0f);
    tiny3d_VertexPos(x + dx, y, layer);
    tiny3d_VertexTexture(0.99f, 0.0f);
    tiny3d_VertexPos(x + dx, y + dy, layer);
    tiny3d_VertexTexture(0.99f, 0.99f);
    tiny3d_VertexPos(x, y + dy, layer);
    tiny3d_VertexTexture(0.0f, 0.99f);
    tiny3d_End();
}

void DrawRect4(float x, float y, float w, float h, float z,
               u32 c_tl, u32 c_tr, u32 c_br, u32 c_bl)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x,     y,     z);
    tiny3d_VertexColor(c_tl);
    tiny3d_VertexPos(x + w, y,     z);
    tiny3d_VertexColor(c_tr);
    tiny3d_VertexPos(x + w, y + h, z);
    tiny3d_VertexColor(c_br);
    tiny3d_VertexPos(x,     y + h, z);
    tiny3d_VertexColor(c_bl);

    tiny3d_End();
}

void DrawWave(float baseY, float amp, float freq, float phase, float thickness, float z, u32 topColor, u32 bottomColor)
{
    int x;
    int step = 16;
    tiny3d_SetPolygon(TINY3D_QUADS);

    for(x = -99; x < 1280; x += step)
    {
        float y1 = baseY + sinf((x * freq) + phase) * amp;
        float y2 = baseY + sinf(((x + step) * freq) + phase) * amp;
		
        tiny3d_VertexPos(x, y1, z);
        tiny3d_VertexColor(topColor);
        tiny3d_VertexPos(x + step, y2, z);
        tiny3d_VertexColor(topColor);
        tiny3d_VertexPos(x + step, y2 + thickness, z);
        tiny3d_VertexColor(bottomColor);
        tiny3d_VertexPos(x, y1 + thickness, z);
        tiny3d_VertexColor(bottomColor);
    }

    tiny3d_End();
}

void DrawRect(float x, float y, float dx, float dy, float layer, u32 color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, layer);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + dx, y, layer);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + dx, y + dy, layer);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x, y + dy, layer);
    tiny3d_VertexColor(color);
    tiny3d_End();
}

void DrawRectOutline(float x, float y, float w, float h, float thickness, float z, u32 color)
{
    DrawRect(x, y, w, thickness, z, color);
    DrawRect(x, y + h - thickness, w, thickness, z, color);
    DrawRect(x, y, thickness, h, z, color);
    DrawRect(x + w - thickness, y, thickness, h, z, color);
}

void DrawSprite2DR(int id, float x, float y, int wid2, int hei2, int wid1, int hei1, int wid0, int hei0, float angle, float layer,  u32 color)
{
    int cols = wid0 / wid1;
    int col = id % cols;
    int row = id / cols;
    float u0 = (float)(col * wid1) / wid0;
    float v0 = (float)(row * hei1) / hei0;
    float u1 = (float)((col + 1) * wid1) / wid0;
    float v1 = (float)((row + 1) * hei1) / hei0;
    float cx = x + wid2 * 0.5f;
    float cy = y + hei2 * 0.5f;
    float hw = wid2 * 0.5f;
    float hh = hei2 * 0.5f;
    float rad = angle * (M_PI / 180.0f);
    float c = cosf(rad);
    float s = sinf(rad);
    float x0 = (-hw * c) - (-hh * s);
    float y0 = (-hw * s) + (-hh * c);
    float x1 = ( hw * c) - (-hh * s);
    float y1 = ( hw * s) + (-hh * c);
    float x2 = ( hw * c) - ( hh * s);
    float y2 = ( hw * s) + ( hh * c);
    float x3 = (-hw * c) - ( hh * s);
    float y3 = (-hw * s) + ( hh * c);
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(cx + x0, cy + y0, layer);
    tiny3d_VertexColor(color);
    tiny3d_VertexTexture(u0, v0);
    tiny3d_VertexPos(cx + x1, cy + y1, layer);
    tiny3d_VertexTexture(u1, v0);
    tiny3d_VertexPos(cx + x2, cy + y2, layer);
    tiny3d_VertexTexture(u1, v1);
    tiny3d_VertexPos(cx + x3, cy + y3, layer);
    tiny3d_VertexTexture(u0, v1);
    tiny3d_End();
}

void SetTexture(pngData icon, u32 icon_off)
{
	tiny3d_SetTexture(0, icon_off, icon.width, icon.height, icon.pitch, TINY3D_TEX_FORMAT_A8R8G8B8, TEXTURE_NEAREST);
}

void Load_PNG()
{
	pngLoadFromBuffer(default_icon_bin, default_icon_bin_size, &DefaultIcon);
	pngLoadFromBuffer(buttons_bin, buttons_bin_size, &Buttons);
}

void LoadTexture()
{
    u32 * texture_mem = tiny3d_AllocTexture(64*1024*1024);
    u32 * texture_pointer;
    if(!texture_mem) return;
    texture_pointer = texture_mem;
    Load_PNG();
	ResetFont();
    LoadFontIDK();
    texture_pointer = (u32 *) AddFontFromTTF((u8 *) texture_pointer, 32, 255, 64, 64, TTF_to_Bitmap);
    TTFUnloadFont();
    
	memcpy(texture_pointer, DefaultIcon.bmp_out, DefaultIcon.pitch * DefaultIcon.height);
	DefaultOff = tiny3d_TextureOffset(texture_pointer);
	free(DefaultIcon.bmp_out);
	DefaultIcon.bmp_out = NULL;
	texture_pointer += ((DefaultIcon.pitch * DefaultIcon.height + 15) & ~15) / 4;
    
	memcpy(texture_pointer, Buttons.bmp_out, Buttons.pitch * Buttons.height);
	ButtonsOff = tiny3d_TextureOffset(texture_pointer);
	free(Buttons.bmp_out);
	Buttons.bmp_out = NULL;
	texture_pointer += ((Buttons.pitch * Buttons.height + 15) & ~15) / 4;
}

void DrawButton(int x, int y, int button, int z)
{
	u32 color = 0xffffffff;
	SetTexture(Buttons, ButtonsOff);
	DrawSprite2D(button, x, y+4, 32, 32, 20, 20, 120, 40, z, color);
}

void drawScene()
{
	u32 color = 0xffffffff;
	int i = 0;
	int off_x = -20+40;
	int base_x = 454;
	int off_y = 60+22;
	int off_y2 = 400;
    tiny3d_Project2D();
    
	static float wave_t = 0.0f;
	SetFontSize(28, 32);
    SetFontColor(0xffffffff, 0x00000000);
    SetCurrentFont(0);
	
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
			DrawFString(off_x, 384+off_y, "%llu / %llu %s (%.1f%%)", (unsigned long long)g_downloaded_bytes,
				(unsigned long long)g_download_total, langStr[8], percent);
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

					if(gIcons[app].loaded) SetTexture(gIcons[app].icon, gIcons[app].icon_off);
					else SetTexture(DefaultIcon, DefaultOff);
					
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
				
				if(gIcons[selected_app].loaded) SetTexture(gIcons[selected_app].icon, gIcons[selected_app].icon_off);
					else SetTexture(DefaultIcon, DefaultOff);
				
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
					
					if(gIcons[app].loaded) SetTexture(gIcons[app].icon, gIcons[app].icon_off);
						else SetTexture(DefaultIcon, DefaultOff);
					
					DrawIcon(off_x, app_y_pos, 1, 88, 88, 0xffffffff);
					SetFontSize(28, 32);
					DrawFString(off_x+92, app_y_pos, "%s %s", gApps[app].name, gApps[app].version);
					SetFontSize(16, 16);
					DrawFString(off_x+92, 36+app_y_pos, "%s: %s", langStr[17], gApps[app].author);
					
					if(i==menu_index2) DrawRectOutline(off_x-5, app_y_pos-5, 1000, 95, 3, 0, 0x00000055);
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
			
			if(gIcons[selected_app].loaded) SetTexture(gIcons[selected_app].icon, gIcons[selected_app].icon_off);
				else SetTexture(DefaultIcon, DefaultOff);
			
			DrawIcon(off_x, off_y, 1, 128, 128, color);
			SetFontSize(28, 32);
			DrawFString(off_x+148, off_y, "%s %s", gApps[selected_app].name, gApps[selected_app].version);
			SetFontSize(16, 16);
			DrawFString(off_x+148, 36+off_y, "%s: %s", langStr[17], gApps[selected_app].author);
			DrawWrappedText(off_x+148, 56+off_y, 16, 100, gApps[selected_app].description);
			
			if(ui_type == 1) DrawRect(-66, off_y2+off_y-45, 1280, 720, 0, colorsa[m_color]);
			
			SetFontSize(28, 32);
			DrawFString(off_x+36, off_y+424, langStr[12]);
			DrawButton(off_x, off_y+424, PAD_CIRCLE, -2);
			if(file_downloaded)
			{
				DrawFString(off_x+36, off_y+384, langStr[18]);
				DrawButton(off_x, off_y+384, PAD_CROSS, -2);
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
			DrawFString(off_x, 384+off_y, "%llu / %llu %s (%.1f%%)", (unsigned long long)g_downloaded_bytes,
				(unsigned long long)g_download_total, langStr[8], percent);
		}
	}
	
	if(ui_type == 1) DrawRect(-66, -66, 1280, 135, 0, colorsa[m_color]);
	
	SetFontSize(28, 32);
	DrawFString(off_x,-4+22, "%s [%s]", global_name, global_version);
}
