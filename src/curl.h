#ifndef __CURL_H__
#define __CURL_H__



typedef struct{
	char WorkOrder_No[20];
	char Product_name[25];
	unsigned int quantity;
	double quantity_double;
	double mes_mix_weight;
	double actual_mix_weight;
	char assign_name[32];
	char operator_id[64];
	char unit[12];
	int material_count;
	struct {
		char name[32];
		double target;
		char unit[12];
	} materials[10];
}ProductOder;

extern unsigned int  http_req_f;


extern ProductOder WorkOrder;

char *get_http(const char *param);
void *socket_main(void *args);
int haas_data_read_tcp(void *ctx);

#endif
