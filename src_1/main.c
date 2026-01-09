#include <pthread.h>
#include "common.h"
#include "mqtt.h"
#include "uart.h"
#include "data.h"
#include "bf_cmd.h"

//int main(int argc, char **argv)
int main()
{
	data_init();

	pthread_t thread_mqtt;
	pthread_t thread_uart_1_rx;
	pthread_t thread_uart_2_rx;
	pthread_t thread_data;
	pthread_t thread_cmd;
//	pthread_t thread_yield;

	pthread_create(&thread_mqtt, NULL, mqtt_main, NULL);
	pthread_create(&thread_uart_1_rx, NULL, uart_rx_task, (void *)g_tty1_index);
	pthread_create(&thread_uart_2_rx, NULL, uart_rx_task, (void *)g_tty2_index);
	pthread_create(&thread_data, NULL, data_main, NULL);
	pthread_create(&thread_cmd, NULL, cmd_main, NULL);
//	pthread_create(&thread_yield, NULL, yield_main, NULL);

	pthread_join(thread_mqtt, NULL);
	pthread_join(thread_uart_1_rx, NULL);
	pthread_join(thread_uart_2_rx, NULL);
	pthread_join(thread_data, NULL);
	pthread_join(thread_cmd, NULL);
//	pthread_join(thread_yield, NULL);

	return 0;
}
