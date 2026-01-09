#define JSON_BUF_SIZE  (0x1000)

#include "lib/cJSON.h"
#include <stdio.h>
#include "json.h"

int test_cjson(cJSON *cjson)
{
	if (cjson == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			printf("Test json: error before: %s\n", error_ptr);
		}
		else
		{
			printf("Test json: unknow error.\n");
		}
		return 0;
	}
	else
	{
		return 1;
	}
}

cJSON *read_json_str(char *jstr)
{
	cJSON *cjson = cJSON_Parse(jstr);
	test_cjson(cjson);
	return cjson;
}

cJSON *read_json_file(char *jfile)
{
	char read_buf[JSON_BUF_SIZE] = {0};
	FILE *fp = fopen(jfile, "r");
        if (!fp)
	{
		printf("read_json_file: open file %s error!\n", jfile);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	long int fsize = ftell(fp);

	if (fsize >= JSON_BUF_SIZE)
	{
		printf("file size (%ld) too big to buffer (%u)!\n", fsize, JSON_BUF_SIZE);
		return NULL;
	}

	rewind(fp);
	fread(read_buf, sizeof(char), fsize, fp);
	read_buf[fsize] = '\0';
	fclose(fp);

	// dbg_printf("========== JSON String: ==========\n%s\n=============== END ===============\n\n", read_buf);

	return read_json_str(read_buf);
}

cJSON *read_json_obj(cJSON *parent_json, char *name)
{
	cJSON *cjson = cJSON_GetObjectItemCaseSensitive(parent_json, name);
	test_cjson(cjson);
	return cjson;
}

char *read_json_string(cJSON *cjson)
{
	return cJSON_GetStringValue(cjson);
}

void close_json(cJSON *cjson)
{
	cJSON_Delete(cjson);
}
