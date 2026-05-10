#ifndef _CJSON_PROCESS_H_
#define _CJSON_PROCESS_H_
#include "cJSON.h"
#include <stdint.h>


#define   JSON_MSG_ID          "id"
#define   JSON_MSG_VERSION     "version"
#define   JSON_MSG_PARAMS      "params"
#define   JSON_PARAM_VALUE     "value"

#define   POWER_STM32       "power"
#define   TEMP_STM32        "temp"

/* OneNET 物模型属性上报默认示例 */
#define   DEFAULT_MSG_ID      "123"
#define   DEFAULT_MSG_VERSION "1.0"
#define   DEFAULT_POWER_STR   "789"
#define   DEFAULT_TEMP        27.4


#define   UPDATE_SUCCESS       1 
#define   UPDATE_FAIL          0

cJSON* cJSON_Data_Init(void);
uint8_t cJSON_Update(const cJSON * const object,const char * const string,void * d);
void Proscess(void* data);
#endif

