#include "modbus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <curl/curl.h>
#include "curl.h"

#include <arpa/inet.h>
#include <sys/socket.h>



#define HTTP_BUF_SIZE (128 * 1024)
#define URL_BUF_SIZE  128


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

static int mbtcp_try_read_1(modbus_t *ctx, int addr, uint16_t *out)
{
	int rc = modbus_read_registers(ctx, addr, 1, out);
	return (rc == 1) ? 0 : -1;
}

static int mbtcp_detect_start(modbus_t *ctx, int desired_start)
{
	uint16_t tmp = 0;

	// 优先尝试用户期望的 start（这里默认为 0）
	if (mbtcp_try_read_1(ctx, desired_start, &tmp) == 0) {
		return desired_start;
	}

	// 常见的模拟器/PLC 使用 1-based 地址（0 读会报 Illegal data address）
	if (mbtcp_try_read_1(ctx, desired_start + 1, &tmp) == 0) {
		return desired_start + 1;
	}

	// 都不行：保持原值（上层会处理失败）
	return desired_start;
}

static int mbtcp_detect_count(modbus_t *ctx, int start, int desired_count)
{
	if (desired_count <= 0) {
		return 0;
	}

	int count = desired_count;
	uint16_t tmp[100];
	if (count > (int)(sizeof(tmp) / sizeof(tmp[0]))) {
		count = (int)(sizeof(tmp) / sizeof(tmp[0]));
	}

	while (count > 0) {
		int rc = modbus_read_registers(ctx, start, count, tmp);
		if (rc == count) {
			return count;
		}
		if (errno != EMBXILADD) {
			// 非地址错误（如超时/断链），不在这里兜底
			return -1;
		}
		count /= 2;
	}
	return 0;
}

static void mbtcp_print_sample(unsigned int round, int start, int count,
                               const uint16_t *before, const uint16_t *after, int mismatch)
{
	int n = count;
	if (n > 3) n = 3;

	printf("[MBTCP] round=%u ok, mismatch=%d, range=HR[%d..%d], sample:",
	       round, mismatch, start, start + count - 1);
	for (int i = 0; i < n; i++) {
		printf(" HR[%d]=%u->%u", start + i, before[i], after[i]);
	}
	printf("\n");
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
	const char *ip = "192.168.0.175";
	const int port = 1502;
	const int unit_id = 1;  // 服务端接受任意 Unit ID，此处固定为 1 便于测试

	const int desired_start = 0;
	const int desired_count = 100;  // 期望轮询 0~99 holding registers

	uint16_t regs[100];
	uint16_t next[100];
	uint16_t verify[100];

	unsigned int round = 0;

	(void)args;

	for (;;) {
		modbus_t *ctx = modbus_new_tcp(ip, port);
		if (!ctx) {
			printf("[MBTCP] modbus_new_tcp failed\n");
			sleep(1);
			continue;
		}

		modbus_set_slave(ctx, unit_id);
		modbus_set_response_timeout(ctx, 2, 0);

		printf("[MBTCP] connecting to %s:%d (unit=%d)...\n", ip, port, unit_id);
		if (modbus_connect(ctx) == -1) {
			printf("[MBTCP] connect failed: %s\n", modbus_strerror(errno));
			modbus_free(ctx);
			sleep(1);
			continue;
		}
		printf("[MBTCP] connected\n");

		int start = mbtcp_detect_start(ctx, desired_start);
		int count = mbtcp_detect_count(ctx, start, desired_count);
		if (count <= 0) {
			if (errno == EMBXILADD) {
				printf("[MBTCP] no valid holding register address (try start=%d or %d)\n",
				       desired_start, desired_start + 1);
			} else {
				printf("[MBTCP] detect range failed: %s\n", modbus_strerror(errno));
			}
			modbus_close(ctx);
			modbus_free(ctx);
			sleep(1);
			continue;
		}
		if (start != desired_start || count != desired_count) {
			printf("[MBTCP] detected range=HR[%d..%d] (requested HR[%d..%d])\n",
			       start, start + count - 1,
			       desired_start, desired_start + desired_count - 1);
		}

		for (;;) {
			round++;

			int rc = modbus_read_registers(ctx, start, count, regs);
			if (rc == -1) {
				printf("[MBTCP] read HR[%d..%d] failed: %s\n",
				       start, start + count - 1, modbus_strerror(errno));
				// 地址错误通常是配置问题，重连无意义：尝试重新探测范围
				if (errno == EMBXILADD) {
					int new_start = mbtcp_detect_start(ctx, desired_start);
					int new_count = mbtcp_detect_count(ctx, new_start, desired_count);
					if (new_count > 0) {
						start = new_start;
						count = new_count;
						printf("[MBTCP] re-detected range=HR[%d..%d]\n",
						       start, start + count - 1);
						sleep(1);
						continue;
					}
				}
				break;
			}

			for (int i = 0; i < count; i++) {
				next[i] = (uint16_t)(regs[i] + 1);  // 溢出回到 0（uint16_t 自然回绕）
			}

			rc = modbus_write_registers(ctx, start, count, next);
			if (rc == -1) {
				// 若服务端不支持 FC=0x10，则降级为逐个 FC=0x06
				if (errno == EMBXILFUN) {
					int ok = 1;
					for (int i = 0; i < count; i++) {
						if (modbus_write_register(ctx, start + i, next[i]) == -1) {
							printf("[MBTCP] write1 HR[%d] failed: %s\n",
							       start + i, modbus_strerror(errno));
							ok = 0;
							break;
						}
					}
					if (!ok) {
						break;
					}
				} else {
					printf("[MBTCP] write HR[%d..%d] failed: %s\n",
					       start, start + count - 1, modbus_strerror(errno));
					break;
				}
			}

			rc = modbus_read_registers(ctx, start, count, verify);
			if (rc == -1) {
				printf("[MBTCP] readback HR[%d..%d] failed: %s\n",
				       start, start + count - 1, modbus_strerror(errno));
				break;
			}

			int mismatch = 0;
			for (int i = 0; i < count; i++) {
				if (verify[i] != next[i]) {
					if (mismatch < 10) {
						printf("[MBTCP] mismatch HR[%d]: expect=%u got=%u (before=%u)\n",
						       start + i, next[i], verify[i], regs[i]);
					}
					mismatch++;
				}
			}

			mbtcp_print_sample(round, start, count, regs, verify, mismatch);

			sleep(1);
		}

		modbus_close(ctx);
		modbus_free(ctx);
		printf("[MBTCP] disconnected, retry in 1s...\n");
		sleep(1);
	}

	return NULL;
}
