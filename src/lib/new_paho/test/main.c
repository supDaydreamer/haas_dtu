//#define ADDRESS			"mqtt://47.100.192.18:1883"
#define ADDRESS			"mqtt://mqtt.51beefind.com:1883"
#define USERNAME		"hengyuan"
#define PASSWORD		"hengyuanIot"
#define CLIENTID		"S&DF9CD002&629&1"
#define TOPIC			"testtopic"
#define SEND_TOPIC		"testtopic/send"
#define SEND_MSG		"Hello PAHO!"
#define QOS				1
#define CONNECT_TIMEOUT	10
#define KEEPALIVE		10
#define RUN_SLEEP_TIME	5


#include <unistd.h>
#include <string.h>
#include "MQTTAsync.h"


static int do_mqtt_connect(MQTTAsync client, unsigned int wait_time);
static int do_mqtt_subscribe(MQTTAsync client, unsigned int wait_time);


static int on_message(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
	(void)context;
	(void)topicLen;

	printf("[PAHO] on_message:\n");
	printf("    --> topic: %s\n", topicName);
	printf("    --> message: %.*s\n", message->payloadlen, (char*)message->payload);
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

static int do_mqtt_send(MQTTAsync client)
{
	static unsigned int s_seq = 0;

	int rc;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTAsync_responseOptions pub_opts = MQTTAsync_responseOptions_initializer;

	pubmsg.payload = SEND_MSG;
	pubmsg.payloadlen = strlen(SEND_MSG);
	pubmsg.qos = QOS;
	pubmsg.retained = 0;
	pub_opts.onSuccess = on_send;
	pub_opts.onFailure = on_send_failure;
	pub_opts.context = (void *)s_seq;

	if (client && MQTTAsync_isConnected(client)) {
		printf("[PAHO] seq=%u do_mqtt_send() ...\n", s_seq);
		if ((rc = MQTTAsync_sendMessage(client, SEND_TOPIC, &pubmsg, &pub_opts)) != MQTTASYNC_SUCCESS) {
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
	int rc;
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
	if ((rc = MQTTAsync_subscribe(client, TOPIC, QOS, &opts)) != MQTTASYNC_SUCCESS) {
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

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	int rc;
	MQTTAsync client;

	do {
		rc = do_mqtt_init(&client);
	} while (rc != MQTTASYNC_SUCCESS && (sleep(1) || 1));
	do_mqtt_connect(client, 0);

	while (1) {
		printf("[PAHO] running @ %lu...\n", time(NULL));
		do_mqtt_send(client);
		sleep(RUN_SLEEP_TIME);
	}

	return 0;
}
