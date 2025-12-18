#include <stdbool.h>
#include <stdint.h>

#ifndef __OUT_MQTT_H__
#define __OUT_MQTT_H__

void *mqtt_main();
void *yield_main();
void control_result_publish(char *mac_str, uint8_t result, uint8_t lock_status);
void sample_device_state_publish(uint8_t slave_index, uint8_t err_code);
void device_state_publish(char *mac_str, uint8_t lock_status, uint8_t err_code, uint8_t power, uint8_t park_status, uint16_t ultrasonic_distance, uint16_t ultrasonic_threshold, char *version);
void heart_beat_publish();
void params_publish();
void device_online_publish();

void mqtt_humiDevice_data_publish();
void mqtt_airDevice_data_publish();

void uart_rx_publish(uint32_t index, char *str);
void uart_tx_publish(uint32_t index, char *str);

bool is_mqtt_online();
void mqtt_haas_data_publish();

void mqtt_data_upload(void);

extern time_t g_last_heart_beat_time;
extern uint8_t RS485_type;

#endif
