/*
 * Minimal private definitions for libmodbus TCP backend.
 *
 * The repository vendors libmodbus sources, but this private header was
 * missing; it is required to build `src/modbus-tcp.c`.
 */

#ifndef MODBUS_TCP_PRIVATE_H
#define MODBUS_TCP_PRIVATE_H

#include <stdint.h>

/* TCP MODBUS ADU: MBAP(7) + PDU */
#define _MODBUS_TCP_HEADER_LENGTH      7
#define _MODBUS_TCP_CHECKSUM_LENGTH    0

/* Preset request/response length used by modbus-tcp.c */
#define _MODBUS_TCP_PRESET_REQ_LENGTH  12
#define _MODBUS_TCP_PRESET_RSP_LENGTH  8

/* Lengths for TCP PI mode (node/service strings) */
#define _MODBUS_TCP_PI_NODE_LENGTH     1025
#define _MODBUS_TCP_PI_SERVICE_LENGTH  32

typedef struct _modbus_tcp {
    uint16_t t_id;
    int port;
    char ip[16];
} modbus_tcp_t;

typedef struct _modbus_tcp_pi {
    uint16_t t_id;
    char node[_MODBUS_TCP_PI_NODE_LENGTH];
    char service[_MODBUS_TCP_PI_SERVICE_LENGTH];
} modbus_tcp_pi_t;

#endif /* MODBUS_TCP_PRIVATE_H */

