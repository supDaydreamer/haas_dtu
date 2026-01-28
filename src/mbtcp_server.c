#include "modbus.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "data.h"
#include "haas_mqtt.h"
#include "ini.h"
#include "mbtcp_server.h"

#define DEFAULT_MBTCP_SERVER_PORT 502
#define DEFAULT_MBTCP_SERVER_IP   "0.0.0.0"
#define DEFAULT_MBTCP_SERVER_ENABLE 0
#define MBTCP_SERVER_MAX_REGISTERS 600
#define VISION_REG_START 500
#define VISION_REG_COUNT 5

static modbus_mapping_t *g_mbtcp_map = NULL;

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

	if ((VISION_REG_START + VISION_REG_COUNT) <= MBTCP_SERVER_MAX_REGISTERS) {
		for (i = 0; i < VISION_REG_COUNT; i++) {
			mb_map->tab_registers[VISION_REG_START + i] = 0;
		}
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

static float regs_to_float(uint16_t high, uint16_t low)
{
	union {
		uint32_t u;
		float f;
	} conv;

	conv.u = ((uint32_t)high << 16) | low;
	return conv.f;
}

static int send_write_response(modbus_t *ctx, const uint8_t *req, uint16_t start, uint16_t count)
{
	uint8_t rsp[12];
	int sock = modbus_get_socket(ctx);
	int rc = 0;

	rsp[0] = req[0];
	rsp[1] = req[1];
	rsp[2] = req[2];
	rsp[3] = req[3];
	rsp[4] = 0x00;
	rsp[5] = 0x06;
	rsp[6] = req[6];
	rsp[7] = req[7];
	rsp[8] = (uint8_t)(start >> 8);
	rsp[9] = (uint8_t)(start & 0xFF);
	rsp[10] = (uint8_t)(count >> 8);
	rsp[11] = (uint8_t)(count & 0xFF);

	printf("[MBTCP-S] TX:");
	for (int i = 0; i < (int)sizeof(rsp); i++) {
		printf(" %02X", rsp[i]);
	}
	printf("\n");

	rc = send(sock, rsp, sizeof(rsp), 0);
	return rc;
}

static int handle_vision_write(modbus_t *ctx, const uint8_t *req, int req_len)
{
	int offset = modbus_get_header_length(ctx);
	uint16_t start = 0;
	uint16_t count = 0;
	uint8_t byte_count = 0;
	const uint8_t *payload = NULL;
	uint16_t reg0 = 0;
	uint16_t reg1 = 0;
	uint16_t reg2 = 0;
	uint16_t reg3 = 0;
	uint16_t reg4 = 0;
	float length = 0.0f;
	float width = 0.0f;
	uint16_t ok_value = 0;
	int wrote_length = 0;
	int wrote_width = 0;
	int wrote_ok = 0;

	if (req_len < offset + 6) {
		return modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
	}

	start = (uint16_t)((req[offset + 1] << 8) | req[offset + 2]);
	count = (uint16_t)((req[offset + 3] << 8) | req[offset + 4]);
	byte_count = req[offset + 5];

	if (count == 0 || byte_count != count * 2) {
		return modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
	}

	if (!((start == VISION_REG_START &&
	       (count == VISION_REG_COUNT || count == 4 || count == 2)) ||
	      (start == (VISION_REG_START + 2) && count == 2) ||
	      (start == (VISION_REG_START + 4) && count == 1))) {
		return modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
	}

	if (req_len < offset + 6 + byte_count) {
		return modbus_reply_exception(ctx, req, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
	}

	payload = req + offset + 6;

	if (start == VISION_REG_START && count == VISION_REG_COUNT) {
		reg0 = (uint16_t)((payload[0] << 8) | payload[1]);
		reg1 = (uint16_t)((payload[2] << 8) | payload[3]);
		reg2 = (uint16_t)((payload[4] << 8) | payload[5]);
		reg3 = (uint16_t)((payload[6] << 8) | payload[7]);
		reg4 = (uint16_t)((payload[8] << 8) | payload[9]);
		wrote_length = 1;
		wrote_width = 1;
		wrote_ok = 1;
	} else if (start == VISION_REG_START && count == 4) {
		reg0 = (uint16_t)((payload[0] << 8) | payload[1]);
		reg1 = (uint16_t)((payload[2] << 8) | payload[3]);
		reg2 = (uint16_t)((payload[4] << 8) | payload[5]);
		reg3 = (uint16_t)((payload[6] << 8) | payload[7]);
		wrote_length = 1;
		wrote_width = 1;
	} else if (start == VISION_REG_START && count == 2) {
		reg0 = (uint16_t)((payload[0] << 8) | payload[1]);
		reg1 = (uint16_t)((payload[2] << 8) | payload[3]);
		wrote_length = 1;
	} else if (start == (VISION_REG_START + 2) && count == 2) {
		reg2 = (uint16_t)((payload[0] << 8) | payload[1]);
		reg3 = (uint16_t)((payload[2] << 8) | payload[3]);
		wrote_width = 1;
	} else {
		reg4 = (uint16_t)((payload[0] << 8) | payload[1]);
		wrote_ok = 1;
	}

	if (g_mbtcp_map && g_mbtcp_map->tab_registers &&
	    (start + count) <= g_mbtcp_map->nb_registers) {
		for (int i = 0; i < count; i++) {
			uint16_t reg = (uint16_t)((payload[i * 2] << 8) | payload[i * 2 + 1]);
			g_mbtcp_map->tab_registers[start + i] = reg;
		}
	}

	if (wrote_length) {
		length = regs_to_float(reg0, reg1);
		printf("[MBTCP-S] Vision write length=%.6f\n", length);
	}
	if (wrote_width) {
		width = regs_to_float(reg2, reg3);
		printf("[MBTCP-S] Vision write width=%.6f\n", width);
	}
	if (wrote_length && wrote_width) {
		if (wrote_ok) {
			ok_value = reg4;
		} else if (g_mbtcp_map && g_mbtcp_map->tab_registers &&
		           (VISION_REG_START + 4) < g_mbtcp_map->nb_registers) {
			ok_value = g_mbtcp_map->tab_registers[VISION_REG_START + 4];
		}
		printf("[MBTCP-S] Vision write ok=%u\n", (unsigned int)ok_value);
		vision_store_sample(length, width, ok_value);
		if (wrote_ok) {
			haas_mqtt_vision_upload(length, width, ok_value);
		}
	}

	return send_write_response(ctx, req, start, count);
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

	modbus_set_slave(ctx, MODBUS_TCP_SLAVE);

	mb_map = modbus_mapping_new(0, 0, MBTCP_SERVER_MAX_REGISTERS, 0);
	if (!mb_map) {
		printf("[MBTCP-S] modbus_mapping_new failed\n");
		modbus_free(ctx);
		return NULL;
	}

	init_fixed_registers(mb_map);
	g_mbtcp_map = mb_map;

	server_socket = modbus_tcp_listen(ctx, 1);
	if (server_socket < 0) {
		printf("[MBTCP-S] listen failed: %s\n", modbus_strerror(errno));
		modbus_mapping_free(mb_map);
		modbus_free(ctx);
		return NULL;
	}

	printf("[MBTCP-S] listening on %s:%d unit=any\n", ip, port);

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
				uint8_t function = query[offset];

				printf("[MBTCP-S] RX:");
				for (int i = 0; i < rc; i++) {
					printf(" %02X", query[i]);
				}
				printf("\n");

				if (function == MODBUS_FC_READ_HOLDING_REGISTERS) {
					modbus_reply(ctx, query, rc, mb_map);
				} else if (function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS) {
					handle_vision_write(ctx, query, rc);
				} else {
					modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
				}
			} else {
				break;
			}
		}

		printf("[MBTCP-S] client disconnected\n");
		modbus_close(ctx);
	}

	g_mbtcp_map = NULL;
	modbus_mapping_free(mb_map);
	modbus_free(ctx);
	return NULL;
}
