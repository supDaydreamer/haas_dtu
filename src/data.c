#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "data.h"
#include "uart.h"
#include "mqtt.h"
#include "haas_mqtt.h"
#include "wifi.h"
#include "system.h"
#include "mcu_api.h"
#include "bf_cmd.h"
#include "ini.h"
#include "curl.h"
#include "json.h"
#include "MQTTAsync.h"

DEVICE_485_type g_485_device_type = DEVICE_485_NO_DEVICE;
int dev_type = 0;

HAAS_DEV_RS485 g_haas_dev_rs485[100];
uint8_t haas_device_num = 0;
uint8_t device_no = 0;
time_t s_haas_data_send_time = 0;

uint16_t DATA_FUNCTION_INTERVAL_S = 300;

uint16_t measure_value = 0;
uint16_t measure_value_last = 0;

char *g_bf_code = NULL;
char *g_version = NULL;
Measure_data M_value = {0};

static char read_energy_type_cmd[] = {0x05,0x03,0x10,0x00,0x00,0x04,0x41,0x4d};
static char read_energy_params_cmd[] = {0x05,0x03,0x10,0x10,0x00,0x0B,0x00,0x8C};
static char write_energy_restart_cmd[] = {0x05,0x10,0x10,0x18,0x00,0x01,0x02,0x00,0x03,0xC6,0x88};
static char set_ThreePrase_cmd[] = {};
//static char read_measure_data_cmd[] = {0xFF,0x03,0x15,0x00,0x00,0x18,0x54,0x12};
static char read_measure_data_cmd[] = {0x05,0x03,0x15,0x00,0x00,0x1A,0xC1,0x89};

static char work_quantity[] ={0x06,0x03,0x02,0x00,0x00,0x00,0x00,0x00,0x00};

// Pre-built frames for energy-meter initialization when RS485_type == 2/////////////////////////////////////////////////////////
static const uint8_t kEnergyInitBaud9600[]   = {0xFF,0x10,0x10,0x11,0x00,0x01,0x02,0x00,0x01,0x3D,0x74};
static const uint8_t kEnergyInitVoltGain[]   = {0xFF,0x10,0x10,0x12,0x00,0x02,0x04,0x3F,0x80,0x00,0x00,0x84,0xAD};
static const uint8_t kEnergyInitCurrGain[]   = {0xFF,0x10,0x10,0x14,0x00,0x02,0x04,0x3F,0x80,0x00,0x00,0x04,0x87};
static const uint8_t kEnergyInitDataInt[]    = {0xFF,0x10,0x10,0x16,0x00,0x01,0x02,0x00,0x00,0xFD,0x03};
static const uint8_t kEnergyInitDataFloat[]  = {0xFF,0x10,0x10,0x16,0x00,0x01,0x02,0x00,0x01,0x3C,0xC3};
static const uint8_t kEnergyInitClearStart[] = {0xFF,0x10,0x10,0x18,0x00,0x01,0x02,0x00,0x03,0xBC,0x2C};
static const uint8_t kEnergyInitClearStop[]  = {0xFF,0x10,0x10,0x18,0x00,0x01,0x02,0x00,0x02,0x7D,0xEC};
static const uint8_t kEnergyInitAddr10[]     = {0xFF,0x10,0x10,0x10,0x00,0x01,0x02,0x00,0x0A,0x7D,0x62};

typedef struct {
	const char *desc;
	const uint8_t *frame;
	size_t len;
} EnergyInitFrame;

static const EnergyInitFrame k_energy_init_frames[] = {
    {"set_addr_10", kEnergyInitAddr10,     sizeof(kEnergyInitAddr10)},
    {"baud_9600",   kEnergyInitBaud9600,   sizeof(kEnergyInitBaud9600)},
    {"vt_1_0",      kEnergyInitVoltGain,   sizeof(kEnergyInitVoltGain)},
    {"ct_1_0",      kEnergyInitCurrGain,   sizeof(kEnergyInitCurrGain)},
    {"data_int",    kEnergyInitDataInt,    sizeof(kEnergyInitDataInt)},
    {"data_float",  kEnergyInitDataFloat,  sizeof(kEnergyInitDataFloat)},
    {"clear_start", kEnergyInitClearStart, sizeof(kEnergyInitClearStart)},
    {"clear_stop",  kEnergyInitClearStop,  sizeof(kEnergyInitClearStop)},
};

static bool s_energy_type2_init_done = false;
static const unsigned int k_type2_poll_interval_us = 1000 * 1000;  // 1s

// Modbus监测函数前向声明
static bool check_modbus_crc(uint8_t *data, size_t len);
static bool parse_modbus_request(uint8_t *data, size_t len, ModbusRequest *req);
static void add_pending_request(ModbusRequest *req);
static void cleanup_timeout_requests(void);
static ModbusRequest* match_response_with_request(uint8_t slave_addr, uint8_t function_code);
static void store_register_data(uint8_t slave_addr, uint16_t reg_addr, uint16_t value, uint8_t cmd);
static bool parse_modbus_response(uint8_t *data, size_t len);
static void init_register_map(void);
static void ensure_register_map_initialized(void);
static void process_modbus_sniffer_data(uint8_t *data, size_t len);
static uint16_t clamp_data_len(uint16_t requested_len);
static bool recalc_register_outputs(RegisterData *slot);
static void sync_register_to_rs485(int index, RegisterData *slot, const RegisterMap *map,
                                   bool aggregated, uint16_t last_word);
void haas_energy_type2_init(void)
{
	if (s_energy_type2_init_done) {
		return;
	}

	ensure_register_map_initialized();

	static const size_t k_type2_init_index[] = {0, 1, 2, 3, 4, 6};

	for (size_t i = 0; i < sizeof(k_type2_init_index) / sizeof(k_type2_init_index[0]); ++i) {
		const EnergyInitFrame *frame = &k_energy_init_frames[k_type2_init_index[i]];
		dbg_printf("[RS485Type2] Init step: %s (%zu bytes)\n", frame->desc, frame->len);
		uart_tx(2, (uint8_t *)frame->frame, frame->len);
		sleep(1);
	}

	s_energy_type2_init_done = true;
}

static bool is_type2_read_cmd(uint8_t cmd)
{
	switch (cmd) {
	case 0x01:  // Read Coils
	case 0x02:  // Read Discrete Inputs
	case 0x03:  // Read Holding Registers
	case 0x04:  // Read Input Registers
		return true;
	default:
		return false;
	}
}

static void send_type2_read_request(const RegisterMap *map)
{
	if (!map || !map->enabled) {
		return;
	}

	if (!is_type2_read_cmd(map->cmd)) {
		dbg_printf("[RS485Type2] Skip cmd 0x%02X for %s\n", map->cmd, map->name);
		return;
	}

	uint16_t quantity = map->data_len;
	if (quantity == 0) {
		quantity = 1;
	}

	uint8_t frame[8] = {0};
	frame[0] = map->slave_addr;
	frame[1] = map->cmd;
	frame[2] = (map->reg_addr >> 8) & 0xFF;
	frame[3] = map->reg_addr & 0xFF;
	frame[4] = (quantity >> 8) & 0xFF;
	frame[5] = quantity & 0xFF;

	uint16_t crc = ModbusCrc(frame, 6);
	frame[6] = crc & 0xFF;
	frame[7] = (crc >> 8) & 0xFF;

	dbg_printf("[RS485Type2] Poll %s Addr:0x%02X Reg:0x%04X Cmd:0x%02X Qty:%u\n",
	           map->name, map->slave_addr, map->reg_addr, map->cmd, quantity);
	uart_tx(2, frame, sizeof(frame));

	ModbusRequest req = {
		.slave_addr = map->slave_addr,
		.function_code = map->cmd,
		.start_reg = map->reg_addr,
		.reg_count = quantity,
		.timestamp = time(NULL),
		.is_valid = true
	};
	add_pending_request(&req);
}

void haas_energy_type2_poll(void)
{
	ensure_register_map_initialized();

	for (int i = 0; i < g_register_map_count; ++i) {
		send_type2_read_request(&g_register_map[i]);
		usleep(k_type2_poll_interval_us);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t uartCom_Status = 0;
uint8_t uartReceive_length = 0;
uint8_t uartControl_index = 0;  // 0-----energy   1 --- humi device

static bool s_waiting_energy_type = false;
static bool s_waiting_energy_param = false;
static bool s_waiting_energy_zero_fix = false;
static bool s_waiting_energy_read = false;
static bool s_waiting_haas_config = false;
//static bool s_waiting_haas_online = false;
static bool s_waiting_haas_sync_time = false;
//static bool s_waiting_haas_upload_data = false;

//read haas device command

const char read_haas_th_cmd[] = {0x05,0x03,0x10,0x00,0x00,0x04,0x41,0x4d};
const char read_haas_temp_cmd[] = {0x05,0x03,0x10,0x00,0x00,0x04,0x41,0x4d};

static bool s_waiting_haas_th = false;
//static bool s_waiting_haas_temp = false;

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Modbus监测相关全局变量
RegisterData g_register_data[MAX_REGISTER_MAP_SIZE] = {0};
RegisterMap g_register_map[MAX_REGISTER_MAP_SIZE] = {0};
ModbusRequest g_pending_requests[MAX_PENDING_REQUESTS] = {0};
uint8_t g_register_count = 0;
uint8_t g_register_map_count = 0;
static bool s_register_map_initialized = false;

// Modbus帧缓冲
static uint8_t s_modbus_rx_buffer[256] = {0};
static uint8_t s_modbus_rx_index = 0;
static time_t s_last_rx_time = 0;

////////////////////////////////////////////////////////////////////////////////
void on_haas_time_receive(HAAS_TIME haas_time)
{
	static char date_cmd[32] = {0};
	snprintf(date_cmd, sizeof(date_cmd), "date -s '%04u-%02u-%02u %02u:%02u:%02u'",
		haas_time.year + 2000,
		haas_time.month,
		haas_time.day,
		haas_time.hour,
		haas_time.minute,
		haas_time.second
	);
	dbg_printf("----------> receive haas time: %s\n", date_cmd);
	system(date_cmd);

	s_waiting_haas_sync_time = false;
}

char *get_version()
{
	static char version[64] = {0};
	memset(version, '\0', sizeof(version));

	FILE *fp = fopen(VERSION_FILE, "r");
	if (!fp) {
		snprintf(version, sizeof(version), "UNKNOW_VERSION");
		return version;
	}
	fread(version, sizeof(version), 1, fp);
	fclose(fp);

	return version;
}

char *get_bf_code()
{
	static char bf_code[32] = {0};
	memset(bf_code, '\0', sizeof(bf_code));

	FILE *fp = fopen(BF_CODE_FILE, "r");
	if (!fp) {
		snprintf(bf_code, sizeof(bf_code), "UNKNOW_%ld", random());
		return bf_code;
	}
	fread(bf_code, sizeof(bf_code), 1, fp);
	fclose(fp);

	size_t len = strlen(bf_code);
	if (len > sizeof(bf_code)) len = sizeof(bf_code);
	while (len > 0) {
		char *last_char_p = &bf_code[len - 1];
		if (*last_char_p == '\n'
				|| *last_char_p == '\r'
				|| *last_char_p == '\t'
				|| *last_char_p == '\v'
				|| *last_char_p == '\a'
				|| *last_char_p == '\f'
				|| *last_char_p == '\b'
				|| *last_char_p == ' '
		   ) {
			printf("[%s] trim bf_code last char @ %zu: 0x%02x\n", __FUNCTION__, (len - 1), *last_char_p);
			*last_char_p = '\0';
		} else {
			break;
		}
		len = strlen(bf_code);
		if (len > sizeof(bf_code)) len = sizeof(bf_code);
	}

	if (strlen(bf_code) <= 0) {
		snprintf(bf_code, sizeof(bf_code), "UNKNOW_%lu", random());
		return bf_code;
	}

	return bf_code;
}

static char *store_buf(uint8_t *buf, size_t len)
{
	static char s_buf[1024] = {0};
	size_t buf_size = 0;
	if (buf != NULL && len > 0) {
		for (size_t i = 0; i < len; i++) {
			snprintf(&s_buf[buf_size], sizeof(s_buf), "%02X ", buf[i]);
			buf_size += 3;
		}
		s_buf[buf_size - 1] = '\0';
	} else {
		s_buf[0] = '\0';
	}
	//dbg_printf("========> %s: %s\n", __FUNCTION__, s_buf);
	return s_buf;
}

void print_buf(uint8_t *buf, size_t len)
{
	dbg_printf("\033[35m");
	if (buf != NULL && len > 0) {
		for (size_t i = 0; i < len; i++) {
			dbg_printf("%02X ", buf[i] & 0xFF);
		}
	} else {
		dbg_printf("(__NULL__)");
	}
	dbg_printf("\033[0m\n");
}

void energy_process(uint8_t *data, size_t len)
{
	switch (len) {
	case 13:
		if((data[1] == 0x03) && (data[9] == 0x2e))
			s_waiting_energy_type = false;
		break;
	case 27:
		if((data[1] == 0x03) && (data[2] == 0x16))
			s_waiting_energy_param = false;
		break;
	case 8:
		if((data[1] == 0x10) && (data[2] == 0x10))
			s_waiting_energy_zero_fix = false;
		break;
	case 57:
		if((data[1] == 0x03) && (data[2] == 0x34))
			s_waiting_energy_read = false;

		M_value.dev_voltage1 = data[5];
		M_value.dev_voltage1 = (M_value.dev_voltage1 << 8) + data[6];

		M_value.dev_voltage2 = data[9];
		M_value.dev_voltage2 = (M_value.dev_voltage2 << 8) + data[10];

		M_value.dev_voltage3 = data[13];
		M_value.dev_voltage3 = (M_value.dev_voltage3 << 8) + data[14];

		M_value.dev_current1 = data[17];
		M_value.dev_current1 = (M_value.dev_current1 << 8) + data[18];
		M_value.dev_current1 = M_value.dev_current1/10;

		M_value.dev_current2 = data[21];
		M_value.dev_current2 = (M_value.dev_current2 << 8) + data[22];
		M_value.dev_current2 = M_value.dev_current2/10;

		M_value.dev_current3 = data[25];
		M_value.dev_current3 = (M_value.dev_current3 << 8) + data[26];
		M_value.dev_current3 = M_value.dev_current3/10;

		M_value.factor = data[33];
		M_value.factor = (M_value.factor << 8) + data[34];

		printf("test receive data for voltage factor!!!!!!!!!!!!!!!!!!!!!!<%d,%d>\r\n",data[33],data[34]);
		long power_temp_value = data[27];
		power_temp_value = (power_temp_value << 8) + data[28];
		power_temp_value = (power_temp_value << 8) + data[29];
		power_temp_value = (power_temp_value << 8) + data[30];
		if (power_temp_value < 0) {
			power_temp_value = power_temp_value * (-1);
		}

		//M_value.dev_power_value = data[45];
		//M_value.dev_power_value = (M_value.dev_power_value << 8) + data[46];
		M_value.dev_power_value = power_temp_value/10;

		long power_ele_temp_value = data[47];
		power_ele_temp_value = (power_ele_temp_value << 8) + data[48];
		power_ele_temp_value = (power_ele_temp_value << 8) + data[49];
		power_ele_temp_value = (power_ele_temp_value << 8) + data[50];
		if (power_ele_temp_value < 0) {
			power_ele_temp_value = power_ele_temp_value * (-1);
		}
		//printf("5-power_ele_temp_value:%04x\r\n",power_ele_temp_value);
		//printf("00000-power_ele_temp_value:%02x-%02x-%02x-%02x\r\n",power_ele_temp_value);

		M_value.dev_ele_times = data[51];
		M_value.dev_ele_times = (M_value.dev_ele_times << 8) + data[52];
		M_value.dev_ele_times = (M_value.dev_ele_times << 8) + data[53];
		M_value.dev_ele_times = (M_value.dev_ele_times << 8) + data[54];
	//	printf("111111111111111111-power_ele_temp_value:%d\r\n",power_ele_temp_value);
		//float power_val = M_value.dev_ele_times/3600.0;
		//float power_ele_value = power_ele_temp_value/10.0;
		M_value.dev_power_ele = power_ele_temp_value/10;
		//printf("222222222222222222-power_ele_temp_value:%d\r\n",M_value.dev_power_ele);
		//M_value.dev_power_ele = M_value.dev_power_ele * power_val;
		//printf("3333333333333333333-power_ele_temp_value:%d\r\n",M_value.dev_power_ele);
		M_value.dev_power_ele_mqtt = M_value.dev_power_ele;
		printf("test receive data for ele!!!!!!!!!!!!!!!!!!!!!!<%d,%d>\r\n",M_value.dev_power_ele,M_value.dev_ele_times);

		//				if(M_value.factor == 0)
		//				{
		//					M_value.factor = 1;
		//				}
		//				M_value.dev_power_ele = Measure_temp/M_value.factor;
		//M_value.dev_power_ele = M_value.dev_power_ele;
		printf("test receive data for 485!!!!!!!!!!!!!!!!!!!!!!<%d>\r\n",data[2]);
		//sprintf(M_value.read_vol_c,"111");
		sprintf(M_value.read_vol_c,"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",data[5],data[6],data[9],data[10],data[13],data[14],data[33],data[34],data[45],data[46],data[49],data[50]);
		printf("test receive data for 485 end!!!!<1111>\r\n");
		break;
	default:
		break;
	}
}

void humi_process(uint8_t *data, size_t len)
{
	uint16_t crc = ModbusCrc(data, len - 2);
	//dbg_printf("\e[31m======= humi_process (%u): ======\e[0m\n", len);
	//for (size_t i = 0; i < len; i++) {
	//	dbg_printf("%02X ", data[i]);
	//}
	//dbg_printf("\n");
	if ((data[0] == 0x01) && (data[1] == 0x03 || data[1] == 0x06) && (data[len - 2] == (crc & 0xFF)) && (data[len - 1] == (crc >> 8))) {
		dbg_printf("\e[32m======= uart 2 read (%u) humi data: ======\e[0m\n", len);
		for (size_t i = 0; i < len; i++) {
			dbg_printf("%02X ", data[i]);
		}
		dbg_printf("\n\e[32m=======------------------=========\e[0m\n");
		uint8_t reg_len = data[2];
		uint8_t *reg_p = &data[3];
		if (data[1] == 0x03 && reg_len == 2) {
			// humi on/off data
			if (reg_p[1] == 0xAA) {
				humiDevice.power_sta = 1;
			} else if (reg_p[1] == 0x55) {
				humiDevice.power_sta = 0;
			}
			humiDevice.data_update = 1;
		} else if (data[1] == 0x03 && reg_len == 0x26) {
			// humi all data
			HUMIDEVICE_DATA *humi = (HUMIDEVICE_DATA *)reg_p;

			//humiDevice.switch_mode = from-mqtt;
			//humiDevice.setHumi_value = from-mqtt;
			//humiDevice.control_mode = from-mqtt;

			humiDevice.power_sta = humi->power_status[1];
			dbg_printf("\e[32m\t##### power_status => %u\e[0m\n", humiDevice.power_sta);
			humiDevice.getHumi_value = humi->humi_set_value[1];
			dbg_printf("\e[32m\t##### humi_set_value => %u\e[0m\n", humiDevice.getHumi_value);
			humiDevice.get_control_mode = humi->force_control[1];
			dbg_printf("\e[32m\t##### force_control => %u\e[0m\n", humiDevice.get_control_mode);

			humiDevice.work_mode = humi->device_work_mode[1];
			dbg_printf("\e[32m\t##### device_work_mode => %u\e[0m\n", humiDevice.work_mode);
			humiDevice.error_code = humi->error_code[1];
			dbg_printf("\e[32m\t##### error_code => %u\e[0m\n", humiDevice.error_code);
			humiDevice.windSpeed = humi->wind_speed[1];
			dbg_printf("\e[32m\t##### wind_speed => %u\e[0m\n", humiDevice.windSpeed);
			humiDevice.swing_mode = humi->swind_mode[1];
			dbg_printf("\e[32m\t##### swind_mode => %u\e[0m\n", humiDevice.swing_mode);
			humiDevice.windspeed_loop = humi->cycle_wind_speed[1];
			dbg_printf("\e[32m\t##### cycle_wind_speed => %u\e[0m\n", humiDevice.windspeed_loop);
			humiDevice.windspeed_ex = humi->exhaust_wind_speed[1];
			dbg_printf("\e[32m\t##### exhaust_wind_speed => %u\e[0m\n", humiDevice.windspeed_ex);
			humiDevice.data_update = 1;
		}
	}
}

void haas_device_dataRead1(uint8_t *data)
{
	for(int i=0;i<11;i++)
		{
			WorkOrder.WorkOrder_No[i] = data[i+7];
		}
	 if(WorkOrder.WorkOrder_No[0] == 'M')
		{
			WorkOrder.WorkOrder_No[1] = 'O';
			printf("receive WorkOrder.WorkOrder_No:%s\r\n",WorkOrder.WorkOrder_No);
			http_req_f = 1;
		}

}


void haas_device_dataRead(uint8_t *data)
{
//	if((data[0] == 0x03) && (data[1] == 0x10))
//		{
	//	  snprintf(WorkOrder.WorkOrder_No,12,data[7]);
//		  for(int i=0;i<12;i++)
//		  {
//			  WorkOrder.WorkOrder_No[i] = data[i+7];
//		  }
//		  if(WorkOrder.WorkOrder_No[0] == 'M')
//		  {
//			printf("receive WorkOrder.WorkOrder_No:%s\r\n",WorkOrder.WorkOrder_No);
//			http_req_f = 1;
//		  }
//		  return;
//		}
	if((data[1] == 0x03)&&(device_no > 0))
	{
	uint8_t addr = device_no - 1;
	g_haas_dev_rs485[addr].value1 = data[3] << 8;
	g_haas_dev_rs485[addr].value1 += data[4];
	printf("receive data is:%d\r\n",g_haas_dev_rs485[addr].type);
		if(g_haas_dev_rs485[addr].type == 1)
			{
				if(g_haas_dev_rs485[addr].value1 > 4000)
				{
				g_haas_dev_rs485[addr].value1 -=4000;
				g_haas_dev_rs485[addr].data_s = 0;
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/100.0;
				}
				else
				{
					g_haas_dev_rs485[addr].value1 = 4000 -g_haas_dev_rs485[addr].value1;
					g_haas_dev_rs485[addr].data_s = 1;
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/100.0;
					g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value2 * -1;
				}
			//	g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/100.0;
				
			}
		else if (g_haas_dev_rs485[addr].type == 2)
			{
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/100.0;
			}
		else if (g_haas_dev_rs485[addr].type == 3)
			{
				printf("type is 3,receive data ok!");
				//g_haas_dev_rs485[addr].value1 -=4000;
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/100.0;
				if(g_haas_dev_rs485[addr].value2 > 200)
				{
					g_haas_dev_rs485[addr].value2 = 200.0;
				}
					
			}
		else if (g_haas_dev_rs485[addr].type == 4)
			{
				printf("11111------------------receive data is:%d\r\n",g_haas_dev_rs485[addr].value1);
				if(g_haas_dev_rs485[addr].value1 > 32767)
				{
					 printf("22222------------------receive data is:%d\r\n",g_haas_dev_rs485[addr].value1);
					g_haas_dev_rs485[addr].data_s = 1;
					g_haas_dev_rs485[addr].value1 = g_haas_dev_rs485[addr].value1 - 32768;
					printf("22111------------------receive data is:%d\r\n",g_haas_dev_rs485[addr].value1);
					g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/10.0;
					printf("2333------------------receive data is:%.1f\r\n",g_haas_dev_rs485[addr].value2);
					g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value2 * (-1.0);
					printf("444------------------receive data is:%.1f\r\n",g_haas_dev_rs485[addr].value2);
				}
				else
				{
					g_haas_dev_rs485[addr].data_s = 0;
			//	g_haas_dev_rs485[addr].value1 = g_haas_dev_rs485[addr].value1 & 0;
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/10.0;
				}
				if(g_haas_dev_rs485[addr].value2 > 200)
				{
					g_haas_dev_rs485[addr].value2 = 200.0;
					g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value2 - 1.0;
				}
			}
		else if(g_haas_dev_rs485[addr].type == 5)
		{
		  if(g_haas_dev_rs485[addr].value1 == 63488)
		  {
		   g_haas_dev_rs485[addr].value1 = 0;
		   g_haas_dev_rs485[addr].value2 = 0.0;
		  }
		  else if (g_haas_dev_rs485[addr].value1 > 32767)
		  {
				g_haas_dev_rs485[addr].value1 = 65536 - g_haas_dev_rs485[addr].value1;
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/10.0;
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value2 * (-1.0);
		  }
		  else
		  {
		     g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/10.0;
		  }
		}
		printf("receive data is:%d,%.1f\r\n",g_haas_dev_rs485[addr].value1,g_haas_dev_rs485[addr].value2);
		s_waiting_haas_th = false;
	}
}



void on_uart_2_read(uint8_t *data, size_t len)
{
	uart_rx_publish(2, store_buf(data, len));

	extern uint8_t RS485_type;

	// 打印 UART2 收到的数据
	dbg_printf("======= uart 2 read %u: ======\n", (unsigned int)len);
	for (size_t i = 0; i < len; i++) {
		dbg_printf("%02X ", data[i]);
	}
	dbg_printf("\n=======------------------=========\n");

	if (RS485_type == 1) {
		/* 如果收到目标序列的前10字节，则发送指定响应 10 次，每次间隔 50ms
		 * 触发序列（前10字节）：03 10 00 00 00 06 0C 4D 4F 32
		 * 响应帧（9字节）：06 03 04 00 09 30 31 89 25
		 */
		static const uint8_t _trigger_prefix[10] = {
			0x03, 0x10, 0x00, 0x00, 0x00, 0x06, 0x0C, 0x4D, 0x4F, 0x32
		};
		static const uint8_t _response_frame[9] = {
			0x06, 0x03, 0x04, 0x00, 0x14, 0x30, 0x31,0x19,0x23
		};

		if (len >= 10 && memcmp(data, _trigger_prefix, 10) == 0) {
			/* 发送10次响应，每次间隔50ms */
			for (int _i = 0; _i < 10; ++_i) {
				uart_tx(1, (uint8_t*)_response_frame, sizeof(_response_frame));
				/* 使用 usleep 以毫秒为单位等待 50ms */
				usleep(50 * 1000);
			}
		}
		process_modbus_sniffer_data(data, len);
		return;
	}


	haas_device_dataRead(data);

	//humi_process(data, len);
}

void on_uart_1_write(uint8_t *data, size_t len)
{
	uart_tx_publish(1, store_buf(data, len));
}

void on_uart_2_write(uint8_t *data, size_t len)
{
	uart_tx_publish(2, store_buf(data, len));

	dbg_printf("======= uart 2 write %u: ======\n", len);
	for (size_t i = 0; i < len; i++) {
		dbg_printf("%02X ", data[i]);
	}
	dbg_printf("\n=======------------------=========\n");
}

void data_init()
{
	dbg_printf("\033[0m");

	g_bf_code = get_bf_code();
	g_version = get_version();

	if (0 == access(CONFIG_FILE, F_OK)) {
		g_485_device_type = GetIniKeyInt("cfg", "device_type", CONFIG_FILE);
		dbg_printf(">>> read device_type: %u\n", g_485_device_type);
	} else {
		dbg_printf(">>> no config file!\n");
	}

	if (0 == access(FILENAME, F_OK)) {
		dev_type = GetIniKeyInt("config", "dev_type", FILENAME);
		dbg_printf(">>> read dev_type: %d\n", dev_type);
	} else {
		dbg_printf(">>> no device.conf, dev_type default 0\n");
	}
}

char *check_net()
{
	int res = -1;

	res = system("[ $(ifconfig 3g-ppp | grep 'inet addr' | wc -l) -gt 0 ]");
	if (res == 0) return "main";

	res = system("[ $(ifconfig apcli0 | grep 'inet addr' | wc -l) -gt 0 ]");
	if (res == 0) return "wifi";

	res = system("[ $(ifconfig eth0.2 | grep 'inet addr' | wc -l) -gt 0 ]");
	if (res == 0) return "ethernet";

	return "unknown";
}

char *check_net_name()
{
	return "0";
}

char *check_sim()
{
	static char sim_buf[128] = {0};
	const char *cmd = "echo -e 'opengt\nset com 115200n81\nset comecho off\nset senddelay 0.02\nwaitquiet 0.2 0.2\nflash 0.1\n:start\nsend \"ATI^mAT+QCCID^m\"\nget 1 \"\" $s\nprint $s\n:continue\nexit 0' > /tmp/at.gcom && if [ -c /dev/ttyUSB3 ];then comgt -d /dev/ttyUSB3 -s /tmp/at.gcom | awk '/QCCID:/{print $2}' | tr -d ' \r\n';else comgt -d /dev/ttyUSB1 -s /tmp/at.gcom | awk '/QCCID:/{print $2}' | tr -d ' \r\n';fi";

	FILE *fp;
	fp = popen(cmd, "r");
	if (fp == NULL) {
		dbg_printf("%s: Failed to run command\n", __FUNCTION__);
		return "?";
	}

	memset(sim_buf, '\0', sizeof(sim_buf));
	fread(sim_buf, 1, sizeof(sim_buf), fp);

	pclose(fp);

	if (strlen(sim_buf) == 0) sim_buf[0] = '0';

	return sim_buf;
}

uint16_t ModbusCrc(uint8_t *data,uint16_t count)
{
   uint16_t crc = 0xffff;
   uint16_t polynomial = 0xa001;
   for (uint16_t i = 0; i < count; i++) {
		crc ^= data[i];
		for (uint16_t j = 0; j < 8; j++) {
			if (crc & 0x0001) {
				crc >>= 1;
				crc ^= polynomial;
			} 
			else {
				crc >>= 1;
			}
		}
	}
	return crc;    //crc = crc16(buffer, sizeof(buffer));
}

// 通用控制入口：device_type==1 时控制开关量设备（功能码 0x05）
void haas_device_control(uint8_t device_type,
                         uint8_t slave_addr,
                         uint16_t reg_addr,
                         uint16_t data,
                         uint32_t uartx)
{
	switch (device_type) {
	case 1: {  // 开关量设备
		if (reg_addr <= 3) {
			// data 非零视为上电(0000)，零视为断电(FF00)
			static uint8_t frame[8];
			const uint16_t coil_value = (data != 0) ? 0x0000 : 0xFF00;

			frame[0] = slave_addr;
			frame[1] = 0x05;
			frame[2] = (reg_addr >> 8) & 0xFF;
			frame[3] = reg_addr & 0xFF;
			frame[4] = (coil_value >> 8) & 0xFF;
			frame[5] = coil_value & 0xFF;

			uint16_t crc = ModbusCrc(frame, 6);
			frame[6] = crc & 0xFF;
			frame[7] = (crc >> 8) & 0xFF;

			uart_tx(uartx, frame, sizeof(frame));
			dbg_printf("[HAAS_CTRL] ch:%u uart:%u tx:", reg_addr+1, uartx);
			for (size_t i = 0; i < sizeof(frame); i++) {
				dbg_printf(" %02X", frame[i]);
			}
			dbg_printf("\n");
		} else if (reg_addr == 4) {
			// 4 表示控制全部继电器，上电使用 0x00，断电使用 0xFF
			static uint8_t frame_all[10];
			frame_all[0] = slave_addr;
			frame_all[1] = 0x0F;
			frame_all[2] = 0x00;
			frame_all[3] = 0x00;
			frame_all[4] = 0x00;
			frame_all[5] = 0x08;
			frame_all[6] = 0x01;
			frame_all[7] = (data != 0) ? 0x00 : 0xFF;

			uint16_t crc_all = ModbusCrc(frame_all, 8);
			frame_all[8] = crc_all & 0xFF;
			frame_all[9] = (crc_all >> 8) & 0xFF;

			uart_tx(uartx, frame_all, sizeof(frame_all));
			dbg_printf("[HAAS_CTRL] ch:ALL uart:%u tx:", uartx);
			for (size_t i = 0; i < sizeof(frame_all); i++) {
				dbg_printf(" %02X", frame_all[i]);
			}
			dbg_printf("\n");
		} else {
			dbg_printf("[HAAS_CTRL] invalid reg_addr: %u\n", reg_addr);
		}
		break;
	}
	default:
		dbg_printf("[HAAS_CTRL] unsupported device_type: %u\n", device_type);
		break;
	}
}


void humi_device_control(uint8_t cmd)
{
	uint16_t crc = 0;
	uint8_t *send_data_p = NULL;
	size_t send_data_len = 0;
	switch(cmd)
	{
		case 0x01:    //read data
			{
				static uint8_t s_send_data[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x13, 0xBF, 0xFB };
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x02:    //read ON/OFF
			{
				static uint8_t s_send_data[] = { 0x01, 0x03, 0x00, 0xEE, 0x00, 0x01, 0xBF, 0xFB };
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x03:    //ON/OFF control
			{
				static uint8_t s_send_data[] = { 0x00, 0x06, 0x00, 0xEE, 0x00, 0xFF, 0xBF, 0xFB };
				if (humiDevice.switch_mode) {
					// ON
					s_send_data[5] = 0xAA;
				} else {
					// OFF
					s_send_data[5] = 0x55;
				}
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x04:    //humi set
			{
				static uint8_t s_send_data[] = { 0x01, 0x06, 0x00, 0x01, 0x00, 0xFF, 0xBF, 0xFB };
				s_send_data[5] = humiDevice.setHumi_value;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x05:    //Constant humidity work mode
			{
				static uint8_t s_send_data[] = { 0x01, 0x06, 0x00, 0x12, 0x00, 0xFF, 0xBF, 0xFB };
				s_send_data[5] = humiDevice.control_mode;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x06:    //Circulating wind speed
			{
				static uint8_t s_send_data[] = { 0x01, 0x06, 0x00, 0x0B, 0x00, 0xFF, 0xBF, 0xFB };
				s_send_data[5] = humiDevice.windspeed_loop;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x07:    //Exhaust wind speed
			{
				static uint8_t s_send_data[] = { 0x01, 0x06, 0x00, 0x0C, 0x00, 0xFF, 0xBF, 0xFB };
				s_send_data[5] = humiDevice.windspeed_ex;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x08:    //swing on-off
			{
				static uint8_t s_send_data[] = { 0x01, 0x06, 0x00, 0x0D, 0x00, 0xFF, 0xBF, 0xFB };
				s_send_data[5] = humiDevice.swing_mode;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		default:
			break;
	}

	if ((send_data_p != NULL) && (send_data_len > 0)) {
		//printf("22222-----------------%p,%d\r\n",send_data_p,send_data_len);
		crc = ModbusCrc(send_data_p, send_data_len - 2);
		send_data_p[send_data_len - 2] = crc & 0xFF;
		send_data_p[send_data_len - 1] = crc >> 8;
		uart_tx(2, send_data_p, send_data_len);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////

void on_uart_1_read(uint8_t *data, size_t len)
{
	uart_rx_publish(1, store_buf(data, len));
	
	// 如果配置为监测模式，则处理Modbus数据
	// RS485_type == 1 表示被动监测
	extern uint8_t RS485_type;
	if (RS485_type == 1) {
			process_modbus_sniffer_data(data, len);
}

	else
	{
	uart_receive_buff_input(data, len);
	printf("uart1 receive data:");
	for(int i = 0;i<len;i++)
	{
		printf(" %02x",data[i]);
	}
	printf("\r\n");
	//printf("on_uart_1_read data ok!!!!!!!!!!!!!!!\r\n");
	if((data[0] == 0x03) && (data[1] == 0x10))
	{
		if((len == 21)&&(http_req_f == 0))
			haas_device_dataRead1(data);
	}
	else
		haas_device_dataRead(data);
	}
	

}

////////////////////////////////////////////////////////////////////////////////////////////
// Modbus监测辅助函数

/**
 * @brief 初始化寄存器映射表(从配置文件加载)
 */
static void init_register_map(void)
{
	extern uint8_t haas_device_num;
	haas_device_num = GetIniKeyInt("config", "haas_dev_num", FILENAME);

	dbg_printf("[Modbus Monitor] Loading register map from config file...\n");
	dbg_printf("[Modbus Monitor] Device count: %d\n", haas_device_num);

	g_register_map_count = 0;

	for (int i = 0; i < haas_device_num && i < MAX_REGISTER_MAP_SIZE; i++) {
		char item_name[20];
		char item_num1[20];
		char item_num2[20];
		char item_num3[20];
		char item_num4[20];
		char item_num5[20];

		if (i < 9) {
			sprintf(item_name, "dev0%d", i + 1);
			sprintf(item_num1, "dev_add0%d", i + 1);
			sprintf(item_num2, "reg_add0%d", i + 1);
			sprintf(item_num3, "data_len0%d", i + 1);
			sprintf(item_num4, "cmd0%d", i + 1);
			sprintf(item_num5, "type0%d", i + 1);
		} else {
			sprintf(item_name, "dev%d", i + 1);
			sprintf(item_num1, "dev_add%d", i + 1);
			sprintf(item_num2, "reg_add%d", i + 1);
			sprintf(item_num3, "data_len%d", i + 1);
			sprintf(item_num4, "cmd%d", i + 1);
			sprintf(item_num5, "type%d", i + 1);
		}

		uint8_t dev_add = GetIniKeyInt(item_name, item_num1, FILENAME);
		uint16_t reg_add = GetIniKeyInt(item_name, item_num2, FILENAME);
		uint16_t data_len = GetIniKeyInt(item_name, item_num3, FILENAME);
		uint8_t cmd = GetIniKeyInt(item_name, item_num4, FILENAME);
		uint8_t type = GetIniKeyInt(item_name, item_num5, FILENAME);

		if (dev_add == 0) {
			dbg_printf("[Modbus Monitor] Skip device %s (invalid address)\n", item_name);
			continue;
		}
		uint16_t effective_len = clamp_data_len(data_len);

		RegisterMap *map = &g_register_map[g_register_map_count];
		map->slave_addr = dev_add;
		map->reg_addr = reg_add;
		snprintf(map->name, sizeof(map->name), "%s", item_name);
		map->cmd = cmd;
		map->data_type = type;
		map->data_len = effective_len;
		map->enabled = true;

		dbg_printf("[Modbus Monitor] Loaded: %s -> Addr:0x%02X Reg:0x%04X Cmd:0x%02X Type:%d Len:%d\n",
		           item_name, dev_add, reg_add, cmd, type, effective_len);

		g_register_map_count++;
	}

	dbg_printf("[Modbus Monitor] Register map initialized: %d entries\n", g_register_map_count);

	for (int i = 0; i < g_register_map_count; i++) {
		RegisterData *slot = &g_register_data[i];
		RegisterMap *map = &g_register_map[i];

		slot->slave_addr = map->slave_addr;
		slot->reg_addr = map->reg_addr;
		slot->cmd = map->cmd;
		slot->data_type = map->data_type;
		slot->data_len = map->data_len;
		memset(slot->reg_values, 0, sizeof(slot->reg_values));
		memset(slot->raw_bytes, 0, sizeof(slot->raw_bytes));
		slot->reg_ready_mask = 0;
		slot->raw_len = 0;
		slot->numeric_value = 0.0;
		slot->text_value[0] = '\0';
		slot->last_update = 0;
		slot->is_valid = false;

		dbg_printf("[Modbus Init] Pre-allocated slot %d for %s (Addr:0x%02X Reg:0x%04X)\n",
		           i, map->name, map->slave_addr, map->reg_addr);
	}

	dbg_printf("[Modbus Init] Pre-allocated %d storage slots in order\n", g_register_map_count);
}

static void ensure_register_map_initialized(void)
{
	if (s_register_map_initialized) {
		return;
	}

	init_register_map();
	s_register_map_initialized = true;
	dbg_printf("[Modbus Monitor] Register map ready\n");
}

void haas_register_map_prepare(void)
{
	ensure_register_map_initialized();
}

void haas_store_register_values(uint8_t slave_addr, uint16_t reg_addr, uint8_t cmd,
                                const uint16_t *values, uint16_t count)
{
	if (!values || count == 0) {
		return;
	}

	ensure_register_map_initialized();

	for (uint16_t i = 0; i < count; i++) {
		store_register_data(slave_addr, reg_addr + i, values[i], cmd);
	}
}

void haas_store_bit_values(uint8_t slave_addr, uint16_t reg_addr, uint8_t cmd,
                           const uint8_t *bits, uint16_t count)
{
	if (!bits || count == 0) {
		return;
	}

	ensure_register_map_initialized();

	for (uint16_t i = 0; i < count; i++) {
		store_register_data(slave_addr, reg_addr + i, (uint16_t)(bits[i] ? 1 : 0), cmd);
	}
}

/**
 * @brief 添加寄存器映射（用于运行时动态添加）
 */
bool add_register_map(uint8_t slave_addr, uint16_t reg_addr, const char *name, 
                      uint8_t data_type, uint8_t cmd)
{
	if (g_register_map_count >= MAX_REGISTER_MAP_SIZE) {
		dbg_printf("[Modbus Monitor] Register map full!\n");
		return false;
	}
	
	RegisterMap *map = &g_register_map[g_register_map_count];
	map->slave_addr = slave_addr;
	map->reg_addr = reg_addr;
	snprintf(map->name, sizeof(map->name), "%s", name);
	map->data_type = data_type;
	map->data_len = 1;
	map->cmd = cmd;
	map->enabled = true;
	
	g_register_map_count++;
	dbg_printf("[Modbus Monitor] Added register: %s (Addr:%02X Reg:%04X Cmd:%02X)\n", 
	           name, slave_addr, reg_addr, cmd);
	return true;
}

/**
 * @brief 检查Modbus帧CRC
 */
static bool check_modbus_crc(uint8_t *data, size_t len)
{
	if (len < 4) return false;
	
	uint16_t calc_crc = ModbusCrc(data, len - 2);
	uint16_t recv_crc = data[len - 2] | (data[len - 1] << 8);
	
	return (calc_crc == recv_crc);
}

/**
 * @brief 解析Modbus请求帧
 */
static bool parse_modbus_request(uint8_t *data, size_t len, ModbusRequest *req)
{
	// 最小Modbus请求帧: 地址(1) + 功能码(1) + 起始地址(2) + 数量(2) + CRC(2) = 8字节
	if (len < 8 || !check_modbus_crc(data, len)) {
		return false;
	}

	// 只处理03功能码（读保持寄存器）和01功能码（读线圈状态）
	if (data[1] != 0x03 && data[1] != 0x01) {
		return false;
	}
	
	req->slave_addr = data[0];
	req->function_code = data[1];
	req->start_reg = (data[2] << 8) | data[3];
	req->reg_count = (data[4] << 8) | data[5];
	req->timestamp = time(NULL);
	req->is_valid = true;
	
	dbg_printf("[Modbus Req] Addr:%02X Func:%02X StartReg:%04X Count:%d\n",
	           req->slave_addr, req->function_code, req->start_reg, req->reg_count);
	
	return true;
}

/**
 * @brief 添加请求到待匹配队列
 */
static void add_pending_request(ModbusRequest *req)
{
	// 查找空闲位置
	for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
		if (!g_pending_requests[i].is_valid) {
			memcpy(&g_pending_requests[i], req, sizeof(ModbusRequest));
			dbg_printf("[Modbus Monitor] Request queued at slot %d\n", i);
			return;
		}
	}
	
	// 队列满，覆盖最旧的
	dbg_printf("[Modbus Monitor] Request queue full, overwriting oldest\n");
	memmove(&g_pending_requests[0], &g_pending_requests[1], 
	        sizeof(ModbusRequest) * (MAX_PENDING_REQUESTS - 1));
	memcpy(&g_pending_requests[MAX_PENDING_REQUESTS - 1], req, sizeof(ModbusRequest));
}

/**
 * @brief 清理超时的请求
 */
static void cleanup_timeout_requests(void)
{
	time_t now = time(NULL);
	for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
		if (g_pending_requests[i].is_valid) {
			if (now - g_pending_requests[i].timestamp > 3) {  // 3秒超时
				dbg_printf("[Modbus Monitor] Request timeout at slot %d\n", i);
				g_pending_requests[i].is_valid = false;
			}
		}
	}
}

/**
 * @brief 匹配响应与请求
 */
static ModbusRequest* match_response_with_request(uint8_t slave_addr, uint8_t function_code)
{
	for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
		if (g_pending_requests[i].is_valid &&
		    g_pending_requests[i].slave_addr == slave_addr &&
		    g_pending_requests[i].function_code == function_code) {
			return &g_pending_requests[i];
		}
	}
	return NULL;
}

/**
 * @brief 存储寄存器数据（按照配置顺序存储到对应槽位）
 */
static void store_register_data(uint8_t slave_addr, uint16_t reg_addr, uint16_t value, uint8_t cmd)
{
	int map_index = -1;
	uint16_t offset = 0;

	for (int j = 0; j < g_register_map_count; j++) {
		RegisterMap *map = &g_register_map[j];

		if (!map->enabled || map->slave_addr != slave_addr || map->cmd != cmd) {
			continue;
		}

		uint16_t effective_len = clamp_data_len(map->data_len);

		if (reg_addr < map->reg_addr) {
			continue;
		}

		uint16_t diff = reg_addr - map->reg_addr;
		if (diff < effective_len) {
			map_index = j;
			offset = diff;
			break;
		}
	}

	if (map_index == -1) {
		dbg_printf("[Modbus Store] Ignore: Addr:%02X Reg:%04X value:%d cmd:0x%02X (not in config)\n",
		           slave_addr, reg_addr, value, cmd);
		return;
	}

	RegisterMap *map = &g_register_map[map_index];
	RegisterData *slot = &g_register_data[map_index];
	uint16_t effective_len = clamp_data_len(map->data_len);

	if ((cmd == 0x01 || cmd == 0x02) && map->data_type != 0) {
		if (map_index < (int)(sizeof(g_haas_dev_rs485) / sizeof(g_haas_dev_rs485[0]))) {
			HAAS_DEV_RS485 *dev = &g_haas_dev_rs485[map_index];
			dev->value1 = 0;
			dev->value2 = 0.0f;
			dev->value_numeric = 0.0;
			dev->value_text[0] = '\0';
			dev->is_string = 0;
		}
		return;
	}

	if (offset >= effective_len || offset >= (REGISTER_VALUE_MAX_BYTES / 2)) {
		dbg_printf("[Modbus Store] Ignore: Addr:%02X Reg:%04X offset:%u exceeds range Len:%u (slot %d)\n",
		           slave_addr, reg_addr, offset, effective_len, map_index);
		return;
	}

	slot->slave_addr = slave_addr;
	slot->reg_addr = map->reg_addr;
	slot->cmd = cmd;
	slot->data_type = map->data_type;
	slot->data_len = effective_len;
	slot->reg_values[offset] = value;
	slot->reg_ready_mask |= (1U << offset);

	size_t byte_pos = offset * 2;
	if (byte_pos + 1 < sizeof(slot->raw_bytes)) {
		slot->raw_bytes[byte_pos] = (uint8_t)(value >> 8);
		slot->raw_bytes[byte_pos + 1] = (uint8_t)(value & 0xFF);
	}

	slot->last_update = time(NULL);

	bool first_valid = !slot->is_valid;
	if (first_valid) {
		slot->is_valid = true;
		g_register_count++;
	}

	bool aggregated = recalc_register_outputs(slot);
	sync_register_to_rs485(map_index, slot, map, aggregated, value);

	if (map->data_type == 2) {
		dbg_printf("[Modbus Store] %s ASCII=\"%s\" offset:%u cmd:0x%02X (slot %d)\n",
		           map->name, slot->text_value, offset, cmd, map_index);
	} else if (map->data_type == 3 && aggregated) {
		dbg_printf("[Modbus Store] %s Value(signed32)=%s offset:%u cmd:0x%02X (slot %d)\n",
		           map->name, slot->text_value, offset, cmd, map_index);
	} else if (map->data_type == 4 && aggregated) {
		dbg_printf("[Modbus Store] %s Value(float32)=%s offset:%u cmd:0x%02X (slot %d)\n",
		           map->name, slot->text_value, offset, cmd, map_index);
	} else if (aggregated) {
		dbg_printf("[Modbus Store] %s Value:%s offset:%u cmd:0x%02X (slot %d)\n",
		           map->name, slot->text_value, offset, cmd, map_index);
	} else {
		dbg_printf("[Modbus Store] %s Raw:0x%04X offset:%u cmd:0x%02X (slot %d)\n",
		           map->name, value, offset, cmd, map_index);
	}
}

// 将声明的寄存器数量限制在缓冲区上限之内，避免后续拷贝越界
static uint16_t clamp_data_len(uint16_t requested_len)
{
	uint16_t max_regs = REGISTER_VALUE_MAX_BYTES / 2;
	if (requested_len == 0) {
		return 1;
	}
	if (requested_len > max_regs) {
		return max_regs;
	}
	return requested_len;
}

// 根据已完整写入的连续寄存器重新聚合结果，生成十进制文本或可打印字符串
static bool recalc_register_outputs(RegisterData *slot)
{
	uint16_t effective_len = clamp_data_len(slot->data_len);
	uint16_t contiguous = 0;

	for (; contiguous < effective_len; contiguous++) {
		if (!(slot->reg_ready_mask & (1U << contiguous))) {
			break;
		}
	}

	if (contiguous > (REGISTER_VALUE_MAX_BYTES / 2)) {
		contiguous = REGISTER_VALUE_MAX_BYTES / 2;
	}

	uint16_t raw_bytes_len = contiguous * 2;
	if (raw_bytes_len > sizeof(slot->raw_bytes)) {
		raw_bytes_len = sizeof(slot->raw_bytes);
	}
	slot->raw_len = (uint8_t)raw_bytes_len;

	// ASCII 类型：将寄存器高低字节依次拷贝为字符，遇到 0x00/0x0D 视为结束
	if (slot->data_type == 2) {
		size_t out_idx = 0;
		for (uint16_t i = 0; i < contiguous && out_idx < sizeof(slot->text_value) - 1; i++) {
			uint16_t word = slot->reg_values[i];
			uint8_t bytes[2] = {
				(uint8_t)(word >> 8),
				(uint8_t)(word & 0xFF)
			};
			for (int b = 0; b < 2 && out_idx < sizeof(slot->text_value) - 1; b++) {
				uint8_t ch = bytes[b];
				if (ch == 0x00 || ch == 0x0D) {
					slot->text_value[out_idx] = '\0';
					slot->numeric_value = 0.0;
					return out_idx > 0;
				}
				if (ch >= 0x20 && ch <= 0x7E) {
					slot->text_value[out_idx++] = (char)ch;
				}
			}
		}
		slot->text_value[out_idx] = '\0';
		slot->numeric_value = 0.0;
		return out_idx > 0;
	}

	if (!(slot->reg_ready_mask & 0x01)) {
		return false;
	}

	// 32 位整型需要两个寄存器拼接，输出十进制字符串
	if (slot->data_type == 1 || slot->data_type == 3) {
		if (contiguous < 2) {
			return false;
		}
		uint32_t raw = ((uint32_t)slot->reg_values[0] << 16) | slot->reg_values[1];

		if (slot->data_type == 3) {
			int32_t signed_val = (int32_t)raw;
			slot->numeric_value = ((double)signed_val) / 10.0;
			snprintf(slot->text_value, sizeof(slot->text_value), "%.1f", slot->numeric_value);
		} else {
			slot->numeric_value = (double)raw;
			snprintf(slot->text_value, sizeof(slot->text_value), "%u", raw);
		}
		return true;
	}

	// 32 位浮点数（IEEE754，高字在前）
	if (slot->data_type == 4) {
		if (contiguous < 2) {
			return false;
		}
		uint32_t raw = ((uint32_t)slot->reg_values[0] << 16) | slot->reg_values[1];
		float f = 0.0f;
		memcpy(&f, &raw, sizeof(f));
		slot->numeric_value = (double)f;
		snprintf(slot->text_value, sizeof(slot->text_value), "%g", slot->numeric_value);
		return true;
	}

	uint16_t raw16 = slot->reg_values[0];
	slot->numeric_value = (double)raw16;
	snprintf(slot->text_value, sizeof(slot->text_value), "%u", raw16);
	return true;
}

// 将聚合后的寄存器结果同步到 g_haas_dev_rs485，确保上传顺序与配置一致
static void sync_register_to_rs485(int index, RegisterData *slot, const RegisterMap *map,
                                   bool aggregated, uint16_t last_word)
{
	if (index < 0 || index >= (int)(sizeof(g_haas_dev_rs485) / sizeof(g_haas_dev_rs485[0]))) {
		return;
	}

	HAAS_DEV_RS485 *dev = &g_haas_dev_rs485[index];
	dev->index = index + 1;
	dev->dev_add = map->slave_addr;
	dev->reg_add = map->reg_addr;
	dev->cmd = map->cmd;
	dev->data_len = map->data_len;
	dev->type = map->data_type;

	if (slot->reg_ready_mask & 0x01) {
		dev->value1 = slot->reg_values[0];
	} else {
		dev->value1 = last_word;
	}

	dev->is_string = (map->data_type == 2);

	if (map->data_type == 2) {
		dev->value_numeric = 0.0;
		dev->value2 = 0.0f;
		if (aggregated) {
			snprintf(dev->value_text, sizeof(dev->value_text), "%s", slot->text_value);
		}
	} else if (aggregated) {
		dev->value_numeric = slot->numeric_value;
		dev->value2 = (float)slot->numeric_value;
		snprintf(dev->value_text, sizeof(dev->value_text), "%s", slot->text_value);
	}
}

/**
 * @brief 解析Modbus响应帧并匹配请求
 */
static bool parse_modbus_response(uint8_t *data, size_t len)
{
	// 最小Modbus响应帧: 地址(1) + 功能码(1) + 字节计数(1) + 数据(N) + CRC(2)
	if (!check_modbus_crc(data, len)) {
		return false;
	}
	
	uint8_t slave_addr = data[0];
	uint8_t function_code = data[1];
	uint8_t byte_count = data[2];
	
	// 只处理03功能码响应和01功能码响应
	if (function_code != 0x03 && function_code != 0x01) {
		return false;
	}
	
	// 验证数据长度
	if (len != (5 + byte_count)) {
		dbg_printf("[Modbus Resp] Length mismatch: expected %d, got %zu\n", 
		           5 + byte_count, len);
		return false;
	}
	
	// 匹配请求
	ModbusRequest *req = match_response_with_request(slave_addr, function_code);
	if (!req) {
		dbg_printf("[Modbus Resp] No matching request for Addr:%02X\n", slave_addr);
		return false;
	}
	
	dbg_printf("[Modbus Resp] Matched! Addr:%02X Func:%02X ByteCount:%d\n",
	           slave_addr, function_code, byte_count);
	
	// 根据功能码分别提取数据
	if (function_code == 0x03) {
		// 功能码03：读保持寄存器（每个寄存器2字节）
		uint16_t reg_count = byte_count / 2;
		for (int i = 0; i < reg_count; i++) {
			uint16_t reg_addr = req->start_reg + i;
			uint16_t value = (data[3 + i * 2] << 8) | data[4 + i * 2];
			
			dbg_printf("[Modbus 0x03] Reg:0x%04X Value:0x%04X (%d)\n", 
			           reg_addr, value, value);
			
			// 存储数据
			store_register_data(slave_addr, reg_addr, value, 0x03);
		}
	} 
	else if (function_code == 0x01) {
		// 功能码01：读线圈状态（每个线圈1位，8个线圈打包成1字节）
		// byte_count 表示字节数，每个字节包含8个线圈状态
		uint16_t coil_count = req->reg_count;  // 实际请求的线圈数量
		
		for (int i = 0; i < coil_count; i++) {
			uint16_t coil_addr = req->start_reg + i;
			
			// 计算当前线圈在哪个字节的哪一位
			int byte_index = i / 8;           // 字节索引
			int bit_index = i % 8;            // 位索引
			
			// 提取线圈状态（1位）
			uint8_t coil_byte = data[3 + byte_index];
			uint16_t coil_value = (coil_byte >> bit_index) & 0x01;
			
			dbg_printf("[Modbus 0x01] Coil:0x%04X Value:%d\n", 
			           coil_addr, coil_value);
			
			// 存储线圈状态（以uint16_t格式存储，值为0或1）
			store_register_data(slave_addr, coil_addr, coil_value, 0x01);
		}
	}
	
	// 清除已匹配的请求
	req->is_valid = false;
	
	return true;
}

/**
 * @brief 处理UART1接收到的Modbus数据（用于被动监测）
 */
static void process_modbus_sniffer_data(uint8_t *data, size_t len)
{
	// 判断帧类型（请求或响应）
	// Modbus请求帧特征：
	//   - 功能码03：后面跟4字节地址和数量
	//   - 功能码06：后面跟2字节寄存器地址 + 2字节写入值
	// Modbus响应帧特征：功能码03，后面跟1字节数据长度
	if (len < 5) return;  // 太短，不是有效Modbus帧

	uint8_t function_code = data[1];

	// 处理功能码03（读保持寄存器）
	if (function_code == 0x03) {
		// 1) 请求：固定长度 8 字节，且 CRC 正确
		if (len == 8 && check_modbus_crc(data, len)) {
			ModbusRequest req = (ModbusRequest){0};
			if (parse_modbus_request(data, len, &req)) {
				add_pending_request(&req);
			}
			return;
		}

		// 2) 响应：奇数长度且 >= 5，byte_count 为偶数，长度匹配 5+byte_count，且 CRC 正确
		// 利用响应包总长度必为奇数的特性（5 + 偶数byte_count = 奇数）
		if ((len % 2) == 1 && len >= 5) {
			uint8_t byte_count = data[2];
			if ((byte_count % 2) == 0 && len == (size_t)(5 + byte_count) && check_modbus_crc(data, len)) {
				parse_modbus_response(data, len);
			}
			return;
		}
	}
	
	// 处理功能码01（读线圈状态）
	if (function_code == 0x01) {
		// 1) 请求：固定长度 8 字节，且 CRC 正确
		if (len == 8 && check_modbus_crc(data, len)) {
			ModbusRequest req = (ModbusRequest){0};
			if (parse_modbus_request(data, len, &req)) {
				add_pending_request(&req);
			}
			return;
		}

		// 2) 响应：变长，格式为 地址(1) + 功能码(1) + 字节计数(1) + 数据(N) + CRC(2)
		// 响应包的总长度 = 5 + byte_count
		if (len >= 5) {
			uint8_t byte_count = data[2];
			if (len == (size_t)(5 + byte_count) && check_modbus_crc(data, len)) {
				parse_modbus_response(data, len);
			}
			return;
		}
	}
	
	// 处理功能码06（写单个寄存器）
	if (function_code == 0x06) {
		// 功能码06的请求和响应格式相同：
		// 地址(1) + 功能码(1) + 寄存器地址(2) + 写入值(2) + CRC(2) = 8字节
		if (len == 8 && check_modbus_crc(data, len)) {
			uint8_t slave_addr = data[0];
			uint16_t reg_addr = (data[2] << 8) | data[3];
			uint16_t write_value = (data[4] << 8) | data[5];
			
			// 使用请求-响应匹配机制区分请求和响应
			// 1. 先检查是否有匹配的待处理请求（如果有，这是响应）
			bool is_response = false;
			for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
				if (g_pending_requests[i].is_valid &&
					g_pending_requests[i].slave_addr == slave_addr &&
					g_pending_requests[i].start_reg == reg_addr &&
					g_pending_requests[i].function_code == 0x06) {
					// 找到匹配的请求，这是响应包
					is_response = true;
					// 清除已匹配的请求
					g_pending_requests[i].is_valid = false;
					break;
				}
			}
			
			if (is_response) {
				// 这是响应包，存储数据
				store_register_data(slave_addr, reg_addr, write_value, 0x06);
				dbg_printf("[Modbus Write Response] Addr:0x%02X Reg:0x%04X Value:0x%04X (%d)\n",
				           slave_addr, reg_addr, write_value, write_value);
			} else {
				// 这是请求包，加入待匹配队列
				ModbusRequest req = {
					.slave_addr = slave_addr,
					.start_reg = reg_addr,
					.function_code = 0x06,
					.reg_count = 1,  // 写单个寄存器
					.timestamp = time(NULL),
					.is_valid = true
				};
				add_pending_request(&req);
			//	dbg_printf("[Modbus Write Request] Addr:0x%02X Reg:0x%04X Value:0x%04X (待响应)\n",
			//	           slave_addr, reg_addr, write_value);
			}
			
			return;
		}
	}
	
	// 处理功能码0x10（写多个寄存器）
	if (function_code == 0x10) {
		// 1) 响应帧：固定8字节
		// 地址(1) + 功能码(1) + 起始地址(2) + 寄存器数量(2) + CRC(2) = 8字节
		if (len == 8 && check_modbus_crc(data, len)) {
			uint8_t slave_addr = data[0];
			uint16_t start_reg = (data[2] << 8) | data[3];
			uint16_t reg_count = (data[4] << 8) | data[5];
			
			// 先检查是否有匹配的待处理请求（响应帧）
			ModbusRequest *req = NULL;
			for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
				if (g_pending_requests[i].is_valid &&
				    g_pending_requests[i].slave_addr == slave_addr &&
				    g_pending_requests[i].start_reg == start_reg &&
				    g_pending_requests[i].function_code == 0x10) {
					req = &g_pending_requests[i];
					break;
				}
			}
			
			if (req != NULL) {
				// 这是响应包，提取请求中保存的写入值并存储
				dbg_printf("[Modbus 0x10 Response] Addr:0x%02X StartReg:0x%04X Count:%d\n",
				           slave_addr, start_reg, reg_count);
				
				// 由于响应包不包含写入的数据，我们需要从请求包中提取
				// 这里标记请求已处理，实际数据在请求阶段已经存储
				req->is_valid = false;
			}
			return;
		}
		
		// 2) 请求帧：变长
		// 地址(1) + 功能码(1) + 起始地址(2) + 寄存器数量(2) + 字节计数(1) + 数据(N) + CRC(2)
		// 最小长度：1+1+2+2+1+2+2 = 11字节
		if (len >= 11 && check_modbus_crc(data, len)) {
			uint8_t slave_addr = data[0];
			uint16_t start_reg = (data[2] << 8) | data[3];
			uint16_t reg_count = (data[4] << 8) | data[5];
			uint8_t byte_count = data[6];
			
			// 验证长度：7 + byte_count + 2(CRC) = 9 + byte_count
			if (len == (size_t)(9 + byte_count) && byte_count == reg_count * 2) {
				dbg_printf("[Modbus 0x10 Request] Addr:0x%02X StartReg:0x%04X Count:%d\n",
				           slave_addr, start_reg, reg_count);
				
				// 提取每个寄存器的值并存储（用于监测写入操作）
				for (int i = 0; i < reg_count; i++) {
					uint16_t reg_addr = start_reg + i;
					uint16_t value = (data[7 + i * 2] << 8) | data[8 + i * 2];
					
					// 存储写入的数据
					store_register_data(slave_addr, reg_addr, value, 0x10);
					dbg_printf("[Modbus 0x10] Write Reg:0x%04X Value:0x%04X (%d)\n",
					           reg_addr, value, value);
				}
				
				// 加入待匹配队列等待响应确认
				ModbusRequest req = {
					.slave_addr = slave_addr,
					.start_reg = start_reg,
					.function_code = 0x10,
					.reg_count = reg_count,
					.timestamp = time(NULL),
					.is_valid = true
				};
				add_pending_request(&req);
			}
			return;
		}
	}
}

/**
 * @brief Modbus数据监测主函数
 */
void haas_data_detect(void)
{
	static bool s_logged_ready = false;

	ensure_register_map_initialized();
	if (!s_logged_ready) {
		dbg_printf("[Modbus Monitor] System initialized\n");
		s_logged_ready = true;
	}
	
	// 清理超时请求
	cleanup_timeout_requests();
	
	// 这里应该从UART1读取数据
	// 由于UART读取在专门的线程中，这里演示如何处理接收到的数据
	// 实际使用时，需要在on_uart_1_read回调中调用处理函数
	
	dbg_printf("[Modbus Monitor] Monitoring... Stored registers: %d\n", g_register_count);
}

//////////////////////////////////////////////////////////////
void haas_data_read(void)
{
	uint16_t crc = 0;
	uint8_t *send_data_p = NULL;
	size_t send_data_len = 0;
	device_no = 0;
 
#if 0
		
for (uint16_t addr = 1; addr <= 0xFF; ++addr) {
    uint8_t frame[8] = {addr, 0x06, 0x00, 0x01, 0x00, 0x01};
    uint16_t crc = ModbusCrc(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = crc >> 8;
    uart_tx(1, frame, sizeof(frame));
    /* ...wait for reply / timeout logic... */
printf("uart1 send data is:");
	for(int j=0; j<8; j++) 
	{
		printf("%02X ", frame[j]);
	}
	printf("\r\n");
	// 等待响应，最多 100ms，每 50ms 轮询一次以降低 CPU 占用
	s_waiting_haas_th = true;
	const unsigned int wait_step_us = 500000;
	const unsigned int timeout_us = 1000000;
	unsigned int waited_us = 0;
	while (s_waiting_haas_th) {
		if (waited_us >= timeout_us) {
			s_waiting_haas_th = false;
			break;
		}
		usleep(wait_step_us);
		waited_us += wait_step_us;
	}
  }
#endif

#if 1
	for(int i=0;i<haas_device_num;i++)
	{
		device_no = i+1;
		g_haas_dev_rs485[i].index = i+1;
		

		
		// 根据cmd字段动态构建Modbus数据包
		uint8_t s_send_data[8] = {0};
		s_send_data[0] = g_haas_dev_rs485[i].dev_add;        // 从机地址
		s_send_data[1] = /*0x03;*/g_haas_dev_rs485[i].cmd;            // 功能码（从配置读取，不再写死0x03）
		s_send_data[2] = (g_haas_dev_rs485[i].reg_add >> 8) & 0xFF;   // 寄存器地址高字节
		s_send_data[3] = g_haas_dev_rs485[i].reg_add & 0xFF;          // 寄存器地址低字节
		s_send_data[4] = (g_haas_dev_rs485[i].data_len >> 8) & 0xFF;  // 数据长度/值高字节
		s_send_data[5] = g_haas_dev_rs485[i].data_len & 0xFF;         // 数据长度/值低字节
		
		// 计算CRC校验
		crc = ModbusCrc(s_send_data, 6);
		s_send_data[6] = crc & 0xFF;
		s_send_data[7] = crc >> 8;
		
		// 发送数据
		printf("Device[%d] Modbus CMD=0x%02X: ", i+1, g_haas_dev_rs485[i].cmd);
		uart_tx(2, s_send_data, 8);
		printf("uart2 send data is:");
		for(int j=0; j<8; j++) 
		{
			printf("%02X ", s_send_data[j]);
		}
		printf("\r\n");
		
		// 等待响应
		time_t now_time = time(NULL);
		s_haas_data_send_time = now_time;
		s_waiting_haas_th = true;
		while(s_waiting_haas_th) 
		{
			time_t now_time = time(NULL);
			if (now_time - s_haas_data_send_time >= 3) {
				s_waiting_haas_th = false;
			}
		}
//	}

	//sleep(2);
	}
#endif
}

void haas_data_save(void)
{
	system("mkdir -p " HUMI_SAVE_DIR ";if [ $(df /overlay | tail -n 1 | awk '{print $4}') -lt 256 ];then rm " HUMI_SAVE_DIR "/$(ls -1 " HUMI_SAVE_DIR " | head -n 1);fi");

	// 8 + , + 4 + '\0' = 14
	static char data_save_buf[32] = {0};
	static char cmd_buf[64] = {0};
	for(int i = 0; i < haas_device_num; i++) {
		uint32_t time_now = time(NULL);
		uint16_t value1 = g_haas_dev_rs485[i].value1;
		uint8_t index = g_haas_dev_rs485[i].index;
		snprintf(data_save_buf, sizeof(data_save_buf), 
			"%02X%02X%02X%02X,%02X%02X",
			(time_now >> 24) & 0xFF, (time_now >> 16) & 0xFF, (time_now >> 8) & 0xFF, time_now & 0xFF,
			(value1 >> 8) & 0xFF, value1 & 0xFF
		);
		snprintf(cmd_buf, sizeof(cmd_buf), "echo %s >> %s/$(date +%%F)_%02d.csv", data_save_buf, HUMI_SAVE_DIR, index);
		dbg_printf("haas_data_save cmd: %s\n", cmd_buf);
		system(cmd_buf);
	}
}


void haas_data_cal(void)
{
	double value_tmp = 0.0;
	double value_humi = 0.0;

	if (haas_device_num > 0) {
		value_tmp = g_haas_dev_rs485[0].value_numeric;
	}
	if (haas_device_num > 1) {
		value_humi = g_haas_dev_rs485[1].value_numeric;
	}
	measure_value = (uint16_t)(value_humi * 100.0 + value_tmp);
}

void haas_data_display_cmd(void)
{
	uint8_t dispaly_data_buf[30];
     haas_data_cal();
     if(measure_value == measure_value_last)
	 {
	  return;
	 }
	 else
	 {
	 measure_value_last = measure_value;
	 dispaly_data_buf[0] = 0x5A;
	 dispaly_data_buf[1] = 0xA5;
	 dispaly_data_buf[2] = 0xA1;
	 dispaly_data_buf[3] = measure_value >> 8;
	 dispaly_data_buf[4] = measure_value;
	 uart_tx(1, dispaly_data_buf, 5);
	 }
}

void haas_data_debug_print(void)
{
	uint8_t *send_data_p = NULL;
	size_t send_data_len = 0;
	char payload[512];
	size_t len = 0;
	bool first = true;

	len += snprintf(payload + len, sizeof(payload) - len, "{");

	for (int i = 0; i < haas_device_num && i < g_register_map_count; i++) {
		RegisterData *slot = &g_register_data[i];
		HAAS_DEV_RS485 *dev = &g_haas_dev_rs485[i];

		if (!slot->is_valid) {
			continue;
		}

		if (dev->value_text[0] == '\0' && (dev->is_string || slot->text_value[0] == '\0')) {
			continue;
		}

		if (len >= sizeof(payload)) {
			break;
		}

		char key[8];
		if (i < 9) {
			snprintf(key, sizeof(key), "\"V0%d\"", i + 1);
		} else {
			snprintf(key, sizeof(key), "\"V%d\"", i + 1);
		}

		if (!first) {
			len += snprintf(payload + len, sizeof(payload) - len, ",");
		}
		first = false;

		if (dev->is_string) {
			len += snprintf(payload + len, sizeof(payload) - len, "%s:\"%s\"",
			               key,
			               dev->value_text);
		} else {
			len += snprintf(payload + len, sizeof(payload) - len, "%s:%.6g",
			               key,
			               dev->value_numeric);
		}
	}

	len += snprintf(payload + len, sizeof(payload) - len, "}\r\n");

	send_data_p = (uint8_t *)payload;
	// Use the accumulated 'len' to avoid strnlen dependency on older C libraries
	send_data_len = len;

	if (send_data_len > 0 && send_data_len < sizeof(payload)) {
		printf("[haas_data_upload] payload: %s\n", payload);
		// uart_tx(2, send_data_p, send_data_len);
	}
}

void air_device_control(uint8_t cmd)
{
	uint16_t crc = 0;
	uint8_t *send_data_p = NULL;
	size_t send_data_len = 0;
	switch(cmd)
	{
		case 0x01:    //read data
			{
				static uint8_t s_send_data[] = { 0xA5, 0x03, 0x00, 0x42, 0x00, 0x05, 0x3C, 0xF9 };
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x02:    //read ON/OFF
			break;
		case 0x03:    //ON/OFF control
			{
				static uint8_t s_send_data[] = { 0xA5, 0x06, 0x00, 0x25, 0x00, 0xBF, 0x00, 0x00 };
				s_send_data[5] = airDevice.switch_mode;
#if 0
				uint16_t crc_temp = ModbusCrc(s_send_data,6);
				s_send_data[7] = crc_temp >> 8;
				s_send_data[6] = crc_temp;
				printf("air control command send data:");
				for(int i =0; i< 8;i++)
				{
					printf(" %02x",s_send_data[i]);
				}
				printf("\r\n");
				//uint16_t crc_temp = ModbusCrc(s_send_data,6);
				//printf("air control command receive and crc is:%04x",crc_temp);
#endif
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
				printf("-----------------%p,%d\r\n",send_data_p,send_data_len);
			}
			break;
		case 0x04:    //humi set
			{
				static uint8_t s_send_data[] = { 0xA5, 0x06, 0x00, 0x26, 0x00, 0xBF, 0x30, 0xE2 };
				s_send_data[5] = airDevice.temp_value - 16;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x05:    //Constant humidity work mode
			{
				static uint8_t s_send_data[] = { 0xA5, 0x06, 0x00, 0x27, 0x00, 0xBF, 0x20, 0xE5 };
				s_send_data[5] = airDevice.work_mode;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x06:    //Circulating wind speed
			{
				static uint8_t s_send_data[] = { 0xA5, 0x06, 0x00, 0x28, 0x00, 0xBF, 0x30, 0xE2 };
				s_send_data[5] = airDevice.wind_speed;
				send_data_p = s_send_data;
				send_data_len = sizeof(s_send_data);
			}
			break;
		case 0x07:    //Exhaust wind speed
			break;
		case 0x08:    //swing on-off
			break;
		default:
			break;
	
	}
	
	if ((send_data_p != NULL) && (send_data_len > 0)) {
		printf("22222-----------------%p,%d\r\n",send_data_p,send_data_len);
		crc = ModbusCrc(send_data_p, send_data_len - 2);
		send_data_p[send_data_len - 2] = crc & 0xFF;
		send_data_p[send_data_len - 1] = crc >> 8;
		uart_tx(2, send_data_p, send_data_len);
	}
}

void device_self_test(void)
{

}

void check_btn()
{
	static time_t last_run_time = 0;
	static time_t long_press_time = 0;
	static bool is_long_press = false;
	static bool last_is_long_press = false;

	char btn_status[2] = {0};
	time_t now_time = time(NULL);
	if (now_time - last_run_time >= 1) {

		// -------------
		memset(btn_status, 0, sizeof(btn_status));
		FILE *fp = fopen("/tmp/btn.status", "r");
		if (fp) {
			fread(btn_status, sizeof(btn_status), 1, fp);
			fclose(fp);
		}

		int btn_status_num = atoi(btn_status);
		if (btn_status_num == 1) {
			// long press ...
			if (!last_is_long_press) {
				is_long_press = true;
				long_press_time = now_time;
				dbg_printf("############ start long press\n");
			}
		}

		if (last_is_long_press) {
			// normal ...
			if (is_long_press && (now_time - long_press_time >= 120)) {
				is_long_press = false;
				system("rm /tmp/btn.status");
				dbg_printf("############ clear long press\n");
			}
		}

		if (!last_is_long_press && is_long_press) {
			system("/root/app/led.flash 500 > /dev/null");
			mcu_set_wifi_mode(0);
		} else if (last_is_long_press && !is_long_press) {
			system("/root/app/led.flash 1000 > /dev/null");
		}
		// ==============
		//dbg_printf("############ is_long_press: %u, now_time: %u\n", is_long_press, now_time);
		last_is_long_press = is_long_press;
		last_run_time = now_time;
	}
}

void http_data_get(void)
{
	uint16_t crc = 0;
	uint8_t *send_data_p = NULL;

	char *workOrder = WorkOrder.WorkOrder_No;
	printf("workorder is:%s\r\n",workOrder);
	char *res = get_http(workOrder);
	if (res != NULL) {
		printf("http raw json:%s\r\n", res);
	} else {
		printf("http raw json is NULL\r\n");
	}
	WorkOrder.material_count = 0;
	WorkOrder.assign_name[0] = '\0';
	WorkOrder.operator_id[0] = '\0';
	WorkOrder.unit[0] = '\0';
	WorkOrder.quantity_double = 0.0;
//	printf("%s\n", res);
	cJSON *data_json = read_json_str(res);
//	cJSON *data_json = cJSON_GetObjectItemCaseSensitive(res, "data");
	if (data_json != NULL) {
		cJSON *dataobject_json = cJSON_GetObjectItemCaseSensitive(data_json, "data");
		cJSON *name_json = cJSON_GetObjectItemCaseSensitive(dataobject_json, "Craft_Name");
		cJSON *quantity_json = cJSON_GetObjectItemCaseSensitive(dataobject_json, "AssignProduct_Quantity");
		cJSON *order_name_json = cJSON_GetObjectItemCaseSensitive(dataobject_json, "Assign_Name");
		cJSON *operator_json = cJSON_GetObjectItemCaseSensitive(dataobject_json, "Create_User");
		cJSON *unit_json = cJSON_GetObjectItemCaseSensitive(dataobject_json, "UnitNick");
		const char *unit_str = (unit_json && cJSON_IsString(unit_json)) ? unit_json->valuestring : "";
		double total_quantity = 0.0;
		if (quantity_json && cJSON_IsNumber(quantity_json)) {
			total_quantity = quantity_json->valuedouble;
			WorkOrder.quantity = (unsigned int)total_quantity;
			WorkOrder.quantity_double = total_quantity;
		} else {
			WorkOrder.quantity = 0;
		}

		if (name_json && cJSON_IsString(name_json)) {
			sprintf(WorkOrder.Product_name,"%s",name_json->valuestring);
		} else {
			WorkOrder.Product_name[0] = '\0';
		}
		if (order_name_json && cJSON_IsString(order_name_json)) {
			snprintf(WorkOrder.assign_name, sizeof(WorkOrder.assign_name), "%s", order_name_json->valuestring);
		}
		if (operator_json && cJSON_IsString(operator_json)) {
			snprintf(WorkOrder.operator_id, sizeof(WorkOrder.operator_id), "%s", operator_json->valuestring);
		}
		if (unit_str[0] != '\0') {
			snprintf(WorkOrder.unit, sizeof(WorkOrder.unit), "%s", unit_str);
		}

		printf("assign info: order=%s, operator=%s, craft=%s, total=%.4f %s\r\n",
		       (order_name_json && cJSON_IsString(order_name_json)) ? order_name_json->valuestring : "",
		       (operator_json && cJSON_IsString(operator_json)) ? operator_json->valuestring : "",
		       WorkOrder.Product_name,
		       total_quantity,
		       unit_str);

		cJSON *details_json = cJSON_GetObjectItemCaseSensitive(dataobject_json, "Details");
		if (details_json && cJSON_IsArray(details_json)) {
			int detail_size = cJSON_GetArraySize(details_json);
			for (int i = 0; i < detail_size; ++i) {
				if (WorkOrder.material_count >= 10) {
					break;
				}
				cJSON *item = cJSON_GetArrayItem(details_json, i);
				if (!item) {
					continue;
				}
				cJSON *material_name_json = cJSON_GetObjectItemCaseSensitive(item, "Material_Name");
				cJSON *entry_quantity_json = cJSON_GetObjectItemCaseSensitive(item, "AssignEntry_Quantity");
				cJSON *entry_unit_json = cJSON_GetObjectItemCaseSensitive(item, "MaterialUnit");
				const char *material_name = (material_name_json && cJSON_IsString(material_name_json)) ? material_name_json->valuestring : "";
				const char *entry_unit = (entry_unit_json && cJSON_IsString(entry_unit_json)) ? entry_unit_json->valuestring : unit_str;
				double entry_quantity = (entry_quantity_json && cJSON_IsNumber(entry_quantity_json)) ? entry_quantity_json->valuedouble : 0.0;
				printf("material[%d]: %s target=%.4f %s\r\n", i, material_name, entry_quantity, entry_unit);
				snprintf(WorkOrder.materials[WorkOrder.material_count].name,
				         sizeof(WorkOrder.materials[WorkOrder.material_count].name),
				         "%s", material_name);
				snprintf(WorkOrder.materials[WorkOrder.material_count].unit,
				         sizeof(WorkOrder.materials[WorkOrder.material_count].unit),
				         "%s", entry_unit);
				WorkOrder.materials[WorkOrder.material_count].target = entry_quantity;
				WorkOrder.material_count++;
			}
		}

		work_quantity[3] = WorkOrder.quantity << 8;
		work_quantity[4] = WorkOrder.quantity;
		send_data_p = work_quantity;
		crc = ModbusCrc(send_data_p, 5);
		send_data_p[5] = crc & 0xFF;
		send_data_p[6] = crc >> 8;
		uart_tx(1, send_data_p, 7);
		printf("http get pd quantity is:%d,name:%s\r\n",WorkOrder.quantity,WorkOrder.Product_name);
//		rk_quantity[] ={0x06,0x03,0x04,0x00,0x00,0x00,0x00,0x00,0x00};

	}

}

void *data_main()
{
	time_t s_humi_last_save_time = 0;
	time_t s_mqtt_dataUpload_time = 0;
	s_humi_last_save_time = time(NULL);
	s_mqtt_dataUpload_time = time(NULL);
	//wait uart init end.
	//device_self_test();
	////////////////////////
	  wifi_protocol_init();
	while (1) {
		//dbg_printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
		    //  wifi_uart_service();
		//      check_btn();
#if 1	
		if(http_req_f == 1)
		{
		  //char *res = get_http("MO251212001");
		 // printf("%s\n", res);
			http_data_get();
		}
		time_t now_time = time(NULL);

		int upload_time = GetIniKeyInt("config", "upload_time", FILENAME);
		if (upload_time <= 0) {
			upload_time = DATA_MQTT_INTERVAL_S;
		}
		if(now_time - s_mqtt_dataUpload_time >= upload_time)
		{
			mqtt_data_upload();
			haas_mqtt_data_upload();
			//  heart_beat_publish();
			//      mqtt_airDevice_data_publish();
			s_mqtt_dataUpload_time = now_time;
			printf("s_mqtt_dataUpload_time is over,data upload!!!!!!!!!!!!!!!!!!!!!!!\r\n");
		}

	//	if (now_time - s_humi_last_save_time >= HUMI_SAVE_INTERVAL_S){
	//		haas_data_save();
	//		s_humi_last_save_time = now_time;
	//	}
#endif
		haas_data_display_cmd();
		sleep(2);
	}
	return 0;
}



void energy_init()
{
	s_waiting_energy_type = true;
	uartReceive_length = 13;
	while (s_waiting_energy_type) {
		uart_tx(2, read_energy_type_cmd, sizeof(read_energy_type_cmd));
		sleep(1);
	}

	s_waiting_energy_param = true;
	uartReceive_length = 27;
	while (s_waiting_energy_param) {
		uart_tx(2, read_energy_params_cmd, sizeof(read_energy_params_cmd));
		sleep(1);
	}

	s_waiting_energy_zero_fix = true;
	uartReceive_length = 8;
	while (s_waiting_energy_zero_fix) {
		uart_tx(2, write_energy_restart_cmd, sizeof(write_energy_restart_cmd));
		sleep(1);
	}
}

void energy_restart_measure()
{
	s_waiting_energy_zero_fix = true;
	uartReceive_length = 8;
	while (s_waiting_energy_zero_fix) {
		uart_tx(2, write_energy_restart_cmd, sizeof(write_energy_restart_cmd));
		sleep(1);
	}
}

void energy_read()
{
	s_waiting_energy_read = true;
	uartReceive_length = 57;
	while (s_waiting_energy_read) {
		uart_tx(2,read_measure_data_cmd , sizeof(read_measure_data_cmd));
		sleep(5);
	}
}

bool haas_check_wifi_config()
{
	s_waiting_haas_config = true;
	while (s_waiting_haas_config) {

		//uart_tx(2,read_measure_data_cmd , sizeof(read_measure_data_cmd));
		sleep(1);
		if (0) break; //config status
	}
	return true;
}

bool haas_check_wifi_online()
{
	if (cb3s_wifi_state == 0x04) return true;
	return false;
}

void haas_sync_time()
{
	s_waiting_haas_sync_time = true;
	while (s_waiting_haas_sync_time) {
		mcu_get_system_time();
		sleep(1);
	}
}

void get_Tywifi_status()
{
	static bool last_online_status = false;
	static bool first_online = true;

	cb3s_wifi_state = mcu_get_wifi_work_state();
	//dbg_printf("=== mcu_get_wifi_work_state: %X\n", cb3s_wifi_state);
	bool online_status = (cb3s_wifi_state == 0x04);

	if (!last_online_status && online_status && first_online) {
		first_online = false;

		s_cmd_last_run_time = 0;
	}

	last_online_status = online_status;
}

void haas_upload_data()
{
	//s_waiting_haas_upload_data = true;
	//while (s_waiting_haas_upload_data) {
		//all_data_update();
	//	sleep(3);
	//}
}

////////////////////////////////////////////////////
// Modbus监测数据查询和管理函数

/**
 * @brief 获取指定寄存器的数据
 * @param slave_addr 从机地址
 * @param reg_addr 寄存器地址
 * @param cmd Modbus功能码
 * @return RegisterData指针，未找到返回NULL
 */
RegisterData* get_register_data(uint8_t slave_addr, uint16_t reg_addr, uint8_t cmd)
{
	for (int i = 0; i < g_register_map_count; i++) {
		RegisterData *slot = &g_register_data[i];

		if (!slot->is_valid || slot->slave_addr != slave_addr || slot->cmd != cmd) {
			continue;
		}

		uint16_t start_reg = slot->reg_addr;
		uint16_t effective_len = clamp_data_len(slot->data_len);

		if (reg_addr >= start_reg && reg_addr < (start_reg + effective_len)) {
			return slot;
		}
	}
	return NULL;
}

/**
 * @brief 按配置顺序获取寄存器数据（用于按 dev01, dev02... 顺序遍历）
 * @param index 配置索引（0 表示 dev01, 1 表示 dev02...）
 * @return RegisterData指针，如果索引越界或数据无效返回NULL
 */
RegisterData* get_register_data_by_index(int index)
{
	if (index < 0 || index >= g_register_map_count) {
		return NULL;
	}
	
	// 直接返回对应索引的数据（已按顺序存储）
	if (g_register_data[index].is_valid) {
		return &g_register_data[index];
	}
	
	return NULL;
}

/**
 * @brief 打印所有监测到的寄存器数据（按配置顺序显示）
 */
void print_all_register_data(void)
{
	printf("\n========== Modbus Monitor Data (Ordered by Config) ==========\n");
	printf("Total registers monitored: %d\n", g_register_count);
	printf("%-6s %-10s %-10s %-10s %-10s %-10s %-20s\n",
	       "Index", "DevName", "SlaveAddr", "RegAddr", "Value/Text", "CMD", "LastUpdate");
	printf("---------------------------------------------------------------------------------\n");

	time_t now = time(NULL);
	int valid_count = 0;

	for (int i = 0; i < g_register_map_count; i++) {
		const RegisterMap *map = &g_register_map[i];
		RegisterData *slot = &g_register_data[i];

		if (slot->is_valid) {
			int age = now - slot->last_update;
			const char *cmd_name = (slot->cmd == 0x03) ? "READ" :
			                       (slot->cmd == 0x06) ? "WRITE" : "UNKN";
			char value_buf[64];

			if (map->data_type == 2) {
				snprintf(value_buf, sizeof(value_buf), "\"%s\"", slot->text_value);
			} else if (slot->text_value[0] != '\0') {
				snprintf(value_buf, sizeof(value_buf), "%s", slot->text_value);
			} else if (slot->reg_ready_mask & 0x01) {
				snprintf(value_buf, sizeof(value_buf), "%u", slot->reg_values[0]);
			} else {
				snprintf(value_buf, sizeof(value_buf), "--");
			}

			printf("%-6d %-10s 0x%02X       0x%04X     %-10s 0x%02X(%s) %ds ago\n",
			       i,
			       map->name,
			       slot->slave_addr,
			       map->reg_addr,
			       value_buf,
			       slot->cmd,
			       cmd_name,
			       age);
			valid_count++;
		} else {
			printf("%-6d %-10s 0x%02X       0x%04X     %-10s %-10s (no data)\n",
			       i,
			       map->name,
			       map->slave_addr,
			       map->reg_addr,
			       "---",
			       "---");
		}
	}

	printf("=================================================================\n");
	printf("Valid entries: %d / %d configured devices\n\n", valid_count, g_register_map_count);
}

/**
 * @brief 清除所有寄存器数据
 */
void clear_register_data(void)
{
	for (int i = 0; i < g_register_map_count; i++) {
		RegisterData *slot = &g_register_data[i];
		memset(slot->reg_values, 0, sizeof(slot->reg_values));
		memset(slot->raw_bytes, 0, sizeof(slot->raw_bytes));
		slot->reg_ready_mask = 0;
		slot->raw_len = 0;
		slot->numeric_value = 0.0;
		slot->text_value[0] = '\0';
		slot->last_update = 0;
		slot->is_valid = false;

		if (i < (int)(sizeof(g_haas_dev_rs485) / sizeof(g_haas_dev_rs485[0]))) {
			HAAS_DEV_RS485 *dev = &g_haas_dev_rs485[i];
			dev->value1 = 0;
			dev->value2 = 0.0f;
			dev->value_numeric = 0.0;
			dev->value_text[0] = '\0';
			dev->is_string = 0;
		}
	}

	g_register_count = 0;
	dbg_printf("[Modbus Monitor] All register data cleared\n");
}
