#include "modbus.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "data.h"
#include "ini.h"
#include "mbtcp_server.h"

#define DEFAULT_MBTCP_SERVER_PORT 502
#define DEFAULT_MBTCP_SERVER_IP   "0.0.0.0"
#define DEFAULT_MBTCP_SERVER_ENABLE 0
#define MBTCP_SERVER_MAX_REGISTERS 200
#define MBTCP_SERVER_UNIT_ID 1

static void load_mbtcp_server_config(char *ip_buf, size_t ip_buf_size,
                                     int *port_out, int *enable_out)
{
	const char *cfg_ip = NULL;
	int cfg_port = 0;
	int cfg_enable = 0;

	if (!ip_buf || ip_buf_size == 0 || !port_out || !enable_out) {
		return;
	}

	cfg_enable = GetIniKeyInt("mbtcp", "server_enable", FILENAME);
	cfg_port = GetIniKeyInt("mbtcp", "server_port", FILENAME);
	cfg_ip = GetIniKeyString("mbtcp", "server_ip", FILENAME);

	if (cfg_ip && cfg_ip[0] != '\0' && strcmp(cfg_ip, "0") != 0) {
		snprintf(ip_buf, ip_buf_size, "%s", cfg_ip);
	} else {
		snprintf(ip_buf, ip_buf_size, "%s", DEFAULT_MBTCP_SERVER_IP);
	}

	if (cfg_port > 0 && cfg_port <= 65535) {
		*port_out = cfg_port;
	} else {
		*port_out = DEFAULT_MBTCP_SERVER_PORT;
	}

	*enable_out = (cfg_enable != 0) ? 1 : DEFAULT_MBTCP_SERVER_ENABLE;
}

static void init_fixed_registers(modbus_mapping_t *mb_map)
{
	int i = 0;

	if (!mb_map || !mb_map->tab_registers) {
		return;
	}

	for (i = 0; i < 10 && i < MBTCP_SERVER_MAX_REGISTERS; i++) {
		mb_map->tab_registers[i] = (uint16_t)(i + 1);
	}
}

static void log_client_connected(int sock)
{
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if (getpeername(sock, (struct sockaddr *)&addr, &addrlen) == 0) {
		printf("[MBTCP-S] client %s:%d connected\n",
		       inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	} else {
		printf("[MBTCP-S] client connected\n");
	}
}

void *mbtcp_server_main(void *args)
{
	char ip[64] = {0};
	int port = 0;
	int enable = 0;
	int server_socket = -1;
	modbus_t *ctx = NULL;
	modbus_mapping_t *mb_map = NULL;

	(void)args;

	load_mbtcp_server_config(ip, sizeof(ip), &port, &enable);
	if (!enable) {
		printf("[MBTCP-S] server disabled\n");
		return NULL;
	}

	ctx = modbus_new_tcp(ip, port);
	if (!ctx) {
		printf("[MBTCP-S] modbus_new_tcp failed\n");
		return NULL;
	}

	modbus_set_slave(ctx, MBTCP_SERVER_UNIT_ID);

	mb_map = modbus_mapping_new(0, 0, MBTCP_SERVER_MAX_REGISTERS, 0);
	if (!mb_map) {
		printf("[MBTCP-S] modbus_mapping_new failed\n");
		modbus_free(ctx);
		return NULL;
	}

	init_fixed_registers(mb_map);

	server_socket = modbus_tcp_listen(ctx, 1);
	if (server_socket < 0) {
		printf("[MBTCP-S] listen failed: %s\n", modbus_strerror(errno));
		modbus_mapping_free(mb_map);
		modbus_free(ctx);
		return NULL;
	}

	printf("[MBTCP-S] listening on %s:%d unit=%d\n", ip, port, MBTCP_SERVER_UNIT_ID);

	for (;;) {
		uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

		if (modbus_tcp_accept(ctx, &server_socket) < 0) {
			printf("[MBTCP-S] accept failed: %s\n", modbus_strerror(errno));
			if (server_socket < 0) {
				server_socket = modbus_tcp_listen(ctx, 1);
				if (server_socket < 0) {
					printf("[MBTCP-S] listen retry failed: %s\n", modbus_strerror(errno));
				}
			}
			sleep(1);
			continue;
		}

		log_client_connected(modbus_get_socket(ctx));

		for (;;) {
			int rc = modbus_receive(ctx, query);
			if (rc > 0) {
				int offset = modbus_get_header_length(ctx);
				if (query[offset] != MODBUS_FC_READ_HOLDING_REGISTERS) {
					modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
					continue;
				}
				modbus_reply(ctx, query, rc, mb_map);
			} else {
				break;
			}
		}

		printf("[MBTCP-S] client disconnected\n");
		modbus_close(ctx);
	}

	modbus_mapping_free(mb_map);
	modbus_free(ctx);
	return NULL;
}
