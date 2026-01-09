#ifndef __CURL_H__
#define __CURL_H__



typedef struct{
	char WorkOrder_No[20];
	char Product_name[25];
	unsigned int quantity;
}ProductOder;

extern unsigned int  http_req_f;


extern ProductOder WorkOrder;

char *get_http(const char *param);
void *socket_main();

#endif

