#include <stdbool.h>
#include "common.h"
#include "data.h"
#include "bf_cmd.h"
#include "mqtt.h"
#include "bf_uart.h"
//#include "bf_common.h"
#include "haas_mqtt.h"
#include "string.h"
//#include "bf_file.h"
//#include "bf_wifi.h"
//#include "bf_gatt.h"
//#include "bf_ble.h"
#include "wifi.h"
#include "system.h"
#include "mcu_api.h"
#include "protocol.h"
#include "uart.h"
#include "ini.h"

//ESP_EVENT_DEFINE_BASE(TASK_CMD);

//������
static uint32_t     unique_code = 0;
uint32_t 	dev_ID0=0;		//�豸���
char  	unique_id[10]={0};	//��ά��
char net_color[10]={0};
uint8_t lost_connection=0;
uint32_t bt_color =0;
unsigned char cb3s_wifi_state = 0;
uint8_t mqtt_publish_en = 0;
//================param save=================================================
uint8_t Flash_buf[120]={0};
//static BF_KV flash_kv = {0};
static uint8_t read_param_flag = 0;	//��������־
uint8_t write_param_flag = 0;	//д������־

uint32_t upload_time = 0;
uint32_t measure_time = 0;
uint32_t current_time = 0;

HumiDev humiDevice = {
	.switch_mode = 1,
	.setHumi_value = 60,
	.control_mode = 0
};

AirDev airDevice = {
	.switch_mode = 1,
	.temp_value = 20,
	.work_mode = 0,
	.wind_speed = 0
};

void humiDev_control_process()
{
	if(humiDevice.switch_mode != humiDevice.power_sta)
	{
		humi_device_control(0x03);
		sleep(1);
	}
	if(humiDevice.setHumi_value != humiDevice.getHumi_value)
	{
		humi_device_control(0x04);
		sleep(1);
	}
	if(humiDevice.control_mode != humiDevice.get_control_mode)
	{
		humi_device_control(0x05);
		sleep(1);
	}
}

void get_humiDev_work_data()
{
	humi_device_control(0x01);
	sleep(1);
}

void airDev_control_process()
{
	// if(airDevice.switch_mode == 0)
	// {
	// 	air_device_control(0x02);
	// 	sleep(1);
	// }
	// else
	// {
		air_device_control(0x03);
		sleep(1);
		air_device_control(0x04);
		sleep(1);
	// }
}

void get_airDev_work_data()
{
	air_device_control(0x01);
	sleep(1);
}

//��־λ
#if 0
void my_memcpy_b(uint8_t* tostr,uint8_t* fromstr,uint8_t len)
{

	uint8_t i;
	for(i = 0;i < len;i++)
	{
		 tostr[i] = fromstr[len-1-i];
	}
}

/*
 *
 *
 *
 */
uint32_t HexStrToByte(const uint8_t* source)
{
    short i;
    unsigned char highByte, lowByte;
    unsigned char dest[4];
    uint32_t dev_data;
    int sourceLen=8;
    for (i = 0; i < sourceLen; i += 2)
    {
        highByte = toupper(source[i]);
        lowByte  = toupper(source[i + 1]);
        if (highByte > 0x39)
            highByte -= 0x37;
        else
            highByte -= 0x30;

        if (lowByte > 0x39)
            lowByte -= 0x37;
        else
            lowByte -= 0x30;

        dest[i / 2] = (highByte << 4) | lowByte;
    }
    dev_data =dest[0];dev_data=dev_data<<8;
    dev_data+=dest[1];dev_data=dev_data<<8;
    dev_data+=dest[2];dev_data=dev_data<<8;
    dev_data+=dest[3];
    return dev_data;
}
/*
 * �����洢,��ȡ
 * ģʽ����
 * ����ɫ�ָ� set_color
 * �����صƣ�����ɨ��ֹͣ�ָ������ϱ�������
 */
/*
 *
 *
 * �豸���в��� ��������
 */

void write_parameter(uint8_t a)
{
	strcpy(flash_kv.k, KEY_LIGHT_PARAM);
	memset(Flash_buf, 0, 120);
	if(a)
	{
		Flash_buf[0] = SET_data;
		//dev_data.set = 1;
	}
	else
	{
		Flash_buf[0] = 0;
	}
	memcpy(Flash_buf+1,(uint8_t*)&dev_data,sizeof(dev_data));
	memcpy(Flash_buf+40,(uint8_t*)&ble_data,sizeof(ble_data));
	memcpy(flash_kv.v,Flash_buf,120);
	flash_kv.vlen = 120;
	ESP_ERROR_CHECK(esp_event_post_to(bf_event_loop, TASK_FILE, CMD_FILE_SET_BF_KV, &flash_kv, sizeof(flash_kv), portMAX_DELAY));
}

//������
void read_parameter(void)
{
	if(Flash_buf[0] == SET_data)
	{
		memcpy((uint8_t*)&dev_data,Flash_buf+1,sizeof(dev_data));
		//ESP_LOGI(TASK_CMD, "read data,mode:%d,major:%d, minor:%d,Freq:%08x,uuid:%d,scan_mode:%d",dev_data.rgb_mode,dev_data.major
		//	            		,dev_data.minor ,dev_data.custom_color,dev_data.uuid,dev_data.scan_mode);
		ESP_LOGI(TASK_CMD, "read data,mode:%d,major:%d, minor:%d,Freq:%08x,uuid:%d",dev_data.rgb_mode,dev_data.major
					            		,dev_data.minor ,dev_data.custom_color,dev_data.uuid);
		if(dev_data.uuid > 0)
		{
			memcpy((uint8_t*)&ble_data,Flash_buf+40,sizeof(ble_data));
		}
	}
	else
	{
		dev_data.rgb_mode = 0;
		dev_data.offline_color = 0xff0000;
		ESP_LOGE(TASK_CMD, "read parameter flash_buf error!FLash[0]:%02x",Flash_buf[0]);
	}
}


//д����
static void cmd_task_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{

	if(base == TASK_FILE)		//�ļ�
	{
		switch(id)
		{
		case EVENT_FILE_VALUE_BF_KV:	//�õ��ļ�
			//ESP_LOGI(TASK_CMD, "read flash buf:%d,%s",strlen(event_data),(char*)event_data);
			if(strlen(event_data) <= 0)
				break;
			else
			{
				BF_KV *kv = (BF_KV *)event_data;
				if(ble_data.set == 1)
				{
					esp_restart();
				}
				if(read_param_flag == 0)
				{
					if (0 == strncmp(KEY_LIGHT_PARAM, kv->k, strlen(KEY_LIGHT_PARAM))) {
						read_param_flag = 1;
						ESP_LOGE(TASK_CMD, "read flash buf success!");
						memcpy(Flash_buf, kv->v, sizeof(Flash_buf));
					}
					else
						ESP_LOGE(TASK_CMD, "read flash buf error!");
				}
			//	else
			//		esp_restart();
			}
			break;
		}

	}
}


/*
 *
 *
 *
 */
uint8_t color_switch(uint8_t  data)
{
	uint8_t set_color = 0;
	if(data > 0xf)
		data = dev_data.offline_color;
	switch(data)
	{
		case 0x00:
					set_color = 0;
					break;
		case 0x01:
					set_color = 0x30;
					break;

		case 0x02:
					set_color = 0x0c;
					break;
		case 0x03:
					set_color = 0x3c;
					break;
		case 0x04:
					set_color = 0x03;
					break;
		case 0x05:
					set_color = 0x33;
					break;
		case 0x06:
					set_color = 0x0f;
					break;
		case 0x0F:
		case 0x07:
					set_color = 0x3f;
					break;

	}
	return set_color;
}
//�ж� �޸� �ϱ�
/**
 *
 *
 */
void Rgb_Blecmd(uint8_t* Rx_Buffer, uint8_t len)
{
	if(len < 58)
		return;
	uint8_t i = 0,offset = 0;
	uint32_t dev_cid = 0;
	uint32_t color = 0;
	for(i = 0;i < 3;i++)
	{
		if(i == 0)
			offset= 32;//
		else if(i == 1)
			offset = 43;//
		else if(i == 2)
		{
			offset = 54;//
		}
			my_memcpy_b((uint8_t*)&dev_cid,Rx_Buffer+offset,4);
		if(dev_cid == unique_code)
		{
			lost_connection = 1;
			dev_data.err_code = 0;
			color = RGB222_888(color_switch(Rx_Buffer[offset+4]));
			if(dev_data.custom_color != color)
			{
				//device_params_upmqtt();
				bt_color = color;
				RGB_ColorSet(color);//
			}
			ESP_LOGI(TASK_CMD, "BLE color SET:0x%02x",Rx_Buffer[offset+4]);
			break;
			//100,0 20w
		}
	}
//	else
//		ESP_LOGI(TASK_CMD, "BLE code SET:0x%08x",dev_cid);

}
//�Ƶ�״ָ̬ʾ
void Light_status_check(void *argv)
{
	vTaskDelay(8000/portTICK_RATE_MS);//wait unique_code read
	while(1)
	{
		if(dev_data.rgb_mode == 0)
		{
			RGB_ColorSet(0xffffff);
			vTaskDelay(1000/portTICK_RATE_MS);
			if(strlen(unique_id) != 8)
			{
				RGB_ColorSet(0xff00);
			}
			else
			{
				RGB_ColorSet(0xffffff);
				vTaskDelay(1000/portTICK_RATE_MS);
				if(wifi_consta != 1)
					RGB_ColorSet(0xff00ff);
				else
					RGB_ColorSet(0xffffff);
				vTaskDelay(1000/portTICK_RATE_MS);
			}
		}
		else if(dev_data.rgb_mode == 1)
		{
			//70s lost connection set red
			if( lost_connection ++ > 250)
			{
				lost_connection = 0;
				dev_data.err_code = 2;
				RGB_ColorSet(dev_data.offline_color);
			}
			vTaskDelay(3000/portTICK_RATE_MS);
		}
		else if(dev_data.rgb_mode == 2)
		{
			RGB_ColorSet(dev_data.set_color);
			vTaskDelay(5000/portTICK_RATE_MS);
		}
		else
		{
			dev_data.rgb_mode = 0;

		}


		 if(check_ble++ > 3)//������������������
		 {
			 check_ble = 0;
			// ble_init();
			 //ble retart
			 if(check_repeat++ > 5)
			 {
				 check_repeat = 0;
				 esp_restart();
				 //systeam restart
			 }
		 }

		ESP_LOGI(TASK_CMD, "Mode:%d,wifi_Sta:%d,lost_connection:%d,offline_color:%08x,check_ble:%d", dev_data.rgb_mode,wifi_consta,lost_connection,dev_data.offline_color,check_ble);
		vTaskDelay(1000/portTICK_RATE_MS);
	}
}

void device_switch_detect(void)
{
	if(M_value.dev_switch_value == 0x55)
	{
		M_value.dev_switch = 0x01;
	}
	else if (M_value.dev_switch_value == 0xAA)
	{
		M_value.dev_switch = 0x00;
	}
		if(M_value.dev_switch_last != M_value.dev_switch)
	{
		M_value.dev_switch_last = M_value.dev_switch;
		switch_data_update();
	}
}

uint8_t crcCalc(const char* data, const int len)
{
	int i;
	uint8_t calc_result;
	uint8_t calc_sum = 0;
	for(i=0;i<len-1;i++)
	{
		calc_sum = calc_sum + data[i];
		//ESP_LOGI(logName, "send data calc sum is (%d):<%02x>", i,calc_sum);
	}
	//printf("send data calc sum is %02x", calc_sum);
	calc_result = calc_sum;
	//printf("send data calc crc is %02x", calc_result);
	return calc_result;
}
#endif

time_t bf_get_timestamp_s()
{
#if 0
	uint32_t timestamp_s = 0;
	struct timeval tv = {0};
	struct timezone tz = {0};
	gettimeofday(&tv, &tz);
	timestamp_s = tv.tv_sec;
//	printf("bf_get_timestamp_ms:%u\r\n",timestamp_s);
	return timestamp_s;
#endif
	return time(NULL);
}

#if 0
void upload_data(uint8_t dp_id,unsigned short value)
{
	char send_bt3l_data[80];
	char send_bt3l_buf[80];
	const char * crc_data_temp;

	send_bt3l_data[0] = 0x55;
	send_bt3l_data[1] = 0xaa;
	send_bt3l_data[2] = 0x03;
	send_bt3l_data[3] = 0x34;

	send_bt3l_data[4] = 0x00;   //len high
	send_bt3l_data[5] = 0x11;   //len low

	send_bt3l_data[6] = 0x0B;  // upload mode

	send_bt3l_data[7] = 0x01;
	send_bt3l_data[8] = 0x01;
			send_bt3l_data[9] = local_time.year - 2000;
			send_bt3l_data[10] = local_time.month;
			send_bt3l_data[11] = local_time.day;
			send_bt3l_data[12] = local_time.hour;
			send_bt3l_data[13] = local_time.miniter;
			send_bt3l_data[14] = local_time.sencond;

			send_bt3l_data[15] = dp_id;
			send_bt3l_data[16] = 0x02;
			send_bt3l_data[17] = 0x00;
			send_bt3l_data[18] = 0x04;
			send_bt3l_data[19] = 0x00;
			send_bt3l_data[20] = 0x00;
			send_bt3l_data[21] = value>>8;
			send_bt3l_data[22] = value;


			crc_data_temp = send_bt3l_data;
			send_bt3l_data[23] = crcCalc(crc_data_temp,24);
			//sendData("TX_BT3L", crc_data_temp,37);
			//HAL_UART_Transmit_DMA(&uart1.uart, (uint8_t *)&send_bt3l_data, 72);

			sendData("senddata", crc_data_temp,24);
			printf("Send data:");
			for(int i = 0;i<24;i++)
			{
				printf("%02x ",send_bt3l_data[i]);
			}
			printf("\r\n");
}


void energy_value_upload(void)
{
	mcu_get_system_time();
	vTaskDelay(1000/portTICK_RATE_MS);
	upload_data(DPID_CUR_VOLTAGE,M_value.dev_voltage1); //VALUE�������ϱ�;
	vTaskDelay(250/portTICK_RATE_MS);
	upload_data(DPID_CUR_CURRENT,M_value.dev_current1); //VALUE�������ϱ�;
	vTaskDelay(250/portTICK_RATE_MS);
	upload_data(DPID_CUR_POWER,M_value.dev_power_value); //VALUE�������ϱ�;
	vTaskDelay(250/portTICK_RATE_MS);
	upload_data(DPID_ADD_ELE,M_value.dev_power_ele); //VALUE�������ϱ�;
	vTaskDelay(250/portTICK_RATE_MS);

	upload_data(DPID_VOLTAGE_COE,M_value.dev_voltage2); //VALUE�������ϱ�;
   	vTaskDelay(250/portTICK_RATE_MS);
   	upload_data(DPID_ELECTRIC_COE,M_value.dev_current2); //VALUE�������ϱ�;
   	vTaskDelay(250/portTICK_RATE_MS);
   	upload_data(DPID_POWER_COE,M_value.dev_voltage3); //VALUE�������ϱ�;
  	vTaskDelay(250/portTICK_RATE_MS);
  	upload_data(DPID_ELECTRICITY_COE,M_value.dev_current3); //VALUE�������ϱ�;
  	vTaskDelay(250/portTICK_RATE_MS);


}
#endif
/*
void energy_value_upload(void)
{
	char send_bt3l_data[100];
	char send_bt3l_buf[100];
	const char * crc_data_temp;

		send_bt3l_data[0] = 0x55;
		send_bt3l_data[1] = 0xaa;
		send_bt3l_data[2] = 0x03;
		send_bt3l_data[3] = 0x07;

		send_bt3l_data[4] = 0x00;   //len high
		send_bt3l_data[5] = 0x48;   //len low

		send_bt3l_data[6] = 0x66;  // AV
		send_bt3l_data[7] = 0x02;
		send_bt3l_data[8] = 0x00;
		send_bt3l_data[9] = 0x04;
		send_bt3l_data[10] = 0x00;
		send_bt3l_data[11] = 0x00;
		send_bt3l_data[12] = 0x08;//M_value.dev_voltage1>>8;
		send_bt3l_data[13] = 0xaa;//M_value.dev_voltage1;

		send_bt3l_data[14] = 0x67;  // AC
		send_bt3l_data[15] = 0x02;
		send_bt3l_data[16] = 0x00;
		send_bt3l_data[17] = 0x04;
		send_bt3l_data[18] = 0x00;
		send_bt3l_data[19] = 0x00;
		send_bt3l_data[20] = 0x00;//M_value.dev_current1>>8;
		send_bt3l_data[21] = 0x59;//M_value.dev_current1;

		send_bt3l_data[22] = 0x69;  // BV
		send_bt3l_data[23] = 0x02;
		send_bt3l_data[24] = 0x00;
		send_bt3l_data[25] = 0x04;
		send_bt3l_data[26] = 0x00;
		send_bt3l_data[27] = 0x00;
		send_bt3l_data[28] = 0x08;//M_value.dev_voltage2>>8;
		send_bt3l_data[29] = 0x05;//M_value.dev_voltage2;

		send_bt3l_data[30] = 0x6A;  // BC
		send_bt3l_data[31] = 0x02;
		send_bt3l_data[32] = 0x00;
		send_bt3l_data[33] = 0x04;
		send_bt3l_data[34] = 0x00;
		send_bt3l_data[35] = 0x00;
		send_bt3l_data[36] = 0x00;//M_value.dev_current2>>8;
		send_bt3l_data[37] = 0x79;//M_value.dev_current2;

		send_bt3l_data[38] = 0x6C;  // CV
		send_bt3l_data[39] = 0x02;
		send_bt3l_data[40] = 0x00;
		send_bt3l_data[41] = 0x04;
		send_bt3l_data[42] = 0x00;
		send_bt3l_data[43] = 0x00;
		send_bt3l_data[44] = 0x80;//M_value.dev_voltage3>>8;
		send_bt3l_data[45] = 0xab;//M_value.dev_voltage3;

		send_bt3l_data[46] = 0x6D;  // CC
		send_bt3l_data[47] = 0x02;
		send_bt3l_data[48] = 0x00;
		send_bt3l_data[49] = 0x04;
		send_bt3l_data[50] = 0x00;
		send_bt3l_data[51] = 0x00;
		send_bt3l_data[52] = 0x00;//M_value.dev_current3>>8;
		send_bt3l_data[53] = 0x68;//M_value.dev_current3;

		send_bt3l_data[54] = 0x6f;  // Current power
		send_bt3l_data[55] = 0x02;
		send_bt3l_data[56] = 0x00;
		send_bt3l_data[57] = 0x04;
		send_bt3l_data[58] = 0x00;
		send_bt3l_data[59] = 0x00;
		send_bt3l_data[60] = 0x05;//M_value.dev_power_value>>8;
		send_bt3l_data[61] = 0x88;//M_value.dev_power_value;

		send_bt3l_data[62] = 0x6e;  // Current power
		send_bt3l_data[63] = 0x02;
		send_bt3l_data[64] = 0x00;
		send_bt3l_data[65] = 0x04;
		send_bt3l_data[66] = 0x00;
		send_bt3l_data[67] = 0x00;
		send_bt3l_data[68] = 0x09;//M_value.dev_today_power>>8;
		send_bt3l_data[69] = 0x01;//M_value.dev_today_power;

		send_bt3l_data[70] = 0x11;  // Current power
		send_bt3l_data[71] = 0x02;
		send_bt3l_data[72] = 0x00;
		send_bt3l_data[73] = 0x04;
		send_bt3l_data[74] = 0x00;
		send_bt3l_data[75] = 0x00;
		send_bt3l_data[76] = 0x00;//M_value.dev_power_ele>>8;
		send_bt3l_data[77] = 0xaa;//M_value.dev_power_ele;

		crc_data_temp = send_bt3l_data;
		send_bt3l_data[78] = crcCalc(crc_data_temp,79);
		sendData("senddata", crc_data_temp,79);

		printf("Send data:");
		for(int i = 0;i<79;i++)
		{
			printf("%02x ",send_bt3l_data[i]);
		}
		printf("\r\n");
}
*/
//����������
#if 0
int old_cmd_main()
{
	bool first_upload = true;

	cb3s_wifi_state = mcu_get_wifi_work_state();
	printf("cb3s wifi work state is:%02x\n",cb3s_wifi_state);
	current_time = bf_get_timestamp_s();
	if (cb3s_wifi_state == 0x04)
	{
		//mcu_get_system_time();
		//vTaskDelay(1000/portTICK_RATE_MS);
		//printf("local current time is:%d:%d:%d\n",local_time.hour,local_time.miniter,local_time.sencond);
		if(first_upload == 0)
		{
			while(local_time.sencond == 0)
			{
				mcu_get_system_time();
				//		vTaskDelay(1000/portTICK_RATE_MS);
				printf("local current time is:%d:%d:%d\n",local_time.hour,local_time.miniter,local_time.sencond);
			}
			first_upload = 1;
			M_value.dev_mesure_enable = 1;
			M_value.dev_power_ele = M_value.dev_power_H - M_value.dev_power_L;
			M_value.dev_power_ele = M_value.dev_power_ele*10;
			M_value.dev_today_power += M_value.dev_power_H - M_value.dev_power_L;
			//	M_value.dev_today_power = M_value.dev_today_power*10;
			M_value.dev_power_L = M_value.dev_power_H;

			//	vTaskDelay(5000/portTICK_RATE_MS);
			//all_data_update();
			//energy_value_upload();
			//add_en_flag = 1;
			M_value.dev_power_ele_mqtt = M_value.dev_power_ele;
			//	vTaskDelay(1000/portTICK_RATE_MS);
			upload_time = bf_get_timestamp_s();
			//	device_switch_detect();
			//	ele_data_update();

		}
		else //if((current_time - upload_time) > 600)
		{
			mcu_get_system_time();
			//vTaskDelay(1000/portTICK_RATE_MS);
			//printf("local current time is:%d:%d:%d\n",local_time.hour,local_time.miniter,local_time.sencond);
			//if(local_time.miniter == 0)
			//{
			if((local_time.hour == 0)&&(local_time.miniter == 0))
			{
				M_value.dev_today_power = 0;
			}
			M_value.dev_power_ele =  M_value.dev_power_H - M_value.dev_power_L;
			M_value.dev_power_ele = M_value.dev_power_ele*10;
			M_value.dev_today_power += M_value.dev_power_H - M_value.dev_power_L;

			M_value.dev_power_L = M_value.dev_power_H;

			//		vTaskDelay(5000/portTICK_RATE_MS);
			//		energy_value_upload();
			//add_en_flag = 1;
			M_value.dev_power_ele_mqtt = M_value.dev_power_ele;
			//all_data_update();
			upload_time = bf_get_timestamp_s();
			//		vTaskDelay(1000/portTICK_RATE_MS);
			//	device_switch_detect();
			//	ele_data_update();
			//}
		}

	}


	if(first_upload == 0)
	{
		//vTaskDelay(2000/portTICK_RATE_MS);
	}
	else
	{
		//vTaskDelay(57000/portTICK_RATE_MS);
		//vTaskDelay(30000/portTICK_RATE_MS);
		M_value.dev_mesure_enable = 1;
	}



	//}

	//    while(1)//
	//	{
	//		if(strlen(unique_id) == 8 )
	//		{
	//			ESP_LOGI(TASK_CMD, "RGB star run");
	//			break;
	//		}
	//		ESP_LOGI(TASK_CMD, "waiting unique_code set ...");
//		vTaskDelay(2000/portTICK_RATE_MS);
//	}
//    unique_code = HexStrToByte((const uint8_t*)unique_id);
//	ESP_LOGI(TASK_CMD, "RGB read param,UID:%08x,V:%s",unique_code,BF_APP_VERSION);
////	vTaskDelay(5000/portTICK_RATE_MS);
//	while(1)
//	{
//		if(read_param_flag == 1)
//		{
//			read_parameter();	//������
//			printf("get value success\n");
//			break;
//		}
//		else
//		{
//			printf("get value\n");
//			ESP_ERROR_CHECK(esp_event_post_to(bf_event_loop, TASK_FILE, CMD_FILE_GET_BF_KV, KEY_LIGHT_PARAM, strlen(KEY_LIGHT_PARAM) + 1, portMAX_DELAY));
//		}
//		vTaskDelay(2000/portTICK_RATE_MS);
//	}
//
//
//	dev_data.unique_id = unique_code;
	//ESP_LOGI(TASK_CMD, "TASK_CMD delete ...");
	//vTaskDelete(NULL);
	
	return NULL;
}
#endif
time_t s_cmd_last_run_time = 0;
//time_t s_humi_last_save_time = 0;

void data_send_test(void)
{
//	char test_buf[]={};
	char test_buf[10] = {0x05,0x03,0x10,0x00,0x00,0x04,0x41,0x4d};
	printf("--------------------------test data send start -----------------------\r\n");
	uart_tx(2, test_buf,8);
	for(int i=0;i<8;i++)
	{
	   printf(" %02x",test_buf[i]);
	}
	printf("\r\n");
	printf("--------------------------test data send end -----------------------\r\n");

}



void *cmd_main(void *args)
{
//	system("/root/app/led.flash 1000 > /dev/null");

//	energy_init();

//	s_humi_last_save_time = time(NULL);

	while (1) {
		get_Tywifi_status();
		time_t now_time = time(NULL);
		int modbus_tcp_enable = GetIniKeyInt("config", "modbus_tcp_enable", FILENAME);
		if (RS485_type == 1) {
			// 被动监测：只监听总线
			haas_data_detect();
			sleep(1);
		} else {
			// 主动发包（互斥优先级）：Modbus TCP -> 能耗采集 -> RTU
			if (modbus_tcp_enable == 1) {
				sleep(1);
			} else if (dev_type == 2) {
				haas_energy_type2_init();
				haas_energy_type2_poll();
			} else {
				//haas_data_read();
				sleep(2);																																	//haas_switch_device_smoke_once();
			}
		}
	//	if(RS485_type == 0)
//		{
//			haas_data_read();
//			sleep(2);
//		}
//		else
//		{
//			haas_data_detect();
//			sleep(1);
//		}
		//data_send_test();
		DATA_FUNCTION_INTERVAL_S = GetIniKeyInt("config", "upload_time", FILENAME);
		printf("upload data interval:%d\r\n",DATA_FUNCTION_INTERVAL_S);
		if (now_time - s_cmd_last_run_time >= DATA_FUNCTION_INTERVAL_S){
			// haas_data_debug_print();
		s_cmd_last_run_time = now_time;
		}
	//	if (now_time - s_humi_last_save_time >= HUMI_SAVE_INTERVAL_S){
	//		haas_data_save();
	//		s_humi_last_save_time = now_time;
	//	}
#if 0
		if (now_time - s_cmd_last_run_time >= DATA_FUNCTION_INTERVAL_S) {
			// run every DATA_FUNCTION_INTERVAL_S

			// ----------------------------------
			dbg_printf("************ energy_read\n");
			energy_read();
            
			dbg_printf("************ get mqtt online\n");
			//if (is_mqtt_online())
			mqtt_publish_en = 1;
			//mqtt_haas_data_publish();

			dbg_printf("************ get haas online\n");
			if (haas_check_wifi_online()) {
				dbg_printf("************ haas sync time\n");
				haas_sync_time();
				dbg_printf("************ haas upload\n");
				haas_upload_data();
			}
			if((mqtt_publish_en == 2) || (haas_check_wifi_online()))
			{
				energy_restart_measure();
				M_value.dev_power_ele = 0;
				M_value.dev_power_ele_mqtt = 0;
				mqtt_publish_en = 0;
			}	
			get_humiDev_work_data();
			get_humiDev_work_data();

			s_cmd_last_run_time = now_time;
		}

		if (g_485_device_type == DEVICE_485_AIR) {
			if(airDevice.data_update == 1)
			{
				airDevice.data_update = 0;
				airDev_control_process();				
			}
		} else if (g_485_device_type == DEVICE_485_HUMI) {
			if(humiDevice.data_update == 1)
			{
				humiDevice.data_update = 0;
				humiDev_control_process();				
			}
		}
#endif
//		sleep(2);
	}
	return NULL;
}
