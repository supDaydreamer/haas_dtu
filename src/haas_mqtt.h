#ifndef __HAAS_MQTT_H__
#define __HAAS_MQTT_H__

#include <stdint.h>

void *haas_mqtt_main();
void haas_mqtt_data_upload(void);
void haas_mqtt_vision_upload(float length, float width, uint16_t ok);

#endif
