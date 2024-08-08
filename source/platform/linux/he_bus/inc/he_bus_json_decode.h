/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:

  Copyright 2024 RDK Management

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 **************************************************************************/
#ifndef HE_BUS_JSON_DECODE_H
#define HE_BUS_JSON_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "he_bus_common.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIFI_OBJ_DEFINITIONS_NAME "definitions"
#define RADIO_OBJ_NAME "Radio"
#define ACCESSPOINT_OBJ_NAME "AccessPoint"

#define LIST_OF_DEFINITION_NAME "List_Of_Def"
#define MAX_NUM_OF_OBJECTS_NAME "Num_Of_Objects"

#define WIFI_OBJ_TREE_NAME "Device.WiFi"
#define RADIO_OBJ_TREE_NAME "Device.WiFi.Radio.{i}"
#define ACCESSPOINT_OBJ_TREE_NAME "Device.WiFi.AccessPoint.{i}"
#define SSID_OBJ_TREE_NAME "Device.WiFi.SSID.{i}"

#define decode_json_param_object(json, key, value)                                               \
    {                                                                                            \
        value = cJSON_GetObjectItem(json, key);                                                  \
        if ((value == NULL) || (cJSON_IsObject(value) == false)) {                               \
            he_bus_core_error_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, \
                key);                                                                            \
            return HE_BUS_RETURN_ERR;                                                            \
        }                                                                                        \
    }

#define decode_json_param_string(json, key, value)                                                 \
    {                                                                                              \
        value = cJSON_GetObjectItem(json, key);                                                    \
        if ((value == NULL) || (cJSON_IsString(value) == false) || (value->valuestring == NULL) || \
            (strcmp(value->valuestring, "") == 0)) {                                               \
            he_bus_core_error_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__,   \
                key);                                                                              \
            return HE_BUS_RETURN_ERR;                                                              \
        }                                                                                          \
    }

#define decode_json_param_integer(json, key, value)                                              \
    {                                                                                            \
        value = cJSON_GetObjectItem(json, key);                                                  \
        if ((value == NULL) || (cJSON_IsNumber(value) == false)) {                               \
            he_bus_core_error_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, \
                key);                                                                            \
            return HE_BUS_RETURN_ERR;                                                            \
        }                                                                                        \
    }

#define validate_current_json_obj_param_name(json)                                              \
    {                                                                                           \
        if (json == NULL || json->string == NULL) {                                             \
            he_bus_core_error_print("%s:%d: current json obj param name not found\n", __func__, \
                __LINE__);                                                                      \
            return HE_BUS_RETURN_ERR;                                                           \
        }                                                                                       \
    }

#define decode_json_param_bool(json, key, value)                                                 \
    {                                                                                            \
        value = cJSON_GetObjectItem(json, key);                                                  \
        if ((value == NULL) || (cJSON_IsBool(value) == false)) {                                 \
            he_bus_core_error_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, \
                key);                                                                            \
            return HE_BUS_RETURN_ERR;                                                            \
        }                                                                                        \
    }

#define get_func_address dlsym

typedef enum he_bus_cb_func_list {
    wifi_cb_func,
    radio_cb_func,
    accesspoint_cb_func,
    ssid_cb_func,
    default_cb_func,
    max_cb_func_list
} he_bus_cb_func_list_t;

int decode_json_object(he_bus_handle_t handle, const char *json_name);

#ifdef __cplusplus
}
#endif
#endif // HE_BUS_JSON_DECODE_H
