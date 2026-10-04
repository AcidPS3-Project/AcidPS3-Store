#ifndef GLOBAL_H
#define GLOBAL_H

#include "libs.h"

#define MAX_APPS 1000

extern u32 colorsa[16];
extern u32 colorsb[16];
extern u32 colorsc[16];
extern u32 colorsd[16];
extern u32 colorse[16];
extern u32 colorsf[16];

extern char global_name[];
extern char global_ver[];
extern char global_version[16];

extern int need_update;
extern int show_screen_idk;
extern int m_color;
extern int menu_selected;
extern int menu_type;
extern int menu_index;
extern float menu_index_i;

extern int visibleApps[MAX_APPS];
extern int visibleCount;
extern int scrollRow;

extern bool version_ignore;
extern bool in_settings;
extern int menu_setting;

void setup_colors();
float lerp(float min, float max, float ratio);

void EmptyDialogCallback(msgButton button, void *usrData);
void DemoCompleteCallback(msgButton button, void *usrData);

void Initialize();
int RandRange(int min, int max);
int max(int min, int max);

#endif