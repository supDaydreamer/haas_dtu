//#define TEMP_STATUS_FILE				("/tmp/fct.status")
//#define FCT_OFFLINE_TIME_S				(10)
//#define FCT_OFFLINE_PUSH_AGAIN_TIME_S	(5)
//#define FCT_START_WAIT_TIME_MS			(3000)

//#define MQTT_KEEP_ALIVE_TIME_S			(60)

//#define OUT_MQTT_YIELD_MS				(1000)
#define OUT_MQTT_HOST					("47.100.192.18")
//#define OUT_MQTT_PORT					(1883)
//#define OUT_MQTT_QOS					(QOS0)
//#define OUT_MQTT_FAILED_SLEEP_TIME		(3)
//#define OUT_MQTT_BUF_SIZE				(100 * 1024)	//100KB
//#define OUT_CLIENT_ID_PRIFIX			("BFM-LOCK_GATE")
//#define OUT_TOPIC_BUF_SIZE				(1024)

//#define MQTT_TOPIC_LEN_MAX				(128)

//#define FILENAME "/mnt/usr/device.conf"

#include <stdbool.h>
#include "common.h"
#include "MQTTClient.h"
#include "json.h"
#include "mqtt.h"
#include "data.h"
#include "uart.h"
#include "bf_cmd.h"

time_t g_last_heart_beat_time = 0;
time_t g_last_device_sta_time = 0;

//pthread_mutex_t g_publish_mutex = PTHREAD_MUTEX_INITIALIZER;
uint8_t g_publish_mutex_lock = 0;
uint8_t yield_flag = 0;

char g_payload[UART_DATA_BUF_SIZE];
char g_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};

static Network n;
static MQTTClient c;
static MQTTMessage pubmsg;


static uint8_t mqtt_read_buf[OUT_MQTT_BUF_SIZE];
static uint8_t mqtt_send_buf[OUT_MQTT_BUF_SIZE];
//static uint8_t pubmsg_buf[OUT_MQTT_BUF_SIZE];
static uint8_t inmsg_buf[OUT_MQTT_BUF_SIZE];

static char s_get_params_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
static char s_set_params_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
static char s_device_control_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
static char s_get_device_state_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
static char s_device_reset_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};

static char s_uart_1_write_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
static char s_uart_2_write_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};

static char s_set_humidifier_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
static char s_set_aircondition_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};


uint16_t  product_ID;
char s_client_id[30];


#if 0
static void haas_out_publish_msg_with_len(char *topic, char* msg, size_t len)
{
	pubmsg.payload = msg;
	pubmsg.payloadlen = len;
	pubmsg.qos = OUT_MQTT_QOS;
	pubmsg.retained = 0;
	pubmsg.dup = 0;
	MQTTPublish(&c, topic, &pubmsg);
}

static void haas_out_publish_msg(char *topic, char* msg)
{
	haas_out_publish_msg_with_len(topic, msg, strlen(msg));
}
#endif

static uint8_t *hexstr2buf(char *str, size_t len, size_t *const out_len)
{
	static uint8_t data_buf[1024] = {0};
	bool high4bit = true;
	char tmp4bit = 0;
	uint8_t tmp = 0;
	size_t buf_len = 0;

	for (size_t i = 0; i < len; i++) {
		char c = str[i];
		switch (c) {
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			c = c - 'a' + 'A';
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			if (c < 'A') {
				tmp4bit = c - '0';
			} else {
				tmp4bit = c - 'A' + 10;
			}
			if (high4bit) {
				tmp = ((tmp4bit & 0xF) << 4) & 0xF0;
			} else {
				tmp = ((tmp & 0xF0) | (tmp4bit & 0xF)) & 0xFF;
				data_buf[buf_len++] = tmp;
			}
			high4bit = !high4bit;
			break;
		default:
			break;
		}
	}
	*out_len = buf_len;
	return data_buf;
}


void device_control_cmd(uint8_t h_val,uint8_t l_val)
{
   uint8_t control_cmd_buf[20];
   control_cmd_buf[0] = 0x5A;
   control_cmd_buf[1] = 0xA5;
   control_cmd_buf[2] = 0xA2;
   control_cmd_buf[3] = h_val;
   control_cmd_buf[4] = l_val;
   uart_tx(1, control_cmd_buf,5);

}

static void haas_out_messageArrived(MessageData* md)
{
	MQTTMessage* message = md->message;

	dbg_printf("---------- Message ----------\n");
	dbg_printf("topicName: %.*s\n", md->topicName->lenstring.len, md->topicName->lenstring.data);
	dbg_printf("qos: %u\n", message->qos);
	dbg_printf("dup: %u\n", message->dup);
	dbg_printf("retained: %u\n", message->retained);
	dbg_printf("id: %u\n", message->id);
	dbg_printf("payload: %.*s\n", (int)message->payloadlen, (char*)message->payload);
	dbg_printf("payloadlen: %zu\n", message->payloadlen);
	dbg_printf("-----------------------------\n\n");

	char endstr[] = { '\0' };
	size_t inmsg_payloadlen = (sizeof(inmsg_buf) > message->payloadlen + strlen(endstr))? message->payloadlen : sizeof(inmsg_buf) - sizeof(endstr);
	memcpy(inmsg_buf, message->payload, inmsg_payloadlen);
	memcpy(inmsg_buf + inmsg_payloadlen, endstr, sizeof(endstr));

	dbg_printf("--> in message: %s\n", inmsg_buf);
//s_set_aircondition_topic_buf);s_set_humidifier_topic_buf
	if (0 == strncmp(s_get_params_topic_buf, md->topicName->lenstring.data, strlen(s_get_params_topic_buf))) {
	} 
	else if (0 == strncmp(s_set_humidifier_topic_buf, md->topicName->lenstring.data, strlen(s_set_humidifier_topic_buf))) {
		cJSON *data_json = read_json_str(inmsg_buf);
		if (data_json) {
			cJSON *uniqueCode_json = read_json_obj(data_json, "uniqueCode");
			cJSON *timeStamp_json = read_json_obj(data_json, "timeStamp");
			cJSON *data_params_json = read_json_obj(data_json, "data");

			cJSON *unitSwitch_json = read_json_obj(data_params_json, "unitSwitch");
			cJSON *setHumi_json = read_json_obj(data_params_json, "sHumi");
			cJSON *forceAct_json = read_json_obj(data_params_json, "forceAct");

			if (unitSwitch_json) humiDevice.switch_mode = unitSwitch_json->valueint;
			if (setHumi_json) humiDevice.setHumi_value = setHumi_json->valueint;
			if (forceAct_json) humiDevice.control_mode = forceAct_json->valueint;
			humiDevice.data_update = 1;
			close_json(data_json);
		}

	}
	else if (0 == strncmp(s_set_aircondition_topic_buf, md->topicName->lenstring.data, strlen(s_set_aircondition_topic_buf))) {
		cJSON *data_json = read_json_str(inmsg_buf);
		if (data_json) {
			cJSON *uniqueCode_json = read_json_obj(data_json, "uniqueCode");
			cJSON *timeStamp_json = read_json_obj(data_json, "timeStamp");
			cJSON *data_params_json = read_json_obj(data_json, "data");

			cJSON *unitSwitch_json = read_json_obj(data_params_json, "unitSwitch");
			cJSON *setTemp_json = read_json_obj(data_params_json, "sTemp");
			cJSON *workMode_json = read_json_obj(data_params_json, "workMode");
			cJSON *windSpeed_json = read_json_obj(data_params_json, "speed");

			if (unitSwitch_json) airDevice.switch_mode = unitSwitch_json->valueint;
			if (setTemp_json) airDevice.temp_value = setTemp_json->valueint;
			if (workMode_json) airDevice.work_mode = workMode_json->valueint;
			if (windSpeed_json) airDevice.wind_speed = windSpeed_json->valueint;
			airDevice.data_update = 1;
			close_json(data_json);
		}
	}
	else if (0 == strncmp(s_set_params_topic_buf, md->topicName->lenstring.data, strlen(s_set_params_topic_buf))) {
	} else if (0 == strncmp(s_device_control_topic_buf, md->topicName->lenstring.data, strlen(s_device_control_topic_buf))) {
			cJSON *data_json = read_json_str(inmsg_buf);
			if (data_json) {
				cJSON *ctrl_json = read_json_obj(data_json, "heat_ctrl");
				cJSON *light_json = read_json_obj(data_json, "light_ctrl");
				uint8_t heat_control_val = ctrl_json->valueint;
				uint8_t light_control_val = light_json->valueint;
				device_control_cmd(heat_control_val,light_control_val);
				close_json(data_json);
			}


	} else if (0 == strncmp(s_get_device_state_topic_buf, md->topicName->lenstring.data, strlen(s_get_device_state_topic_buf))) {
	} else if (0 == strncmp(s_device_reset_topic_buf, md->topicName->lenstring.data, strlen(s_device_reset_topic_buf))) {
	} else if (0 == strncmp(s_uart_1_write_topic_buf, md->topicName->lenstring.data, strlen(s_device_reset_topic_buf))) {
		uint8_t *p = NULL;
		size_t len = 0;
		p = hexstr2buf(message->payload, message->payloadlen, &len);
		printf("uart1 receive data: len --- %d",len);
		uart_tx(1, p, len);
	} else if (0 == strncmp(s_uart_2_write_topic_buf, md->topicName->lenstring.data, strlen(s_device_reset_topic_buf))) {
		uint8_t *p = NULL;
		size_t len = 0;
		p = hexstr2buf(message->payload, message->payloadlen, &len);
		uart_tx(2, p, len);
	} else {
		printf("########## other topic: %.*s\n", md->topicName->lenstring.len, md->topicName->lenstring.data);
	}
}

static void haas_out_mqtt_init(Network* network, MQTTClient* mqtt, MQTTPacket_connectData* connect_data)
{
	int rc = -1;

	snprintf(s_get_params_topic_buf, sizeof(s_get_params_topic_buf), "/Beefind/prep/v1/down/lockgateway/get_params/%s", g_bf_code);
	snprintf(s_set_params_topic_buf, sizeof(s_set_params_topic_buf), "/Beefind/prep/v1/down/lockgateway/set_params/%s", g_bf_code);

//	snprintf(s_set_humidifier_topic_buf, sizeof(s_set_humidifier_topic_buf), "/%d/%s/function/get",product_ID,g_bf_code);
	//snprintf(s_set_aircondition_topic_buf, sizeof(s_set_aircondition_topic_buf), "/Beefind/prep/v1/down/haas/ac_set/%s", g_bf_code);

	snprintf(s_device_control_topic_buf, sizeof(s_device_control_topic_buf), "/%d/%s/function/get",product_ID,g_bf_code);
//	snprintf(s_get_device_state_topic_buf, sizeof(s_get_device_state_topic_buf), "/Beefind/prep/v1/down/lockgateway/get_device_state/%s", g_bf_code);
	snprintf(s_device_reset_topic_buf, sizeof(s_device_reset_topic_buf), "/Beefind/prep/v1/down/lockgateway/device_reset/%s", g_bf_code);

	snprintf(s_uart_1_write_topic_buf, sizeof(s_uart_1_write_topic_buf), "/Beefind/prep/v1/down/lockgateway/uart_1_write/%s", g_bf_code);
	snprintf(s_uart_2_write_topic_buf, sizeof(s_uart_2_write_topic_buf), "/Beefind/prep/v1/down/lockgateway/uart_2_write/%s", g_bf_code);

	while (rc != 0) {
		NetworkInit(network);
		NetworkConnect(network, OUT_MQTT_HOST, OUT_MQTT_PORT);
		MQTTClientInit(mqtt, network, 1000, mqtt_send_buf, sizeof(mqtt_send_buf), mqtt_read_buf, sizeof(mqtt_read_buf));

		dbg_printf("Connecting to %s:%d\n", OUT_MQTT_HOST, OUT_MQTT_PORT);
		rc = MQTTConnect(mqtt, connect_data);
		dbg_printf("Connected, code: %d\n", rc);

		dbg_printf("Subscribing to %s\n", s_get_params_topic_buf);
		rc += MQTTSubscribe(mqtt, s_get_params_topic_buf, OUT_MQTT_QOS, out_messageArrived);
		dbg_printf("Subscribed, code: %d\n", rc);

		dbg_printf("Subscribing to %s\n", s_set_params_topic_buf);
		rc += MQTTSubscribe(mqtt, s_set_params_topic_buf, OUT_MQTT_QOS, out_messageArrived);
		dbg_printf("Subscribed, code: %d\n", rc);

//		dbg_printf("Subscribing to %s\n", s_set_humidifier_topic_buf);
//		rc += MQTTSubscribe(mqtt, s_set_humidifier_topic_buf, OUT_MQTT_QOS, out_messageArrived);
//		dbg_printf("Subscribed, code: %d\n", rc);

//		dbg_printf("Subscribing to %s\n", s_set_aircondition_topic_buf);
//		rc += MQTTSubscribe(mqtt, s_set_aircondition_topic_buf, OUT_MQTT_QOS, out_messageArrived);
//		dbg_printf("Subscribed, code: %d\n", rc);

		dbg_printf("Subscribing to %s\n", s_device_control_topic_buf);
		rc += MQTTSubscribe(mqtt, s_device_control_topic_buf, OUT_MQTT_QOS, out_messageArrived);
		dbg_printf("Subscribed, code: %d\n", rc);

//		dbg_printf("Subscribing to %s\n", s_get_device_state_topic_buf);
//		rc += MQTTSubscribe(mqtt, s_get_device_state_topic_buf, OUT_MQTT_QOS, out_messageArrived);
//		dbg_printf("Subscribed, code: %d\n", rc);

		dbg_printf("Subscribing to %s\n", s_device_reset_topic_buf);
		rc += MQTTSubscribe(mqtt, s_device_reset_topic_buf, OUT_MQTT_QOS, out_messageArrived);
		dbg_printf("Subscribed, code: %d\n", rc);

		dbg_printf("Subscribing to %s\n", s_uart_1_write_topic_buf);
		rc += MQTTSubscribe(mqtt, s_uart_1_write_topic_buf, OUT_MQTT_QOS, out_messageArrived);
		dbg_printf("Subscribed, code: %d\n", rc);

		dbg_printf("Subscribing to %s\n", s_uart_2_write_topic_buf);
		rc += MQTTSubscribe(mqtt, s_uart_2_write_topic_buf, OUT_MQTT_QOS, out_messageArrived);
		dbg_printf("Subscribed, code: %d\n", rc);

		if (rc != 0) {
			dbg_printf("MQTT init failed! Retry after %d s...\n\n", OUT_MQTT_FAILED_SLEEP_TIME);
			MQTTDisconnect(mqtt);
			NetworkDisconnect(network);

			sleep(OUT_MQTT_FAILED_SLEEP_TIME);
		}
	}

}

void haas_heart_beat_publish()
{
	char s_payload[UART_DATA_BUF_SIZE];
	char s_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
	snprintf(s_topic_buf, sizeof(s_topic_buf), "/Beefind/prep/v1/up/lockgateway/heartbeat/%s", g_bf_code);
	
	char *net_type = check_net();
	char *net_name = check_net_name();
	char *sim = check_sim();
	const char *g_bt_version = "TEST";
	const char *bt_version = ((g_bt_version[0] == '\0') ? "unknown" : g_bt_version);

	snprintf(s_payload, sizeof(s_payload), "{\r\n\
\t\"uniqueCode\": \"%s\",\r\n\
\t\"isConfig\": %u,\r\n\
\t\"errCode\": %d,\r\n\
\t\"netType\": \"%s\",\r\n\
\t\"netName\": \"%s\",\r\n\
\t\"sim\": \"%s\",\r\n\
\t\"version\": \"%s\",\r\n\
\t\"btVersion\": \"%s\"\r\n\
}", g_bf_code, 0, 0, net_type, net_name, sim, g_version, bt_version);

	out_publish_msg(s_topic_buf, s_payload);
}

void uart_rx_publish(uint32_t index, char *str)
{
	//pthread_mutex_lock(&g_publish_mutex);

	snprintf(g_topic_buf, sizeof(g_topic_buf), "/Beefind/prep/v1/debug/lockgateway/uart_%u_rx/%s", index, g_bf_code);
	snprintf(g_payload, sizeof(g_payload), str);

	g_publish_mutex_lock = 1;
}

void uart_tx_publish(uint32_t index, char *str)
{
	//pthread_mutex_lock(&g_publish_mutex);
	printf("debug test 11111111111111111111111\r\n");
	snprintf(g_topic_buf, sizeof(g_topic_buf), "/Beefind/prep/v1/debug/lockgateway/uart_%u_tx/%s", index, g_bf_code);
	snprintf(g_payload, sizeof(g_payload), str);

	g_publish_mutex_lock = 1;
}

static void haas_out_mqtt_loop()
{
	//yield_flag = 1;
	while (1) {
		if (!MQTTIsConnected(&c)) {
			//yield_flag = 0;
			dbg_printf("MQTT OFFLINE! Retry after %d s...\n\n", OUT_MQTT_FAILED_SLEEP_TIME);
			sleep(OUT_MQTT_FAILED_SLEEP_TIME);
			break;
		}
		 int mqtt_return = MQTTYield(&c, OUT_MQTT_YIELD_MS);
		 //dbg_printf("---> MQTTYield: %d\n", mqtt_return);
		 //dbg_printf("------> device_type: %u\n", g_485_device_type);
#if 1
		// test2
		uint8_t msg_buf[1024] = {0};
		size_t msg_buf_len = 0;
		while (read_uart_msg(g_uart1_rx_box_handle, msg_buf, &msg_buf_len)) {
			if (msg_buf_len > sizeof(msg_buf)) msg_buf_len = sizeof(msg_buf);
#if 0
			dbg_printf("\n==========> msg_print (%zu): \n", msg_buf_len);
			for (size_t i = 0; i < msg_buf_len; i++) {
				dbg_printf("%02X ", msg_buf[i]);
			}
			dbg_printf("\n\n");
#endif
			on_uart_1_read(msg_buf, msg_buf_len);
			haas_out_publish_msg(g_topic_buf, g_payload);
		}
		while (read_uart_msg(g_uart2_rx_box_handle, msg_buf, &msg_buf_len)) {
			on_uart_2_read(msg_buf, msg_buf_len);
			haas_out_publish_msg(g_topic_buf, g_payload);
		}
		while (read_uart_msg(g_uart1_tx_box_handle, msg_buf, &msg_buf_len)) {
			on_uart_1_write(msg_buf, msg_buf_len);
			haas_out_publish_msg(g_topic_buf, g_payload);
		}
		while (read_uart_msg(g_uart2_tx_box_handle, msg_buf, &msg_buf_len)) {
			on_uart_2_write(msg_buf, msg_buf_len);
			haas_out_publish_msg(g_topic_buf, g_payload);
		}
		static uint8_t bt_version_not_found = 1;
		time_t now_time = time(NULL);
		const char *g_bt_version = "TEST";
		if (bt_version_not_found == 1 && g_bt_version[0] != '\0') {
			bt_version_not_found = 0;
			haas_heart_beat_publish();
			g_last_heart_beat_time = now_time;
		} else if (now_time - g_last_heart_beat_time > 60) {
			//mqtt_haas_data_publish();
			haas_heart_beat_publish();
			g_last_heart_beat_time = now_time;
		}
		if(mqtt_publish_en == 1)
		{			
			if (haas_is_mqtt_online()) {
				mqtt_haas_data_publish();
				mqtt_publish_en = 2;
			}
		}
		if(now_time - g_last_device_sta_time > 300)
		{
			if (g_485_device_type == DEVICE_485_AIR) {
				mqtt_airDevice_data_publish();
			} else if (g_485_device_type == DEVICE_485_HUMI) {
				mqtt_humiDevice_data_publish();
			}
			g_last_device_sta_time = now_time;
		}
#endif
	}

	dbg_printf("mqtt loop closing...\n");	
	//close_fp();
	//dbg_printf("(1/3)fp clesed.\n");
	if (MQTTIsConnected(&c)) MQTTDisconnect(&c);
	dbg_printf("(2/3)client clesed.\n");
	NetworkDisconnect(&n);
	dbg_printf("(3/3)network clesed.\n");
	dbg_printf("===== mqtt loop closed. =====\n\n");	
}

bool haas_is_mqtt_online()
{
	if (MQTTIsConnected(&c)) return true;
	sleep(1);
	 if (MQTTIsConnected(&c)) return true;
	// sleep(1);
	// if (MQTTIsConnected(&c)) return true;
	// sleep(1);
	// if (MQTTIsConnected(&c)) return true;
	// sleep(1);
	// if (MQTTIsConnected(&c)) return true;
	return false;
	//return true;
}

void mqtt_humiDevice_data_publish()
{
	char s_payload[UART_DATA_BUF_SIZE];
	char s_data[400];
	char s_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
	time_t now_timestamp = time(NULL);

	snprintf(s_topic_buf, sizeof(s_topic_buf), "/Beefind/prep/v1/up/haas/humidifier_data/%s", g_bf_code);
	snprintf(s_data,sizeof(s_data),"{\r\n\
			\t\"unitSwitch\": %d,\r\n\
			\t\"sHumi\": %u,\r\n\
			\t\"workMode\": %u,\r\n\
			\t\"errorCode\": %u,\r\n\
			\t\"speed\": %u,\r\n\
			\t\"loopSpeed\": %u,\r\n\
			\t\"exSpeed\": %u,\r\n\
			\t\"swingSwitch\": %u,\r\n\
			\t\"forceAct\": %u\r\n\
			}", humiDevice.switch_mode, humiDevice.setHumi_value, humiDevice.work_mode, humiDevice.error_code, humiDevice.windSpeed, humiDevice.windspeed_loop, humiDevice.windspeed_ex, humiDevice.swing_mode,humiDevice.control_mode);
	snprintf(s_payload, sizeof(s_payload), "{\r\n\
			\t\"uniqueCode\": \"%s\",\r\n\
			\t\"timestamp\": %u,\r\n\
			\t\"data\": %s\r\n\
			}", g_bf_code, now_timestamp, s_data);

	out_publish_msg(s_topic_buf, s_payload);	
}

void mqtt_airDevice_data_publish()
{
	char s_payload[UART_DATA_BUF_SIZE];
	char s_data[200];
	char s_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
	time_t now_timestamp = time(NULL);

	snprintf(s_topic_buf, sizeof(s_topic_buf), "/Beefind/prep/v1/up/haas/ac_data/%s", g_bf_code);
	snprintf(s_data,sizeof(s_data),"{\r\n\
			\t\"unitSwitch\": %d,\r\n\
			\t\"sTemp\": %u,\r\n\
			\t\"workMode\": %u,\r\n\
			\t\"errorCode\": %u\r\n\
			}", airDevice.switch_mode, airDevice.temp_value, airDevice.work_mode, airDevice.error_code);
	snprintf(s_payload, sizeof(s_payload), "{\r\n\
			\t\"uniqueCode\": \"%s\",\r\n\
			\t\"timestamp\": %u,\r\n\
			\t\"data\": %s\r\n\
			}", g_bf_code, now_timestamp, s_data);

	out_publish_msg(s_topic_buf, s_payload);	
}

void mqtt_haas_data_publish()
{
	char s_payload[UART_DATA_BUF_SIZE];
	char s_data[200];
	char s_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
	time_t now_timestamp = time(NULL);
	snprintf(s_topic_buf, sizeof(s_topic_buf), "/Beefind/prep/v1/up/haas/energy_data/%s", g_bf_code);
	snprintf(s_data,sizeof(s_data),"{\r\n\
			\t\"powerOnOff\": \"%d\",\r\n\
			\t\"energy\": %u,\r\n\
			\t\"power\": %u,\r\n\
			\t\"v1\": \"%u\",\r\n\
			\t\"v2\": \"%u\",\r\n\
			\t\"v3\": \"%u\",\r\n\
			\t\"a1\": \"%u\",\r\n\
			\t\"a2\": \"%u\",\r\n\
			\t\"a3\": \"%u\"\r\n\
			}", 1, M_value.dev_power_value, M_value.dev_power_ele_mqtt, M_value.dev_voltage1, M_value.dev_voltage2, M_value.dev_voltage3, M_value.dev_current1, M_value.dev_current2,M_value.dev_current3);
	snprintf(s_payload, sizeof(s_payload), "{\r\n\
			\t\"uniqueCode\": \"%s\",\r\n\
			\t\"timestamp\": \"%u\",\r\n\
			\t\"data\": %s\r\n\
			}", g_bf_code, now_timestamp, s_data);

	out_publish_msg(s_topic_buf, s_payload);
}

//void mqtt_data_upload(void)

void mqtt_data_upload(void)
{
	char s_payload[UART_DATA_BUF_SIZE];
	char s_data[16384];
	char s_topic_buf[MQTT_TOPIC_LEN_MAX] = {0};
	snprintf(s_topic_buf, sizeof(s_topic_buf), "/%d/%s/property/post",product_ID,g_bf_code);
	// snprintf(s_payload, sizeof(s_payload), "{\r\n\
	\t\"V01\": %d,\r\n\
		\t\"V24\": %d\r\n\

//}",0,0);
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
			                "\t\"V0%d\": %.1f,\r\n", i + 1, dev->value2);
		}
	}
	else
	{
		if (dev->is_string) {
			len1 = snprintf(s_data + len, sizeof(s_data) - len,
			                "\t\"V%d\": \"%s\",\r\n", i + 1, dev->value_text);
		} else {
			len1 = snprintf(s_data + len, sizeof(s_data) - len,
			                "\t\"V%d\": %.1f,\r\n", i + 1, dev->value2);
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
sprintf(s_data + len - 3,"}");

printf("mqtt_data_upload topic:%s\r\n",s_topic_buf);
//      out_publish_msg(s_topic_buf, s_payload);
#if 0
snprintf(s_payload, sizeof(s_payload), "{\r\n\
		\t\"V01\": %.1f,\r\n\
		\t\"V02\": %.1f,\r\n\
		\t\"V03\": %.1f,\r\n\
		\t\"V04\": %.1f,\r\n\
		\t\"V05\": %.1f,\r\n\
		\t\"V06\": %.1f,\r\n\
		\t\"V07\": %.1f,\r\n\
		\t\"V08\": %.1f,\r\n\
		\t\"V09\": %.1f,\r\n\
		\t\"V10\": %.1f,\r\n\
		\t\"V11\": %.1f,\r\n\
		\t\"V12\": %.1f,\r\n\
		\t\"V13\": %.1f,\r\n\
		\t\"V14\": %.1f,\r\n\
		\t\"V15\": %.1f,\r\n\
		\t\"V16\": %.1f,\r\n\
		\t\"V17\": %.1f,\r\n\
		\t\"V18\": %.1f,\r\n\
		\t\"V19\": %.1f,\r\n\
		\t\"V20\": %.1f,\r\n\
		\t\"V21\": %.1f,\r\n\
		\t\"V22\": %.1f,\r\n\
		\t\"V23\": %.1f,\r\n\
		\t\"V24\": %.1f,\r\n\
		\t\"V25\": %.1f,\r\n\
		\t\"V26\": %.1f\r\n\
		}", g_haas_dev_rs485[0].value2,g_haas_dev_rs485[1].value2,g_haas_dev_rs485[2].value2,g_haas_dev_rs485[3].value2,g_haas_dev_rs485[4].value2,g_haas_dev_rs485[5].value2,g_haas_dev_rs485[6].value2,g_haas_dev_rs485[7].value2,g_haas_dev_rs485[8].value2,g_haas_dev_rs485[9].value2,g_haas_dev_rs485[10].value2,g_haas_dev_rs485[11].value2,g_haas_dev_rs485[12].value2,g_haas_dev_rs485[13].value2,g_haas_dev_rs485[14].value2,g_haas_dev_rs485[15].value2,g_haas_dev_rs485[16].value2,g_haas_dev_rs485[17].value2,g_haas_dev_rs485[18].value2,g_haas_dev_rs485[19].value2,g_haas_dev_rs485[20].value2,g_haas_dev_rs485[21].value2,g_haas_dev_rs485[22].value2,g_haas_dev_rs485[23].value2,g_haas_dev_rs485[24].value2,g_haas_dev_rs485[25].value2);
#endif
printf("upload message:%s\r\n",s_data);
out_publish_msg(s_topic_buf,s_data);

}


void *haas_mqtt_main(void)
{
#if 1
	char mqtt_clientid[OUT_TOPIC_BUF_SIZE];

	snprintf(mqtt_clientid, sizeof(mqtt_clientid), "%s-%s", OUT_CLIENT_ID_PRIFIX, g_bf_code);

	dbg_printf("My client NAME is: %s\n", mqtt_clientid);
	

	product_ID =GetIniKeyInt("config", "device_id", FILENAME);
	sprintf(s_client_id,"S&%s&%d&1",g_bf_code,product_ID);
	printf("\r\n");
	printf("s_client_id:%s",s_client_id);
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
	data.willFlag = 0;
	data.MQTTVersion = 3;
	data.clientID.cstring = s_client_id;
	data.username.cstring = "hengyuan";
	data.password.cstring = "hengyuanIot";

	data.keepAliveInterval = MQTT_KEEP_ALIVE_TIME_S;
	data.cleansession = 1;
	/*
	typedef struct{
     uint8_t index;
     uint8_t dev_add;
	 uint16_t reg_add;
	 uint16_t data_len;
	 } HAAS_DEV_RS485;
*/
	//strcpy(build_id,GetIniKeyString("config","build_id",FILENAME));
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
		sprintf(item_num5,"type0%d",i+1);
		}
		else
		{
		sprintf(item_name,"dev%d",i+1);
		sprintf(item_num1,"dev_add%d",i+1);
		sprintf(item_num2,"reg_add%d",i+1);
		sprintf(item_num3,"data_len%d",i+1);
		sprintf(item_num5,"type%d",i+1);
		}
		g_haas_dev_rs485[i].dev_add = GetIniKeyInt(item_name, item_num1, FILENAME);
		g_haas_dev_rs485[i].reg_add = GetIniKeyInt(item_name, item_num2, FILENAME);
		g_haas_dev_rs485[i].data_len = GetIniKeyInt(item_name, item_num3, FILENAME);
		g_haas_dev_rs485[i].cmd = GetIniKeyInt(item_name, item_num4, FILENAME);
		g_haas_dev_rs485[i].type = GetIniKeyInt(item_name, item_num5, FILENAME);
		//strcpy(floor_id,GetIniKeyString("config","floor_id",FILENAME));
		printf("haas device num is:%s,dev_add:%d,reg_add:%d,data_len:%d,type:%d\r\n",item_name,g_haas_dev_rs485[i].dev_add,g_haas_dev_rs485[i].reg_add,g_haas_dev_rs485[i].data_len,g_haas_dev_rs485[i].type);
		//item_name[20] = "";
	}
		
	while (1)
	{
		haas_out_mqtt_init(&n, &c, &data);
		haas_out_mqtt_loop();
	}

	return NULL;
#endif
}


// void *yield_main(void)
// {
	// while(1)
	// {
	// 	uint8_t msg_buf[1024] = {0};
	// 	size_t msg_buf_len = 0;
	// 	while (read_uart_msg(g_uart1_rx_box_handle, msg_buf, &msg_buf_len)) {
	// 		if (msg_buf_len > sizeof(msg_buf)) msg_buf_len = sizeof(msg_buf);
	// 		dbg_printf("\n==========> msg_print (%zu): \n", msg_buf_len);
	// 		for (size_t i = 0; i < msg_buf_len; i++) {
	// 			dbg_printf("%02X ", msg_buf[i]);
	// 		}
	// 		dbg_printf("\n\n");
	// 		on_uart_1_read(msg_buf, msg_buf_len);
	// 		out_publish_msg(g_topic_buf, g_payload);
	// 	}
	// 	while (read_uart_msg(g_uart2_rx_box_handle, msg_buf, &msg_buf_len)) {
	// 		on_uart_2_read(msg_buf, msg_buf_len);
	// 		out_publish_msg(g_topic_buf, g_payload);
	// 	}
	// 	while (read_uart_msg(g_uart1_tx_box_handle, msg_buf, &msg_buf_len)) {
	// 		on_uart_1_write(msg_buf, msg_buf_len);
	// 		out_publish_msg(g_topic_buf, g_payload);
	// 	}
	// 	while (read_uart_msg(g_uart2_tx_box_handle, msg_buf, &msg_buf_len)) {
	// 		on_uart_2_write(msg_buf, msg_buf_len);
	// 		out_publish_msg(g_topic_buf, g_payload);
	// 	}
	// 	static uint8_t bt_version_not_found = 1;
	// 	time_t now_time = time(NULL);
	// 	const char *g_bt_version = "TEST";
	// 	if (bt_version_not_found == 1 && g_bt_version[0] != '\0') {
	// 		bt_version_not_found = 0;
	// 		heart_beat_publish();
	// 		g_last_heart_beat_time = now_time;
	// 	} else if (now_time - g_last_heart_beat_time > 60) {
	// 		heart_beat_publish();
	// 		g_last_heart_beat_time = now_time;
	// 	}
// 	// }
// }
