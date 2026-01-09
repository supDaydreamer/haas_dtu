/*
 * bf_uart.h
 *
 *  Created on: 2020Äê11ÔÂ30ÈÕ
 *      Author: Administrator
 */

#ifndef MAIN_BF_UART_H_
#define MAIN_BF_UART_H_


//#include "bf_common.h"
//#include "bf_file.h"
//#include "bf_wifi.h"
#include <stdint.h>


//ESP_EVENT_DECLARE_BASE(TASK_UART);

enum
{
	EVENT_UART_BF_CODE,
//	CMD_SET_PWM_BRIGHTLESS,
	LOAD_VALUE,
	CMD_UART_BF_CODE
};


#define GPIO_OUTPUT_IO_0    14
#define GPIO_OUTPUT_IO_1    26
#define GPIO_OUTPUT_PIN_SEL ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

extern unsigned char led_flash_flag;
void uart_main(void *args);
extern void findCMD(uint8_t RX_num, uint8_t* Rx_Buffer, uint8_t len);
int sendData(const char* logName, const char* data, const int len);
#endif /* MAIN_BF_UART_H_ */
