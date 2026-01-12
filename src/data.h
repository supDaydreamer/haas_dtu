#ifndef __DATA_H__
#define __DATA_H__

//#define DATA_FUNCTION_INTERVAL_S		(300)
#define DATA_MQTT_INTERVAL_S            (60)

#define UART_DATA_BUF_SIZE              (1024)
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
#define FILENAME				"/mnt/usr/device.conf"

#define HUMI_SAVE_DIR			"/root/humi_save"
#define HUMI_SAVE_INTERVAL_S	(60)
////////////////////////////////////////////////////////////////////////////////////////////
// Modbus监测相关定义
#define MAX_REGISTER_MAP_SIZE   100    // 最大寄存器映射数量
#define MODBUS_FRAME_TIMEOUT_MS 200    // Modbus帧超时时间(毫秒)
#define MAX_PENDING_REQUESTS    10     // 最大待匹配请求数量
#define REGISTER_VALUE_MAX_BYTES 32    // 单条监测项允许的最大原始字节数
////////////////////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
	DEVICE_485_NO_DEVICE = 0,
	DEVICE_485_AIR = 1,
	DEVICE_485_HUMI = 2
} DEVICE_485_type;

/////////////////////////////////////////////////////////

// 寄存器数据存储结构
typedef struct {
    uint8_t slave_addr;        // 从机地址
    uint16_t reg_addr;         // 起始寄存器地址
    uint8_t cmd;               // Modbus功能码
    uint8_t data_type;         // 数据类型，对应配置项
    uint16_t data_len;         // 连续寄存器数量
    uint16_t reg_values[REGISTER_VALUE_MAX_BYTES / 2]; // 每个寄存器的最新原始值
    uint32_t reg_ready_mask;   // 已更新寄存器位图
    uint8_t raw_bytes[REGISTER_VALUE_MAX_BYTES];       // 按顺序拼接的原始字节
    uint8_t raw_len;           // 原始字节长度
    double numeric_value;      // 聚合后的数值结果
    char text_value[64];       // 聚合后的文本结果
    time_t last_update;        // 最后更新时间
    bool is_valid;             // 是否已收到有效数据
} RegisterData;

// Modbus请求帧结构
typedef struct ModbusRequest {
	uint8_t slave_addr;      // 从机地址
	uint8_t function_code;   // 功能码（0x03/0x01/0x06/0x10）
	uint16_t start_reg;      // 起始寄存器/线圈地址
	uint16_t reg_count;      // 连续寄存器/线圈数量
	time_t timestamp;        // 入队时间（用于超时清理）
	bool is_valid;           // 槽位是否有效
} ModbusRequest;

// 寄存器映射表定义
typedef struct {
    uint8_t slave_addr;        // 从机地址
    uint16_t reg_addr;         // 起始寄存器地址
    char name[32];             // 寄存器名称
    uint8_t data_type;         // 数据类型: 0=16位数值, 1=32位数值, 2=ASCII字符串
    uint8_t cmd;               // 功能码
    uint16_t data_len;         // 连续寄存器数量
    bool enabled;              // 是否启用监测
} RegisterMap;

/////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    uint8_t index;
    uint8_t type;
    uint8_t cmd;
    uint8_t dev_add;
    uint16_t reg_add;
    uint16_t data_len;
    uint16_t value1;
    uint8_t data_s;
    float value2;
    double value_numeric;      // 聚合后的数值
    char value_text[64];       // 聚合后的文本
    uint8_t is_string;         // 1 表示当前值为字符串
} HAAS_DEV_RS485;

typedef struct {
    uint8_t year;    // Time[1] 年份 (0x00 表示 2000 年)
    uint8_t month;   // Time[2] 月份 (1-12)
    uint8_t day;     // Time[3] 日期 (1-31)
    uint8_t hour;    // Time[4] 小时 (0-23)
    uint8_t minute;  // Time[5] 分钟 (0-59)
    uint8_t second;  // Time[6] 秒钟 (0-59)
    uint8_t week;    // Time[7] 星期 (1-7，1 表示星期一)
} HAAS_TIME;

typedef struct measure_data {
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
extern int dev_type;
extern HAAS_DEV_RS485 g_haas_dev_rs485[100];

extern uint8_t haas_device_num;
extern uint8_t device_no;

extern Measure_data M_value;
extern char *g_bf_code;
extern char *g_version;

extern uint16_t measure_value;

extern uint16_t measure_value_last;

extern uint16_t DATA_FUNCTION_INTERVAL_S;
//////////////////////////////////////////////////////////////////////////////
// Modbus监测相关全局变量
extern RegisterData g_register_data[MAX_REGISTER_MAP_SIZE];
extern RegisterMap g_register_map[MAX_REGISTER_MAP_SIZE];

extern ModbusRequest g_pending_requests[MAX_PENDING_REQUESTS];
extern uint8_t g_register_count;
extern uint8_t g_register_map_count;
/////////////////////////////////////////////////////////////////////////////

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
void haas_data_debug_print();
void haas_data_detect();
void haas_energy_type2_init(void);
void haas_energy_type2_poll(void);

bool haas_check_wifi_config();
bool haas_check_wifi_online();
void haas_sync_time();
void haas_upload_data();
void get_Tywifi_status();

void haas_data_cal(void);
void haas_data_display_cmd(void);
void humi_device_control(uint8_t cmd);
void air_device_control(uint8_t cmd);
void haas_device_control(uint8_t device_type, uint8_t slave_addr, uint16_t reg_addr,
                         uint16_t data, uint32_t uartx);
void haas_register_map_prepare(void);
void haas_store_register_values(uint8_t slave_addr, uint16_t reg_addr, uint8_t cmd,
                                const uint16_t *values, uint16_t count);
void haas_store_bit_values(uint8_t slave_addr, uint16_t reg_addr, uint8_t cmd,
                           const uint8_t *bits, uint16_t count);

uint16_t ModbusCrc(uint8_t *data,uint16_t count);
///////////////////////////////////////////////////////////////////////////////////////////////
// Modbus monitor helper functions
bool add_register_map(uint8_t slave_addr, uint16_t reg_addr, const char *name,
                      uint8_t data_type, uint8_t cmd);
RegisterData* get_register_data(uint8_t slave_addr, uint16_t reg_addr, uint8_t cmd);
RegisterData* get_register_data_by_index(int index);  // access by config order (0=dev01, 1=dev02, ...)
void print_all_register_data(void);
void clear_register_data(void);
///////////////////////////////////////////////////////////////////////////////////////////////

void vision_store_sample(float length, float width);
void vision_check_timeout(void);
bool vision_is_upload_pending(void);
int vision_prepare_upload_snapshot(void);
void vision_clear_upload_snapshot(void);
int vision_get_upload_count(void);
float vision_get_upload_length(int index);
float vision_get_upload_width(int index);


#endif

