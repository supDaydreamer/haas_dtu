#define ADDRESS			"mqtt://47.100.192.18:1883"
//#define ADDRESS			"mqtt://mqtt.51beefind.com:1883"
#define USERNAME		"hengyuan"
#define PASSWORD		"hengyuanIot"
//#define CLIENTID		"S&DF9CD002&629&1"
#define CLIENTID		s_client_id
//#define TOPIC			"testtopic"
#define SEND_TOPIC		"testtopic/send"
#define SEND_MSG		"Hello PAHO!"
#define QOS				1
#define CONNECT_TIMEOUT	10
#define KEEPALIVE		30
#define RUN_SLEEP_TIME	15


#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "MQTTAsync.h"



#include "common.h"
#include "ini.h"
#include "json.h"
#include "data.h"
#include "uart.h"
#include "curl.h"
#include "haas_mqtt_private.h"



uint8_t RS485_type = 0;
static char s_client_id[30];
static uint16_t product_ID;
static char s_device_control_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};



static MQTTAsync s_mqtt_client;


static int do_mqtt_connect(MQTTAsync client, unsigned int wait_time);
static int do_mqtt_subscribe(MQTTAsync client, unsigned int wait_time);



static void device_control_cmd(uint8_t h_val,uint8_t l_val)
{
	uint8_t control_cmd_buf[20];
	control_cmd_buf[0] = 0x5A;
	control_cmd_buf[1] = 0xA5;
	control_cmd_buf[2] = 0xA2;
	control_cmd_buf[3] = h_val;
	control_cmd_buf[4] = l_val;
	uart_tx(1, control_cmd_buf,5);
}



static int on_message(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
	(void)context;
	(void)topicLen;

	printf("[PAHO] on_message:\n");
	printf("    --> topic: %s\n", topicName);
	printf("    --> message: %.*s\n", message->payloadlen, (char *)message->payload);

	if (strlen(topicName) <= MQTT_TOPIC_LEN_MAX) {
		if (0 == strncmp(s_device_control_topic_buf, topicName, strlen(topicName))) {
			cJSON *data_json = read_json_str(message->payload);
			if (data_json != NULL) {
				sprintf(WorkOrder.WorkOrder_No,"MO251212001");
				http_req_f = 1;
			//	cJSON *ctrl_json = read_json_obj(data_json, "heat_ctrl");
				cJSON *light_json = cJSON_GetObjectItemCaseSensitive(data_json, "light_ctrl");
				cJSON *haas_ctrl_json = cJSON_GetObjectItemCaseSensitive(data_json, "haas_device_ctrl");
				uint8_t light_control_val = 0;
				uint8_t heat_control_val = 0;
				uint8_t ctrl_device_type = 0;
				uint8_t ctrl_slave_addr = 0;
				uint16_t ctrl_reg_addr = 0;
				uint16_t ctrl_data = 0;
				uint32_t ctrl_uart = 1;

				if (light_json && cJSON_IsNumber(light_json)) {
					light_control_val = light_json->valueint;
					PutIniKeyInt("config","dev_ctrl02",light_control_val,FILENAME);
					device_control_cmd(heat_control_val,light_control_val);
				}

				// haas_device_control ½âÎö£¬Ö§³Ö haas_device_ctrl °ü¹ü»òÖ±½ÓÆ½ÆÌ£¬Ò²Ö§³Ö¶ººÅ·Ö¸ô×Ö·û´®
				bool handled = false;
				if (haas_ctrl_json && cJSON_IsString(haas_ctrl_json) && haas_ctrl_json->valuestring) {
					const char *p = haas_ctrl_json->valuestring;
					long vals[5] = {0};
					size_t idx = 0;
					while (*p != '\0' && idx < 5) {
						char *endp = NULL;
						long v = strtol(p, &endp, 0);
						if (endp == p) {
							break;
						}
						vals[idx++] = v;
						if (*endp == ',') {
							p = endp + 1;
						} else {
							p = endp;
						}
					}
					if (idx >= 4) {
						ctrl_device_type = (uint8_t)vals[0];
						ctrl_slave_addr = (uint8_t)vals[1];
						ctrl_reg_addr = (uint16_t)vals[2];
						ctrl_data = (uint16_t)vals[3];
						if (idx >= 5) {
							ctrl_uart = (uint32_t)vals[4];
						}
						handled = true;
					}
				}

				if (!handled) {
					cJSON *ctrl_obj = haas_ctrl_json ? haas_ctrl_json : data_json;
					if (ctrl_obj) {
						cJSON *dev_type_json = cJSON_GetObjectItemCaseSensitive(ctrl_obj, "device_type");
						cJSON *slave_addr_json = cJSON_GetObjectItemCaseSensitive(ctrl_obj, "slave_addr");
						cJSON *reg_addr_json = cJSON_GetObjectItemCaseSensitive(ctrl_obj, "reg_addr");
						cJSON *data_json_obj = cJSON_GetObjectItemCaseSensitive(ctrl_obj, "data");
						cJSON *uart_json = cJSON_GetObjectItemCaseSensitive(ctrl_obj, "uart");

						if (dev_type_json && cJSON_IsNumber(dev_type_json))   ctrl_device_type = dev_type_json->valueint;
						if (slave_addr_json && cJSON_IsNumber(slave_addr_json)) ctrl_slave_addr = slave_addr_json->valueint;
						if (reg_addr_json && cJSON_IsNumber(reg_addr_json))   ctrl_reg_addr = reg_addr_json->valueint;
						if (data_json_obj && cJSON_IsNumber(data_json_obj))   ctrl_data = data_json_obj->valueint;
						if (uart_json && cJSON_IsNumber(uart_json))       ctrl_uart = uart_json->valueint;
					}
				}

				if (ctrl_device_type != 0) {
					dbg_printf("[MQTT_CTRL] device_type:%u slave:0x%02X reg:%u data:%u uart:%u\n",
					           ctrl_device_type, ctrl_slave_addr, ctrl_reg_addr, ctrl_data, ctrl_uart);
					haas_device_control(ctrl_device_type, ctrl_slave_addr, ctrl_reg_addr, ctrl_data, ctrl_uart);
				}

				close_json(data_json);
			}
		}
	} else {
		dbg_printf("payload len bigger than %u, skip ...\n", MQTT_TOPIC_LEN_MAX);
	}

	MQTTAsync_freeMessage(&message);
	MQTTAsync_free(topicName);
	return 1;
}

static void on_disconnect(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;

	if (cause) {
		printf("[PAHO] on_disconnect(): %s\n", cause);
	} else {
		printf("[PAHO] on_disconnect(): UNKNOWN\n");
	}
	// exit(-1);
	do_mqtt_connect(client, 3);
}

void on_send(void* context, MQTTAsync_successData* response)
{
	(void)response;

	unsigned int seq = (unsigned int)context;

	printf("[PAHO] seq=%u on_send() OK.\n", seq);
}

void on_send_failure(void* context, MQTTAsync_failureData* response)
{
	unsigned int seq = (unsigned int)context;

	printf("[PAHO] seq=%u on_send_failure(): %d\n", seq, response->code);
}

static void on_subscribe(void* context, MQTTAsync_successData* response)
{
	(void)context;
	(void)response;

	printf("[PAHO] on_subscribe() OK.\n");
}

static void on_subscribe_failure(void* context, MQTTAsync_failureData* response)
{
	MQTTAsync client = (MQTTAsync)context;

	printf("[PAHO] on_subscribe_failure(): %d\n", response->code);
	do_mqtt_subscribe(client, 2);
}

static void on_connect(void* context, MQTTAsync_successData* response)
{
	(void)response;

	MQTTAsync client = (MQTTAsync)context;

	printf("[PAHO] on_connect() OK.\n");
	do_mqtt_subscribe(client, 0);
}

static void on_connect_failure(void* context, MQTTAsync_failureData* response)
{
	MQTTAsync client = (MQTTAsync)context;

	printf("[PAHO] on_connect_failure(): %d\n", response->code);
	do_mqtt_connect(client, 3);
}

static int do_mqtt_send(MQTTAsync client, char *topic, char *msg)
{
	static unsigned int s_seq = 0;

	int rc;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTAsync_responseOptions pub_opts = MQTTAsync_responseOptions_initializer;

	pubmsg.payload = msg;
	pubmsg.payloadlen = strlen(msg);
	pubmsg.qos = QOS;
	pubmsg.retained = 0;
	pub_opts.onSuccess = on_send;
	pub_opts.onFailure = on_send_failure;
	pub_opts.context = (void *)s_seq;

	if (client && MQTTAsync_isConnected(client)) {
		printf("[PAHO] seq=%u do_mqtt_send() ...\n", s_seq);
		if ((rc = MQTTAsync_sendMessage(client, topic, &pubmsg, &pub_opts)) != MQTTASYNC_SUCCESS) {
			printf("[PAHO] seq=%u do_mqtt_send() failed: %d\n", s_seq, rc);
		}
	} else {
		rc = MQTTASYNC_DISCONNECTED;
		printf("[PAHO] not connected, seq=%u do_mqtt_send() skip ...\n", s_seq);
	}
	s_seq++;
	return rc;
}

static int do_mqtt_subscribe(MQTTAsync client, unsigned int wait_time)
{
	int rc = MQTTASYNC_SUCCESS;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

	opts.onSuccess = on_subscribe;
	opts.onFailure = on_subscribe_failure;
	opts.context = client;

	if (wait_time == 0) {
		printf("[PAHO] do_mqtt_subscribe() ...\n");
	} else {
		printf("[PAHO] wait %u s to do_mqtt_subscribe() ...\n", wait_time);
		sleep(wait_time);
	}

	snprintf(s_device_control_topic_buf, sizeof(s_device_control_topic_buf), "/%d/%s/function/get",product_ID,g_bf_code);
	dbg_printf("Subscribing to %s\n", s_device_control_topic_buf);
	rc += MQTTAsync_subscribe(client, s_device_control_topic_buf, QOS, &opts);
	dbg_printf("Subscribed, code: %d\n", rc);

	if (rc != MQTTASYNC_SUCCESS) {
		printf("[PAHO] do_mqtt_subscribe() failed: %d\n", rc);
	}
	return rc;
}

static int do_mqtt_connect(MQTTAsync client, unsigned int wait_time)
{
	int rc;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

	conn_opts.username = USERNAME;
	conn_opts.password = PASSWORD;
	conn_opts.connectTimeout = CONNECT_TIMEOUT;
	conn_opts.keepAliveInterval = KEEPALIVE;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = on_connect;
	conn_opts.onFailure = on_connect_failure;
	conn_opts.context = client;

	if (wait_time == 0) {
		printf("[PAHO] do_mqtt_connect() ...\n");
	} else {
		printf("[PAHO] wait %u s to do_mqtt_connect() ...\n", wait_time);
		sleep(wait_time);
	}
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
		printf("[PAHO] do_mqtt_connect() failed: %d\n", rc);
	}
	return rc;
}

static int do_mqtt_init(MQTTAsync *client)
{
	int rc;

	printf("[PAHO] do_mqtt_init() create ...\n");
	if ((rc = MQTTAsync_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS) {
		printf("[PAHO] do_mqtt_init() create failed: %d\n", rc);
		return rc;
	}

	printf("[PAHO] do_mqtt_init() set callbacks ...\n");
	if ((rc = MQTTAsync_setCallbacks(*client, *client, on_disconnect, on_message, NULL)) != MQTTASYNC_SUCCESS) {
		printf("[PAHO] do_mqtt_init() set callbacks failed: %d\n", rc);
		return rc;
	}
	return rc;
}

static void do_mqtt_main()
{
	int rc;
	MQTTAsync *client = &s_mqtt_client;

	do {
		rc = do_mqtt_init(client);
	} while (rc != MQTTASYNC_SUCCESS && (sleep(1) || 1));
	do_mqtt_connect(*client, 0);

	while (1) {
		printf("[PAHO] running @ %lu...\n", time(NULL));
		//do_mqtt_send(*client);
		sleep(RUN_SLEEP_TIME);
	}
}



void *haas_mqtt_main(void)
{
#if 1
	//char mqtt_clientid[OUT_TOPIC_BUF_SIZE];

	//snprintf(mqtt_clientid, sizeof(mqtt_clientid), "%s-%s", OUT_CLIENT_ID_PRIFIX, g_bf_code);

	//dbg_printf("My client NAME is: %s\n", mqtt_clientid);


	product_ID =GetIniKeyInt("config", "device_id", FILENAME);
	sprintf(s_client_id,"S&%s&%d&1",g_bf_code,product_ID);
	printf("\r\n");
	printf("s_client_id:%s",s_client_id);
	//MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
	//data.willFlag = 0;
	//data.MQTTVersion = 3;
	//data.clientID.cstring = s_client_id;
	//data.username.cstring = "hengyuan";
	//data.password.cstring = "hengyuanIot";

	//data.keepAliveInterval = MQTT_KEEP_ALIVE_TIME_S;
	//data.cleansession = 1;
	/*
	typedef struct{
     uint8_t index;
     uint8_t dev_add;
	 uint16_t reg_add;
	 uint16_t data_len;
	 } HAAS_DEV_RS485;
*/
    //strcpy(build_id,GetIniKeyString("config","build_id",FILENAME));
	RS485_type = GetIniKeyInt("config", "RS485_type", FILENAME);
	haas_device_num = GetIniKeyInt("config", "haas_dev_num", FILENAME);
	if (haas_device_num > (sizeof(g_haas_dev_rs485) / sizeof(g_haas_dev_rs485[0]))) {
		haas_device_num = (sizeof(g_haas_dev_rs485) / sizeof(g_haas_dev_rs485[0]));
	}
	g_haas_dev_rs485[1].dev_add = GetIniKeyInt("dev02", "dev_add", FILENAME);

	printf("num 2 add is: %d\r\n",g_haas_dev_rs485[1].dev_add);
	for(int i =0;i<haas_device_num;i++)
//	for(int i =0;i<2;i++)
	{
		g_haas_dev_rs485[i].index = i+1;
		char item_name[20];
		char item_num1[20];
		char item_num2[20];
		char item_num3[20];
		char item_num4[20];
		char item_num5[20];
		if(i<9)
		{
		sprintf(item_name,"dev0%d",i+1);
		sprintf(item_num1,"dev_add0%d",i+1);
		sprintf(item_num2,"reg_add0%d",i+1);
		sprintf(item_num3,"data_len0%d",i+1);
		sprintf(item_num4,"cmd0%d",i+1);
		sprintf(item_num5,"type0%d",i+1);
		}
		else
		{
		sprintf(item_name,"dev%d",i+1);
		sprintf(item_num1,"dev_add%d",i+1);
		sprintf(item_num2,"reg_add%d",i+1);
		sprintf(item_num3,"data_len%d",i+1);
		sprintf(item_num4,"cmd%d",i+1);
		sprintf(item_num5,"type%d",i+1);
		}
		g_haas_dev_rs485[i].dev_add = GetIniKeyInt(item_name, item_num1, FILENAME);
		g_haas_dev_rs485[i].reg_add = GetIniKeyInt(item_name, item_num2, FILENAME);
		g_haas_dev_rs485[i].data_len = GetIniKeyInt(item_name, item_num3, FILENAME);
		g_haas_dev_rs485[i].cmd = GetIniKeyInt(item_name, item_num4, FILENAME);
		g_haas_dev_rs485[i].type = GetIniKeyInt(item_name, item_num5, FILENAME);
		//strcpy(floor_id,GetIniKeyString("config","floor_id",FILENAME));
		printf("haas device num is:%s,dev_add:%d,reg_add:%d,cmd:%d,data_len:%d,type:%d\r\n",item_name,g_haas_dev_rs485[i].dev_add,g_haas_dev_rs485[i].reg_add,g_haas_dev_rs485[i].cmd,g_haas_dev_rs485[i].data_len,g_haas_dev_rs485[i].type);
		//item_name[20] = "";
	}
	int dev_ctrl01 = GetIniKeyInt("config", "dev_ctrl01", FILENAME);
	int dev_ctrl02 = GetIniKeyInt("config", "dev_ctrl02", FILENAME);
	device_control_cmd(0,dev_ctrl02);
		
	//while (1)
	//{
		//out_mqtt_init(&n, &c, &data);
		//out_mqtt_loop();
	//}

	do_mqtt_main();

	return NULL;
#endif
}

void haas_mqtt_data_upload(void)
{
	char s_payload[UART_DATA_BUF_SIZE];
	char s_data[16384];
	char s_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
	snprintf(s_topic_buf, sizeof(s_topic_buf), "/%d/%s/property/post",product_ID,g_bf_code);

#if 0
	snprintf(s_payload, sizeof(s_payload), "{\r\n\
	\t\"V01\": %d,\r\n\
	\t\"V24\": %d\r\n\
	}",0,0);
#endif

int len = 0;
int len1 = 0;
sprintf(s_data,"{");
len +=1;
	for(int i =0;i<haas_device_num;i++)
	{
		HAAS_DEV_RS485 *dev = &g_haas_dev_rs485[i];
		if (len >= (int)sizeof(s_data) - 16) {
			break;
		}
		if(i<9)
		{
			if (dev->is_string) {
				len1 = snprintf(s_data + len, sizeof(s_data) - len,
			                "\t\"V0%d\": \"%s\",\r\n", i + 1, dev->value_text);
		} else {
			len1 = snprintf(s_data + len, sizeof(s_data) - len,
			                "\t\"V0%d\": %.6f,\r\n", i + 1, dev->value2);
		}
	}
	else
	{
		if (dev->is_string) {
			len1 = snprintf(s_data + len, sizeof(s_data) - len,
			                "\t\"V%d\": \"%s\",\r\n", i + 1, dev->value_text);
		} else {
			len1 = snprintf(s_data + len, sizeof(s_data) - len,
			                "\t\"V%d\": %.6f,\r\n", i + 1, dev->value2);
		}
	}
	if (len1 < 0) {
		len1 = 0;
	}
	if (len + len1 >= (int)sizeof(s_data)) {
		len1 = 0;
		break;
	}
	len += len1;
}
if (len >= 3) {
	len -= 3;  // 去掉最后一个字段后的 ",\r\n"
	if (len < (int)sizeof(s_data)) {
		s_data[len] = '\0';
	}
}

	/* 追加工单解析字段 */
	if (WorkOrder.assign_name[0] != '\0') {
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                "%s\"PN\": \"%s\"", (len > 1) ? ",\r\n\t" : "\r\n\t", WorkOrder.assign_name);
	}
	if (WorkOrder.Product_name[0] != '\0') {
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                ",\r\n\t\"RN\": \"%s\"", WorkOrder.Product_name);
	}
	if (WorkOrder.quantity_double > 0.0) {
		char lw_buf[32];
		if (WorkOrder.unit[0] != '\0') {
			snprintf(lw_buf, sizeof(lw_buf), "%.4f %s",
			         WorkOrder.quantity_double, WorkOrder.unit);
		} else {
			snprintf(lw_buf, sizeof(lw_buf), "%.4f", WorkOrder.quantity_double);
		}
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                ",\r\n\t\"LW\": \"%s\"", lw_buf);
	}
	if (WorkOrder.mes_mix_weight > 0.0 || WorkOrder.actual_mix_weight > 0.0) {
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                ",\r\n\t\"LW_MES\": %.4f", WorkOrder.mes_mix_weight);
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                ",\r\n\t\"LW_ACT\": %.4f", WorkOrder.actual_mix_weight);
	}
	if (WorkOrder.operator_id[0] != '\0') {
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                ",\r\n\t\"Operator\": \"%s\"", WorkOrder.operator_id);
	}

	for (int i = 0; i < WorkOrder.material_count && i < 10; ++i) {
		char key_rmc[8];
		char key_tw[8];
		snprintf(key_rmc, sizeof(key_rmc), "RMC%02d", i + 1);
		snprintf(key_tw, sizeof(key_tw), "TW%02d", i + 1);
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                ",\r\n\t\"%s\": \"%s\"", key_rmc, WorkOrder.materials[i].name);

		char tw_buf[48];
		const char *tw_unit = NULL;
		if (WorkOrder.materials[i].unit[0] != '\0') {
			tw_unit = WorkOrder.materials[i].unit;
		} else if (WorkOrder.unit[0] != '\0') {
			tw_unit = WorkOrder.unit;
		}
		if (tw_unit) {
			snprintf(tw_buf, sizeof(tw_buf), "%.4f %s",
			         WorkOrder.materials[i].target, tw_unit);
		} else {
			snprintf(tw_buf, sizeof(tw_buf), "%.4f",
			         WorkOrder.materials[i].target);
		}
		len += snprintf(s_data + len, sizeof(s_data) - len,
		                ",\r\n\t\"%s\": \"%s\"", key_tw, tw_buf);
	}

len += snprintf(s_data + len, sizeof(s_data) - len, "\r\n}");

printf("mqtt_data_upload topic:%s\r\n",s_topic_buf);
//      out_publish_msg(s_topic_buf, s_payload);
#if 0
snprintf(s_payload, sizeof(s_payload), "{\r\n\
		\t\"V01\": %.6f,\r\n\
		\t\"V02\": %.6f,\r\n\
		\t\"V03\": %.6f,\r\n\
		\t\"V04\": %.6f,\r\n\
		\t\"V05\": %.6f,\r\n\
		\t\"V06\": %.6f,\r\n\
		\t\"V07\": %.6f,\r\n\
		\t\"V08\": %.6f,\r\n\
		\t\"V09\": %.6f,\r\n\
		\t\"V10\": %.6f,\r\n\
		\t\"V11\": %.6f,\r\n\
		\t\"V12\": %.6f,\r\n\
		\t\"V13\": %.6f,\r\n\
		\t\"V14\": %.6f,\r\n\
		\t\"V15\": %.6f,\r\n\
		\t\"V16\": %.6f,\r\n\
		\t\"V17\": %.6f,\r\n\
		\t\"V18\": %.6f,\r\n\
		\t\"V19\": %.6f,\r\n\
		\t\"V20\": %.6f,\r\n\
		\t\"V21\": %.6f,\r\n\
		\t\"V22\": %.6f,\r\n\
		\t\"V23\": %.6f,\r\n\
		\t\"V24\": %.6f,\r\n\
		\t\"V25\": %.6f,\r\n\
		\t\"V26\": %.6f\r\n\
		}", g_haas_dev_rs485[0].value2,g_haas_dev_rs485[1].value2,g_haas_dev_rs485[2].value2,g_haas_dev_rs485[3].value2,g_haas_dev_rs485[4].value2,g_haas_dev_rs485[5].value2,g_haas_dev_rs485[6].value2,g_haas_dev_rs485[7].value2,g_haas_dev_rs485[8].value2,g_haas_dev_rs485[9].value2,g_haas_dev_rs485[10].value2,g_haas_dev_rs485[11].value2,g_haas_dev_rs485[12].value2,g_haas_dev_rs485[13].value2,g_haas_dev_rs485[14].value2,g_haas_dev_rs485[15].value2,g_haas_dev_rs485[16].value2,g_haas_dev_rs485[17].value2,g_haas_dev_rs485[18].value2,g_haas_dev_rs485[19].value2,g_haas_dev_rs485[20].value2,g_haas_dev_rs485[21].value2,g_haas_dev_rs485[22].value2,g_haas_dev_rs485[23].value2,g_haas_dev_rs485[24].value2,g_haas_dev_rs485[25].value2);
#endif
printf("upload message:%s\r\n",s_data);
//out_publish_msg(s_topic_buf,s_data);
do_mqtt_send(s_mqtt_client, s_topic_buf, s_data);

}
