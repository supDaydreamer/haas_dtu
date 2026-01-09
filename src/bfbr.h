#ifndef __BFBR_H__
#define __BFBR_H__

typedef struct mg_mgr* BFBR;
typedef void (*ON_MSG_FUNC)(char *br_msg, uint8_t *data, size_t len);

void bfbr_init(uint16_t port, int milli);
BFBR bfbr_new(char *br_name);
void bfbr_receive(BFBR bfbr, char *br_name);
void bfbr_send(BFBR bfbr, char *br_name, char *br_msg, uint8_t *data, size_t len);

#endif
