#ifndef __JSON_H__
#define __JSON_H__
#include "lib/cJSON.h"


cJSON *read_json_str(char *jstr);
cJSON *read_json_obj(cJSON *parent_json, char *name);
void close_json(cJSON *cjson);





#endif
