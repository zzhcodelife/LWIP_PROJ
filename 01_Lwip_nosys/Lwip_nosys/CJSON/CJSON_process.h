#include <stdint.h>
#include "cJSON.h"
#ifndef __CJSON_PROCESS_H
#define __CJSON_PROCESS_H
cJSON *cJSON_Data_Init(void);
uint8_t cJSON_Update(const cJSON *const object, const char *const string,void *d);
void Proscess(void *data);
#endif // __CJSON_PROCESS_H