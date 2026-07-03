#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <io/pad.h>
#include <pngdec/pngdec.h>
#include <sysmodule/sysmodule.h>
#include <sysutil/msg.h>
#include <assert.h>
#include <sys/thread.h>
#include <unistd.h>
#include <sys/stat.h>

#include "https.h"
#include "http.h"
#include "ssl.h"
#include "net.h"
#include "util.h"
#include "cJSON.h"

#include <tiny3d.h>
#include <libfont.h>

#include <ft2build.h>
#include <freetype/freetype.h> 
#include <freetype/ftglyph.h>

#include "asimov_ttf_bin.h"

char data_url[] = "https://github.com/AcidPS3-Project/AcidPS3Data/raw/refs/heads/main/games/";
char assets_url[] = "http://acidnt31.w10.site/assets/store/";

#define HTTP_YES		1
#define HTTP_NO 		0
#define HTTP_SUCCESS 	1
#define HTTP_FAILED	 	0

#define HTTP_USER_AGENT "Mozilla/5.0 (PLAYSTATION 3; 1.00)"

typedef struct
{
    void* http_pool;
    void* ssl_pool;
    httpsData* caList;
    void* cert_buffer;
} t_http_pools;

static t_http_pools http_pools;
u8 cancel = 0;

static char getBuffer[64*1024];

volatile int g_download_active = 0;
volatile uint64_t g_downloaded_bytes = 0;
volatile uint64_t g_download_total = 0;
volatile int g_download_finished = 0;
volatile int g_download_result = 0;
char g_download_name[256];

char manifestVersion[16];
char global_name[] = "AcidPS3 Store";
char global_version[16];
int check_update = 1;

int http_init(void)
{
    int ret;
	u32 cert_size=0;
	u8 module_https_loaded=0;
	u8 module_http_loaded=0;
	u8 module_net_loaded=0;
	u8 module_ssl_loaded=0;
	
	u8 https_init=0;
	u8 http_init=0;
	u8 net_init=0;
	u8 ssl_init=0;

	//init
	ret = sysModuleLoad(SYSMODULE_NET);
	if (ret < 0) {
		printf("Error : sysModuleLoad(SYSMODULE_NET) HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else module_net_loaded=HTTP_YES;

	ret = netInitialize();
	if (ret < 0) {
		printf("Error : netInitialize HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else net_init=HTTP_YES;

	ret = sysModuleLoad(SYSMODULE_HTTP);
	if (ret < 0) {
		printf("Error : sysModuleLoad(SYSMODULE_HTTP) HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else module_http_loaded=HTTP_YES;

	http_pools.http_pool = malloc(0x10000);
	if (http_pools.http_pool == NULL) {
		printf("Error : out of memory (http_pool)\n");
		ret=HTTP_FAILED;
		goto end;
	}

	ret = httpInit(http_pools.http_pool, 0x10000);
	if (ret < 0) {
		printf("Error : httpInit HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else http_init=HTTP_YES;

	ret = sysModuleLoad(SYSMODULE_HTTPS);
	if (ret < 0) {
		printf("Error : sysModuleLoad(SYSMODULE_HTTP) HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else module_https_loaded=HTTP_YES;

	ret = sysModuleLoad(SYSMODULE_SSL);
	if (ret < 0) {
		printf("Error : sysModuleLoad(SYSMODULE_HTTP) HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else module_ssl_loaded=HTTP_YES;

	http_pools.ssl_pool = malloc(0x40000);
	if (http_pools.ssl_pool == NULL) {
		printf("Error : out of memory (ssl_pool)\n");
		ret=HTTP_FAILED;
		goto end;
	}

	ret = sslInit(http_pools.ssl_pool, 0x40000);
	if (ret < 0) {
		printf("Error : sslInit HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else ssl_init=HTTP_YES;

	http_pools.caList = (httpsData *)malloc(sizeof(httpsData));
	ret = sslCertificateLoader(SSL_LOAD_CERT_ALL, NULL, 0, &cert_size);
	if (ret < 0) {
		printf("Error : sslCertificateLoader HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}

	http_pools.cert_buffer = malloc(cert_size);
	if (http_pools.cert_buffer==NULL) {
		printf("Error : out of memory (cert_buffer)\n");
		ret=HTTP_FAILED;
		goto end;
	}

	ret = sslCertificateLoader(SSL_LOAD_CERT_ALL, http_pools.cert_buffer, cert_size, NULL);
	if (ret < 0) {
		printf("Error : sslCertificateLoader HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}

	(&http_pools.caList[0])->ptr = http_pools.cert_buffer;
	(&http_pools.caList[0])->size = cert_size;

	ret = httpsInit(1, (httpsData *) http_pools.caList);
	if (ret < 0) {
		printf("Error : httpsInit HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	} else https_init=HTTP_YES;

	return HTTP_SUCCESS;

end:
	if(http_pools.caList) free(http_pools.caList);
	if(https_init) httpsEnd();
	if(ssl_init) sslEnd();
	if(http_init) httpEnd();
	if(net_init) netDeinitialize();
	
	if(module_http_loaded) sysModuleUnload(SYSMODULE_HTTP);
	if(module_https_loaded) sysModuleUnload(SYSMODULE_HTTPS);
	if(module_ssl_loaded) sysModuleUnload(SYSMODULE_SSL);
	if(module_net_loaded) sysModuleUnload(SYSMODULE_NET);
	
	if(http_pools.http_pool) free(http_pools.http_pool);
	if(http_pools.ssl_pool) free(http_pools.ssl_pool);
	if(http_pools.cert_buffer) free(http_pools.cert_buffer);
	
	return ret;
}

void http_end(void)
{
	if(http_pools.caList) free(http_pools.caList);
	httpsEnd();
	sslEnd();
	httpEnd();
	netDeinitialize();
	
	sysModuleUnload(SYSMODULE_HTTP);
	sysModuleUnload(SYSMODULE_HTTPS);
	sysModuleUnload(SYSMODULE_SSL);
	sysModuleUnload(SYSMODULE_NET);

	if(http_pools.http_pool) free(http_pools.http_pool);
	if(http_pools.ssl_pool) free(http_pools.ssl_pool);
	if(http_pools.cert_buffer) free(http_pools.cert_buffer);
	
	return;
}

char* escape_filename(const char* filename)
{
	int len = strlen(filename);
    char* ret = (char *)calloc(1, len*3);

	httpUtilEscapeUri(ret, len*3, (uint8_t*) filename, len, 0);

	return ret;
}

int http_download(const char* url, const char* filename, const char* local_dst)
{
	int ret = 0, httpCode = 0;
	httpUri uri;
	httpClientId httpClient = 0;
	httpTransId httpTrans = 0;
	FILE* fp=NULL;
	u32 nRecv = 1;
	u32 size = 0;
	uint64_t length = 0;
	void *uri_pool = NULL;
	char* escaped_name = NULL;
	char* escaped_url = NULL;
	
	g_download_active = 1;
	g_download_finished = 0;
	g_download_result = 0;
	g_downloaded_bytes = 0;
	g_download_total = 0;
	snprintf(g_download_name, sizeof(g_download_name), "%s", filename);

	ret = httpCreateClient(&httpClient);
	if (ret < 0) {
		printf("Error : httpCreateClient HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}
    httpClientSetConnTimeout(httpClient, 10 * 1000 * 1000);
    httpClientSetUserAgent(httpClient, HTTP_USER_AGENT);
    httpClientSetAutoRedirect(httpClient, 1);

	// Escape URL file name characters
	escaped_name = escape_filename(filename);
	asprintf(&escaped_url, "%s%s", url, escaped_name);
	
	printf("Downloading (%s) -> (%s)\n", escaped_url, local_dst);

	//URI
	ret = httpUtilParseUri(&uri, escaped_url, NULL, 0, &size);
	if (ret < 0) {
		printf("Error : httpUtilParseUri() HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}

	uri_pool = malloc(size);
	if (uri_pool == NULL) {
		printf("Error : out of memory (uri_pool)\n");
		ret=HTTP_FAILED;
		goto end;
	}

	ret = httpUtilParseUri(&uri, escaped_url, uri_pool, size, 0);
	if (ret < 0) {
		printf("Error : httpUtilParseUri() HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}
	//END of URI	

	//SEND REQUEST
	ret = httpCreateTransaction(&httpTrans, httpClient, HTTP_METHOD_GET, &uri);
	if (ret < 0) {
		printf("Error : httpCreateTransaction() HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}

	ret = httpSendRequest(httpTrans, NULL, 0, NULL);
	if (ret < 0) {
		printf("Error : httpSendRequest() HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}
	
	//GET SIZE
	httpResponseGetContentLength(httpTrans, &length);
	g_download_total = length;

	ret = httpResponseGetStatusCode(httpTrans, &httpCode);
	if (ret < 0) {
		printf("Error : cellHttpResponseGetStatusCode() HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}

	if(httpCode != HTTP_STATUS_CODE_OK && httpCode >= 400 ) {
		printf("Error : Status code (%d)\n", httpCode);
		ret=HTTP_FAILED;
		goto end;
	}
	
	//TRANSFER
	fp = fopen(local_dst, "wb");
	if(fp == NULL) {
		printf("Error : fopen() HTTP_FAILED : %s\n", local_dst);
		ret=HTTP_FAILED;
		goto end;
	}
	
	while(nRecv != 0)
	{
		if(httpRecvResponse(httpTrans, (void*) getBuffer, sizeof(getBuffer)-1, &nRecv) > 0) break;
		if(nRecv == 0) break;
		fwrite((char*) getBuffer, 1, nRecv, fp);
		g_downloaded_bytes += nRecv;
		if(cancel) break;
	}
	fclose(fp);
	
	if(cancel) {
		unlink((char*)local_dst);
		ret=HTTP_FAILED;
		cancel=0;
		g_download_result = ret;
		g_download_finished = 1;
		g_download_active = 0;
	}

	//END of TRANSFER
	ret=1;
	g_download_result = 1;
	g_download_finished = 1;
	g_download_active = 0;

	end:
	{
		g_download_result = ret;
		g_download_finished = 1;
		g_download_active = 0;
		if(httpTrans) httpDestroyTransaction(httpTrans);
		if(httpClient) httpDestroyClient(httpClient);
		if(uri_pool) free(uri_pool);
		if(escaped_url) free(escaped_url);
		if(escaped_name) free(escaped_name);

		return ret;
	}
}









#include "spu_soundlib.h"
#include "audioplayer.h"
#include "spu_soundmodule.bin.h"

// IMPORT IMAGES
#include "default_icon_png_bin.h"

#include "loop_mp3_bin.h"
#include "c_mp3_bin.h"
#include "d_mp3_bin.h"
#include "m_mp3_bin.h"

float lerp(float min, float max, float ratio)
{
	return min + (max - min) * ratio;
}


int ttf_inited = 0;
int store_loaded = 0;
int menu_selected = 0;
int menu_type = 0; // 0 - ps3 games, 1 - psp games, 2 - update
int menu_index = 0;

int downloading_game = 0;

int file_downloaded = 0;
int manifest_gather = 0;
int manifest_done = 0;

FT_Library freetype;
FT_Face face;

pngData DefaultIcon;
u32 DefaultOff;

void Load_PNG()
{
	pngLoadFromBuffer(default_icon_png_bin, default_icon_png_bin_size, &DefaultIcon);
}

int LoadIconPNG(const char *path, pngData *tex, u32 *icon_off)
{
    FILE *fp = fopen(path, "rb");
    if(fp)
	{
		fseek(fp, 0, SEEK_END);
		size_t size = ftell(fp);
		rewind(fp);

		void *buf = memalign(128, size);

		fread(buf, 1, size, fp);
		fclose(fp);

		pngLoadFromBuffer(buf, size, tex);
		
		u32 *texture = tiny3d_AllocTexture(tex->pitch * tex->height);
		memcpy(texture, tex->bmp_out, tex->pitch * tex->height);
		
		*icon_off = tiny3d_TextureOffset(texture);

		free(tex->bmp_out);
		tex->bmp_out = NULL;
		
		free(buf);
		return 0;
	}
	return 1;
}









char manifest_path[] = "/dev_hdd0/game/ACIDSTORE/USRDIR/cache/manifest.json";

#define MAX_APPS 64

typedef struct
{
    char id[64];
    char name[64];
    char author[64];
    char version[16];
    char console[16];

    char pkg[256];
    char icon[256];

    char description[2048];
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

static char download_url[512];
static char download_file[256];
static char download_dest[256];

void manifest_parse()
{
	int i = 0;
	FILE *fp = fopen(manifest_path, "rb");

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	rewind(fp);

	char *json = malloc(size + 1);

	fread(json, 1, size, fp);
	json[size] = 0;

	fclose(fp);
	
	cJSON *root = cJSON_Parse(json);

	if(!root)
	{
		printf("Invalid JSON\n");
		free(json);
	}
	else
	{
		strcpy(manifestVersion, cJSON_GetObjectItem(root, "version")->valuestring);
		printf("JSON VERSION: %s\n", manifestVersion);
		
		cJSON *apps = cJSON_GetObjectItem(root, "apps");
		printf("TEST-OUTPUT-0\n");

		gAppCount = cJSON_GetArraySize(apps);
		printf("TEST-OUTPUT-1\n");

		for(i=0;i<gAppCount;i++)
		{
			
			cJSON *app = cJSON_GetArrayItem(apps,i);

			strcpy(gApps[i].id, cJSON_GetObjectItem(app,"id")->valuestring);
			strcpy(gApps[i].name, cJSON_GetObjectItem(app,"name")->valuestring);
			strcpy(gApps[i].author, cJSON_GetObjectItem(app,"author")->valuestring);
			strcpy(gApps[i].version, cJSON_GetObjectItem(app,"version")->valuestring);
			
			strcpy(gApps[i].console, cJSON_GetObjectItem(app,"console")->valuestring);
			
			strcpy(gApps[i].pkg, cJSON_GetObjectItem(app,"pkg")->valuestring);
			strcpy(gApps[i].icon, cJSON_GetObjectItem(app,"icon")->valuestring);
			
			strcpy(gApps[i].description, cJSON_GetObjectItem(app,"description")->valuestring);
			printf("GAME NAME: %s; ID: %s\n", gApps[i].name, gApps[i].id);
		}
		
		cJSON_Delete(root);
		free(json);
	}
	
	
	check_update=0;
	manifest_done = 1;
}

void EmptyDialogCallback(msgButton button, void *usrData)
{
    // ehhehe fuck this shit
}











static sys_ppu_thread_t download_thread;

static void DownloadThread(void *arg)
{
	printf("GET FILE...\n");
    int ret = http_download(download_url, download_file, download_dest);
	
	if(ret)
	{
		if(!manifest_gather)
		{
			manifest_gather=1;
			manifest_parse();
		}
		else
		{
			msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK | MSG_DIALOG_DISABLE_CANCEL_ON,
				"Download completed. Check the /dev_hdd0/packages/ folder.", EmptyDialogCallback, NULL, NULL);
		}
	}
	else
	{
		msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK | MSG_DIALOG_DISABLE_CANCEL_ON,
			"Failed.", EmptyDialogCallback, NULL, NULL);
	}
	file_downloaded=1;

	download_thread = 0;
	sysThreadExit(0);
}

int Download_Start(const char *url, const char *file, const char *dest)
{
    if(download_thread != 0) return -1;

    snprintf(download_url, sizeof(download_url), "%s", url);
	snprintf(download_file, sizeof(download_file), "%s", file);
	snprintf(download_dest, sizeof(download_dest), "%s", dest);

    return sysThreadCreate(&download_thread, DownloadThread, NULL, 1000, 0x4000, 0, "DownloadThread");
}

void UpdatePromptCallback(msgButton button, void *usrData)
{
    switch(button)
    {
        case MSG_DIALOG_BTN_YES:
        {
            printf("User accepted update\n");
            char data_url[] = "https://github.com/AcidPS3-Project/AcidPS3Data/raw/refs/heads/main/";

            char zpath[256];
            char dpath[256];
			snprintf(zpath, sizeof(zpath), "/dev_hdd0/packages/acidps3-store-%s.pkg", manifestVersion);
			snprintf(dpath, sizeof(dpath), "acidps3-store-%s.pkg", manifestVersion);
			
            Download_Start(data_url, dpath, zpath);

            break;
        }

        case MSG_DIALOG_BTN_NO:
        case MSG_DIALOG_BTN_ESCAPE:
        default:
            printf("User declined update\n");
            break;
    }
}









typedef struct
{
    short *pcm;
    int pcm_size;
    int freq;
    int stereo;
} SoundEffect;

SoundEffect sfx_decide;
SoundEffect sfx_cancel;
SoundEffect sfx_move;

int LoadSFX(SoundEffect *sfx, const void *data, int size)
{
    sfx->pcm_size = size * 20;
	sfx->pcm = memalign(32, sfx->pcm_size);
    if(!sfx->pcm) return 0;
    if( DecodeAudio((void *)data,size,sfx->pcm,&sfx->pcm_size,&sfx->freq,&sfx->stereo) != 0)
    {
        free(sfx->pcm);
        sfx->pcm = NULL;
        return 0;
    }
	return 1;
}

void init_sfx()
{
	LoadSFX(&sfx_cancel, c_mp3_bin, c_mp3_bin_size);
	LoadSFX(&sfx_decide, d_mp3_bin, d_mp3_bin_size);
	LoadSFX(&sfx_move, m_mp3_bin, m_mp3_bin_size);
}

int screen_w = 848 / 2;
int screen_h = 512 / 2;
int offset_x = -34;
int offset_y = -22;

static int fps = 0;
static int frames = 0;
static u64 last_time = 0;

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

void PlayBGM()
{
    FILE *fp = (FILE *)mem_open((char *)loop_mp3_bin, loop_mp3_bin_size);
    PlayAudiofd(fp, 0, AUDIO_INFINITE_TIME);
}

void PlaySFX(SoundEffect *sfx)
{
	int voice = SND_GetFirstUnusedVoice();
    if (voice < 1) return;
    SND_SetVoice(voice, sfx->stereo ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT, sfx->freq,
        0, sfx->pcm, sfx->pcm_size, 255, 255, NULL);
}

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

int TTFLoadFont(char * path, void * from_memory, int size_from_memory)
{
    if(!ttf_inited) FT_Init_FreeType(&freetype);
    ttf_inited = 1;
    if(path) {
        if(FT_New_Face(freetype, path, 0, &face)) return -1;
    } else {
        if(FT_New_Memory_Face(freetype, from_memory, size_from_memory, 0, &face)) return -1;
        }
    return 0;
}

void TTFUnloadFont()
{
   FT_Done_FreeType(freetype);
   ttf_inited = 0;
}

void TTF_to_Bitmap(u8 chr, u8 * bitmap, short *w, short *h, short *y_correction)
{
    FT_Set_Pixel_Sizes(face, (*w), (*h));
    FT_GlyphSlot slot = face->glyph;
    memset(bitmap, 0, (*w) * (*h));
    if(FT_Load_Char(face, (char) chr, FT_LOAD_RENDER )) {(*w) = 0; return;}
    int n, m, ww;
    *y_correction = (*h) - 1 - slot->bitmap_top;
    ww = 0;
    for(n = 0; n < slot->bitmap.rows; n++) {
        for (m = 0; m < slot->bitmap.width; m++) {
            if(m >= (*w) || n >= (*h)) continue;
            bitmap[m] = (u8) slot->bitmap.buffer[ww + m];
        }
		bitmap += *w;
		ww += slot->bitmap.width;
    }
    *w = ((slot->advance.x + 31) >> 6) + ((slot->bitmap_left < 0) ? -slot->bitmap_left : 0);
    *h = slot->bitmap.rows;
}

void DrawWrappedText(float x, float y, float line_h, int max_chars, const char *text)
{
    char line[512];
    char word[128];
    int line_len = 0;
    int word_len = 0;
    int i = 0;

    line[0] = '\0';
    word[0] = '\0';

    while(1)
    {
        char c = text[i];

        // build current word
        if(c != ' ' && c != '\0' && c != '\n')
        {
            if(word_len < sizeof(word) - 1)
            {
                word[word_len++] = c;
                word[word_len] = '\0';
            }
        }

        // if word ended / newline / end of string
        if(c == ' ' || c == '\0' || c == '\n')
        {
            if(word_len > 0)
            {
                int needed = word_len;
                if(line_len > 0) needed += 1; // space

                if(line_len + needed > max_chars)
                {
                    DrawFormatString(x, y, "%s", line);
                    y += line_h;

                    strcpy(line, word);
                    line_len = word_len;
                }
                else
                {
                    if(line_len > 0)
                    {
                        strcat(line, " ");
                        line_len++;
                    }
                    strcat(line, word);
                    line_len += word_len;
                }

                word[0] = '\0';
                word_len = 0;
            }

            if(c == '\n')
            {
                DrawFormatString(x, y, "%s", line);
                y += line_h;
                line[0] = '\0';
                line_len = 0;
            }

            if(c == '\0') break;
        }

        i++;
    }

    if(line_len > 0) DrawFormatString(x, y, "%s", line);
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

void DrawWave(float baseY, float amp, float freq, float phase,
              float thickness, float z, u32 topColor, u32 bottomColor)
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
	tiny3d_SetTexture(0, icon_off, icon.width, icon.height,
		icon.pitch, TINY3D_TEX_FORMAT_A8R8G8B8, TEXTURE_NEAREST);
}

int visibleApps[MAX_APPS];
int visibleCount = 0;

void RefreshVisibleApps()
{
    int i;
    visibleCount = 0;

    for(i = 0; i < gAppCount; i++)
    {
        if(menu_type == 0 && strcmp(gApps[i].console, "ps3") != 0)
            continue;

        if(menu_type == 1 && strcmp(gApps[i].console, "psp") != 0)
            continue;

        visibleApps[visibleCount++] = i;
    }

    if(menu_index >= visibleCount)
        menu_index = visibleCount - 1;

    if(menu_index < 0)
        menu_index = 0;
}

float menu_index_i = 0;

void drawScene()
{
	u32 color = 0xffffffff;
	u32 color2 = 0x000077ff;
	int i = 0;
	int off_x = -20;
	int off_y = 60;
	int space_y = 120;
    tiny3d_Project2D();
    
	static float wave_t = 0.0f;
	SetFontSize(28, 32);
    SetFontColor(0xffffffff, 0x00000000);
    SetCurrentFont(0);
	
	wave_t += 0.015f;
	
	menu_index_i = lerp(menu_index_i, menu_index, 0.1f);
	
	DrawRect4(-99, -99, 1280, 720, 5, 0x333399ff, 0x333399ff, 0x111188ff, 0x111188ff);
	
	DrawWave(240.0f, 18.0f, 0.015f, wave_t * 0.7f, 70.0f, 4, 0x2020a0ff, 0x00000000);
	DrawWave(299.0f, 24.0f, 0.012f, wave_t, 80.0f, 3, 0x3030ffff, 0x00000000);
	
	//DrawSprite2D(0, 0, 0, 32, 32, 16, 16, 320, 320, 2, color);
	//DrawSpriteUI(-34, -22, 0, 0, 384, 192, 1024, 1024, 302, 112, 1, color);
	
	if(!manifest_gather)
	{
		DrawRect(-66, 410+off_y, 1280, 720, 0, color2);
		DrawFormatString(-20, 420+off_y, "Collecting data from our server...");
	}
	else
	{
		if(!menu_selected)
		{
			for(i = 0; i < visibleCount; i++)
			{
				int app = visibleApps[i];
				int app_y_pos = off_y+(i*space_y)+((-menu_index_i)*space_y);
				
				if(gIcons[app].loaded) SetTexture(gIcons[app].icon, gIcons[app].icon_off);
					else SetTexture(DefaultIcon, DefaultOff);
				
				DrawIcon(off_x, app_y_pos, 1, 320/2, 176/2, color);
				SetFontSize(28, 32);
				DrawFormatString(off_x+170, app_y_pos, "%s %s", gApps[app].name, gApps[app].version);
				SetFontSize(16, 16);
				DrawFormatString(off_x+170, 36+app_y_pos, "Developed by: %s", gApps[app].author);
				
				if(i==menu_index)
					DrawRectOutline(off_x-5, app_y_pos-5, 1000, 95, 3, 0, color2);
			}
			SetFontSize(28, 32);
			
			DrawRect(-66, 333+off_y, 1280, 720, 0, color2);
			DrawFormatString(off_x, 420+off_y, "^/v - LIST GAMES      X - SELECT");
		}
		else
		{
			int selected_app = visibleApps[menu_index];
			
			if(gIcons[selected_app].loaded) SetTexture(gIcons[selected_app].icon, gIcons[selected_app].icon_off);
				else SetTexture(DefaultIcon, DefaultOff);
			
			DrawIcon(off_x, off_y, 1, 320, 176, color);
			SetFontSize(28, 32);
			DrawFormatString(off_x+340, off_y, "%s %s", gApps[selected_app].name, gApps[selected_app].version);
			SetFontSize(16, 16);
			DrawFormatString(off_x+340, 36+off_y, "Developed by: %s", gApps[selected_app].author);
			DrawWrappedText(off_x+340, 56+off_y, 16, 77, gApps[selected_app].description);
			SetFontSize(28, 32);
			DrawRect(-66, 333+off_y, 1280, 720, 0, color2);
			if(file_downloaded) DrawFormatString(off_x, 420+off_y, "X - DOWNLOAD   O - BACK");
			else
			{
				// if(downloading_game == selected_app) DrawFormatString(off_x, 380+off_y, "DOWNLOADING...");
				// else DrawFormatString(off_x, 380+off_y, "Something else is downloading!");
				
				DrawFormatString(off_x, 420+off_y, "O - BACK");
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
			
			DrawFormatString(off_x, 344+off_y, "Downloading: %s", display_name);
			DrawFormatString(off_x, 380+off_y, "%llu / %llu bytes (%.1f%%)", (unsigned long long)g_downloaded_bytes,
				(unsigned long long)g_download_total, percent);
		}
	}
	
	SetFontSize(28, 32);
	DrawRect(-66, -66, 1280, 111, 0, color2);
	DrawFormatString(-20,0, "%s [%s]", global_name, global_version);
}

void LoadTexture()
{
    u32 * texture_mem = tiny3d_AllocTexture(64*1024*1024);
    u32 * texture_pointer;
    if(!texture_mem) return;
    texture_pointer = texture_mem;
    Load_PNG();
	ResetFont();
    TTFLoadFont(NULL, (void *) asimov_ttf_bin, asimov_ttf_bin_size);
    texture_pointer = (u32 *) AddFontFromTTF((u8 *) texture_pointer, 32, 255, 64, 64, TTF_to_Bitmap);
    TTFUnloadFont();
	memcpy(texture_pointer, DefaultIcon.bmp_out, DefaultIcon.pitch * DefaultIcon.height);
	DefaultOff = tiny3d_TextureOffset(texture_pointer);
	free(DefaultIcon.bmp_out);
	DefaultIcon.bmp_out = NULL;
	texture_pointer += ((DefaultIcon.pitch * DefaultIcon.height + 15) & ~15) / 4;
}






void ShowUpdatePrompt()
{
	char msg[1024];
	snprintf(msg, sizeof(msg), "A new update (%s) is here!\n\n"
		"Do you want to install the update?\n"
		"( pls install this update :> )", manifestVersion);

	msgDialogOpen2(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_YESNO | MSG_DIALOG_DISABLE_CANCEL_ON,
		msg, UpdatePromptCallback, NULL, NULL);
}



void DemoCompleteCallback(msgButton button, void *usrData)
{
    sysProcessExit(0);
}








int main(int argc, const char* argv[])
{
	padInfo padinfo;
	int i;
	static padData oldpad;
	padData paddata;
	srand(time(NULL));
	tiny3d_Init(1024*1024);

	mkdir("/dev_hdd0/game/ACIDSTORE/USRDIR/cache", 0777);
	mkdir("/dev_hdd0/game/ACIDSTORE/USRDIR/cache/icons", 0777);
	mkdir("/dev_hdd0/packages", 0777);

	snprintf(global_version, sizeof(global_version), "v0.1");
	
	sysModuleLoad(SYSMODULE_HTTP);
	sysModuleLoad(SYSMODULE_NETCTL);

	sysModuleLoad(SYSMODULE_PNGDEC);
	Initialize();
	init_sfx();
	
	ioPadInit(7);
    LoadTexture();
	
	http_init();
	
	store_loaded = 1;
    PlayBGM();
	printf("received call to get manifest..\n");
	
	Download_Start(assets_url, "manifest.json", manifest_path);
	
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
		
		for(i = 0; i < MAX_PADS; i++){

			if(padinfo.status[i]){
				ioPadGetData(i, &paddata);
									
				// basic
				if(!menu_selected)
				{
					if(paddata.BTN_CROSS && !oldpad.BTN_CROSS)
					{
						PlaySFX(&sfx_decide);
						menu_selected = 1;
					}
					// if(paddata.BTN_LEFT && !oldpad.BTN_LEFT)
					// {
						// menu_index = 0;
						// menu_type--;
						// if(menu_type<0) menu_type=0;
						
						// RefreshVisibleApps();
					// }
					// if(paddata.BTN_RIGHT && !oldpad.BTN_RIGHT)
					// {
						// menu_index = 0;
						// menu_type++;
						// if(menu_type>2) menu_type=2;
						
						// RefreshVisibleApps();
					// }
					if(paddata.BTN_UP && !oldpad.BTN_UP)
					{
						PlaySFX(&sfx_move);
						if(menu_index>0) menu_index--;
					}
					if(paddata.BTN_DOWN && !oldpad.BTN_DOWN)
					{
						PlaySFX(&sfx_move);
						if(menu_index < visibleCount - 1) menu_index++;
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
						char pkg[256];
						
						snprintf(dst, sizeof(dst), "/dev_hdd0/packages/%s.pkg", gApps[selected_app].id);
						snprintf(pkg, sizeof(pkg), "%s/%s", gApps[selected_app].console, gApps[selected_app].pkg);

						Download_Start(data_url, pkg, dst);
					}
					if(paddata.BTN_CIRCLE && !oldpad.BTN_CIRCLE)
					{
						PlaySFX(&sfx_cancel);
						menu_selected = 0;
					}
				}
				
				oldpad = paddata;
			}
		}
		
		if(manifest_done)
		{
			int i = 0;
			for(i=0; i<gAppCount; i++)
			{
				gIcons[i].init = 0;
				printf("GET ICON...\n");
				
				char path[256];
				snprintf(path, sizeof(path), "/dev_hdd0/game/ACIDSTORE/USRDIR/cache/icons/%s.png", gApps[i].id);
				
				int ret = http_download(assets_url, gApps[i].icon, path);
				if(ret)
				{
					printf("START ICON LOAD...\n");
					gIcons[i].init = 1;
				}
			}
			
			for(i=0; i<gAppCount; i++)
			{
				char path[256];
				snprintf(path, sizeof(path), "/dev_hdd0/game/ACIDSTORE/USRDIR/cache/icons/%s.png", gApps[i].id);
				
				if(!LoadIconPNG(path, &gIcons[i].icon, &gIcons[i].icon_off)) gIcons[i].loaded = 1;
			}
			
			RefreshVisibleApps();
			manifest_done = 0;
		}
		
		if(store_loaded)
		{
			if(!check_update)
			{
				printf("have: %s, available: %s\n", global_version, manifestVersion);
				if(strcmp(manifestVersion, global_version) != 0) ShowUpdatePrompt();
				check_update=1;
			}
			drawScene();
		}
		
		tiny3d_Flip();
		usleep(3666);
    }

    return 0;
}