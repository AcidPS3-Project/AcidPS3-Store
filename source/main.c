#include "libs.h"
#include "global.h"
#include "download.h"
#include "sound.h"
#include "lang.h"
#include "draw.h"

static int fps = 0;
static int frames = 0;
static u64 last_time = 0;

int max_color = 12;
int max_lang = 3;

void UpdateNOW()
{
	char dpath[256];
	snprintf(dpath, sizeof(dpath), "github.com/AcidPS3-Project/AcidPS3-Store/releases/latest/download/AcidPS3-Store.pkg");
	Download_Start(data_url, dpath, "/dev_hdd0/packages/AcidPS3-Store-LATEST.pkg");
	download_kind = DOWNLOAD_UPDATE;
}

void UpdateFPS()
{
    u64 now = sysGetSystemTime();
    frames++;
    if(now - last_time >= 1000000ULL)
    {
        fps = frames;
        frames = 0;
        last_time = now;
    }
}

void RefreshVisibleApps()
{
    int i;
    visibleCount = 0;

    for(i = 0; i < gAppCount; i++)
    {
        if(menu_type == 0 && strcmp(gApps[i].hidden_game, "yes") == 0) continue;
        visibleApps[visibleCount++] = i;
    }

    if(menu_index >= visibleCount) menu_index = visibleCount - 1;
    if(menu_index2 > visibleCount-1) menu_index2 = visibleCount - 1;

    if(menu_index < 0) menu_index = 0;
    if(menu_index2 < 0) menu_index2 = 0;
}

FILE *settings_file = NULL;

void save_settings()
{
	settings_file = fopen("/dev_hdd0/game/ACIDSTORE/USRDIR/settings.acid", "w");
	if(settings_file == NULL)
	{
		printf("Error opening file!\n");
		return;
	}
	
	fprintf(settings_file, "Color: %d\n", m_color);
	fprintf(settings_file, "Language: %d\n", language);
    fprintf(settings_file, "Column Size: %d\n", cols);
    fprintf(settings_file, "UI type: %d\n", ui_type);
	fclose(settings_file);
}

bool old_control_left = false;
bool old_control_right = false;
bool old_control_up = false;
bool old_control_down = false;

int main(int argc, const char* argv[])
{
	padInfo padinfo;
	int i;
	static padData oldpad;
	padData paddata;
	srand(time(NULL));
	tiny3d_Init(2*1024*1024);
	tiny3d_UserViewport(1, 0, 0, (float)(Video_Resolution.width / 916.0f), (float)(Video_Resolution.height / 582.0f),
		(float)(Video_Resolution.width / 1920.0f), (float)(Video_Resolution.height / 1080.0f));

	mkdir("/dev_hdd0/game/ACIDSTORE/USRDIR/cache", 0777);
	mkdir("/dev_hdd0/game/ACIDSTORE/USRDIR/cache/icons", 0777);
	mkdir("/dev_hdd0/packages", 0777);
	
	snprintf(global_version, sizeof(global_version), global_ver);
	
	s32 ret = netInitialize();
	printf("netInitialize: 0x%08X\n", ret);
	
	http_init();
	
	sysModuleLoad(SYSMODULE_PNGDEC);
	Initialize();
	init_sfx();
	
	ioPadInit(7);
    LoadTexture();
	
	setup_colors();
	
	settings_file = fopen("/dev_hdd0/game/ACIDSTORE/USRDIR/settings.acid", "r");
	if(settings_file==NULL) save_settings();
	else
	{
		fscanf(settings_file, "Color: %d\n", &m_color);
    	fscanf(settings_file, "Language: %d\n", &language);
    	fscanf(settings_file, "Column Size: %d\n", &cols);
    	fscanf(settings_file, "UI type: %d\n", &ui_type);
		fclose(settings_file);
	}
	
	if(cols<4) cols=4;
	if(cols>6) cols=6;
	if(language<0) language=0;
	if(language>max_lang) language=max_lang;
	if(m_color<0) m_color=0;
	if(m_color>max_color) m_color=max_color;
	
	LangInit();
	
    PlayBGM();
	printf("received call to get manifest..\n");
	
	Download_Start(assets_url, "manifest.json", manifest_path);
	download_kind = DOWNLOAD_MANIFEST;
	
    while(1)
    {
		sysUtilCheckCallback();
		UpdateFPS();
		
        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
		tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
		tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ZERO,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
		ioPadGetInfo(&padinfo);
		
		for(i = 0; i < MAX_PADS; i++)
		{
			if(padinfo.status[i])
			{
				ioPadGetData(i, &paddata);
				
				bool control_left = paddata.BTN_LEFT || paddata.ANA_L_H < 80;
				bool control_right = paddata.BTN_RIGHT || paddata.ANA_L_H > 176;

				bool control_up = paddata.BTN_UP || paddata.ANA_L_V < 80;
				bool control_down = paddata.BTN_DOWN || paddata.ANA_L_V > 176;
				
				if(need_update)
				{
					if(paddata.BTN_CROSS && !oldpad.BTN_CROSS && file_downloaded)
					{
						file_downloaded = 0;
						UpdateNOW();
					}
					oldpad = paddata;
					continue;
				}
				
				// basic
				if(in_settings)
				{
					if(paddata.BTN_CIRCLE && !oldpad.BTN_CIRCLE)
					{
						PlaySFX(&sfx_cancel);
						in_settings = false;
						save_settings();
					}
					if(control_up && !old_control_up && menu_setting>0)
					{
						PlaySFX(&sfx_move);
						menu_setting--;
					}
					if(control_down && !old_control_down && menu_setting<3)
					{
						PlaySFX(&sfx_move);
						menu_setting++;
					}
					if(control_left && !old_control_left)
					{
						PlaySFX(&sfx_move);
						switch(menu_setting)
						{
							case 0:	if(language>0) language--; LangInit(); break;
							case 1:	if(m_color>0) m_color--; break;
							case 2:	if(cols>4) cols--; break;
							case 3:	if(ui_type>0) ui_type--; break;
							default: break;
						}
					}
					if(control_right && !old_control_right)
					{
						PlaySFX(&sfx_move);
						switch(menu_setting)
						{
							case 0:	if(language<max_lang) language++; LangInit(); break;
							case 1:	if(m_color<max_color) m_color++; break;
							case 2:	if(cols<6) cols++; break;
							case 3:	if(ui_type<1) ui_type++; break;
							default: break;
						}
					}
				}
				else if(!menu_selected)
				{
					if(paddata.BTN_CROSS && !oldpad.BTN_CROSS)
					{
						PlaySFX(&sfx_decide);
						menu_selected = 1;
					}
					
					if(paddata.BTN_TRIANGLE && !oldpad.BTN_TRIANGLE)
					{
						PlaySFX(&sfx_decide);
						menu_type = !menu_type;
						RefreshVisibleApps();
					}
					
					if(paddata.BTN_SELECT && !oldpad.BTN_SELECT)
					{
						PlaySFX(&sfx_decide);
						msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK | MSG_DIALOG_DISABLE_CANCEL_ON,
							langStr[20], EmptyDialogCallback, NULL, NULL);
					}
					
					if(paddata.BTN_START && !oldpad.BTN_START)
					{
						PlaySFX(&sfx_decide);
						menu_setting = 0;
						in_settings = true;
					}
					
					if(ui_type==0)
					{
						menu_index2 = menu_index;
						if(control_left && !old_control_left)
						{
							if(menu_index % cols != 0)
							{
								PlaySFX(&sfx_move);
								menu_index--;
							}
						}

						if(control_right && !old_control_right)
						{
							if((menu_index % cols) != (cols - 1) &&
							   (menu_index + 1) < visibleCount)
							{
								PlaySFX(&sfx_move);
								menu_index++;
							}
						}

						if(control_up && !old_control_up)
						{
							if(menu_index >= cols)
							{
								PlaySFX(&sfx_move);
								menu_index -= cols;
							}
						}

						if(control_down && !old_control_down)
						{
							if(menu_index + cols < visibleCount)
							{
								PlaySFX(&sfx_move);
								menu_index += cols;
							}
						}
					}
					else if(ui_type==1)
					{
						menu_index = menu_index2;
						if(control_up && !old_control_up)
						{
							if(menu_index2 > 0)
							{
								PlaySFX(&sfx_move);
								menu_index2--;
							}
						}

						if(control_down && !old_control_down)
						{
							if(menu_index2 < visibleCount-1)
							{
								PlaySFX(&sfx_move);
								menu_index2++;
							}
						}
					}
				}
				else
				{
					if(paddata.BTN_CROSS && !oldpad.BTN_CROSS && file_downloaded)
					{
						PlaySFX(&sfx_decide);
						file_downloaded=0;
						int selected_app = visibleApps[menu_index];
						downloading_game = selected_app;
						
						char dst[256];
						
						snprintf(dst, sizeof(dst), "/dev_hdd0/packages/%s.pkg", gApps[selected_app].id);

						Download_Start(data_url, gApps[selected_app].pkg, dst);
						download_kind = DOWNLOAD_GAME;
					}
					if(paddata.BTN_CIRCLE && !oldpad.BTN_CIRCLE)
					{
						PlaySFX(&sfx_cancel);
						menu_selected = 0;
					}
				}
					
				old_control_left = control_left;
				old_control_right = control_right;
				old_control_up = control_up;
				old_control_down = control_down;
				
				oldpad = paddata;
			}
		}
		
		drawScene();
		
		if(manifest_done)
		{
			manifest_done = 0;

			printf("have: %s, available: %s\n", global_version, manifestVersion);
			if(strcmp(manifestVersion, global_version) != 0 && !version_ignore)
			{
				UpdateNOW();
				need_update = 1;
			}
			
			if(!need_update)
			{
				icon_thread_running = 1;
				RefreshVisibleApps();
				sysThreadCreate(&icon_thread, IconLoaderThread, NULL, 2000, 0x4000, 0, "IconLoaderThread");
			}
		}
		
		if(icons_ready)
		{
			icons_ready = 0;
			RefreshVisibleApps();
		}
		
		uint64_t exit_code;

		if(download_finished)
		{
			download_finished = 0;
			download_thread = 0;
			printf("download_thread finished\n");
			
			if(download_kind == DOWNLOAD_MANIFEST)
			{
				if(download_success)
				{
					show_screen_idk = 1;
				    printf("Manifest downloaded successfully. Parsing now...\n");
				    sysThreadCreate(&manifest_manage, manifest_parse, NULL, 2000, 0x4000, 0, "manifest_parse");
				}
				else
				{
					msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK | MSG_DIALOG_DISABLE_CANCEL_ON,
						langStr[21], EmptyDialogCallback, NULL, NULL);
				}
			}

			if(show_download_dialog)
			{
				show_download_dialog = 0;

				if(download_success)
				{
					if(need_update)
					{
						msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK | MSG_DIALOG_DISABLE_CANCEL_ON,
							langStr[22], EmptyDialogCallback, NULL, NULL);
					}
					else
					{
						msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK | MSG_DIALOG_DISABLE_CANCEL_ON,
							langStr[23], EmptyDialogCallback, NULL, NULL);
					}
				}
				else
				{
					msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK | MSG_DIALOG_DISABLE_CANCEL_ON,
						langStr[24], EmptyDialogCallback, NULL, NULL);
				}
			}
		}
		
		if(icon_thread)
		{
			if(sysThreadJoin(icon_thread, &exit_code) == 0) icon_thread = 0;
		}
		
		tiny3d_Flip();
		usleep(3666);
    }

    return 0;
}