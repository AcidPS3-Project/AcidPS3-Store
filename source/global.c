#include "global.h"
#include "libs.h"

#include "spu_soundmodule.bin.h"

#include "soundlib/spu_soundlib.h"
#include "soundlib/audioplayer.h"

char global_name[] = "AcidPS3 Store";
char global_ver[] = "v0.3.11";
char global_version[16];

int show_screen_idk = 0;
int m_color = 0;

int need_update = 0;

int cols = 5;

int menu_setting = 0;

bool version_ignore = false;
bool in_settings = false;

unsigned int color_shade(unsigned int color, float brightness, float alpha)
{
    int r = (color >> 24) & 0xFF;
    int g = (color >> 16) & 0xFF;
    int b = (color >> 8)  & 0xFF;
    int a = color & 0xFF;

    r = (int)(r * brightness);
    g = (int)(g * brightness);
    b = (int)(b * brightness);
    a = (int)(a * alpha);

    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(a > 255) a = 255;

    return (r << 24) | (g << 16) | (b << 8) | a;
}

void setup_color(int index, unsigned int base)
{
    colorsa[index] = color_shade(base, 1.0f, 1.0f);
    colorsb[index] = color_shade(base, 1.0f, 0.87f); // ff -> dd

    colorsc[index] = color_shade(base, 0.53f, 1.0f);
    colorsd[index] = color_shade(base, 0.53f, 0.87f); // ff -> dd

    colorse[index] = color_shade(base, 0.63f, 1.0f);
    colorsf[index] = color_shade(base, 1.6f, 1.0f);
}

void setup_colors()
{
	setup_color(0, 0x333399FF);
	setup_color(1, 0xCBCBCBFF);
	setup_color(2, 0xD8BF1AFF);
	setup_color(3, 0x6DB217FF);
	setup_color(4, 0xE17E9AFF);
	setup_color(5, 0x178816FF);
	setup_color(6, 0x9A61C8FF);
	setup_color(7, 0x02CDC7FF);
	setup_color(8, 0x0C76C0FF);
	setup_color(9, 0xB444C0FF);
	setup_color(10, 0xE5A708FF);
	setup_color(11, 0x875B1EFF);
	setup_color(12, 0xE3412AFF);
}

float lerp(float min, float max, float ratio)
{
	return min + (max - min) * ratio;
}

int menu_selected = 0;
int menu_type = 0;
int menu_index = 0;
float menu_index_i = 0;

void EmptyDialogCallback(msgButton button, void *usrData)
{
    // ehhehe fuck this shit
}

void DemoCompleteCallback(msgButton button, void *usrData)
{
    sysProcessExit(0);
}

void Initialize()
{
	u32 spu;
	sysSpuImage spu_image;
	sysSpuInitialize(6, 5);
	sysSpuRawCreate(&spu, NULL);
	sysSpuImageImport(&spu_image, spu_soundmodule_bin, 0);
	sysSpuRawImageLoad(spu, &spu_image);
	SND_Init(spu);
}

int visibleApps[MAX_APPS];
int visibleCount = 0;
int scrollRow = 0;

int RandRange(int min, int max)
{
	int rndom;
	rndom = rand() % (max-min+1) + min;
	return rndom;
}

int max(int min, int max)
{
	int biggest;
	biggest = (min > max) ? min : max;
	return biggest;
}