#include <stdint.h>
#include "bfmsg.h"

#ifndef __UART_H__
#define __UART_H__

#define TTY_1_INDEX		(1)
#define TTY_2_INDEX		(2)

void *uart_rx_task(void *args);
void uart_tx(uint32_t uart_index, uint8_t *data, size_t len);

bool read_uart_msg(BFMSG_BOX_HANDLE box_handle, uint8_t *data, size_t *len);
void write_uart_msg(BFMSG_BOX_HANDLE box_handle, uint8_t *data, size_t len);

void on_uart_1_read(uint8_t *data, size_t len);
void on_uart_2_read(uint8_t *data, size_t len);
void on_uart_1_write(uint8_t *data, size_t len);
void on_uart_2_write(uint8_t *data, size_t len);

extern BFMSG_BOX_HANDLE g_uart1_rx_box_handle;
extern BFMSG_BOX_HANDLE g_uart2_rx_box_handle;
extern BFMSG_BOX_HANDLE g_uart1_tx_box_handle;
extern BFMSG_BOX_HANDLE g_uart2_tx_box_handle;

extern uint32_t g_tty1_index;
extern uint32_t g_tty2_index;

#endif
