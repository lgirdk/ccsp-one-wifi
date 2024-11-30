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

#ifndef WIFI_STUBS_H
#define WIFI_STUBS_H

typedef int (* wifi_t2_event_d_t)(char *marker, int value);
typedef int (* wifi_t2_event_s_t)(char *marker, char *buff);
typedef int (* wifi_v_secure_system_t)(const char *command);
typedef bool (* wifi_drop_root_t)();
typedef void (* wifi_gain_root_privilege_t)();
typedef char* (* wifi_getDeviceMac_t)();
typedef int (* wifi_onewifi_pktgen_uninit_t)();
typedef int (* wifi_strcpy_t) (char *dst, size_t max, const char *src);

typedef struct {
    wifi_t2_event_d_t t2_event_d_fn;
    wifi_t2_event_s_t t2_event_s_fn;
    wifi_v_secure_system_t v_secure_system_fn;
    wifi_drop_root_t drop_root_fn;
    wifi_gain_root_privilege_t gain_root_privilege_fn;
    wifi_getDeviceMac_t getDeviceMac_fn;
    wifi_onewifi_pktgen_uninit_t onewifi_pktgen_uninit_fn;
    wifi_strcpy_t strcpy_fn;
} wifi_stubs_descriptor_t;

wifi_stubs_descriptor_t *get_stubs_descriptor();

#endif // STUBS_H
