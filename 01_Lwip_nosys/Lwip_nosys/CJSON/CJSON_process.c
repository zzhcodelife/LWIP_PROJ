#include "cJSON_Process.h"

/*******************************************************************
 *                          变量声明                               
 *******************************************************************/



cJSON* cJSON_Data_Init(void)
{
  cJSON *cJSON_Root = NULL;
  cJSON *params = NULL;
  cJSON *node_power = NULL;
  cJSON *node_temp = NULL;

  cJSON_Root = cJSON_CreateObject();
  if (cJSON_Root == NULL) {
    return NULL;
  }

  /* OneNET: { "id","version","params":{ "power":{"value":"..."}, "temp":{"value":n} } } */
  cJSON_AddStringToObject(cJSON_Root, JSON_MSG_ID, DEFAULT_MSG_ID);
  cJSON_AddStringToObject(cJSON_Root, JSON_MSG_VERSION, DEFAULT_MSG_VERSION);

  params = cJSON_AddObjectToObject(cJSON_Root, JSON_MSG_PARAMS);
  if (params == NULL) {
    cJSON_Delete(cJSON_Root);
    return NULL;
  }

  node_power = cJSON_AddObjectToObject(params, POWER_STM32);
  if (node_power == NULL) {
    cJSON_Delete(cJSON_Root);
    return NULL;
  }
  cJSON_AddStringToObject(node_power, JSON_PARAM_VALUE, DEFAULT_POWER_STR);

  node_temp = cJSON_AddObjectToObject(params, TEMP_STM32);
  if (node_temp == NULL) {
    cJSON_Delete(cJSON_Root);
    return NULL;
  }
  cJSON_AddNumberToObject(node_temp, JSON_PARAM_VALUE, DEFAULT_TEMP);

  return cJSON_Root;
}
uint8_t cJSON_Update(const cJSON * const object, const char * const string, void *d)
{
  cJSON *params;
  cJSON *param_item;
  cJSON *val;
  cJSON *new_str;

  params = cJSON_GetObjectItem((cJSON *)object, JSON_MSG_PARAMS);
  if (params == NULL) {
    return UPDATE_FAIL;
  }
  param_item = cJSON_GetObjectItem(params, string);
  if (param_item == NULL) {
    return UPDATE_FAIL;
  }
  val = cJSON_GetObjectItem(param_item, JSON_PARAM_VALUE);
  if (val == NULL) {
    return UPDATE_FAIL;
  }

  if (cJSON_IsString(val)) {
    new_str = cJSON_CreateString((char *)d);
    if (new_str == NULL) {
      return UPDATE_FAIL;
    }
    cJSON_ReplaceItemInObject(param_item, JSON_PARAM_VALUE, new_str);
    return UPDATE_SUCCESS;
  }
  if (cJSON_IsNumber(val)) {
    cJSON_SetNumberValue(val, *(double *)d);
    return UPDATE_SUCCESS;
  }

  return UPDATE_FAIL;
}

void Proscess(void *data)
{
  cJSON *root;
  cJSON *params;
  cJSON *json_power;
  cJSON *json_temp;
  cJSON *pv;
  cJSON *tv;

  root = cJSON_Parse((char *)data);
  if (root == NULL) {
    return;
  }

  /* 平台下行多为 id/version/params 或仅 data，优先按物模型解析 */
  params = cJSON_GetObjectItem(root, JSON_MSG_PARAMS);
  if (params != NULL) {
    json_power = cJSON_GetObjectItem(params, POWER_STM32);
    json_temp = cJSON_GetObjectItem(params, TEMP_STM32);
    if (json_power != NULL) {
      pv = cJSON_GetObjectItem(json_power, JSON_PARAM_VALUE);
      (void)pv;
    }
    if (json_temp != NULL) {
      tv = cJSON_GetObjectItem(json_temp, JSON_PARAM_VALUE);
      (void)tv;
    }
  }

  cJSON_Delete(root);
}








