#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "data.h"
#include "ini.h"
#include "json.h"
#include "tcp_client.h"

#define DEFAULT_TCP_CLIENT_IP   "192.168.5.150"
#define DEFAULT_TCP_CLIENT_PORT 9000
#define TCP_JSON_MAX_LEN        4096
#define TCP_RECONNECT_INTERVAL_S 5

static void load_tcp_client_config(char *ip_buf, size_t ip_buf_size, int *port_out)
{
	const char *cfg_ip = NULL;
	int cfg_port = 0;

	if (!ip_buf || ip_buf_size == 0 || !port_out) {
		return;
	}

	cfg_port = GetIniKeyInt("tcp_client", "port", FILENAME);
	cfg_ip = GetIniKeyString("tcp_client", "ip", FILENAME);

	if (cfg_ip && cfg_ip[0] != '\0' && strcmp(cfg_ip, "0") != 0) {
		snprintf(ip_buf, ip_buf_size, "%s", cfg_ip);
	} else {
		snprintf(ip_buf, ip_buf_size, "%s", DEFAULT_TCP_CLIENT_IP);
	}

	if (cfg_port > 0 && cfg_port <= 65535) {
		*port_out = cfg_port;
	} else {
		*port_out = DEFAULT_TCP_CLIENT_PORT;
	}
}

static int recv_all(int sockfd, void *buf, size_t len)
{
	size_t received = 0;
	uint8_t *p = (uint8_t *)buf;

	while (received < len) {
		ssize_t rc = recv(sockfd, p + received, len - received, 0);
		if (rc == 0) {
			return -1;
		}
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -1;
		}
		received += (size_t)rc;
	}

	return 0;
}

static void log_scan_json(const char *json_buf)
{
	cJSON *root = read_json_str((char *)json_buf);
	if (!root) {
		printf("[TCP-CLIENT] invalid json: %s\n", json_buf);
		return;
	}

	const char *device_no = NULL;
	const char *barcode = NULL;
	const char *status = NULL;
	const char *timestamp = NULL;

	cJSON *device_no_json = cJSON_GetObjectItemCaseSensitive(root, "deviceNo");
	cJSON *barcode_json = cJSON_GetObjectItemCaseSensitive(root, "barcode");
	cJSON *status_json = cJSON_GetObjectItemCaseSensitive(root, "status");
	cJSON *timestamp_json = cJSON_GetObjectItemCaseSensitive(root, "timestamp");

	device_no = cJSON_GetStringValue(device_no_json);
	barcode = cJSON_GetStringValue(barcode_json);
	status = cJSON_GetStringValue(status_json);
	timestamp = cJSON_GetStringValue(timestamp_json);

	printf("[TCP-CLIENT] deviceNo=%s barcode=%s status=%s timestamp=%s\n",
	       device_no ? device_no : "",
	       barcode ? barcode : "",
	       status ? status : "",
	       timestamp ? timestamp : "");

	close_json(root);
}

void *tcp_client_main(void *args)
{
	(void)args;

	for (;;) {
		char ip[64] = {0};
		int port = 0;
		int sockfd = -1;
		struct sockaddr_in addr;

		load_tcp_client_config(ip, sizeof(ip), &port);

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			printf("[TCP-CLIENT] socket error: %s\n", strerror(errno));
			sleep(TCP_RECONNECT_INTERVAL_S);
			continue;
		}

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons((uint16_t)port);
		addr.sin_addr.s_addr = inet_addr(ip);

		printf("[TCP-CLIENT] connecting to %s:%d...\n", ip, port);
		if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			printf("[TCP-CLIENT] connect failed: %s\n", strerror(errno));
			close(sockfd);
			sleep(TCP_RECONNECT_INTERVAL_S);
			continue;
		}

		printf("[TCP-CLIENT] connected\n");

		for (;;) {
			uint8_t len_buf[4];
			uint32_t json_len = 0;
			char *json_buf = NULL;

			if (recv_all(sockfd, len_buf, sizeof(len_buf)) != 0) {
				break;
			}

			json_len = ((uint32_t)len_buf[0] << 24) |
				   ((uint32_t)len_buf[1] << 16) |
				   ((uint32_t)len_buf[2] << 8) |
				   (uint32_t)len_buf[3];

			if (json_len == 0 || json_len > TCP_JSON_MAX_LEN) {
				printf("[TCP-CLIENT] invalid length: %u\n", json_len);
				break;
			}

			json_buf = (char *)malloc((size_t)json_len + 1);
			if (!json_buf) {
				printf("[TCP-CLIENT] alloc failed, len=%u\n", json_len);
				break;
			}

			if (recv_all(sockfd, json_buf, json_len) != 0) {
				free(json_buf);
				break;
			}

			json_buf[json_len] = '\0';
			printf("[TCP-CLIENT] recv: %s\n", json_buf);
			log_scan_json(json_buf);
			free(json_buf);
		}

		printf("[TCP-CLIENT] disconnected\n");
		close(sockfd);
		sleep(TCP_RECONNECT_INTERVAL_S);
	}
}
