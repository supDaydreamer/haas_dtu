#include "modbus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


void *socket_main()
{
#if 0
	int sockfd;
	struct sockaddr_in server_addr;
	unsigned char request[12];
	unsigned char response[256];
	int bytes_received;
					    
	// 创建socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("socket creation failed\r\n");
		return -1;
	}

	// 设置服务器地址
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
				    
	// 连接到服务器
	if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("connection failed");
		return -1;
	}
	build_modbus_tcp_frame(request, TRANSACTION_ID, 0x0000, UNIT_ID, 
							0x03, 0x0000, 0x0003); // 读取0x0000开始的3个寄存器
	    
	    // 发送请求
	send(sockfd, request, 12, 0);
		    
		// 接收响应
	bytes_received = recv(sockfd, response, sizeof(response), 0);
	if (bytes_received > 0) {
		printf("Received %d bytes of response:\n", bytes_received);
		for (int i = 0; i < bytes_received; i++) {
			printf("%02X ", response[i]);
		}
		printf("\n");
	}
				    
		// 关闭连接
	close(sockfd);
#endif
	while(1)
	{
		sleep(1);
	}
}


