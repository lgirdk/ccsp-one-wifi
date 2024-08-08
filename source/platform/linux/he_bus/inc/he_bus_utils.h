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
#ifndef HE_BUS_UTILS_H
#define HE_BUS_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { he_bus_core, he_bus_memory, he_bus_connector, he_bus_dml } he_bus_log_type_t;

typedef enum {
    he_bus_log_lvl_debug,
    he_bus_log_lvl_info,
    he_bus_log_lvl_error,
    he_bus_log_lvl_max
} he_bus_log_level_t;

#ifndef HE_BUS_LOG_PATH_PREFIX
#define HE_BUS_LOG_PATH_PREFIX "/nvram/"
#endif // HE_BUS_LOG_PATH_PREFIX

/* enable PID in debug logs */
#define __ENABLE_PID_INFO__ 0

void he_bus_print(he_bus_log_type_t level, he_bus_log_type_t module, char *format, ...);

// ANSI color codes
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#if 1
#define he_bus_dbg_print(module, format, ...) \
    he_bus_print(he_bus_log_lvl_debug, module, format, ##__VA_ARGS__)
#define he_bus_info_print(module, format, ...) \
    he_bus_print(he_bus_log_lvl_info, module, format, ##__VA_ARGS__)
#define he_bus_error_print(module, format, ...) \
    he_bus_print(he_bus_log_lvl_error, module, format, ##__VA_ARGS__)

#define he_bus_core_dbg_print(format, ...) he_bus_dbg_print(he_bus_core, format, ##__VA_ARGS__)
#define he_bus_core_info_print(format, ...) he_bus_info_print(he_bus_core, format, ##__VA_ARGS__)
#define he_bus_core_error_print(format, ...) he_bus_error_print(he_bus_core, format, ##__VA_ARGS__)

#define he_bus_memory_dbg_print(format, ...) he_bus_dbg_print(he_bus_memory, format, ##__VA_ARGS__)
#define he_bus_memory_info_print(format, ...) \
    he_bus_info_print(he_bus_memory, format, ##__VA_ARGS__)
#define he_bus_memory_error_print(format, ...) \
    he_bus_error_print(he_bus_memory, format, ##__VA_ARGS__)

#define he_bus_conn_dbg_print(format, ...) he_bus_dbg_print(he_bus_connector, format, ##__VA_ARGS__)
#define he_bus_conn_info_print(format, ...) \
    he_bus_info_print(he_bus_connector, format, ##__VA_ARGS__)
#define he_bus_conn_error_print(format, ...) \
    he_bus_error_print(he_bus_connector, format, ##__VA_ARGS__)

#define he_bus_dml_dbg_print(format, ...) he_bus_dbg_print(he_bus_dml, format, ##__VA_ARGS__)
#define he_bus_dml_info_print(format, ...) he_bus_info_print(he_bus_dml, format, ##__VA_ARGS__)
#define he_bus_dml_error_print(format, ...) he_bus_error_print(he_bus_dml, format, ##__VA_ARGS__)
#else
// linux
#define he_bus_dbg_print printf
#define he_bus_info_print printf
#define he_bus_error_print printf

#define he_bus_core_dbg_print printf
#define he_bus_core_info_print printf
#define he_bus_core_error_print printf

#define he_bus_memory_dbg_print printf
#define he_bus_memory_info_print printf
#define he_bus_memory_error_print printf

#define he_bus_conn_dbg_print printf
#define he_bus_conn_info_print printf
#define he_bus_conn_error_print printf

#define he_bus_dml_dbg_print printf
#define he_bus_dml_info_print printf
#define he_bus_dml_error_print printf
#endif

#ifdef __cplusplus
}
#endif
#endif // HE_BUS_UTILS_H
