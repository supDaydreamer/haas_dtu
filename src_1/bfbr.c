#define MQTT_LISTEN_ADDR_PREFIX		("0.0.0.0:")
#define TOPIC_BUF_SIZE				(256)
#define DEFAULT_PORT				(10000)
#define DEFAULT_POLL_TIME_MS		(1000)

#include "mongoose.h"
#include "bfbr.h"

typedef struct __bfbr_link {
	char *name;
	struct mg_mgr *mgr;
	struct __bfbr_link *next;
	pthread_t *thread;
} BFBR_LINK;

static uint16_t s_msg_id = 0;
static BFBR_LINK *s_bfbr_root = NULL;
static uint16_t s_port = DEFAULT_PORT;
static int s_milli = DEFAULT_POLL_TIME_MS;
static char s_listening_address[8 + 5 + 1] = {0};	// PREFIX + port + '\0'
static pthread_t s_thread_bfbr = {0};
struct mg_mgr s_mgr = {0};
struct mg_mqtt_broker s_brk = {0};
struct mg_connection *s_c = NULL;

#ifdef BFBR_MAIN_TEST
static void print_event(int ev)
{
	const char *print_prefix = "USER HANDLER GOT EVENT";
	char *ev_str = NULL;
	switch (ev) {
#define CASE_EV(ev) case (ev): ev_str = #ev; break
		CASE_EV(MG_EV_MQTT_CONNECT);
		CASE_EV(MG_EV_MQTT_CONNACK);
		CASE_EV(MG_EV_MQTT_PUBLISH);
		CASE_EV(MG_EV_MQTT_PUBACK);
		CASE_EV(MG_EV_MQTT_PUBREC);
		CASE_EV(MG_EV_MQTT_PUBREL);
		CASE_EV(MG_EV_MQTT_PUBCOMP);
		CASE_EV(MG_EV_MQTT_SUBSCRIBE);
		CASE_EV(MG_EV_MQTT_SUBACK);
		CASE_EV(MG_EV_MQTT_UNSUBSCRIBE);
		CASE_EV(MG_EV_MQTT_UNSUBACK);
		CASE_EV(MG_EV_MQTT_PINGREQ);
		CASE_EV(MG_EV_MQTT_PINGRESP);
		CASE_EV(MG_EV_MQTT_DISCONNECT);
		CASE_EV(MG_EV_MQTT_CONNACK_ACCEPTED);
		CASE_EV(MG_EV_MQTT_CONNACK_UNACCEPTABLE_VERSION);
		CASE_EV(MG_EV_MQTT_CONNACK_IDENTIFIER_REJECTED);
		CASE_EV(MG_EV_MQTT_CONNACK_SERVER_UNAVAILABLE);
		CASE_EV(MG_EV_MQTT_CONNACK_BAD_AUTH);
		CASE_EV(MG_EV_MQTT_CONNACK_NOT_AUTHORIZED);
	default:
		ev_str = "__UNKNOW_EVENT__";
		break;
	}
	printf("%s { %s }\n", print_prefix, ev_str);
}
#endif

static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
#ifdef BFBR_MAIN_TEST
	if (ev != MG_EV_POLL) print_event(ev);
#endif
	/* Do your custom event processing here */
	mg_mqtt_broker(c, ev, ev_data);
}

static void *bfbr_task(void* arg)
{
	mg_mgr_init(&s_mgr, NULL);

	snprintf(s_listening_address, sizeof(s_listening_address), "%s%u", MQTT_LISTEN_ADDR_PREFIX, s_port);

	if ((s_c = mg_bind(&s_mgr, s_listening_address, ev_handler)) == NULL) {
		fprintf(stderr, "[%s] mg_bind(%s) failed\n", __FUNCTION__, s_listening_address);
		exit(EXIT_FAILURE);
	}
	mg_mqtt_broker_init(&s_brk, NULL);
	s_c->priv_2 = &s_brk;
	mg_set_protocol_mqtt(s_c);
#ifdef BFBR_MAIN_TEST
	printf("[%s] MQTT broker started on %s with poll %d ms\n", __FUNCTION__, s_listening_address, s_milli);
#endif
	while (1) {
		mg_mgr_poll(&s_mgr, s_milli);
	}
	pthread_exit(NULL);
}

void bfbr_init(uint16_t port, int milli)
{
	s_port = port;
	s_milli = milli;
	pthread_create(&s_thread_bfbr, NULL, bfbr_task, NULL);
	pthread_detach(s_thread_bfbr);
}

#ifdef BFBR_MAIN_TEST
int main(int argc, char *argv[])
{
	if (argc < 3) bfbr_init(s_port, s_milli);
	bfbr_init(atoi(argv[1]), atoi(argv[2]));
	return 0;
}
#endif

static BFBR_LINK *each_bfbr_link(BFBR_LINK *bfbr_link)
{
	if (bfbr_link == NULL) bfbr_link = s_bfbr_root;
	return bfbr_link->next;
}

static void bfbr_handler(struct mg_connection *nc, int ev, void *p)
{
	struct mg_mqtt_message *msg = (struct mg_mqtt_message *)p;
	switch (ev) {
	case MG_EV_CONNECT: {
			struct mg_send_mqtt_handshake_opts opts;
			memset(&opts, 0, sizeof(opts));
			//opts.user_name = "username";
			//opts.password = "password";
			mg_set_protocol_mqtt(nc);
			mg_send_mqtt_handshake_opt(nc, "dummy", opts);
		}
		break;
	case MG_EV_MQTT_CONNACK:
		if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
			fprintf(stderr, "Got mqtt connection error: %d\n", msg->connack_ret_code);
		} else {
			printf("Got mqtt connection ok: %d\n", msg->connack_ret_code);
			struct mg_mgr *mgr = nc->mgr;
			BFBR_LINK *bfbr_link = NULL;
			for (bfbr_link = each_bfbr_link(NULL); bfbr_link != NULL; bfbr_link = each_bfbr_link(bfbr_link)) {
				if (bfbr_link->mgr == mgr) {
					char topic_buf[TOPIC_BUF_SIZE] = {0};
					snprintf(topic_buf, sizeof(topic_buf), "/%s/#", bfbr_link->name);
					struct mg_mqtt_topic_expression topic_expr = {
						.topic = topic_buf,
						.qos = 2
					};
					mg_mqtt_subscribe(nc, &topic_expr, 1, s_msg_id++);
					break;
				}
			}
		}
		break;
	case MG_EV_MQTT_PUBACK:
		printf("Message publishing acknowledged. (msg_id: %d)\n", msg->message_id);
		break;
	case MG_EV_MQTT_SUBACK:
		printf("Subscription acknowledged.\n");
		break;
	case MG_EV_MQTT_PUBLISH:
		printf("\n------ MSG Arrival {%.*s}, payload length: %d ------\n", msg->topic.len, msg->topic.p, msg->payload.len);
		printf("%.*s\n", msg->payload.len, msg->payload.p);
		printf("------ === END === ------\n\n");
		break;
	case MG_EV_CLOSE:
		fprintf(stderr, "error: Connection closed.\n");
		break;
	default:
		break;
	}
}

BFBR_LINK *bfbr_link_add(void)
{
	BFBR_LINK *p_bfbr_leaf = s_bfbr_root;
	while (p_bfbr_leaf != NULL) {
		p_bfbr_leaf = p_bfbr_leaf->next;
	}
	p_bfbr_leaf = (BFBR_LINK *)malloc(sizeof(BFBR_LINK));
	if (p_bfbr_leaf) {
		p_bfbr_leaf->name = NULL;
		p_bfbr_leaf->mgr = NULL;
		p_bfbr_leaf->next = NULL;
	}
	return p_bfbr_leaf;
}

static void *bfbr_client_task(void* arg)
{
	struct mg_mgr *mgr = (struct mg_mgr *)arg;
	while (1) {
		mg_mgr_poll(mgr, s_milli);
	}
	pthread_exit(NULL);
}

BFBR bfbr_new(char *br_name)
{
	if (s_listening_address[0] == '\0') return;

	struct mg_mgr *mgr = (struct mg_mgr *)malloc(sizeof(struct mg_mgr));
	BFBR_LINK *bfbr_link = bfbr_link_add();
	pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));

	if (mgr) {
		if (bfbr_link) {
			bfbr_link->name = br_name;
			bfbr_link->mgr = mgr;
			bfbr_link->thread = thread;
		}
		mg_mgr_init(&mgr, NULL);
		if (mg_connect(&mgr, s_listening_address, bfbr_handler) == NULL) {
			fprintf(stderr, "bfbr_new (%s) failed!\n", br_name);
		}

		pthread_create(thread, NULL, bfbr_client_task, mgr);
		pthread_detach(*thread);
	}
	return mgr;
}

void bfbr_receive(BFBR bfbr, char *br_name)
{
	struct mg_mgr *mgr = bfbr;
}

void bfbr_send(BFBR bfbr, char *br_name, char *br_msg, uint8_t *data, size_t len)
{
	struct mg_connection *nc = NULL;
	struct mg_mgr *mgr = bfbr;
	char topic_buf[TOPIC_BUF_SIZE] = {0};
	snprintf(topic_buf, sizeof(topic_buf), "/%s/%s", br_name, br_msg);
	for (nc = mg_next(mgr, NULL); nc != NULL; nc = mg_next(mgr, nc)) {
		mg_mqtt_publish(nc, topic_buf, s_msg_id++, MG_MQTT_QOS(2), data, len);
	}
}

