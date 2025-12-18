#include <stdbool.h>
#include <stdint.h>

#ifndef __HAAS_MQTT_P_H__
#define __HAAS_MQTT_P_H__


#define MQTT_KEEP_ALIVE_TIME_S          (0)
#define OUT_MQTT_YIELD_MS               (1000)
#define OUT_MQTT_PORT                   (1883)
#define OUT_MQTT_QOS                    (QOS0)
#define OUT_MQTT_FAILED_SLEEP_TIME      (3)
#define OUT_MQTT_BUF_SIZE               (100 * 1024)    //100KB
#define OUT_CLIENT_ID_PRIFIX            ("HAAS-DTU")
#define OUT_TOPIC_BUF_SIZE              (1024)
#define MQTT_TOPIC_LEN_MAX              (128)

static void *yield_main();
static void control_result_publish(char *mac_str, uint8_t result, uint8_t lock_status);
static void sample_device_state_publish(uint8_t slave_index, uint8_t err_code);
static void device_state_publish(char *mac_str, uint8_t lock_status, uint8_t err_code, uint8_t power, uint8_t park_status, uint16_t ultrasonic_distance, uint16_t ultrasonic_threshold, char *version);
static void heart_beat_publish();
static void params_publish();
static void device_online_publish();

static void mqtt_humiDevice_data_publish();
static void mqtt_airDevice_data_publish();

static bool is_mqtt_online();
static void mqtt_haas_data_publish();

#endif
