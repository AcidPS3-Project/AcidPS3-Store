#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "global.h"

#include "cJSON.h"

#define HTTP_YES		1
#define HTTP_NO 		0
#define HTTP_SUCCESS 	1
#define HTTP_FAILED	 	0

#define HTTP_USER_AGENT "Mozilla/5.0 (PLAYSTATION 3; 1.00)"

#define DOWNLOAD_MANIFEST 0
#define DOWNLOAD_GAME     1
#define DOWNLOAD_UPDATE   2

typedef struct
{
    void* http_pool;
    void* ssl_pool;
    httpsData* caList;
    void* cert_buffer;
} t_http_pools;

char g_download_name[256];
char manifestVersion[16];

extern char manifest_path[];
extern char data_url[];
extern char assets_url[];

extern volatile int g_download_active;
extern volatile uint64_t g_downloaded_bytes;
extern volatile uint64_t g_download_total;
extern volatile int g_download_finished;
extern volatile int g_download_result;

extern char data_url[];
extern char assets_url[];

typedef struct
{
    char id[64];
    char name[64];
    char author[64];
    char version[16];
    char type[16];

    char pkg[256];
    char icon[256];
	
    char hidden_game[8];

    char description[2048];
    char short_desc[1024];
} StoreApp;

StoreApp gApps[MAX_APPS];
int gAppCount;

typedef struct
{
    pngData icon;
    u32 icon_off;
    int loaded;
    int downloading;
    int init;
} IconCache;

IconCache gIcons[MAX_APPS];

extern sys_ppu_thread_t manifest_manage;
extern t_http_pools http_pools;
extern sys_ppu_thread_t download_thread;
extern sys_ppu_thread_t icon_thread;

int http_init();
void http_end();
int http_download(const char* url, const char* filename, const char* local_dst);
void manifest_parse(void *arg);

extern int downloading_game;
extern int file_downloaded;
extern int manifest_gather;
extern int manifest_done;

extern volatile int download_kind;
extern volatile int show_download_dialog;
extern volatile int download_success;
extern volatile int download_finished;

int Download_Start(const char *url, const char *file, const char *dest);

extern volatile int icon_thread_running;
extern volatile int icons_ready;

void IconLoaderThread(void *arg);

#endif