/*
 * bf_uart.c
 *
 *  Created on: 2020年11月30日
 *      Author: Administrator
 */
#include "data.h"
#include "bf_uart.h"
//#include "bf_mqtt.h"
//#include "bf_pwm.h"
#include "bf_cmd.h"
//#include "driver/uart.h"
//#include "driver/gpio.h"
#include <string.h>
#include <stdlib.h>
#include "wifi.h"
#include "system.h"
#include "mcu_api.h"
#include "protocol.h"

#include "stdio.h"
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/queue.h"
//#include "freertos/timers.h"
#if 0
static xQueueHandle gpio_evt_queue = NULL;

//#define GPIO_INPUT_IO_0     25
//#define GPIO_INPUT_IO_1     5
//#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_0)
//#define ESP_INTR_FLAG_DEFAULT 0

//#define GPIO_OUTPUT_IO_0    14
//#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)

//#define  led_pin 14
//#define  led_pin_H gpio_set_level(led_pin, 1)
//#define  led_pin_L gpio_set_level(led_pin, 0)

unsigned char led_flash_flag = 0;
unsigned char led_flash_times = 0;
unsigned char write_code_en = 0;
unsigned char key_press_times = 0;
unsigned short key_press_start_t = 0;
unsigned short key_press_start_tm = 0;
//unsigned char key_press_start_s = 0;
unsigned short key_press_end_t = 0;
//unsigned char key_press_end_s = 0;
unsigned short key_press_time = 0;
#endif
unsigned char debug_flag = 0;

#if 0
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    static uint32_t tickCount;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));

            if(gpio_get_level(io_num)==1){
                printf("按键短按\n");
            }else if(gpio_get_level(io_num)==0){
                 printf("tickCount=%d, xTaskGetTickCount=%d\n", tickCount, xTaskGetTickCount());
                  if(xTaskGetTickCount()>tickCount+500){
                	mcu_set_wifi_mode(1);
                	led_flash_flag = 2;
                	led_flash_times = 0;
                    printf("按键长按\n");
                }
                else if((xTaskGetTickCount()>tickCount+15) && (xTaskGetTickCount()<tickCount+30))
                {
                	if(key_press_times == 0)
                	{
                		key_press_start_t = tickCount;
                		key_press_times++;
                		//gpio_set_level(GPIO_OUTPUT_IO_0, 0);
                	}
                	else
                	{
                		key_press_end_t = tickCount;
                		key_press_time = key_press_end_t - key_press_start_t;
                		key_press_times++;
                	//	gpio_set_level(GPIO_OUTPUT_IO_0, 1);
                		if (key_press_time > 1000)
                		{
                			key_press_start_t = 0;
                			key_press_end_t = 0;
                			key_press_time = 0;
                			key_press_times = 0;
                		}
                	}
                	if(key_press_times ==3)
                	{
                		//key_press_end_t = tickCount;
                		//key_press_time = key_press_end_t - key_press_start_t;
                	//	gpio_set_level(GPIO_OUTPUT_IO_0, 0);
                		if(key_press_time < 1000)
                		{
                			led_flash_flag = 1;
                			led_flash_times = 0;
                			printf("key press 3 times ,enable wifi airkiss\n");
                			esp_wifi_restore();
                			// vTaskDelete(TASK_WIFI);
                			//vTaskDelete(TASK_MQTT);
                			esp_restart();
                		}

                		key_press_start_t = 0;
                		key_press_end_t = 0;
                		key_press_time = 0;
                		key_press_times = 0;
                	}

                	/*if(key_press_times == 0)
                	{
                		key_press_start_m = local_time.miniter;
                		key_press_start_s = local_time.sencond;
                		//key_press_times++;
                	}
                	else if(key_press_times ==3)
                	{
                		key_press_end_m = local_time.miniter;
                		key_press_end_s = local_time.sencond;

                	    if(key_press_start_m == key_press_end_m)
                		{
                			key_press_time = key_press_end_s - key_press_start_s;
                		}
                		else
                		{
                			key_press_time = key_press_end_s + 60 - key_press_start_s;
                		}

                	    if((key_press_time > 10) && (key_press_time != 0))
                	    {
                	    	key_press_times = 0;
                	    	key_press_time = 0;
                	    }
                	    else
                	    {
                	    	//enable wifi airkiss
                	    }

                	}*/

                	printf("key press time is:%d,press times is:%d\n",key_press_time,key_press_times);
                }
            }
            tickCount = xTaskGetTickCount();
        }
    }
}


ESP_EVENT_DEFINE_BASE(TASK_UART);
//

//static uint8_t bright_less;
static uint8_t write_flag = 0,read_flag = 1;	//写二维码标志


#define ECHO_TEST_TXD  (GPIO_NUM_17)
#define ECHO_TEST_RXD  (GPIO_NUM_16)

#define ECHO_TEST_TXD1  (GPIO_NUM_21)
#define ECHO_TEST_RXD1  (GPIO_NUM_18)

#define ECHO_TXD0  GPIO_NUM_1
#define ECHO_RXD0  GPIO_NUM_3

#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)
#endif

static const int RX_BUF_SIZE = 1024;
#define BUF_SIZE (1024)
#define BUF_CODE_SIZE (20)

uint8_t data_send_index = 0;
unsigned char data_send_buffer[200];
unsigned char * data_temp;
uint8_t data_read_buffer[50];


//static char unique_code[10];

static int len = 0;
unsigned char code_len = 0;


//
#if 0
static void uart_task_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
	if (base == TASK_FILE)
	{
//		char *nvs_unique_id = NULL;
		switch(id)
		{
		case EVENT_FILE_VALUE_BF_CODE: //
				//ESP_LOGI(TASK_WIFI, "command CMD_WIFI_GET_BF_CODE");
				//ESP_LOGE(TASK_FILE, "bf_code:%s", (char *)event_data);
				if(strlen(event_data) <= 0) break;
				memcpy(unique_id, event_data, 8);
				if(strncmp(unique_id,"00000000",8) == 0)
					memset(unique_id,0,10);
				break;
			break;
		}
	}

}
#endif

unsigned short  usMBCRC16( unsigned char * pucFrame, unsigned short usLen )
{
    static const unsigned char aucCRCHi[] =
    {
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40
    };

    static const unsigned char aucCRCLo[] =
    {
        0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7,
        0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E,
        0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9,
        0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
        0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
        0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32,
        0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D,
        0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
        0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF,
        0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
        0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1,
        0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
        0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB,
        0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
        0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
        0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
        0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97,
        0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E,
        0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89,
        0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
        0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83,
        0x41, 0x81, 0x80, 0x40
    };
    unsigned char    ucCRCHi = 0xFF;
    unsigned char    ucCRCLo = 0xFF;
    int              iIndex;

    while( usLen-- )
    {
        iIndex = ucCRCLo ^ *( pucFrame++ );
        ucCRCLo = ( unsigned char )( ucCRCHi ^ aucCRCHi[iIndex] );
        ucCRCHi = aucCRCLo[iIndex];
    }
    return ( unsigned short )( ucCRCHi << 8 | ucCRCLo );
}

#if 0
void findCMD(uint8_t RX_num, uint8_t* Rx_Buffer, uint8_t len)
{
   uint8_t i = 0;
   uint8_t Tx_Buffer[33] = {0};
   uint8_t Tx_len = 0;
//    for(i = 0;i < len;i++)
//    {
//    	printf("%02x ",Rx_Buffer[i]);
//    }
//    printf("\r\n");
    switch(Rx_Buffer[0])
    {

    case 0x49: // ID=12345678
    //	if(RX_num !=3 ) break; //条件限制
    	if(Rx_Buffer[1]==0x44)
    	{
    		if(Rx_Buffer[2]=='=')
    		{
    			if(len==11)
    			{
    				for(i = 0;i < 8;i++)
    				{
    					unique_id[i] = Rx_Buffer[3+i];
    				}
    				memcpy(Tx_Buffer,"ID OK!",6);
    				write_flag = 1;
    				ESP_LOGI(TASK_UART, "%s", Tx_Buffer);
    				Tx_len = 6;
    			}
    			else
    			{
    				memcpy(Tx_Buffer,"ID ERR!",7);
    				ESP_LOGI(TASK_UART, "%s", Tx_Buffer);
    				Tx_len = 7;
    			}
    		}
    		else if(Rx_Buffer[2]=='?')
    		{
    			Tx_Buffer[0]='I';Tx_Buffer[1]='D';Tx_Buffer[2]='=';
    			if(strlen (unique_id) == 8)
    			{
    				memcpy(&Tx_Buffer[3], unique_id, strlen(unique_id));
    			//	write_flag = 1;
    			}
    			else
    			{
    				memcpy(&Tx_Buffer[3],"nothing!",8);
    			}
    			Tx_len = 11;
    			ESP_LOGI(TASK_UART, "%s", Tx_Buffer);
    		}
    		else
    		{
    			memcpy(Tx_Buffer,"ID ERR!",7);
    			Tx_len = 7;
    			ESP_LOGI(TASK_UART, "%s", Tx_Buffer);
    		}
    	}
    	uart_write_bytes(0, (char *) Tx_Buffer, Tx_len);
    	break;

    }


}
static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
//    static uint8_t data[1024];

    while (1) {
//        // Read data from the UART
        //len = uart_read_bytes(UART_NUM_1, data, BUF_SIZE, 20 / portTICK_RATE_MS);
        data[len] = '\0';

//         Write data back to the UART
        uart_write_bytes(UART_NUM_1, (const char *) data, len);
     //   ESP_LOGI(TAG, "send data is <%s>:<%d>", data, len);
        if(len != 0)
        {
        	ESP_LOGI(TASK_UART, "recv data is <%.*s>", len, data);
        	findCMD(1, data, len);		// 1 ：串口
//        	memset(data, 0, sizeof((char *)data));
        }
//        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

//
void Uart0_init(void)
{
	/* Configure parameters of an UART driver,
	 * communication pins and install the driver */
	uart_config_t uart_config = { .baud_rate = 115200, .data_bits =
			UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits =
			UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE };
	uart_param_config(UART_NUM_0, &uart_config);
	uart_set_pin(UART_NUM_0, ECHO_TXD0, ECHO_RXD0, ECHO_TEST_RTS,
	ECHO_TEST_CTS);
	uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
}

void uart_init(void)
{
	/* Configure parameters of an UART driver,
	     * communication pins and install the driver */
	    uart_config_t uart_config = {
	        .baud_rate = 9600,
	        .data_bits = UART_DATA_8_BITS,
	        .parity    = UART_PARITY_DISABLE,
	        .stop_bits = UART_STOP_BITS_1,
	        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	        .source_clk = UART_SCLK_APB,
	    };
	    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);
	    uart_param_config(UART_NUM_2, &uart_config);
	    uart_set_pin(UART_NUM_2, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);

	    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
	    uart_param_config(UART_NUM_1, &uart_config);
	    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD1, ECHO_TEST_RXD1, ECHO_TEST_RTS, ECHO_TEST_CTS);
}

int sendData(const char* logName, const char* data, const int len)
{
    //const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_2, data, len);
    ESP_LOGI(logName, "Write %d bytes", txBytes);
    return txBytes;
}
#endif

int sendData1(const char* logName, const  char* data, const int len)
{
    //const int len = strlen(data);
    int txBytes = 0;
	//txBytes = uart_write_bytes(UART_NUM_1, data, len);
    //ESP_LOGI(logName, "Write %d bytes", txBytes);
    return txBytes;
}

#if 0
void set_params_info()
{
	char init_params[250] = {0};
	uint8_t data_len = 0;
	int i = 0;
	sprintf(init_params,"{\"p\":\"9w6k5ujv9te5gsrm\",\"v\":\"1.0.0\",\"m\":1,\"mt\":10,\"n\":0,\"low\":0}");
	data_len = strlen(init_params);
	data_send_buffer[0] = 0x55;
	data_send_buffer[1] = 0xaa;
	data_send_buffer[2] = 0x03;
	data_send_buffer[3] = 0x01;
	data_send_buffer[4] = data_len/0xff;
	data_send_buffer[5] = data_len%0xff;


	for(i=0;i<data_len;i++)
	{
		data_send_buffer[i+6] = init_params[i];
	}

	printf("send data len is %d, params:%s", data_len,init_params);
}

uint8_t crcCalc(const char* logName, const char* data, const int len)
{
	int i;
	uint8_t calc_result;
	uint8_t calc_sum = 0;
	for(i=0;i<len-1;i++)
	{
		calc_sum = calc_sum + data[i];
		//ESP_LOGI(logName, "send data calc sum is (%d):<%02x>", i,calc_sum);
	}
	ESP_LOGI(logName, "send data calc sum is %02x", calc_sum);
	calc_result = calc_sum;
	ESP_LOGI(logName, "send data calc crc is %02x", calc_result);
	return calc_result;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    printf("tx_task starting!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
    while (1) {
    	switch(data_send_index)
    	{
    	case 1:
    		//����������
    		data_send_index = 0;
			data_send_buffer[0] = 0x55;
			data_send_buffer[1] = 0xaa;
			data_send_buffer[2] = 0x03;
			data_send_buffer[3] = 0x00;
			data_send_buffer[4] = 0x00;
			data_send_buffer[5] = 0x01;
			data_send_buffer[6] = 0x00;
			data_temp = data_send_buffer;
			data_send_buffer[7] = crcCalc(TX_TASK_TAG, data_temp,8);
			//data_temp = data_send_buffer;
    		sendData(TX_TASK_TAG, data_temp,8);
    		ESP_LOGI(TX_TASK_TAG, "send data index is %d", data_send_index);
    		//data_send_index = 1;

    		break;
    	case 2:
    		//��Ʒ��Ϣ����
    		data_send_index = 0;
    		set_params_info();
    		for (int i =0;i < 70; i++)
    		{
    			printf("%02X ",data_send_buffer[i]);
    		}
    		data_temp = data_send_buffer;
    		data_send_buffer[70] = crcCalc(TX_TASK_TAG, data_temp,71);
    		//data_temp = data_send_buffer;
    		sendData(TX_TASK_TAG, data_temp,71);
    		ESP_LOGI(TX_TASK_TAG, "send data index is %d", data_send_index);
    	//	sendData(TX_TASK_TAG, "'p':'v6oyrzv9v7cf3p6j','v':'1.0.0','m':1,'mt':10,'n':0,'ir':'5.12','low':0",74);

    		break;
    	case 3:
    		data_send_index = 0;
    		data_send_buffer[0] = 0x55;
    		data_send_buffer[1] = 0xaa;
    		data_send_buffer[2] = 0x03;
    		data_send_buffer[3] = 0x02;
    		data_send_buffer[4] = 0x00;
    		data_send_buffer[5] = 0x00;
    		//data_send_buffer[6] = 0x00;
    		data_temp = data_send_buffer;
    		printf("222222222222222222222222222222222222222222");
    		data_send_buffer[6] = crcCalc(TX_TASK_TAG, data_temp,7);
    		sendData(TX_TASK_TAG, data_temp,7);
    		break;
    	case 4:
    		//55 aa 03 03 00 00 05
			data_send_index = 6;
			data_send_buffer[0] = 0x55;
			data_send_buffer[1] = 0xaa;
			data_send_buffer[2] = 0x00;
			data_send_buffer[3] = 0x03;
			data_send_buffer[4] = 0x00;
			data_send_buffer[5] = 0x00;
			data_temp = data_send_buffer;
			printf("33333333333333333333333333333333333333333333");
			data_send_buffer[6] = crcCalc(TX_TASK_TAG, data_temp,7);
			sendData(TX_TASK_TAG, data_temp,7);
			break;
    	case 5:
    		data_send_index = 0;
			data_send_buffer[0] = 0x55;
			data_send_buffer[1] = 0xaa;
			data_send_buffer[2] = 0x00;
			data_send_buffer[3] = 0x04;
			data_send_buffer[4] = 0x00;
			data_send_buffer[5] = 0x01;
			data_send_buffer[6] = 0x01;
			data_temp = data_send_buffer;
			printf("444444444444444444444444444444444444444444444");
			data_send_buffer[7] = crcCalc(TX_TASK_TAG, data_temp,8);
			sendData(TX_TASK_TAG, data_temp,8);
    		break;
    	case 6:
    	    		data_send_index = 0;
    				data_send_buffer[0] = 0x55;
    				data_send_buffer[1] = 0xaa;
    				data_send_buffer[2] = 0x00;
    				data_send_buffer[3] = 0x04;
    				data_send_buffer[4] = 0x00;
    				data_send_buffer[5] = 0x01;
    				data_send_buffer[6] = 0x01;
    				data_temp = data_send_buffer;
    				printf("444444444444444444444444444444444444444444444");
    				data_send_buffer[7] = crcCalc(TX_TASK_TAG, data_temp,8);
    				sendData(TX_TASK_TAG, data_temp,8);
    	    		break;

    	}
      //  sendData(TX_TASK_TAG, "Hello world");
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}
#endif

static void rx_task_485(void *arg)
{
	static const char *RX_TASK_485_TAG = "RX_TASK_485";
	unsigned short crc_temp;
	uint8_t* data_485 = (uint8_t*) malloc(RX_BUF_SIZE+1);
	printf("rx_task_485 starting!!!!!!!!!!!!!!!!!!!!!\r\n");
	M_value.dev_upload_en = 0;
	M_value.dev_ele_times = 0;
	M_value.dev_switch = 0x01;
	M_value.dev_mesure_enable = 1;
	M_value.dev_control_index = 0;
	M_value.dev_total_power = 0;
	while(1)
	{
	/////////////////////////////////RS485 test///////////////////////////////////////////////
		int rxBytes_485 = 0;
		//rxBytes_485 = uart_read_bytes(UART_NUM_1, data_485, RX_BUF_SIZE, 200 / portTICK_RATE_MS);
		if (rxBytes_485 > 0)
		{
			printf("Board RS485 communication test  data is:");
			for(int k=0;k<rxBytes_485;k++)
			{
				printf(" %02x",data_485[k]);
			}
			printf("\n");
		}
//////////////////////////////////////////////////////////////////////////////////////////////////////////


		if(debug_flag ==1)
		{
			data_send_buffer[0] = 0x00;
			data_send_buffer[1] = 0x10;
			data_send_buffer[2] = 0x00;
			data_send_buffer[3] = 0x61;
			data_send_buffer[4] = 0x00;
			data_send_buffer[5] = 0x03;
			data_send_buffer[6] = 0x06;
			data_send_buffer[7] = 0x00;
			data_send_buffer[8] = 0x01;
			data_send_buffer[9] = 0x00;
			data_send_buffer[10] = 0x04;
			data_send_buffer[11] = 0x00;
			data_send_buffer[12] = 0x00;
			crc_temp = usMBCRC16(data_send_buffer,13);
			data_send_buffer[13] = crc_temp;
			data_send_buffer[14] = crc_temp >> 8;
			data_temp = data_send_buffer;
			printf("send data is:");
			for(int i=0;i<15;i++)
			{
				printf("%02x ",data_send_buffer[i]);
			}
			printf("\n");

			sendData1(RX_TASK_485_TAG, (char*)data_temp,15);
			debug_flag = 0;
		}
		if(M_value.dev_mesure_enable == 1)
		{
		if(M_value.dev_control_index == 0)
		{
			data_send_buffer[0] = 0x00;
			data_send_buffer[1] = 0x04;
			data_send_buffer[2] = 0x00;
			data_send_buffer[3] = 0x64;
			data_send_buffer[4] = 0x00;
			data_send_buffer[5] = 0x01;
			crc_temp = usMBCRC16(data_send_buffer,6);
			data_send_buffer[6] = crc_temp;
			data_send_buffer[7] = crc_temp >> 8;
			data_temp = data_send_buffer;

			printf("send data is:");
			for(int i=0;i<8;i++)
			{
				printf("%02x ",data_send_buffer[i]);
			}
			printf("\n");

			sendData1(RX_TASK_485_TAG, (char*)data_temp,8);
			M_value.dev_control_index = 1;
		}
		else if(M_value.dev_control_index == 1)
		{
		data_send_buffer[0] = 0x00;
		data_send_buffer[1] = 0x04;
		data_send_buffer[2] = 0x00;
		data_send_buffer[3] = 0x00;
		data_send_buffer[4] = 0x00;
		data_send_buffer[5] = 0x08;
		crc_temp = usMBCRC16(data_send_buffer,6);
		data_send_buffer[6] = crc_temp;
		data_send_buffer[7] = crc_temp >> 8;
		data_temp = data_send_buffer;

		printf("send data is:");
		for(int i=0;i<8;i++)
		{
			printf("%02x ",data_send_buffer[i]);
		}
		printf("\n");

		sendData1(RX_TASK_485_TAG, (char*)data_temp,8);
		M_value.dev_control_index = 5;
		}
		else if(M_value.dev_control_index == 5)
		{
			data_send_buffer[0] = 0x00;
			data_send_buffer[1] = 0x04;
			data_send_buffer[2] = 0x00;
			data_send_buffer[3] = 0x1D;
			data_send_buffer[4] = 0x00;
			data_send_buffer[5] = 0x04;
			crc_temp = usMBCRC16(data_send_buffer,6);
			data_send_buffer[6] = crc_temp;
			data_send_buffer[7] = crc_temp >> 8;
			data_temp = data_send_buffer;

			printf("send data is:");
			for(int i=0;i<8;i++)
			{
				printf("%02x ",data_send_buffer[i]);
			}
			printf("\n");

			sendData1(RX_TASK_485_TAG, (char*)data_temp,8);
			M_value.dev_control_index = 1;
		}
		else if(M_value.dev_control_index == 2)
		{
			data_send_buffer[0] = 0x00;
			data_send_buffer[1] = 0x10;
			data_send_buffer[2] = 0x00;
			data_send_buffer[3] = 0x10;
			data_send_buffer[4] = 0x00;
			data_send_buffer[5] = 0x01;
			data_send_buffer[6] = 0x02;
			if(M_value.dev_switch == 0x01)
			{
				data_send_buffer[7] = 0x55;
				data_send_buffer[8] = 0x55;
			}
			else
			{
				data_send_buffer[7] = 0xaa;
				data_send_buffer[8] = 0xaa;
			}
			crc_temp = usMBCRC16(data_send_buffer,9);
			data_send_buffer[9] = crc_temp;
			data_send_buffer[10] = crc_temp >> 8;
			data_temp = data_send_buffer;

			printf("send data is:");
			for(int i=0;i<11;i++)
			{
				printf("%02x ",data_send_buffer[i]);
			}
			printf("\n");

			sendData1(RX_TASK_485_TAG, (char*)data_temp,11);
			M_value.dev_control_index = 1;
		}

		//const int rxBytes_485 = uart_read_bytes(UART_NUM_1, data_485, RX_BUF_SIZE, 200 / portTICK_RATE_MS);
		if (rxBytes_485 > 0)
		{
			printf("RS485 message is:");
			for(int k=0;k<rxBytes_485;k++)
			{
				printf(" %02x",data_485[k]);
			}
			printf("\n");
			data_485[rxBytes_485] = 0;
			if((data_485[1] == 0x04) && (data_485[2] == 0x10))
			{
				/////////////////////////////////////////////////////////////////////////////
				//////////////////////////get params for energy////////////////////////////////

				M_value.dev_voltage1 = data_485[3];
				M_value.dev_voltage1 = (M_value.dev_voltage1 << 8) + data_485[4];

				M_value.dev_voltage2 = data_485[5];
				M_value.dev_voltage2 = (M_value.dev_voltage2 << 8) + data_485[6];

				M_value.dev_voltage3 = data_485[7];
				M_value.dev_voltage3 = (M_value.dev_voltage3 << 8) + data_485[8];

				M_value.dev_current1 = data_485[9];
				M_value.dev_current1 = (M_value.dev_current1 << 8) + data_485[10];
				//M_value.dev_current1 = M_value.dev_current1*10;

				M_value.dev_current2 = data_485[11];
				M_value.dev_current2 = (M_value.dev_current2 << 8) + data_485[12];
				//M_value.dev_current2 = M_value.dev_current2*10;

				M_value.dev_current3 = data_485[13];
				M_value.dev_current3 = (M_value.dev_current3 << 8) + data_485[14];
				//M_value.dev_current3 = M_value.dev_current3*10;

				M_value.dev_power_value = data_485[17];
				M_value.dev_power_value = (M_value.dev_power_value << 8) + data_485[18];
				//M_value.dev_power_value = M_value.dev_power_value*10;
				//sprintf(M_value.read_vol_c,"111");
				sprintf(M_value.read_vol_c,"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",data_485[3],data_485[4],data_485[5],data_485[6],data_485[7],data_485[8],data_485[9],data_485[10],data_485[11],data_485[12],data_485[13],data_485[14]);
				////////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////

//				M_value.dev_current = data_485[9];
//				M_value.dev_current = (M_value.dev_current << 8) + data_485[10];
//				M_value.dev_current = M_value.dev_current * 10;
//				M_value.dev_voltage = data_485[3];
//				//printf("first measure voltage:%04x\n",M_value.dev_voltage);
//				M_value.dev_voltage = (M_value.dev_voltage << 8) + data_485[4];
//
//				M_value.dev_power = data_485[17];
//				M_value.dev_power = (M_value.dev_power << 8) + data_485[18];
//				M_value.dev_power = M_value.dev_power * 10;
//			//	M_value.dev_ele[local_time.miniter] = M_value.dev_power;
//				M_value.dev_total_power = M_value.dev_total_power + (M_value.dev_power%600);
//				printf("power accumulative error:%04x\n",M_value.dev_total_power);
//				if(M_value.dev_total_power  > 600)
//				{
//					M_value.dev_power_add = (M_value.dev_power/600) + (M_value.dev_total_power/600);
//					M_value.dev_total_power = 0;
//				}
//				else
//				M_value.dev_power_add = M_value.dev_power/600;
//				M_value.dev_power_ele = M_value.dev_power_add;
//				printf("second measure :%04x,current:%04x,power:%04x,power_add:%04x\n",M_value.dev_voltage,M_value.dev_current,M_value.dev_power,M_value.dev_power_add);
//				//M_value.dev_ele_times++;
			}
			else if((data_485[1] == 0x04) && (data_485[2] == 0x08))
			{
				M_value.dev_power_H = data_485[3];
				M_value.dev_power_H = (M_value.dev_power_H << 8) + data_485[4];
				M_value.dev_power_H = (M_value.dev_power_H << 8) + data_485[5];
				M_value.dev_power_H = (M_value.dev_power_H << 8) + data_485[6];
				if(M_value.dev_power_L == 0)
				{
					M_value.dev_power_L = M_value.dev_power_H;
					M_value.dev_power_ele = 0;
				}
//				else
//				{
//					M_value.dev_power_ele = M_value.dev_power_H - M_value.dev_power_L;
//
//				}
				//M_value.dev_power_value = M_value.dev_power_H;

//				M_value.dev_power_L = data_485[7];
//				M_value.dev_power_L = (M_value.dev_power_L << 8) + data_485[8];
//				M_value.dev_power_L = (M_value.dev_power_L << 8) + data_485[9];
//				M_value.dev_power_L = (M_value.dev_power_L << 8) + data_485[10];

				sprintf(M_value.read_powe,"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",data_485[0],data_485[1],data_485[2],data_485[3],data_485[4],data_485[5],data_485[6],data_485[7],data_485[8],data_485[9],data_485[10],data_485[11]);

			}
			//ESP_LOG_BUFFER_HEXDUMP(RX_TASK_485_TAG, data_485, rxBytes_485, ESP_LOG_INFO);

		}
		/*
		if(local_time.miniter == 59)
		{
			for(int i = 0;i< 60;i++)
			{
				M_value.dev_total_power = M_value.dev_total_power + M_value.dev_ele[i];
			}
			M_value.dev_average_power = M_value.dev_total_power/60;
			M_value.dev_power_add = M_value.dev_average_power - M_value.dev_power_add;
			M_value.dev_upload_en = 1;
		}
		if (M_value.dev_upload_en == 0)
		{
			M_value.dev_power_add = M_value.dev_power/60;
			M_value.dev_upload_en = 2;
		}
		*/
		M_value.dev_mesure_enable = 0;
		}
		//vTaskDelay(500/portTICK_RATE_MS);
	}
	//}
	free(data_485);
	//}
}

static void rx_task(uint8_t *data, size_t rxBytes)
{
    //static const char *RX_TASK_TAG = "RX_TASK";

    //esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    //uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    //printf("rx_task starting!!!!!!!!!!!!!!!!!!!!!\r\n");
    //while (1) {
    	unsigned char data_flag = 0;
        //const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 100 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            for (int i=0;i<rxBytes-1;i++)
            {
            	if((data[i] == 0x55) && (data[i+1] == 0xaa))
					{
						data_flag = data_flag + 1;
					}
            }
            if(data_flag ==1)
            {
				if ((data[2] == 0x00) && (data[3] == 0x1c))
				{
					//local_time.year = 0x14;
					local_time.year = 2000 + data[7];
					local_time.month = data[8];
					local_time.day = data[9];
					local_time.hour = data[10];
					local_time.miniter = data[11];
					local_time.sencond = data[12];
				}
				else if ((data[3] == 0x06) && (data[5] == 0x05))
				{
					M_value.dev_control_index = 2;
					M_value.dev_switch = data[10];
					M_value.dev_mesure_enable = 1;
				}
            }
            else
            {
				if ((data[2] == 0x04) && (data[3] == 0x02))
				{
					M_value.dev_switch_value = data[4];
				}
            }
            	//ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s',data_flag:%d", rxBytes, data,data_flag);
            if (data_flag == 1)//((rxBytes == 7) || (rxBytes == 8) || (rxBytes == 12))
            {
            	if ((data[0] == 0x55) && (data[1] == 0xaa))
		        	{
            		uart_receive_buff_input(data,rxBytes);
		        	}
            }
            //ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
        //vTaskDelay(300/portTICK_RATE_MS);
    //}
    //free(data);
}

#if 0
static void uart_task(void *arg)
{
	//static const char *UART_TASK_TAG = "UART_TASK";
	//esp_log_level_set(UART_TASK_TAG, ESP_LOG_INFO);
	printf("uart task begining!!!!!!!!!!!!!\r\n");
	while(1)
	{
		wifi_uart_service();
		vTaskDelay(300/portTICK_RATE_MS);
	}
}

void uart_main(void *args)
{
	uint8_t read_count = 0;
	uint8_t *data_code = (uint8_t *) malloc(BUF_SIZE);
	uint8_t *data_unique_code = (uint8_t *) malloc(BUF_CODE_SIZE);

	    gpio_config_t io_conf;
	    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	    io_conf.mode = GPIO_MODE_OUTPUT;
	    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	    io_conf.pull_down_en = 0;
	    io_conf.pull_up_en = 0;
	    gpio_config(&io_conf);
	    io_conf.intr_type = GPIO_INTR_NEGEDGE;
	    //io_conf.intr_type = GPIO_INTR_POSEDGE;
	    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	    io_conf.mode = GPIO_MODE_INPUT;
	    io_conf.pull_up_en = 0;
	    gpio_config(&io_conf);
	    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

	    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

	    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
	   // gpio_set_direction(14, GPIO_MODE_OUTPUT);
	   // gpio_set_level(14, 0);
	    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
	    gpio_set_level(GPIO_OUTPUT_IO_1, 1);
	Uart0_init();
	uart_init();
	wifi_protocol_init();
	M_value.dev_power_L = 0;
#if 0
	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_event_handler_register_with(bf_event_loop, ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, uart_task_event_handler, bf_event_loop);
	xTaskCreate(uart_task, "uart_task", 2048, NULL, uxTaskPriorityGet(NULL), NULL);
	xTaskCreate(rx_task, "uart_rx_task", 2048, NULL, uxTaskPriorityGet(NULL), NULL);

	xTaskCreate(rx_task_485, "uart_rx_task_485", 2048, NULL, uxTaskPriorityGet(NULL), NULL);

	vTaskDelay(pdMS_TO_TICKS(1000));
#endif
	led_flash_flag = 0;
	led_flash_times = 0;
	while(1)
	{
		if(write_flag == 1)
		{
			ESP_ERROR_CHECK(esp_event_post_to(bf_event_loop, TASK_FILE, CMD_FILE_SET_BF_CODE, unique_id, strlen(unique_id) + 1, portMAX_DELAY));
			vTaskDelay(pdMS_TO_TICKS(1000));
			ESP_LOGI(TASK_UART, "unique_code:%s", unique_id);

			vTaskDelay(pdMS_TO_TICKS(500));
			write_flag = 0;
			printf("system restart!!!!!!!!!!!!!!!!!!!!!!!!!");
			esp_restart();
		}


		if((read_flag == 1) && (led_flash_flag == 0))
		{
			len = 0;
			code_len =0;
			write_code_en = 0;
			if(read_count ++> 20)
			{
				read_count = 0;
				read_flag = 0;
			}
			if(write_code_en == 0)
			{
				ESP_ERROR_CHECK(esp_event_post_to(bf_event_loop, TASK_FILE, CMD_FILE_GET_BF_CODE, NULL, 0, portMAX_DELAY));
				vTaskDelay(1000 / portTICK_PERIOD_MS);
					ESP_LOGI("uart_tag", "wait write code\n");
					len = uart_read_bytes(0, data_code, BUF_SIZE, 20/ portTICK_RATE_MS);
					data_code[len] = '\0';
			}
				//	uart_write_bytes(0, (const char *) data_code, len);
					//   ESP_LOGI(TAG, "send data is <%s>:<%d>", data, len);
					if(len > 0)
					{
						for(int i =0;i<len;i++)
						{
							if((data_code[i] == 0x49) && (data_code[i+1] == 0x44))
							{
							//	printf("111111111111111111111111111111111111111111111111111111");
								if(data_code[i+2] == 0x3D)
								{
								//	printf("222222222222222222222222222222222222222222222222222");
									memcpy(data_unique_code,&data_code[i],11);
									data_unique_code[11] = '\0';
									code_len = 11;
									break;

								}
								else if(data_code[i+2] == 0x3F)
								{
								//	printf("33333333333333333333333333333333333333333333333333333333");
									memcpy(data_unique_code,&data_code[i],3);
									data_unique_code[3] = '\0';
									code_len = 3;
									//break;
								}
							}
						}
					}
					if(code_len != 0)
					{
					  ESP_LOGI("uart_tag", "recv data code is <%.*s>", code_len, data_unique_code);
					  write_code_en = 1;
					  findCMD(0, data_unique_code, code_len);		// 1 ：串口
					//  code_len = 0;

					}
					if(len != 0)
					{
						ESP_LOGI("uart_tag", "recv data is <%.*s>", len, data_code);
					}

				//}

		}
		if(led_flash_flag == 1) //setup wifi for esp32
		{
			led_flash_times++;
			gpio_set_level(GPIO_OUTPUT_IO_0, led_flash_times%2);
			vTaskDelay(pdMS_TO_TICKS(300));
			if(led_flash_times > 60)
			{
				led_flash_times = 0;
				gpio_set_level(GPIO_OUTPUT_IO_0, 0);
				led_flash_flag = 0;
			}
		}
		else if(led_flash_flag == 2) //setup wifi for cb3s
		{
			led_flash_times++;
			gpio_set_level(GPIO_OUTPUT_IO_0, led_flash_times%2);
			vTaskDelay(pdMS_TO_TICKS(1000));
			if(led_flash_times > 30)
			{
				led_flash_times = 0;
			    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
				led_flash_flag = 0;
			}
		}
		else if(led_flash_flag == 0)
		{
		gpio_set_level(GPIO_OUTPUT_IO_0, 0);
		vTaskDelay(pdMS_TO_TICKS(1000));
		}

	}
}
#endif

//
