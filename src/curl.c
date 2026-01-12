#include "modbus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <curl/curl.h>
#include "curl.h"
#include "ini.h"
#include "data.h"

#include <arpa/inet.h>
#include <sys/socket.h>



#define HTTP_BUF_SIZE (128 * 1024)
#define URL_BUF_SIZE  128

#define DEFAULT_MBTCP_IP   "192.168.5.150"
#define DEFAULT_MBTCP_PORT 502

//#define SERVER_IP "192.168.1.100"  // 服务器IP
//#define SERVER_PORT 502            // Modbus TCP默认端口
//#define TRANSACTION_ID 0x0001      // 事务ID
//#define UNIT_ID 0x01 


static const char *kUrlPrefix = "https://www1erp.dingdan100.com/open/v1/Assign/GetAssignProducDetail?assignProducName="; // 固定URL
//static const char *kUrlPrefix = "https://www.beefindtech.com/?"; // 固定URL
static const char *kHeader1 = "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiI4RDFFRTM3NS0zQkNBLTRGQTAtQUZFRC0wNTE0ODY2N0M0MjIiLCJ1bmlxdWVfbmFtZSI6IjMyMDUwODAwMDA2MTMyMCIsImlhdCI6IjE3NDI1MjI4MTQiLCJuYmYiOiIxNzQyNTIyODE0IiwiZXhwIjoiMjA1ODA1NTYxNCIsImh0dHA6Ly9zY2hlbWFzLm1pY3Jvc29mdC5jb20vd3MvMjAwOC8wNi9pZGVudGl0eS9jbGFpbXMvZXhwaXJhdGlvbiI6IjIwMzUvMy8yMSAxMDowNjo1NCIsImlzcyI6Imphc29uLnpoYW5nQHNvdWhvbi5jb20iLCJhdWQiOiJzb3Vob24tcHJvZHVjdCIsIlViIjoiMTEyODE1NjgiLCJVYmkiOiI3YmM3YTkwMi1jYmQ3LTQwMTAtODBmYy1iMjczMmM3M2JkMjAifQ.lOPX_lgJg0ljd5g3exZjruQWcxyxUcdxLYGjsmob2B0"; // 固定头1
static const char *kHeader2 = "Content-Type: application/json"; //固定头2

struct write_ctx {
	char *ptr;
	size_t left;
};

ProductOder WorkOrder;
unsigned int http_req_f = 0;

static void load_mbtcp_config(char *ip_buf, size_t ip_buf_size, int *port_out)
{
	const char *cfg_ip = NULL;
	int cfg_port = 0;

	if (!ip_buf || ip_buf_size == 0 || !port_out) {
		return;
	}

	cfg_port = GetIniKeyInt("mbtcp", "port", FILENAME);
	cfg_ip = GetIniKeyString("mbtcp", "ip", FILENAME);

	if (cfg_ip && cfg_ip[0] != '\0' && strcmp(cfg_ip, "0") != 0) {
		snprintf(ip_buf, ip_buf_size, "%s", cfg_ip);
	} else {
		snprintf(ip_buf, ip_buf_size, "%s", DEFAULT_MBTCP_IP);
	}

	if (cfg_port > 0 && cfg_port <= 65535) {
		*port_out = cfg_port;
	} else {
		*port_out = DEFAULT_MBTCP_PORT;
	}
}

static int load_mbtcp_poll_interval(void)
{
	int interval = GetIniKeyInt("mbtcp", "mbtcp_poll_interval", FILENAME);

	if (interval <= 0) {
		interval = 5;
	}

	return interval;
}

#if 0

// 构建Modbus TCP请求帧
void build_modbus_tcp_frame(unsigned char *frame, uint16_t transaction_id, 
		                    uint16_t protocol_id, uint8_t unit_id, 
							uint8_t function_code, uint16_t start_address, 
							uint16_t quantity) {
	    frame[0] = (transaction_id >> 8) & 0xFF;       // 事务ID高位
		frame[1] = transaction_id & 0xFF;               // 事务ID低位
		frame[2] = (protocol_id >> 8) & 0xFF;           // 协议ID高位
		frame[3] = protocol_id & 0xFF;                  // 协议ID低位
		frame[4] = 0x00;                                // 长度高位
		frame[5] = 6;                                   // 长度低位（固定为6）
		frame[6] = unit_id;                             // 单元ID
		frame[7] = function_code;                       // 功能码
		frame[8] = (start_address >> 8) & 0xFF;         // 起始地址高位
		frame[9] = start_address & 0xFF;                // 起始地址低位
		frame[10] = (quantity >> 8) & 0xFF;             // 数量高位
		frame[11] = quantity & 0xFF;                    // 数量低位
}
#endif
// 回调写入 static buf
static size_t write_cb(char *data, size_t size, size_t nmemb, void *userdata)
{
	struct write_ctx *ctx = (struct write_ctx *)userdata;
	size_t total = size * nmemb;

	if (ctx->left <= 1)
		return total;  // 缓冲区满则丢弃数据但返回成功

	if (total >= ctx->left)
		total = ctx->left - 1;

	memcpy(ctx->ptr, data, total);
	ctx->ptr  += total;
	ctx->left -= total;
	*(ctx->ptr) = '\0';

	return total;
}

/**
 * get_http(param)
 * 会将 param 拼接到 URL 末尾，例如：
 *   https://a.com?s= + param
 *
 * 返回 static 内容缓冲区指针，失败返回 NULL。
 */
char *get_http(const char *param)
{
	static char url_buf[URL_BUF_SIZE];
	static char buf[HTTP_BUF_SIZE];

	CURL *curl = NULL;
	CURLcode res;
	struct curl_slist *headers = NULL;
	struct write_ctx ctx;

	/* ----------- 拼接 URL ----------- */
	snprintf(url_buf, sizeof(url_buf), "%s%s", kUrlPrefix, param ? param : "");

	buf[0] = '\0';

	curl = curl_easy_init();
	if (!curl)
		return "NULL";

	ctx.ptr = buf;
	ctx.left = sizeof(buf);

	/* 固定请求头 */
	headers = curl_slist_append(headers, kHeader1);
	headers = curl_slist_append(headers, kHeader2);

	/* === 关闭 SSL 证书验证（不太安全）=== */
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* 基本 curl 设置 */
	curl_easy_setopt(curl, CURLOPT_URL, url_buf);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);        // 最大阻塞10秒
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // 可选：跟随跳转

	res = curl_easy_perform(curl);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK)
		return "NULL";
	http_req_f = 0;
	return buf;
}

void *socket_main(void *args)
{
	char ip[64] = {0};
	int port = 0;

	(void)args;

	load_mbtcp_config(ip, sizeof(ip), &port);
	haas_register_map_prepare();

	for (;;) {
		modbus_t *ctx = modbus_new_tcp(ip, port);
		if (!ctx) {
			printf("[MBTCP] modbus_new_tcp failed\n");
			sleep(1);
			continue;
		}

		modbus_set_response_timeout(ctx, 2, 0);

		printf("[MBTCP] connecting to %s:%d...\n", ip, port);
		if (modbus_connect(ctx) == -1) {
			printf("[MBTCP] connect failed: %s\n", modbus_strerror(errno));
			modbus_free(ctx);
			sleep(1);
			continue;
		}
		printf("[MBTCP] connected\n");

		for (;;) {
			if (haas_data_read_tcp(ctx) != 0) {
				break;
			}
			{
				int interval = load_mbtcp_poll_interval();
				sleep(interval);
			}
		}

		modbus_close(ctx);
		modbus_free(ctx);
		printf("[MBTCP] disconnected, retry in 1s...\n");
		sleep(1);
	}

	return NULL;
}

int haas_data_read_tcp(void *ctx)
{
	modbus_t *mb = (modbus_t *)ctx;

	if (!mb) {
		return -1;
	}

	haas_register_map_prepare();

	for (int i = 0; i < g_register_map_count; i++) {
		RegisterMap *map = &g_register_map[i];
		uint16_t reg_buf[REGISTER_VALUE_MAX_BYTES / 2];
		uint8_t bit_buf[REGISTER_VALUE_MAX_BYTES];

		if (!map->enabled) {
			continue;
		}

		modbus_set_slave(mb, map->slave_addr);

		switch (map->cmd) {
		case 0x03: {
			int rc = modbus_read_registers(mb, map->reg_addr, map->data_len, reg_buf);
			if (rc < 0) {
				printf("[MBTCP] read HR failed: dev=0x%02X reg=0x%04X len=%u err=%s\n",
				       map->slave_addr, map->reg_addr, map->data_len, modbus_strerror(errno));
				return -1;
			}
			haas_store_register_values(map->slave_addr, map->reg_addr, map->cmd,
			                           reg_buf, (uint16_t)rc);
			break;
		}
		case 0x04: {
			int rc = modbus_read_input_registers(mb, map->reg_addr, map->data_len, reg_buf);
			if (rc < 0) {
				printf("[MBTCP] read IR failed: dev=0x%02X reg=0x%04X len=%u err=%s\n",
				       map->slave_addr, map->reg_addr, map->data_len, modbus_strerror(errno));
				return -1;
			}
			haas_store_register_values(map->slave_addr, map->reg_addr, map->cmd,
			                           reg_buf, (uint16_t)rc);
			break;
		}
		case 0x01: {
			int rc = modbus_read_bits(mb, map->reg_addr, map->data_len, bit_buf);
			if (rc < 0) {
				printf("[MBTCP] read Coils failed: dev=0x%02X reg=0x%04X len=%u err=%s\n",
				       map->slave_addr, map->reg_addr, map->data_len, modbus_strerror(errno));
				return -1;
			}
			haas_store_bit_values(map->slave_addr, map->reg_addr, map->cmd,
			                      bit_buf, (uint16_t)rc);
			break;
		}
		case 0x02: {
			int rc = modbus_read_input_bits(mb, map->reg_addr, map->data_len, bit_buf);
			if (rc < 0) {
				printf("[MBTCP] read Discrete failed: dev=0x%02X reg=0x%04X len=%u err=%s\n",
				       map->slave_addr, map->reg_addr, map->data_len, modbus_strerror(errno));
				return -1;
			}
			haas_store_bit_values(map->slave_addr, map->reg_addr, map->cmd,
			                      bit_buf, (uint16_t)rc);
			break;
		}
		default:
			printf("[MBTCP] skip unsupported cmd=0x%02X dev=0x%02X reg=0x%04X\n",
			       map->cmd, map->slave_addr, map->reg_addr);
			break;
		}
	}

	return 0;
}
