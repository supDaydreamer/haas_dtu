#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv) {
    int port = 1503;
    int unit = 1;
    if (argc >= 2) port = atoi(argv[1]);

    modbus_t *ctx = modbus_new_tcp(NULL, port);
    if (!ctx) { perror("modbus_new_tcp"); return 1; }
    modbus_set_slave(ctx, unit);

    modbus_mapping_t *mb_map = modbus_mapping_new(0, 0, 200, 0); // 200个保持寄存器
    if (!mb_map) { perror("modbus_mapping_new"); modbus_free(ctx); return 1; }
    for (int i = 0; i < 10; i++) mb_map->tab_registers[i] = (uint16_t)(i + 1);

    int server_socket = modbus_tcp_listen(ctx, 1);
    if (server_socket < 0) {
        perror("modbus_tcp_listen");
        modbus_mapping_free(mb_map);
        modbus_free(ctx);
        return 1;
    }

    printf("Modbus TCP server listen on 0.0.0.0:%d (unit=%d)\n", port, unit);

    for (;;) {
        if (modbus_tcp_accept(ctx, &server_socket) < 0) {
            perror("modbus_tcp_accept");
            continue;
        }

        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        for (;;) {
            int rc = modbus_receive(ctx, query);
            if (rc > 0) {
                modbus_reply(ctx, query, rc, mb_map);
            } else {
                break; // 客户端断开或出错
            }
        }
        modbus_close(ctx); // 关闭当前连接(不关闭listen socket)
    }

    modbus_mapping_free(mb_map);
    modbus_free(ctx);
    return 0;
}
