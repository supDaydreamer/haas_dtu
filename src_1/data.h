#ifndef __DATA_H__
#define __DATA_H__

#define DATA_FUNCTION_INTERVAL_S		(300)
#define DATA_MQTT_INTERVAL_S            (60)

#define UART_DATA_BUF_SIZE				(1024)
#define LOCK_CODE_MAX_LEN				(64)
#define LOCK_COUNT_MAX					(6)
#define DEFAULT_HEART_BEAT_INTERVAL_S	(600)
#define DEFAULT_LOCK_CONTROL_TIME_OUT_S	(10)
#define UART_RETURN_DATA_LEN			(12)
#define HEARTBEAT_TRIGGER_TIME			(150)

#define SLAVE_CONNECT_MAP_FILE	"/tmp/slave_connect.map"
#define CONFIG_FILE				"/mnt/usr/haas_energy.conf"
#define VERSION_FILE			("/root/main_app/build/version")
#define BF_CODE_FILE			("/mnt/usr/bf_code")
//#define FILENAME "/mnt/usr/device.conf"

#define HUMI_SAVE_DIR			"/root/humi_save"
#define HUMI_SAVE_INTERVAL_S	(60)

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	DEVICE_485_NO_DEVICE = 0,
	DEVICE_485_AIR = 1,
	DEVICE_485_HUMI = 2
} DEVICE_485_type;


typedef struct{
	uint8_t index;
	uint8_t type;
	uint8_t cmd;
	uint8_t dev_add;
	uint16_t reg_add;
	uint16_t data_len;
	uint16_t value1;
	float  value2;
} HAAS_DEV_RS485;


typedef struct {
	uint8_t year;	//Time[1] 为年份，0x00 表示 2000 年
	uint8_t month;	//Time[2] 为月份，从 1 开始到12 结束
	uint8_t day;	//Time[3] 为日期，从 1 开始到31 结束
	uint8_t hour;	//Time[4] 为时钟，从 0 开始到23 结束
	uint8_t minute;	//Time[5] 为分钟，从 0 开始到59 结束
	uint8_t second;	//Time[6] 为秒钟，从 0 开始到59 结束
	uint8_t week;	//Time[7] 为星期，从 1 开始到 7 结束，1代表星期一
} HAAS_TIME;

typedef struct measure_data
{
	unsigned short dev_power;
	unsigned short dev_voltage;
	unsigned short dev_current;

	unsigned short dev_voltage1;
	unsigned short dev_current1;
	unsigned short dev_voltage2;
	unsigned short dev_current2;
	unsigned short dev_voltage3;
	unsigned short dev_current3;
	unsigned int   dev_power_value;
	unsigned int   dev_today_power;
	unsigned short factor;

	char read_vol_c[50];
	char read_powe[50];

	unsigned long dev_power_H;
	unsigned long dev_power_L;
	//unsigned short dev_power_nowvalue;

	unsigned int   dev_power_add;
	unsigned int   dev_power_ele;
	unsigned int   dev_power_ele_mqtt;
	unsigned int   dev_power_sum_time;
	unsigned int   dev_last_power;
	unsigned int   dev_average_power;
	unsigned int   dev_total_power;
	unsigned int   dev_ele_times;
	unsigned short dev_ele[80];
	unsigned char dev_switch;
	unsigned char dev_switch_last;
	unsigned char dev_mesure_enable;
	unsigned char dev_control_index;
	unsigned char dev_upload_en;
	unsigned char dev_switch_value;
	//unsigned char dev_ala;
} Measure_data;

#if 0
typedef struct __measure_data {
	unsigned short dev_power;
	unsigned short dev_voltage;
	unsigned short dev_current;

	unsigned short dev_voltage1;
	unsigned short dev_current1;
	unsigned short dev_voltage2;
	unsigned short dev_current2;
	unsigned short dev_voltage3;
	unsigned short dev_current3;
	unsigned short dev_power_value;
	unsigned short dev_today_power;

	char read_vol_c[50];
	char read_powe[50];

	unsigned long dev_power_H;
	unsigned long dev_power_L;
	//unsigned short dev_power_nowvalue;

	unsigned short dev_power_add;
	unsigned short dev_power_ele;
	unsigned short dev_power_ele_mqtt;
	unsigned short dev_last_power;
	unsigned short dev_average_power;
	unsigned short dev_total_power;
	unsigned char dev_ele_times;
	unsigned short dev_ele[80];
	unsigned char dev_switch;
	unsigned char dev_switch_last;
	unsigned char dev_mesure_enable;
	unsigned char dev_control_index;
	unsigned char dev_upload_en;
	unsigned char dev_switch_value;
	//unsigned char dev_ala;
} Measure_data;
#endif

typedef enum {
	WORK_MODE_NORMAL = 1
} WORK_MODE_TYPE;

typedef struct {
	uint32_t work_mode;
	uint32_t gate_interval;
	uint32_t data_interval;
	uint32_t lock_num;
	char lock_code_array[LOCK_COUNT_MAX][LOCK_CODE_MAX_LEN];
} RUNTIME_DATA;

typedef struct __const_humiDevice_data
{
	uint8_t power_status;
	uint8_t humi_set_value;
	uint8_t heat_mode;
	uint8_t uv_mode;
	uint8_t defrost_mode;
	uint8_t deHumi_mode;
	uint8_t wind_mode;
	uint8_t humi_mode;
	uint8_t device_temp;
	uint8_t device_humi;
	uint8_t error_code;
	uint8_t wind_speed;
	uint8_t cycle_wind_speed;
	uint8_t exhaust_wind_speed;
	uint8_t swind_mode;
	uint8_t device_work_mode;
}CONST_HUMIDEVICE_DATA;

typedef struct
{
	uint8_t power_status[2];
	uint8_t humi_set_value[2];
	uint8_t temp_set_value[2];
	uint8_t humi_fix_value[2];
	uint8_t temp_fix_value[2];
	uint8_t work_status[2];
	uint8_t envir_humi[2];
	uint8_t envir_temp[2];
	uint8_t error_code[2];
	uint8_t fix_address[2];
	uint8_t wind_speed[2];
	uint8_t cycle_wind_speed[2];
	uint8_t exhaust_wind_speed[2];
	uint8_t swind_mode[2];
	uint8_t baud[2];
	uint8_t device_work_mode[2];
	uint8_t on_time[2];
	uint8_t off_time[2];
	uint8_t force_control[2];
} HUMIDEVICE_DATA;

#pragma pack(push, 1)
typedef struct __bt_notify_data
{
	uint8_t magic[2];					//0
	uint8_t notify_type;				//2->B5
	uint8_t version[6];					//3
	uint8_t lock_mode;					//9
	uint8_t ultrasonic_threshold[2];	//10
	uint8_t lock_default_status;		//12
	uint8_t power;						//13
	uint8_t lock_status;				//14
	uint8_t ultrasonic_distance[2];		//15
	uint8_t park_status;				//17
	uint8_t mac[6];						//18
	uint8_t sta;						//24
	uint8_t error_code;					//25
	uint8_t crc[2];						//26
	uint8_t reserved[2];				//28
} BT_NOTIFY_DATA;
#pragma pack(pop)


extern DEVICE_485_type g_485_device_type;
extern HAAS_DEV_RS485 g_haas_dev_rs485[50];

extern uint8_t haas_device_num;
extern uint8_t device_no;

extern Measure_data M_value;
extern char *g_bf_code;
extern char *g_version;

char *get_bf_code();

char *check_net();
char *check_net_name();
char *check_sim();

void *data_main();
void data_init();

void on_haas_time_receive(HAAS_TIME haas_time);

void energy_init();
void energy_read();
void haas_data_read();
void haas_data_upload();

bool haas_check_wifi_config();
bool haas_check_wifi_online();
void haas_sync_time();
void haas_upload_data();
void get_Tywifi_status();

uint16_t ModbusCrc(uint8_t *data,uint16_t count);

#endif
