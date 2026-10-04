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
t_http_pools http_pools;
sys_ppu_thread_t download_thread;
sys_ppu_thread_t icon_thread;

char getBuffer[64*1024];

char download_url[512];
char download_file[256];
char download_dest[256];

char manifest_path[] = "/dev_hdd0/game/ACIDSTORE/USRDIR/cache/manifest.json";
char data_url[] = "http://";
char assets_url[] = "http://acidnt31.w10.site/assets/store/";

volatile int show_download_dialog = 0;
volatile int download_success = 0;
volatile int download_finished = 0;

volatile int icon_thread_running = 0;
volatile int icons_ready = 0;

volatile int download_kind = 0;

int http_init()
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

void http_end()
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
	//escaped_name = escape_filename(filename);
	if(asprintf(&escaped_url, "%s%s", url, filename) < 0 || escaped_url == NULL)
	{
		printf("Failed to allocate URL\n");
		ret = HTTP_FAILED;
		goto end;
	}
	
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
	printf("HTTP length: %llu\n", (unsigned long long)length);
	g_download_total = length;

	ret = httpResponseGetStatusCode(httpTrans, &httpCode);
	if (ret < 0) {
		printf("Error : cellHttpResponseGetStatusCode() HTTP_FAILED (%x)\n", ret);
		ret=HTTP_FAILED;
		goto end;
	}

	if(httpCode != HTTP_STATUS_CODE_OK)
	{
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
	
	int recvRet;

	while(1)
	{
		recvRet = httpRecvResponse(httpTrans, getBuffer, sizeof(getBuffer), &nRecv);
		if(recvRet < 0)
		{
			printf("httpRecvResponse failed: %08X\n", recvRet);
			ret = HTTP_FAILED;
			goto end;
		}
		
		if(nRecv == 0) break;
		
		size_t written = fwrite(getBuffer, 1, nRecv, fp);

		if(written != nRecv)
		{
			printf("fwrite failed: wrote %u / %u bytes\n", (unsigned)written, (unsigned)nRecv);
			ret = HTTP_FAILED;
			goto end;
		}

		g_downloaded_bytes += written;

		if(cancel) break;
	}
	
	if(fp)
	{
		fclose(fp);
		fp = NULL;
	}
	
	printf("Downloaded:  %llu\n", (unsigned long long)g_downloaded_bytes);
	printf("Difference:  %lld\n", (long long)g_downloaded_bytes - (long long)length);
	
	if(cancel)
	{
		unlink((char*)local_dst);
		ret=HTTP_FAILED;
		cancel=0;
		g_download_result = ret;
		g_download_finished = 1;
		g_download_active = 0;
	}

	struct stat st;

	if(stat(local_dst, &st) == 0)
	{
		printf("Actual file size: %lld\n", (long long)st.st_size);
	}
	
	ret = HTTP_SUCCESS;
	
	g_download_result = ret;
	g_download_finished = 1;
	g_download_active = 0;

	end:
	{
		if(fp)
		{
			fclose(fp);
			fp = NULL;
		}
		
		if(ret != HTTP_SUCCESS) unlink(local_dst);
		
		if(httpTrans) httpDestroyTransaction(httpTrans);
		if(httpClient) httpDestroyClient(httpClient);
		if(uri_pool) free(uri_pool);
		if(escaped_url) free(escaped_url);
		if(escaped_name) free(escaped_name);
		
		g_download_result = ret;
		g_download_finished = 1;
		g_download_active = 0;

		return ret;
	}
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
		strcpy(manifestVersion, cJSON_GetObjectItem(root, "version")->valuestring);
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
    int ret = http_download(download_url, download_file, download_dest);
	
	if(ret)
	{
		printf("Something downloaded\n");
		if(!manifest_gather)
		{
			printf("download of pkg completed!\n");
		}
	}
	else
	{
		printf("failed to download\n");
	}
	
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