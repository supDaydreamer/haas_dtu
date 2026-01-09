#define UDP_POLL_MS				(300)
#define UDP_LISION_URL_1		("udp://0.0.0.0:27001")
#define UDP_LISION_URL_2		("udp://0.0.0.0:27002")

#include "mongoose.h"
#include "uart.h"

static void listen_event_handler_1(struct mg_connection *c, int ev, void *ev_data)
{
	struct mbuf *io = &c->recv_mbuf;
	switch (ev) {
		case MG_EV_RECV:
			uart_tx(1, io->buf, io->len);
			mbuf_remove(io, io->len);
			c->flags |= MG_F_SEND_AND_CLOSE;
			break;
		default:
			break;
	}
}

static void listen_event_handler_2(struct mg_connection *c, int ev, void *ev_data)
{
	struct mbuf *io = &c->recv_mbuf;
	switch (ev) {
		case MG_EV_RECV:
			uart_tx(2, io->buf, io->len);
			mbuf_remove(io, io->len);
			c->flags |= MG_F_SEND_AND_CLOSE;
			break;
		default:
			break;
	}
}

int udp_uart_main_1()
{
	struct mg_mgr mgr;
	mg_mgr_init(&mgr, NULL);
	mg_bind(&mgr, UDP_LISION_URL_1, listen_event_handler_1);

	while (1) mg_mgr_poll(&mgr, UDP_POLL_MS);

	mg_mgr_free(&mgr);
	return 0;
}

int udp_uart_main_2()
{
	struct mg_mgr mgr;
	mg_mgr_init(&mgr, NULL);
	mg_bind(&mgr, UDP_LISION_URL_2, listen_event_handler_2);

	while (1) mg_mgr_poll(&mgr, UDP_POLL_MS);

	mg_mgr_free(&mgr);
	return 0;
}

