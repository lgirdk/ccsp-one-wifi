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
#ifndef BUS_CONNECTION_H
#define BUS_CONNECTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "collection.h"
#include "he_bus_common.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define HE_BUS_MSG_ADDR_MAX 128
#define MSG_BUFF_SIZE 4096
#define SOCKET_INVALID_FD -1

#define HE_BUS_ERROR_STREAM_CLOSED -1001
#define HE_BUS_ERROR_MSG_VARIFICATION -1002

#define SOCKET_BROADCAST_SERVER_NAME "/tmp/bus_broadcast_routed"
#define SOCKET_UNICAST_SERVER_NAME "/tmp/bus_unicast_routed"

typedef struct {
    int fd;
    struct sockaddr_un local_endpoint;
} he_bus_listener;

typedef struct {
    int fd;
    struct sockaddr_un endpoint;
    char identity[HE_BUS_MSG_ADDR_MAX];
    he_bus_comp_name_str_t component_name;
} he_bus_connection_info_t;

typedef struct server_listener_info {
    he_bus_listener listener_info;
    hash_map_t *connected_client_info_map;
} server_listener_info_t;

typedef struct he_bus_server_info {
    bool is_running;
    server_listener_info_t broadcast;
    server_listener_info_t unicast;
} he_bus_server_info_t;

typedef struct he_bus_client_info {
    bool is_running;
    he_bus_connection_info_t conn_info;
} he_bus_client_info_t;

typedef struct he_bus_conn_info {
    he_bus_server_info_t server_info;
    he_bus_client_info_t client_info;
} he_bus_conn_info_t;

int send_data_to_endpoint(int fd, void *data, uint32_t data_len);
int ipc_unix_client_send_data(he_bus_handle_t handle, unsigned char *data, unsigned int len);
void *ipc_unix_broadcast_server_start(void *arg);
void *ipc_unix_unicast_server_start(void *arg);
void *ipc_unix_broadcast_client_start(void *arg);

int get_client_broadcast_fd(he_bus_handle_t handle, char *comp_name, int *fd);

he_bus_conn_info_t *get_bus_connection_object(he_bus_handle_t handle);
he_bus_server_info_t *get_bus_server_info(he_bus_handle_t handle);
server_listener_info_t *get_bus_broadcast_server_info(he_bus_handle_t handle);
he_bus_connection_info_t *get_bus_broadcast_client_info(he_bus_handle_t handle);

int ipc_unix_send_data_and_wait_for_res(he_bus_stretch_buff_t *send_data,
    he_bus_stretch_buff_t *p_res_data, uint32_t recv_timeout);

#ifdef __cplusplus
}
#endif
#endif // BUS_CONNECTION_H
