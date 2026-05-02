#ifndef _CJSON_PROCESS_H_
#define _CJSON_PROCESS_H_
#include "cJSON.h"
#include <stdint.h>


#define   NAME              "name"     
#define   POWER_STM32       "power"  
#define   TEMP_STM32        "temp" 

#define   DEFAULT_NAME        "fire"     
#define   DEFAULT_POWER       25.0 
#define   DEFAULT_TEMP        50.0 


#define   UPDATE_SUCCESS       1 
#define   UPDATE_FAIL          0

cJSON* cJSON_Data_Init(void);
uint8_t cJSON_Update(const cJSON * const object,const char * const string,void * d);
void Proscess(void* data);
#endif

