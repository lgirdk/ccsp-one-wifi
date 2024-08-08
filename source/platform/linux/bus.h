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
#ifndef BUS_H
#define BUS_H

#include "bus_common.h"
#include "wifi_util.h"
#include "he_bus_core.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bus_handle {
    union {
        he_bus_handle_t he_bus_handle;
    } u;
    bool is_bus_init;
} bus_handle_t;

typedef struct {
    wifi_bus_desc_t  desc;
} wifi_bus_t;

wifi_bus_desc_t *get_bus_descriptor();
bus_error_t bus_init(bus_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif // BUS_H
