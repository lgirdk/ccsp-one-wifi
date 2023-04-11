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
#include <stdbool.h>
#include "stdlib.h"
#include <sys/time.h>
#include "wifi_hal.h"
#include "wifi_ctrl.h"
#include "wifi_mgr.h"
#include "wifi_util.h"
#include "wifi_apps_mgr.h"
#include <rbus.h>

wifi_app_descriptor_t app_desc[3] = {
    {
        wifi_app_inst_analytics, 0,
        wifi_event_type_exec | wifi_event_type_webconfig | wifi_event_type_hal_ind | wifi_event_type_command | wifi_event_type_monitor | wifi_event_type_net | wifi_event_type_wifiapi,
        true,
        "Analytics of Real Time Events",
        analytics_init, analytics_event, analytics_deinit
    },
    {
        wifi_app_inst_levl, 0,
        wifi_event_type_hal_ind,
        true,
        "Levl Finger Printing",
        levl_init, levl_event, levl_deinit
    },
    {
        wifi_app_inst_cac, 0,
        wifi_event_type_hal_ind | wifi_event_type_webconfig,
        true,
        "Connection Admission Control for VAPs",
        cac_init, cac_event, cac_deinit
    }
};
