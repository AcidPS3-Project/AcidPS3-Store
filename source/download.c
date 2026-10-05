#include "libs.h"
#include "draw.h"
#include "download.h"

u8 cancel = 0;

int downloading_game = 0;

int file_downloaded = 0;
int manifest_gather = 0;
int manifest_done = 0;

volatile int g_download_active = 0;
volatile uint64_t g_downloaded_bytes = 0;
volatile uint64_t g_download_total = 0;
volatile int g_download_finished = 0;
volatile int g_download_result = 0;

sys_ppu_thread_t manifest_manage;
sys_ppu_thread_t download_thread;
sys_ppu_thread_t icon_thread;

char getBuffer[64*1024];

char download_url[512];
char download_file[256];
char download_dest[256];

char manifest_path[] = "/dev_hdd0/game/ACIDSTORE/USRDIR/cache/manifest.json";
char data_url[] = "https://";
char assets_url[] = "http://acidnt31.w10.site/assets/store/";

volatile int show_download_dialog = 0;
volatile int download_success = 0;
volatile int download_finished = 0;

volatile int icon_thread_running = 0;
volatile int icons_ready = 0;

volatile int download_kind = 0;

int http_init(void)
{
    CURLcode ret;

    ret = curl_global_init(CURL_GLOBAL_DEFAULT);
    
    curl_version_info_data *vi = curl_version_info(CURLVERSION_NOW);

	printf("SSL: %s\n", vi->ssl_version);

    if(ret != CURLE_OK)
    {
        printf("curl_global_init failed: %s\n", curl_easy_strerror(ret));
        return HTTP_FAILED;
    }

    return HTTP_SUCCESS;
}

void http_end(void)
{
    curl_global_cleanup();
}

char* escape_filename(const char* filename)
{
	int len = strlen(filename);
    char* ret = (char *)calloc(1, len*3);

	const char *hex = "0123456789ABCDEF";
    size_t i;
    size_t j = 0;

    for(i = 0; filename[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)filename[i];

        if((c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~')
        {
            ret[j++] = c;
        }
        else
        {
            ret[j++] = '%';
            ret[j++] = hex[(c >> 4) & 0x0F];
            ret[j++] = hex[c & 0x0F];
        }
    }

    ret[j] = '\0';

	return ret;
}

typedef struct
{
    FILE *fp;
    int failed;
} CurlDownload;

static size_t Acurl_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    CurlDownload *download = (CurlDownload *)userdata;
    size_t total = size * nmemb;

    if(cancel) return 0;

    size_t written = fwrite(ptr, 1, total, download->fp);

    g_downloaded_bytes += written;

    if(written != total)
    {
        download->failed = 1;
        return 0;
    }

    return written;
}

static int Acurl_progress_callback(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    (void)userdata;
    (void)ultotal;
    (void)ulnow;

    if(dltotal > 0) g_download_total = (uint64_t)dltotal;

    if(cancel) return 1;

    return 0;
}

int http_download(const char *url, const char *filename, const char *local_dst)
{
    CURL *curl = NULL;
    CURLcode res;
    long http_code = 0;

    FILE *fp = NULL;
    char *full_url = NULL;

    CurlDownload download;

    int ret = HTTP_FAILED;
    int was_cancelled = 0;

    memset(&download, 0, sizeof(download));

    g_download_active = 1;
    g_download_finished = 0;
    g_download_result = HTTP_FAILED;

    g_downloaded_bytes = 0;
    g_download_total = 0;

    snprintf(g_download_name, sizeof(g_download_name), "%s", filename ? filename : "download");

    if(filename && filename[0] != '\0')
    {
        if(asprintf(&full_url, "%s%s", url, filename) < 0) full_url = NULL;
    }
    else full_url = strdup(url);

    if(!full_url)
    {
        printf("Failed to allocate URL\n");
        goto end;
    }

    printf("Downloading (%s) -> (%s)\n", full_url, local_dst);

    fp = fopen(local_dst, "wb");

    if(!fp)
    {
        printf("Failed to open destination: %s\n", local_dst);
        goto end;
    }

    download.fp = fp;

    curl = curl_easy_init();

    if(!curl)
    {
        printf("curl_easy_init failed\n");
        goto end;
    }

    curl_easy_setopt(curl, CURLOPT_URL, full_url);
    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, HTTP_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, CA_CERT_PATH);
    
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Acurl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
    
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Acurl_progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);

    res = curl_easy_perform(curl);
    
    char *ip = NULL;
	CURLcode info_res = curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &ip);

	if(info_res == CURLE_OK && ip) printf("Resolved IP: %s\n", ip);
	else printf("Could not get resolved IP\n");

	if(res != CURLE_OK)
	{
		printf("curl_easy_perform failed: %s\n", curl_easy_strerror(res));
		return 0;
	}
	
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    printf("HTTP status: %ld\n", http_code);
    if(cancel) was_cancelled = 1;
    if(res != CURLE_OK)
    {
        printf("curl_easy_perform failed: %s\n", curl_easy_strerror(res));

        if(download.failed)
            printf("File write failed\n");

        goto end;
    }

    if(http_code < 200 || http_code >= 300)
    {
        printf("HTTP request failed: %ld\n", http_code);
        goto end;
    }

    if(download.failed)
    {
        printf("File write failed\n");
        goto end;
    }

    ret = HTTP_SUCCESS;

end:

    if(fp)
    {
        fclose(fp);
        fp = NULL;
    }

    if(curl) curl_easy_cleanup(curl);

    if(full_url) free(full_url);

    if(was_cancelled || ret != HTTP_SUCCESS)
    {
        unlink(local_dst);
        ret = HTTP_FAILED;
    }

    g_download_result = ret;
    g_download_finished = 1;
    g_download_active = 0;

    cancel = 0;

    return ret;
}

void manifest_parse(void *arg)
{
	int i = 0;
	FILE *fp = fopen(manifest_path, "rb");
	
	if(!fp)
	{
		printf("Couldn't open manifest\n");
		sysThreadExit(0);
	}

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
		strcpy(manifestVersion, cJSON_GetObjectItem(root, "versionX")->valuestring);
		// I added here "versionX", so previous versions find the "version", then after update, search for "versionX"
		printf("JSON VERSION: %s\n", manifestVersion);
		
		cJSON *apps = cJSON_GetObjectItem(root, "apps");
		printf("TEST-OUTPUT-0\n");

		gAppCount = cJSON_GetArraySize(apps);
		printf("TEST-OUTPUT-1\n");

		for(i=0;i<gAppCount;i++)
		{
			cJSON *app = cJSON_GetArrayItem(apps,i);

			cJSON *item = cJSON_GetObjectItem(app,"id");
			if(item && item->valuestring) strcpy(gApps[i].id,item->valuestring);
			else strcpy(gApps[i].id,"id404");
			
			item = cJSON_GetObjectItem(app,"name");
			if(item && item->valuestring) strcpy(gApps[i].name,item->valuestring);
			else strcpy(gApps[i].name,"Undefined");
			
			item = cJSON_GetObjectItem(app,"author");
			if(item && item->valuestring) strcpy(gApps[i].author,item->valuestring);
			else strcpy(gApps[i].author,"Unknown");
			
			item = cJSON_GetObjectItem(app,"version");
			if(item && item->valuestring) strcpy(gApps[i].version,item->valuestring);
			else strcpy(gApps[i].version,"Undetermined");
			
			item = cJSON_GetObjectItem(app,"type");
			if(item && item->valuestring) strcpy(gApps[i].type,item->valuestring);
			else strcpy(gApps[i].type,"game");
			
			item = cJSON_GetObjectItem(app,"pkg");
			if(item && item->valuestring) strcpy(gApps[i].pkg,item->valuestring);
			else strcpy(gApps[i].pkg,"404.pkg");
			
			item = cJSON_GetObjectItem(app,"icon");
			if(item && item->valuestring) strcpy(gApps[i].icon,item->valuestring);
			else strcpy(gApps[i].icon,"icons/placeholder.png");
			
			item = cJSON_GetObjectItem(app,"hidden");
			if(item && item->valuestring) strcpy(gApps[i].hidden_game,item->valuestring);
			else strcpy(gApps[i].hidden_game,"yes");
			
			item = cJSON_GetObjectItem(app,"description");
			if(item && item->valuestring)
				snprintf(gApps[i].description, sizeof(gApps[i].description), "%s", item->valuestring);
			else snprintf(gApps[i].description, sizeof(gApps[i].description), "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur dui nisl, volutpat maximus libero a, bibendum dignissim arcu. Sed sit amet finibus dolor. Fusce maximus, velit at pellentesque laoreet, tellus nulla sodales nisi, non porttitor dolor massa eget dui. Morbi dictum massa sit amet ex porttitor tincidunt. Nullam tempor lacus quis mauris tristique, non pellentesque magna luctus. Suspendisse urna neque, pulvinar a dignissim sed, venenatis tempus massa. Aliquam pretium purus nec odio ullamcorper viverra. Praesent sed ornare lacus.");
			
			item = cJSON_GetObjectItem(app,"short_desc");
			if(item && item->valuestring)
				snprintf(gApps[i].short_desc, sizeof(gApps[i].short_desc), "%s", item->valuestring);
			else snprintf(gApps[i].short_desc, sizeof(gApps[i].short_desc), "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur dui nisl, volutpat maximus libero a, bibendum dignissim arcu. Sed sit amet finibus dolor.");
			
			printf("GAME NAME: %s; ID: %s\n", gApps[i].name, gApps[i].id);
		}
		
		cJSON_Delete(root);
		free(json);
	}
	
	manifest_done = 1;
	
    sysThreadExit(0);
}

void DownloadThread(void *arg)
{
	printf("GET FILE...\n");
	file_downloaded=0;
    int ret = http_download(download_url, download_file, download_dest);
	
	if(ret)
	{
		printf("Something downloaded\n");
		if(!manifest_gather)
		{
			printf("download of pkg completed!\n");
		}
	}
	else printf("failed to download\n");
	
    download_success = ret ? 1 : 0;
	
    if(download_kind != DOWNLOAD_MANIFEST) show_download_dialog = 1;
	
	file_downloaded=1;
	download_finished = 1;
	sysThreadExit(0);
}

int Download_Start(const char *url, const char *file, const char *dest)
{
    if(download_thread != 0) return -1;

    snprintf(download_url, sizeof(download_url), "%s", url);
	snprintf(download_file, sizeof(download_file), "%s", file);
	snprintf(download_dest, sizeof(download_dest), "%s", dest);

    int ret = sysThreadCreate(&download_thread, DownloadThread, NULL, 1000, 0x4000, 0, "DownloadThread");
	printf("Create ret=%08X thread=%llu\n", ret, (unsigned long long)download_thread);
	
	return 0;
}

void IconLoaderThread(void *arg)
{
    int i;

    for(i = 0; i < gAppCount; i++)
    {
        gIcons[i].init = 0;

        printf("Downloading %s...\n", gApps[i].id);

        char path[256];
        snprintf(path, sizeof(path), "/dev_hdd0/game/ACIDSTORE/USRDIR/cache/icons/%s.png", gApps[i].id);

        int ret = http_download(assets_url, gApps[i].icon, path);
		if(ret)
        {
            printf("Loading icon...\n");

            if(!LoadIconPNG(path, &gIcons[i].icon, &gIcons[i].icon_off)) gIcons[i].loaded = 1;
            gIcons[i].init = 1;
        }
    }

    icons_ready = 1;
    icon_thread_running = 0;

    sysThreadExit(0);
}