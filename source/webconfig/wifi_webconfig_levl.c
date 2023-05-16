/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:

  Copyright 2018 RDK Management

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include "secure_wrapper.h"
#include "collection.h"
#include "msgpack.h"
#include "wifi_webconfig.h"
#include "wifi_monitor.h"
#include "wifi_util.h"
#include "wifi_ctrl.h"

webconfig_subdoc_object_t   levl_objects[3] = {
    { webconfig_subdoc_object_type_version, "Version" },
    { webconfig_subdoc_object_type_subdoc, "SubDocName" },
    { webconfig_subdoc_object_type_csi, "WifiLevl" },
};

webconfig_error_t init_levl_subdoc(webconfig_subdoc_t *doc)
{
    doc->num_objects = sizeof(levl_objects)/sizeof(webconfig_subdoc_object_t);
    memcpy((unsigned char *)doc->objects, (unsigned char *)&levl_objects, sizeof(levl_objects));

    return webconfig_error_none;
}

webconfig_error_t access_check_levl_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t translate_from_levl_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t translate_to_levl_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t encode_levl_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    cJSON *json;
    cJSON *obj_levl;
    char *str;
    webconfig_subdoc_decoded_data_t *params;

    if (data == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL data Pointer\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    params = &data->u.decoded;
    if (params == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL Pointer\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: json create object failed\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    data->u.encoded.json = json;

    cJSON_AddStringToObject(json, "Version", "1.0");
    cJSON_AddStringToObject(json, "SubDocName", "levl data");

    obj_levl = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "WifiLevl", obj_levl);

    if (encode_levl_object(&params->levl, obj_levl) != webconfig_error_none) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Failed to encode wifi blaster config\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    memset(data->u.encoded.raw, 0, MAX_SUBDOC_SIZE);
    str = cJSON_Print(json);
    if ((strlen(str)+1) > MAX_SUBDOC_SIZE) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: JSON blob is BIG\n", __func__, __LINE__);
        return webconfig_error_encode;
    }
    memcpy(data->u.encoded.raw, str, strlen(str)+1);
    wifi_util_info_print(WIFI_WEBCONFIG, "%s:%d: encode success %s\n", __func__, __LINE__, str);
    cJSON_free(str);
    cJSON_Delete(json);
    return webconfig_error_none;
}

webconfig_error_t decode_levl_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    webconfig_subdoc_decoded_data_t *params;
    cJSON *obj_config;
    cJSON *json;
    params = &data->u.decoded;
    if (params == NULL) {
        return webconfig_error_decode;
    }

    json = data->u.encoded.json;
    if (json == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL json pointer\n", __func__, __LINE__);
        return webconfig_error_decode;
    }

    memset(&params->levl, 0, sizeof(levl_config_t));

    obj_config = cJSON_GetObjectItem(json, "WifiLevl");
    if (obj_config == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Levl object not present\n", __func__, __LINE__);
        cJSON_Delete(json);
        wifi_util_error_print(WIFI_WEBCONFIG, "%s\n", (char *)data->u.encoded.raw);
        return webconfig_error_invalid_subdoc;
    }

    if (decode_levl_object(obj_config, &params->levl) != webconfig_error_none) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Levl object Validation Failed\n", __func__, __LINE__);
        cJSON_Delete(json);
        wifi_util_error_print(WIFI_WEBCONFIG, "%s\n", (char *)data->u.encoded.raw);
        return webconfig_error_decode;
    }

    cJSON_Delete(json);
    wifi_util_info_print(WIFI_WEBCONFIG, "%s:%d: decode success\n", __func__, __LINE__);
    return webconfig_error_none;
}

