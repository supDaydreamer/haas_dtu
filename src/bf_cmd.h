#ifndef BF_CMD_H_
#define BF_CMD_H_

#include <stdint.h>
#include <time.h>
#include <unistd.h>
//#include "bf_common.h"

#define  SET_data 		0x79		//д������־

extern char unique_id[10];
extern char net_color[10];
extern uint8_t lost_connection;
extern uint32_t bt_color;
extern unsigned char cb3s_wifi_state;
extern uint8_t mqtt_publish_en;
//ESP_EVENT_DECLARE_BASE(TASK_CMD);
#define KEY_LIGHT_PARAM		("rgb_param")

enum
{
	EVENT_CMD_STATUS,
	EVENT_CMD_DATA,
	EVENT_CMD_SET_DATA,
	EVENT_CMD_SET_FREQ,

	CMD_CMD_GET_DATA
};

typedef struct date
{
	int year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char miniter;
	unsigned char sencond;
}Date;

Date local_time;

typedef struct humiDev
{	
	uint8_t power_sta;
	uint8_t switch_mode;
	uint8_t work_mode;
	uint8_t error_code;
	uint8_t setHumi_value;
	uint8_t control_mode;
	uint8_t windSpeed;
	uint8_t swing_mode;
	uint8_t windspeed_loop;
	uint8_t windspeed_ex;
	uint8_t data_update;
	uint8_t getHumi_value;
	uint8_t get_control_mode;
}HumiDev;

extern HumiDev humiDevice;

typedef struct airDev
{
	uint8_t power_sta;
	uint8_t switch_mode;
	uint8_t work_mode;
	uint16_t temp_value;
	uint8_t error_code;
	uint8_t data_update;
	uint8_t wind_speed;
}AirDev;

extern AirDev airDevice;

void my_memcpy_b(uint8_t* tostr,uint8_t* fromstr,uint8_t len);
void write_parameter(uint8_t a);
void read_parameter(void);
void *cmd_main(void *args);
void Rgb_Blecmd(uint8_t* Rx_Buffer, uint8_t len);

extern time_t s_cmd_last_run_time;

#endif
