#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "data.h"
#include "uart.h"
#include "mqtt.h"
#include "mcu_api.h"
#include "bf_cmd.h"

DEVICE_485_type g_485_device_type = DEVICE_485_NO_DEVICE;

HAAS_DEV_RS485 g_haas_dev_rs485[50];
uint8_t haas_device_num = 0;
uint8_t device_no = 0;
time_t s_haas_data_send_time = 0;


char *g_bf_code = NULL;
char *g_version = NULL;
Measure_data M_value = {0};

const char read_energy_type_cmd[] = {0x05,0x03,0x10,0x00,0x00,0x04,0x41,0x4d};
const char read_energy_params_cmd[] = {0x05,0x03,0x10,0x10,0x00,0x0B,0x00,0x8C};
const char write_energy_restart_cmd[] = {0x05,0x10,0x10,0x18,0x00,0x01,0x02,0x00,0x03,0xC6,0x88};
const char set_ThreePrase_cmd[] = {};
//const char read_measure_data_cmd[] = {0xFF,0x03,0x15,0x00,0x00,0x18,0x54,0x12};
const char read_measure_data_cmd[] = {0x05,0x03,0x15,0x00,0x00,0x1A,0xC1,0x89};

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

///////////////////////////////////////////////////////
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

char *store_buf(uint8_t *buf, size_t len)
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
			HUMIDEVICE_DATA *humi = reg_p;

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

void haas_device_dataRead(uint8_t *data)
{
	if((data[1] == 0x03)&&(device_no > 0))
	{
	uint8_t addr = device_no - 1;
	g_haas_dev_rs485[addr].value1 = data[3] << 8;
	g_haas_dev_rs485[addr].value1 += data[4];
	printf("receive data is:%d\r\n",g_haas_dev_rs485[addr].type);
		if(g_haas_dev_rs485[addr].type == 1)
			{
				g_haas_dev_rs485[addr].value1 -=4000;
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/100.0;
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
				g_haas_dev_rs485[addr].value2 = g_haas_dev_rs485[addr].value1/10.0;

				if(g_haas_dev_rs485[addr].value2 > 200)
				{
					g_haas_dev_rs485[addr].value2 = 200.0;
				}
			}
		printf("receive data is:%d,%f\r\n",g_haas_dev_rs485[addr].value1,g_haas_dev_rs485[addr].value2);
		s_waiting_haas_th = false;
	}
}


void on_uart_1_read(uint8_t *data, size_t len)
{
	uart_rx_publish(1, store_buf(data, len));
	uart_receive_buff_input(data, len);
	printf("on_uart_1_read data ok!!!!!!!!!");
	haas_device_dataRead(data);
}

void on_uart_2_read(uint8_t *data, size_t len)
{
	uart_rx_publish(2, store_buf(data, len));

	//static uint8_t uart_2_read_buf[1024];

	if (len == uartReceive_length) {
		dbg_printf("======= uart 2 read 13: ======\n");
		for (size_t i = 0; i < len; i++) {
			dbg_printf("%02X ", data[i]);
		}
		dbg_printf("\n=======------------------=========\n");

		energy_process(data, len);
	}

	humi_process(data, len);
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
	g_version = BF_VERSION;

	if (0 == access(CONFIG_FILE, F_OK)) {
		g_485_device_type = GetIniKeyInt("cfg", "device_type", CONFIG_FILE);
		dbg_printf(">>> read device_type: %u\n", g_485_device_type);
	} else {
		dbg_printf(">>> no config file!\n");
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

void haas_data_read(void)
{
	uint16_t crc = 0;
	uint8_t *send_data_p = NULL;
	size_t send_data_len = 0;
	device_no = 0;
	for(int i=0;i<haas_device_num;i++)
	{
		device_no = i+1;
		g_haas_dev_rs485[i].index = i+1;
		uint8_t s_send_data[8] = {g_haas_dev_rs485[i].dev_add,0x03,0x00,g_haas_dev_rs485[i].reg_add,0x00,g_haas_dev_rs485[i].data_len};
		send_data_p = s_send_data;
		//send_data_len = sizeof(s_send_data);
	//	if ((send_data_p != NULL) && (send_data_len > 0)) {
			printf("22222-----------------%p,%d\r\n",send_data_p,send_data_len);
			crc = ModbusCrc(send_data_p, 6);//send_data_len);
			send_data_p[6 ] = crc & 0xFF;
			send_data_p[7] = crc >> 8;
			uart_tx(1, send_data_p, 8);//send_data_len + 2);
			printf("send data is:");
			for(int j=0;j<8;j++)
			{
				printf("%02x ",s_send_data[j]);
			}
			printf("\r\n");
			time_t now_time = time(NULL);
			s_haas_data_send_time = now_time;
			s_waiting_haas_th = true;
			while(s_waiting_haas_th)
			{
				time_t now_time = time(NULL);
				if (now_time - s_haas_data_send_time >= 3){
					s_waiting_haas_th = false;

				}
				sleep(1);
			}

			printf("------------------------haas_data_read end!\r\n");
	//	}
		//sleep(2);
	}
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

void haas_data_upload(void)
{
	uint16_t crc = 0;
	uint8_t *send_data_p = NULL;
	size_t send_data_len = 0;
	uint8_t s_send_data[500] = {0x7B};//,0x22,0x56,0x00,0x00,0x22,0x3A};
	int len_tmp = 1;
	for(int i=0;i<haas_device_num;i++)
	{
		//7B 0D 0A 20 22 56 30 31 22 3A 30 30 30 30 0D 0A 7D 0D 0A
		//7B 22 56 30 31 22 3A 30 30 30 30 7D 0D 0A
	    //uint8_t s_send_data[50] = {0x7B,0x22,0x56,0x00,0x00,0x22,0x3A};//,0x00,0x00,0x00,0x00,0x7D,0x0D,0x0A};//,g_haas_dev_rs485[i].reg_add,0x00,g_haas_dev_rs485[i].data_len};
	uint8_t send_buf[50] = {};
	uint8_t buf_length;
	if(g_haas_dev_rs485[i].index < 10)
		{
	//		s_send_data[3] = 0x30;
	//		s_send_data[4] = 0x30 + g_haas_dev_rs485[i].index;
			sprintf(send_buf,"\"V0%d\":%.1f,",g_haas_dev_rs485[i].index,g_haas_dev_rs485[i].value2);
		}
		else
		{
			sprintf(send_buf,"\"V%d\":%.1f,",g_haas_dev_rs485[i].index,g_haas_dev_rs485[i].value2);
	//		s_send_data[3] = 0x30 + g_haas_dev_rs485[i].index/10;
	//		s_send_data[4] = 0x30 + g_haas_dev_rs485[i].index%10;
		}
		//uint8_t send_buf[50] = {};
		//uint8_t buf_length;
	//	sprintf(send_buf,"\"V%d\",%.1f,",g_haas_dev_rs485[i].index,g_haas_dev_rs485[i].value2);
		buf_length = strlen(send_buf);
		sprintf(&s_send_data[len_tmp],"%s",send_buf);
		len_tmp +=buf_length;
		}
		s_send_data[len_tmp - 1] = 0x7D;
		s_send_data[len_tmp] = 0x0D;
		s_send_data[len_tmp + 1] = 0x0A;
		//}
		//	float a = 123.5;
		//uint8_t send_buf[50] = {};
      //  uint8_t buf_length;
		//sprintf(send_buf,"%.1f",g_haas_dev_rs485[i].value2);
		//sprintf(&s_send_data[7],"%s}\r\n",send_buf);
		//buf_length = strlen(send_buf);
		send_data_len = strlen(s_send_data);
		printf("test data is:----%s,%d\r\n",s_send_data,send_data_len);
		
		//int digits[5];
		send_data_p = s_send_data;
		send_data_len = strlen(s_send_data);
		if ((send_data_p != NULL) && (send_data_len > 0)) {
			printf("22222-----------------%p,%d\r\n",send_data_p,send_data_len);
			//crc = ModbusCrc(send_data_p, send_data_len);
			//send_data_p[send_data_len ] = crc & 0xFF;
			//send_data_p[send_data_len + 1] = crc >> 8;
			uart_tx(2, send_data_p, send_data_len);
		}
		sleep(3);
//	}
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

void *data_main()
{
	time_t s_humi_last_save_time = 0;
	time_t s_mqtt_dataUpload_time = 0;
	s_humi_last_save_time = time(NULL);
	s_mqtt_dataUpload_time = time(NULL);
	//wait uart init end.
	//device_self_test();
	////////////////////////
//	wifi_protocol_init();
	while (0) {
		//dbg_printf("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//		wifi_uart_service();
//		check_btn();
		time_t now_time = time(NULL);

		if(now_time - s_mqtt_dataUpload_time >= DATA_MQTT_INTERVAL_S)
		{
			mqtt_data_upload();
			//  heart_beat_publish();
			//      mqtt_airDevice_data_publish();
			s_mqtt_dataUpload_time = now_time;
			printf("s_mqtt_dataUpload_time is over,data upload!!!!!!!!!!!!!!!!!!!!!!!\r\n");
		}

		if (now_time - s_humi_last_save_time >= HUMI_SAVE_INTERVAL_S){
			haas_data_save();
			s_humi_last_save_time = now_time;
		}
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
