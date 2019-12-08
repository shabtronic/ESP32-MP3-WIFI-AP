/* Control with a touch pad playing MP3 files from SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_spiram.h"
#include "nvs_flash.h"
#include "esp_audio.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "esp_decoder.h"
#include "mp3_decoder.h"
#include "flac_decoder.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_touch.h"
#include "audio_hal.h"
#include "board.h"
#include "periph_button.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include <pthread.h>
#include <string>
#include <vector>
#include <algorithm>
#include <stdarg.h> 
#include "esp_pm.h"
#include <esp_http_server.h>
volatile static int SpinLock = 0;



// Profile fun! microseconds to seconds
double GetTime() { return (double)esp_timer_get_time() / 1000000; }

#define str std::string
#define vec std::vector
static int file_index = 0;
vec<str> Mp3Files;

static const char *TAG = "Aud1oW1f1AP";
// A1S has 2 GPIO Red LEDs
#define BLINK_GPIO_GREEN 22
#define BLINK_GPIO_RED 19



// Some string helpers
str upper(str &s) { for (int a = 0; a < s.length(); a++)		s[a] = toupper(s[a]);	return s; }
str lower(str &s) { for (int a = 0; a < s.length(); a++)		s[a] = tolower(s[a]);	return s; }
bool fileext(str &s, str e) { for (int b = s.length() - 1, a = e.length() - 1; a >= 0 && b >= 0; a--, b--)		if (tolower(e[a]) != tolower(s[b])) return false;	return true; }
str trim(str sstr) { sstr.erase(0, sstr.find_first_not_of(' '));	sstr.erase(sstr.find_last_not_of(' ') + 1);	return sstr; }
str FileNameOnly(str &s) { str x = "";	for (int a = s.length() - 1; a >= 0; a--) { if (s[a] == '/') return x;		x = s[a] + x; }	return x; }
bool contains(str &s, char* l[], int len)
	{
	for (int a = 0; a < len; a++)
		if (s.find(l[a]) != str::basic_string::npos) return true;
	return false;
	}
extern "C" {
	void app_main(void);
	}

#define CURRENT 0
#define NEXT    1

FRESULT scan_files(str path, vec<str> &list, bool recurse, bool filesonly);


std::string string_format(const std::string fmt, ...)
	{
	int size = ((int)fmt.size()) * 2 + 50;
	str sstr;
	va_list ap;
	while (1) {
		sstr.resize(size);
		va_start(ap, fmt);
		int n = vsnprintf((char *)sstr.data(), size, fmt.c_str(), ap);
		va_end(ap);
		if (n > -1 && n < size) {
			sstr.resize(n);
			return sstr;
			}
		if (n > -1)
			size = n + 1;
		else
			size *= 2;
		}
	return sstr;
	}

str ServerTime()
	{
	return string_format("%2.1f", (float)esp_log_timestamp() / 1000);
	}

// WIFI Event Handler
static esp_err_t event_handler(void *ctx, system_event_t *event)
	{
	switch (event->event_id)
		{
		case SYSTEM_EVENT_STA_START:
			ESP_ERROR_CHECK(esp_wifi_connect());
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			esp_wifi_connect();
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			break;
		case SYSTEM_EVENT_AP_STACONNECTED:
			break;
		case SYSTEM_EVENT_AP_STADISCONNECTED:
			esp_wifi_deauth_sta(0);
			break;
		default:
			break;
		}
	return ESP_OK;
	}
// decode %20 etc in url strings
str urlDecode(str &SRC)
	{
	str ret;
	char ch;
	int i, ii;
	for (i = 0; i < SRC.length(); i++)
		{
		if (int(SRC[i]) == 37) {
			sscanf(SRC.substr(i + 1, 2).c_str(), "%x", &ii);
			ch = static_cast<char>(ii);
			ret += ch;
			i = i + 2;
			}
		else {
			ret += SRC[i];
			}
		}
	return (ret);
	}


#define MSTR(xx) #xx


str GenerateSDCardDir(str dir)
	{
	return "";
	}

bool mycomp(str a, str b) {
	//returns 1 if string a is alphabetically 
	//less than string b
	//quite similar to strcmp operation
	return a < b;
	}

str GenerateSDCardFiles(str dir)
	{
	vec<str> filelist;
	dir = trim(dir);
	scan_files(trim(dir.substr(8, dir.length() - 8)), filelist, false, false);
	sort(filelist.begin(), filelist.end(), mycomp);
	str s;
	s = "<!DOCTYPE html><HTML><HEAD><TITLE>ESP-Vortex AP HTTP Server Test V.100</TITLE></HEAD>";
	s += "<FONT face=\"verdana\">";
	s += "<BODY text=\"B0B0B0\" BGCOLOR = \"202020\" >";
	s += "<H2><a style=\"color:#FFA500;\"> SDCard:" + dir.substr(8, dir.length() - 8) + "/ </a><br/> ";
	s += string_format(" %d Files:<br/><br/>", filelist.size());
	str files;
	str dirs;
	for (int a = 0; a < filelist.size(); a++)
		{
		if (fileext(filelist[a], ".png") || fileext(filelist[a], ".jpg") || fileext(filelist[a], ".jpeg"))
			{
			files += "<img src = \"" + filelist[a] + "\" alt = \"Origin Unknown....\"><br/>\n";
			}
		else if (fileext(filelist[a], ".flac") || fileext(filelist[a], ".mp3") || fileext(filelist[a], ".wma"))
			{
			files += "<audio preload = \"none\" src=\"" + filelist[a] + "\" controls></audio>\n";
			files += "<a href=\"" + filelist[a] + "\" style=\"color:#fFff40;\" >" + FileNameOnly(filelist[a]) + "</a><br/><br/>\n";
			}
		else if (fileext(filelist[a], ".mp4") || fileext(filelist[a], ".webm"))
			{
			files += "<video poster=\"/html/movieicon.jpg\" preload=\"none\"  src=\"" + filelist[a] + "\" controls>.mp4 not supported</video><br/>";
			files += "<a href=\"" + filelist[a] + "\" style=\"color:#fFff40;\" >" + FileNameOnly(filelist[a]) + "</a><br/><br/>";
			}
		else
			dirs += "<a href=\"/sdcard/" + filelist[a] + "\" style=\"color:#4040FF;\" >" + FileNameOnly(filelist[a]) + "</a><br/>\n";
		}
	s += "<br/>" + dirs + "<br/><br/>" + files;
	s += "</H2></BODY>";
	s += "</FONT>";
	s += "</HTML>";

	return s;
	}



void WiFIInit()
	{
	//   wifi_config_t ap_config = {.ap = {.ssid = "ESP-Vortex", .password = "password", .ssid_len = 0,.authmode = WIFI_AUTH_WPA2_PSK,.ssid_hidden = 0,.max_connection = 2, .beacon_interval = 150 }};
	wifi_config_t ap_config;
	strcpy((char*)ap_config.ap.ssid, "ESP-Vortex");
	strcpy((char*)ap_config.ap.password, "");
	ap_config.ap.ssid_len = strlen((const char*)ap_config.ap.ssid);
	ap_config.ap.authmode = WIFI_AUTH_OPEN;
	ap_config.ap.ssid_hidden = 0;
	ap_config.ap.max_connection = 2;
	ap_config.ap.beacon_interval = 150;

	tcpip_adapter_init();
	tcpip_adapter_ip_info_t info;// = { 0, };
	IP4_ADDR(&info.ip, 1, 2, 3, 4);
	IP4_ADDR(&info.gw, 1, 2, 3, 4);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_wifi_set_mode(WIFI_MODE_AP);
	esp_wifi_set_config(WIFI_IF_AP, &ap_config);
	//ESP_ERROR_CHECK(esp_wifi_set_storage());
	}

// Switch HTTP Server on or off





#define Mp3StackSize 256
int Mp3Stack[Mp3StackSize];
int Mp3StackPos = 0;
FRESULT scan_files(str path, vec<str> &list, bool recurse, bool filesonly = true)
	{
	FRESULT res;
	FF_DIR dir;
	//UINT i;
	static FILINFO fno;
	res = (FRESULT)f_opendir(&dir, path.c_str());                       /* Open the directory */
	if (res == FR_OK)
		{
		while (1)
			{
			res = (FRESULT)f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) break;
			for (int a = 0; a < strlen(fno.fname); a++)
				fno.fname[a] = tolower(fno.fname[a]);
			fno.fname[0] = toupper(fno.fname[0]);
			//if (!(fno.fattrib & AM_HID) && !(fno.fattrib & AM_SYS))
			if (!(fno.fattrib & AM_SYS))
				{
				if (fno.fattrib & AM_DIR)
					{
					if (!filesonly)
						list.push_back(str(path + str("/") + fno.fname).c_str());

					if (recurse)
						if (scan_files(path + "/" + fno.fname, Mp3Files, true) != FR_OK) break;
					}
				else
					{
					if (strstr((fno.fname), ".flac") || strstr((fno.fname), ".mp3") || !filesonly)
						{
						if (strstr((fno.fname), ".png") || strstr((fno.fname), ".jpg") || strstr((fno.fname), ".jpeg"))
							list.insert(list.begin(), str(path + str("/") + fno.fname).c_str());
						else
							list.push_back(str(path + str("/") + fno.fname).c_str());
						}
					}
				}
			}
		f_closedir(&dir);
		}

	return res;
	}
void enumDirs()
	{
	float starttime = GetTime();
	scan_files("", Mp3Files, true);
	starttime = (float)(GetTime() - starttime);
	printf("Scan time %2.1f secs\n", starttime);
	printf("Audio file count = %d\n", Mp3Files.size());
	}





void SendFile(httpd_req_t *req, str filename)
	{
	httpd_resp_set_type(req, HTTPD_TYPE_OCTET);
	const int psize = 0x10000; // 64kb blocks 
	char* buf = (char*)malloc(psize);
	FILE* F = fopen(filename.c_str(), "rb");
	if (F)
		{
		fseek(F, 0, SEEK_END);
		int fsize = ftell(F);
		fseek(F, 0, SEEK_SET);
		while (fsize > 0)
			{
			if (fsize > psize)
				{
				fread(buf,  psize,1, F);
				if (httpd_resp_send_chunk(req, buf, psize) != ESP_OK)
					fsize = 0;
				else
					fsize -= psize;
				}
			else
				{
				fread(buf,  fsize,1, F);
				httpd_resp_send_chunk(req, buf, fsize);
				fsize = 0;
				}
			vTaskDelay(0);
			}
		fclose(F);
		}
	httpd_resp_send_chunk(req, buf, 0);
	free(buf);
	}

static esp_err_t MainURLhandler(httpd_req_t *req)
	{
	str url = str(req->uri);
	url = trim(urlDecode(url));
	str test = "[" + str(req->uri) + "]";
	char* ff[] = { ".flac",".mp3",".jpg",".jpeg",".webm",".mp4" };
	if (contains(url, ff, 6))
		{
		SendFile(req, "/sdcard" + url);
		}

	if (url.find("/sdcard/") != std::string::npos)
		{
		test = GenerateSDCardFiles(url);
		httpd_resp_send(req, test.c_str(), test.length());
		}

	if (url == "/")
		{
		test = "<!DOCTYPE html><html>";
		test += "<frameset rows = \"120,*,45\"  RAMEBORDER=\"0\" FRAMEBORDER=\"NO\" BORDER=\"0\" BORDER = \"NO\" FRAMESPACING = \"0\" >";
		test += "<frame  src = \"header.html\" scrolling=\"no\">";
		test += "<frameset cols = \"90,*\" ><frame src = \"blank.html\"><frame src = \"info.html\"></frameset><frame src= \"footer.html\" scrolling=\"no\"></frameset></html>";
		httpd_resp_send(req, test.c_str(), test.length());
		}

	if (url == "/blank.html")
		{
		test = "<!DOCTYPE html><HTML><HEAD><TITLE>ESP - Vortex AP HTTP Server Test V.100</TITLE></HEAD>";
		test += "<FONT face=\"verdana\"><BODY text=\"B0B0B0\" BGCOLOR = \"050520\" >";
		test += "<H2><p><font size=\"+20\"></font></H2>";
		test += "</BODY></FONT></HTML>";
		httpd_resp_send(req, test.c_str(), test.length());
		}


	if (url == "/header.html")
		{
		test = "<!DOCTYPE html><HTML><HEAD><TITLE>ESP - Vortex AP HTTP Server Test V.100</TITLE></HEAD>";
		test += "<FONT face=\"verdana\"><BODY text=\"B0B0B0\" BGCOLOR = \"050520\" >";
		test += "<H2><p><img  src=\"html/ZJIcon_90x90.jpg\" align=\"middle\"><font size=\"+20\">ESP-Vortex AP Server</font></H2>";
		test += "</BODY></FONT></HTML>";
		httpd_resp_send(req, test.c_str(), test.length());
		}

	if (url == "/footer.html")
		{
		test = "<!DOCTYPE html><HTML><FONT face=\"verdana\"><BODY text=\"B0B0B0\" BGCOLOR = \"050520\" >";
		test += "<H2>[Server UpTime: " + ServerTime() + "]</H2></BODY></FONT></HTML>";
		httpd_resp_send(req, test.c_str(), test.length());
		}

	if (url == "/info.html")
		{
		const char* clockspeeds[] = { "xtal","80","160","240" ,"<2" };
		FATFS *fs;
		DWORD fre_clust, fre_sect, tot_sect;
		f_getfree("/sdcard/", &fre_clust, &fs);
		tot_sect = (fs->n_fatent - 2) * fs->csize;
		fre_sect = fre_clust * fs->csize;
		long long tmp_total_bytes = (long long)tot_sect * FF_SS_SDCARD / (1024 * 1024);
		long long tmp_free_bytes = (long long)fre_sect * FF_SS_SDCARD / (1024 * 1024);
		test = "<!DOCTYPE html><HTML><HEAD><TITLE>ESP - Vortex AP HTTP Server Test V.100</TITLE></HEAD>";
		test += "<FONT face=\"verdana\"><BODY text=\"B0B0B0\" BGCOLOR = \"202020\" >";
		test += "<H2> <a href=\"/sdcard/\">Browse SDCard</a><br/><br/><a href=\"/SDKDevKit.html\">SDK Development Instructions</a><br/><br/>";
		test += "System Info:<P> Currently Playing:  ";
		test += "<a style=\"color:#FFA500;\" href=\"" + Mp3Files[file_index] + "\">" + Mp3Files[file_index] + "</a>";
		test += "<P> CPU speed: " + str(clockspeeds[rtc_clk_cpu_freq_get()]) + "mhz";
		test += "<P> SDCard Total space: " + string_format("%dmb", tmp_total_bytes) + " Free space : " + string_format("%dmb", tmp_free_bytes);
		test += "<P> Heap Memory Free: " + string_format("%d", esp_get_free_heap_size()) + " 8Bit: " + string_format("%d", heap_caps_get_free_size(MALLOC_CAP_8BIT)) + " 32Bit: " + string_format("%d", heap_caps_get_free_size(MALLOC_CAP_32BIT));
		test += "</H2></BODY></FONT></HTML>";
		httpd_resp_send(req, test.c_str(), test.length());
		}
	return ESP_OK;
	}

static const httpd_uri_t hello = {
	.uri = "/*",
	.method = HTTP_GET,
	.handler = MainURLhandler,
	};





static httpd_handle_t start_webserver(void)
	{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_start(&server, &config) ;
	httpd_register_uri_handler(server, &hello);
	return server;
	}

static void stop_webserver(httpd_handle_t server)
	{
	httpd_stop(server);
	}

static httpd_handle_t server = NULL;

void WifiAPServer(audio_pipeline_handle_t &pipeline)
	{
	static bool WifiRunning = false;
	if (!WifiRunning)
		{
		esp_wifi_start();
		//	esp_wifi_set_max_tx_power(82);
		//	esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, WIFI_BW_HT40);
		gpio_set_level((gpio_num_t)BLINK_GPIO_GREEN, (gpio_num_t)0);
		server = start_webserver();
		}
	else
		{
		stop_webserver(server);
		esp_wifi_stop();
		gpio_set_level((gpio_num_t)BLINK_GPIO_GREEN, (gpio_num_t)1);
		}
	WifiRunning = !WifiRunning;
	}



static FILE *get_file(int next_file)
	{
	static FILE *file = NULL;


	if (next_file == NEXT)
		{
		if (file != NULL)
			{
			fclose(file);
			file = NULL;
			}
		}
	if (file == NULL)
		{
		static int LastPos = -1;
		if (LastPos == Mp3StackPos)
			Mp3StackPos = (Mp3StackPos + 1) & (Mp3StackSize - 1);

		file_index = Mp3Stack[Mp3StackPos];
		printf("<<<< %s >>>>\n", Mp3Files[file_index].c_str());
		file = fopen(("/sdcard" + Mp3Files[file_index]).c_str(), "r");
		LastPos = Mp3StackPos;
		if (!file)
			{
			printf("Error opening file\n");
			return NULL;
			}
		}
	return file;
	}


/*
 * Callback function to feed audio data stream from sdcard to mp3 decoder element
 */
audio_pipeline_handle_t pipeline;
audio_element_handle_t audio_decoder, flac_decoder, mp3_decoder, i2s_stream_writer;
audio_event_iface_handle_t evt;

audio_element_err_t my_sdcard_read_cb(audio_element_handle_t el, char *buf, int len, unsigned int wait_time, void *ctx)
	{

	int read_len = fread(buf, 1, len, get_file(CURRENT));
	if (read_len == 0)
		{
		audio_pipeline_stop(pipeline);
		read_len = len;
		//read_len = AEL_IO_DONE;
		}
	return (audio_element_err_t)read_len;
	}

// You can get pcm data in write callback.
audio_element_err_t i2s_input_write_cb(audio_element_handle_t el, char *buf, int len, unsigned int wait_time, void *ctx)
	{
	printf("Stream write cb len: %d\n", len);
	return (audio_element_err_t)len;
	}


void SetDecoder(audio_pipeline_handle_t &pipeline)
	{

	if (audio_decoder == mp3_decoder)
		audio_pipeline_breakup_elements(pipeline, mp3_decoder);
	if (audio_decoder == flac_decoder)
		audio_pipeline_breakup_elements(pipeline, flac_decoder);

	if (strstr(Mp3Files[Mp3Stack[Mp3StackPos]].c_str(), (const char*)".mp3"))
		{
		//	printf("Changing Decoder to mp3.\n");
		audio_decoder = mp3_decoder;
		//		audio_element_reset_state(audio_decoder);
				//audio_element_reset_state(i2s_stream_writer);

		const char * boomer[]{ "mp3Decoder", "i2s" };
		audio_pipeline_relink(pipeline, boomer, 2);
		audio_pipeline_set_listener(pipeline, evt);
		//	printf("Finished decoder change!\n");

		}
	else
		{
		//	printf("Changing Decoder to flac.\n");
		audio_decoder = flac_decoder;
		//	audio_element_force_set_state(audio_decoder, AEL_STATE_PAUSED)
		//audio_element_reset_state(audio_decoder);

		const char * boomer[]{ "flacDecoder", "i2s" };
		audio_pipeline_relink(pipeline, boomer, 2);
		audio_pipeline_set_listener(pipeline, evt);
		//printf("Finished decoder change!\n");
		}

	//audio_pipeline_reset_items_state(pipeline);
//	audio_element_reset_state(audio_decoder);
//	audio_element_reset_state(i2s_stream_writer);
	audio_element_run(audio_decoder);

	//audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
	}



void app_main(void)
	{
	printf("App Main\n");
	vec<str> stringlist;
	nvs_flash_init();
	printf("Wifi Init\n");
	WiFIInit();





	esp_log_level_set("*", ESP_LOG_NONE);
	esp_log_level_set(TAG, ESP_LOG_INFO);








	printf("Audio Board Init\n");
	audio_board_handle_t board_handle = audio_board_init();



	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);



	int player_volume;
	audio_hal_get_volume(board_handle->audio_hal, &player_volume);

	//  ESP_LOGI(TAG, "[3.0] Create audio pipeline for playback");
	audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	pipeline = audio_pipeline_init(&pipeline_cfg);
	mem_assert(pipeline);

	//  ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip");

	i2s_stream_cfg_t i2s_cfg;

	i2s_cfg.type = AUDIO_STREAM_WRITER;// 
	i2s_cfg.task_prio = I2S_STREAM_TASK_PRIO;//
	i2s_cfg.task_core = I2S_STREAM_TASK_CORE;//
	i2s_cfg.task_stack = I2S_STREAM_TASK_STACK;//
	i2s_cfg.out_rb_size = I2S_STREAM_RINGBUFFER_SIZE;//
	i2s_cfg.i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);//
	i2s_cfg.i2s_config.sample_rate = 44100;//
	i2s_cfg.i2s_config.bits_per_sample = (i2s_bits_per_sample_t)24;//
	i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;//
	i2s_cfg.i2s_config.communication_format = I2S_COMM_FORMAT_I2S;//
	i2s_cfg.i2s_config.dma_buf_count = 3;//
	i2s_cfg.i2s_config.dma_buf_len = 300;//
	i2s_cfg.i2s_config.use_apll = 1;//
	i2s_cfg.i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2;//
	i2s_cfg.i2s_port = (i2s_port_t)0;
	i2s_cfg.use_alc = false;
	i2s_cfg.volume = 0;
	i2s_cfg.multi_out_num = 0;


	i2s_cfg.i2s_port = (i2s_port_t)0;//
	//i2s_stream_cfg_t i2s_cfg_read = i2s_cfg;

	//i2s_cfg.type = AUDIO_STREAM_WRITER;
	i2s_stream_writer = i2s_stream_init(&i2s_cfg);

	//	i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	//	i2s_cfg.type = AUDIO_STREAM_WRITER;
	//	i2s_stream_writer = i2s_stream_init(&i2s_cfg);


		//i2s_cfg_read.type = AUDIO_STREAM_WRITER;
		//MyDSP = i2s_stream_init(&i2s_cfg_read);
		//audio_element_set_write_cb(MyDSP, i2s_input_write_cb, 0);
	printf("Mp3 Codec init\n");
	mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
	flac_decoder_cfg_t flac_cfg = DEFAULT_FLAC_DECODER_CONFIG();
	mp3_cfg.task_prio = 10;
	mp3_cfg.out_rb_size = (32 * 1024);
	flac_cfg.task_prio = 10;
	flac_cfg.out_rb_size = (32 * 1024);
	mp3_decoder = mp3_decoder_init(&mp3_cfg);
	flac_decoder = flac_decoder_init(&flac_cfg);
	/*
		audio_decoder_t auto_decode[2];

		auto_decode[0].decoder_open = (void* (*)(audio_element_handle_t)) mp3_decoder_open;
		auto_decode[0].decoder_process = (void* (*)(audio_element_handle_t))mp3_decoder_process;
		auto_decode[0].decoder_close = (void* (*)(audio_element_handle_t))mp3_decoder_close;
		auto_decode[0].decoder_type = ESP_CODEC_TYPE_MP3;
		auto_decode[1].decoder_open = (void* (*)(audio_element_handle_t))flac_decoder_open;
		auto_decode[1].decoder_process = (void* (*)(audio_element_handle_t))flac_decoder_process;
		auto_decode[1].decoder_close = (void* (*)(audio_element_handle_t))flac_decoder_close;
		auto_decode[1].decoder_type = ESP_CODEC_TYPE_RAWFLAC;
		*/
	printf("Audio element Callbacks\n");
	audio_element_set_read_cb(mp3_decoder, my_sdcard_read_cb, 0);
	audio_element_set_read_cb(flac_decoder, my_sdcard_read_cb, 0);
	printf("Audio pipeline\n");

	audio_pipeline_register(pipeline, mp3_decoder, "mp3Decoder");
	audio_pipeline_register(pipeline, flac_decoder, "flacDecoder");
	audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");


	// SDCard Init

	esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
	periph_sdcard_cfg_t sdcard_cfg;
	sdcard_cfg.root = "/sdcard";
	sdcard_cfg.card_detect_pin = get_sdcard_intr_gpio();
	esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
	esp_periph_start(set, sdcard_handle);
	while (!periph_sdcard_is_mounted(sdcard_handle))
		vTaskDelay(100 / portTICK_PERIOD_MS);

	audio_board_key_init(set);

	printf("Enum Dirs\n");
	enumDirs();
	for (int a = 0; a < Mp3StackSize; a++)
		Mp3Stack[a] = esp_random() % Mp3Files.size();
	Mp3StackPos = 0;
	file_index = Mp3Stack[Mp3StackPos];


	audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
	evt = audio_event_iface_init(&evt_cfg);
	audio_pipeline_set_listener(pipeline, evt);
	audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

	printf("The Amazing Wifi AP MP3 Player!\n");
	//printf("V1.00\n");
	SetDecoder(pipeline);

	audio_pipeline_run(pipeline);
	player_volume = 5;
	audio_hal_set_volume(board_handle->audio_hal, player_volume);

	gpio_pad_select_gpio(BLINK_GPIO_GREEN);
	/* Set the GPIO as a push/pull output */
	gpio_set_direction((gpio_num_t)BLINK_GPIO_GREEN, (gpio_mode_t)GPIO_MODE_OUTPUT);
	gpio_set_level((gpio_num_t)BLINK_GPIO_GREEN, (gpio_num_t)1);
	bool AllowChange = false;
	while (1)
		{
		/* Handle event interface messages from pipeline
		   to set music info and to advance to the next song
		*/
		//vTaskDelay(10);
		audio_event_iface_msg_t msg;
		esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);

		if (ret != ESP_OK)
			{
			printf("Error Event interface!\n");
			continue;
			}
		//printf("Main Loop..  Msg Source: %d  Msg Cmd: %d\n",  (int)msg.source, (int)msg.cmd);

		if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT)
			{

			str device = "unknown";
			if ((int)msg.source == (int)mp3_decoder)
				device = "mp3_decoder";
			if ((int)msg.source == (int)flac_decoder)
				device = "flac_decoder";
			if ((int)msg.source == (int)i2s_stream_writer)
				device = "i2s_stream_writer";

			//	printf("Main Loop.. %s Msg Source: %d  Msg Cmd: %d\n",device.c_str(), (int)msg.source, (int)msg.cmd);

			if (msg.source == (void *)audio_decoder && msg.cmd == AEL_MSG_CMD_REPORT_STATUS)
				{
				AllowChange = true;
				}
			// Set music info for a new song to be played
			if (msg.source == (void *)audio_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
				{
				audio_element_info_t music_info;// = { 0 };
				memset(&music_info, 0, sizeof(music_info));
				audio_element_getinfo(audio_decoder, &music_info);
				audio_element_setinfo(i2s_stream_writer, &music_info);
				//static bool done = false;
				audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
				if (music_info.bits == 24)
					{
					printf("Can't handle 24bits!\n");
					audio_pipeline_stop(pipeline);
					}
				else
					if (el_state != 3)
						{
						i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
						//printf("I2S Stream Changed! Sample Rate: %d Bits: %d Channels: %d Bytes: %lld \n", music_info.sample_rates, music_info.bits, music_info.channels, music_info.total_bytes);

						//done = true;
						}
				continue;
				}
#define ERRCHECK(xxx) if (xxx==ESP_FAIL) {printf("Error %s Line:%d\n",#xxx,__LINE__);}
			// Advance to the next song when previous finishes
			if (msg.source == (void *)i2s_stream_writer && msg.cmd == AEL_MSG_CMD_REPORT_STATUS)
				{
				//	printf("Streamwriter cmd_report_status\n");

				audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
				//	printf("el_state %d\n", el_state);
				if (el_state == AEL_STATE_STOPPED || el_state == AEL_STATE_FINISHED)
					{
					//	printf("Pausing Pipeline\n");
					audio_pipeline_pause(pipeline);
					get_file(NEXT);
					SetDecoder(pipeline);
					//	printf("Starting Pipeline\n");
						//	if (el_state != AEL_STATE_FINISHED)
					(audio_pipeline_run(pipeline));
					(audio_pipeline_resume(pipeline));
					AllowChange = false;

					}
				continue;
				}
			}
		if (msg.cmd == PERIPH_BUTTON_LONG_PRESSED && msg.source_type == PERIPH_ID_BUTTON)
			{
			WifiAPServer(pipeline);
			}
		if (AllowChange)
			if (msg.cmd == PERIPH_BUTTON_RELEASE && msg.source_type == PERIPH_ID_BUTTON)
				{
				if ((int)msg.data == get_input_set_id())
					{
					Mp3StackPos = (Mp3StackPos - 1) & (Mp3StackSize - 1);
					audio_pipeline_stop(pipeline);

					}
				if ((int)msg.data == get_input_play_id())
					{
					Mp3StackPos = (Mp3StackPos + 1) & (Mp3StackSize - 1);
					audio_pipeline_stop(pipeline);

					}
				if ((int)msg.data == get_input_rec_id())
					{

					static bool Pause = false;
					Pause = !Pause;
					if (Pause)
						audio_pipeline_pause(pipeline);
					else
						audio_pipeline_resume(pipeline);
					}

				if ((int)msg.data == get_input_mode_id())
					{
					Mp3StackPos = (Mp3StackPos + 1) & (Mp3StackSize - 1);
					Mp3Stack[Mp3StackPos] = esp_random() % Mp3Files.size();
					audio_pipeline_stop(pipeline);
					}

				if ((int)msg.data == get_input_volup_id())
					{
					player_volume += 1;
					if (player_volume > 100)
						player_volume = 100;
					audio_hal_set_volume(board_handle->audio_hal, player_volume);
					}
				if ((int)msg.data == get_input_voldown_id())
					{
					player_volume -= 1;
					if (player_volume < 0)
						player_volume = 0;
					audio_hal_set_volume(board_handle->audio_hal, player_volume);
					}
				}
		}

	// ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
	audio_pipeline_terminate(pipeline);

	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(pipeline);

	/* Stop all peripherals before removing the listener */
	//esp_periph_stop_all();
	//audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

	/* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
	audio_event_iface_destroy(evt);

	/* Release all resources */
	audio_pipeline_deinit(pipeline);
	audio_element_deinit(i2s_stream_writer);
	audio_element_deinit(audio_decoder);
	//esp_periph_destroy();
	}
