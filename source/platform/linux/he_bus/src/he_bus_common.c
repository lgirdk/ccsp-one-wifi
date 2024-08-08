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
#include "he_bus_common.h"
#include "he_bus_core.h"
#include "he_bus_utils.h"

he_bus_mgr_t bus_obj;

he_bus_mgr_t *get_bus_mgr_object(void)
{
    return &bus_obj;
}

he_bus_conn_info_t *get_bus_connection_object(he_bus_handle_t handle)
{
    if (handle != NULL) {
        return &handle->conn_info;
    }
    return NULL;
}

he_bus_server_info_t *get_bus_server_info(he_bus_handle_t handle)
{
    if (handle != NULL) {
        return &handle->conn_info.server_info;
    }
    return NULL;
}

server_listener_info_t *get_bus_broadcast_server_info(he_bus_handle_t handle)
{
    if (handle != NULL) {
        return &handle->conn_info.server_info.broadcast;
    }
    return NULL;
}

he_bus_connection_info_t *get_bus_broadcast_client_info(he_bus_handle_t handle)
{
    if (handle != NULL) {
        return &handle->conn_info.client_info.conn_info;
    }
    return NULL;
}

bool is_digit_string(const char *str)
{
    if (str == NULL) {
        return 0;
    }
    while (*str) {
        if ((*str) < '0' || (*str) > '9') {
            return false;
        }
        str++;
    }
    return true;
}

void *get_bus_user_cb(hash_map_t *user_cb_map, char *name)
{
    void *table_cb;
    char *token = NULL;
    char *saveptr = NULL;
    he_bus_name_string_t key = { 0 };
    he_bus_name_string_t full_name = { 0 };
    he_bus_name_string_t original_name = { 0 };
    uint32_t total_str_len = 0;
    he_bus_name_string_t recv_name = { 0 };

    if (user_cb_map == NULL) {
        he_bus_core_error_print("%s:%d: user cb map not found:%s\n", __func__, __LINE__, name);
        return NULL;
    }

    if ((table_cb = hash_map_get(user_cb_map, name)) != NULL) {
        return table_cb;
    }
    strcpy(recv_name, name);
    strcpy(original_name, name);
    token = strtok_r(recv_name, ".", &saveptr);
    while (token != NULL) {
        if (strlen(full_name) == 0) {
            strcpy(full_name, token);
            total_str_len = strlen(token);
        } else if (is_digit_string(token)) {
            strcat(full_name, ".{i}");
            total_str_len = total_str_len + strlen(token) + 1;
            strcpy(key, full_name);
            strcat(key, (original_name + total_str_len));
            if ((table_cb = hash_map_get(user_cb_map, key)) != NULL) {
                return table_cb;
            }
        } else {
            strcat(full_name, ".");
            strcat(full_name, token);
            total_str_len = total_str_len + strlen(token) + 1;
        }
        token = strtok_r(NULL, ".", &saveptr);
    }
    he_bus_core_error_print("%s Rbus callback not found=%s, %s\n", __func__, full_name,
        original_name);
    return NULL;
}
