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
#include <fcntl.h>
#include <errno.h>
#include "wifi_monitor.h"
#include "wifi_ctrl.h"
#include "wifi_util.h"

#define RADIO_SCAN_RESULT_INTERVAL 200 //200 ms
#define RADIO_SCAN_MAX_RESULTS_RETRIES 150 //30 seconds

int validate_radio_scan_args(wifi_mon_stats_args_t *args)
{
    if (args == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n",__func__,__LINE__, args);
        return RETURN_ERR;
    }

    if (args->radio_index > getNumberRadios()) {
        wifi_util_error_print(WIFI_MON, "%s:%d invalid radio index : %d\n",__func__,__LINE__, args->radio_index);
        return RETURN_ERR;
    }

    /*
    if (args->channel_list.num_channels == 0) {
        wifi_util_error_print(WIFI_MON, "%s:%d invalid radio number of channels : 0\n",__func__,__LINE__);
        return RETURN_ERR;
    }*/
    //add validation of channels
    return RETURN_OK;
}

int generate_radio_scan_clctr_stats_key(wifi_mon_stats_args_t *args, char *key_str, size_t key_len)
{
    if ((args == NULL) || (key_str == NULL)) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p key = %p\n",__func__,__LINE__, args, key_str);
        return RETURN_ERR;
    }

    memset(key_str, 0, key_len);

    snprintf(key_str, key_len, "%02d-%02d-%02d", mon_stats_type_radio_scan, args->radio_index, args->scan_mode);

    wifi_util_dbg_print(WIFI_MON, "%s:%d collector stats key: %s\n", __func__,__LINE__, key_str);
    return RETURN_OK;
}


int generate_radio_scan_provider_stats_key(wifi_mon_stats_config_t *config, char *key_str, size_t key_len)
{
    if ((config == NULL) || (key_str == NULL)) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL config : %p key = %p\n",__func__,__LINE__, config, key_str);
        return RETURN_ERR;
    }

    memset(key_str, 0, key_len);

    snprintf(key_str, key_len, "%04d-%02d-%02d-%02d", config->inst, mon_stats_type_radio_scan, config->args.radio_index, config->args.scan_mode);

    wifi_util_dbg_print(WIFI_MON, "%s:%d: provider stats key: %s\n", __func__,__LINE__, key_str);

    return RETURN_OK;
}

int check_scan_complete(void *arg)
{
    int ret = RETURN_OK;
    wifi_neighbor_ap2_t *neigh_stats = NULL;
    wifi_monitor_t *mon_data = (wifi_monitor_t *)get_wifi_monitor();
    wifi_mon_stats_args_t *args = arg;
#if CCSP_WIFI_HAL
    unsigned int ap_count = 0;
    ret = wifi_getNeighboringWiFiStatus(args->radio_index, &neigh_stats, &ap_count);
#endif
    if (ret != RETURN_OK) {
        if (errno == EAGAIN && mon_data->scan_results_retries[args->radio_index] < RADIO_SCAN_MAX_RESULTS_RETRIES) {
            mon_data->scan_results_retries[args->radio_index]++;
            scheduler_add_timer_task(mon_data->sched, FALSE, NULL, check_scan_complete, args,
                RADIO_SCAN_RESULT_INTERVAL, 1, FALSE);
            
            wifi_util_dbg_print(WIFI_MON, "%s : %d  Neighbor wifi status for index %d not ready. Retry (%d)\n",__func__,__LINE__, args->radio_index, mon_data->scan_results_retries[args->radio_index]);
            return RETURN_OK;
        }
        wifi_util_error_print(WIFI_MON, "%s : %d  Failed to get Neighbor wifi status for scan mode %d radio index %d\n",__func__,__LINE__, args->scan_mode, args->radio_index);
        mon_data->scan_status[args->radio_index] = 0;
        return RETURN_ERR;
    }
    wifi_util_dbg_print(WIFI_MON, "%s : %d  Scan complete scan mode %d radio index %d\n",__func__,__LINE__, args->scan_mode, args->radio_index);
    mon_data->scan_status[args->radio_index] = 0;
    if (neigh_stats != NULL) {
        free(neigh_stats);
    }
    return RETURN_OK;
}


int execute_radio_scan_stats_api(wifi_mon_stats_args_t *args, wifi_monitor_t *mon_data, unsigned long task_interval_ms)
{
    int ret = RETURN_OK;
    wifi_radio_capabilities_t *wifi_cap = NULL;
    int   num_channels = 0;
    int channels[64] = {0};
    wifi_radio_operationParam_t* radioOperation = NULL;
    int dwell_time;
    char *channel_buff;
    int bytes_written = 0;
    int count = 0;


    if (args == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n",__func__,__LINE__, args);
        return RETURN_ERR;
    }

    if (mon_data->radio_presence[args->radio_index] == false) {
        wifi_util_info_print(WIFI_MON, "%s:%d radio_presence is false for radio : %d\n",__func__,__LINE__, args->radio_index);
        return RETURN_OK;
    }

    radioOperation = getRadioOperationParam(args->radio_index);
    if (radioOperation == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL radioOperation pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
        return RETURN_ERR;
    }

    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
        num_channels = 1;
        channels[0] = radioOperation->channel;
    } else if (args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL) {

        wifi_cap = getRadioCapability(args->radio_index);

        if (get_allowed_channels(radioOperation->band, wifi_cap, channels, &num_channels, radioOperation->DfsEnabled) != RETURN_OK) {
            wifi_util_error_print(WIFI_MON, "%s:%d get allowed channels failed for the radio : %d\n",__func__,__LINE__, args->radio_index);
            return RETURN_ERR;
        }
    } else {
        int i;
        if (args->channel_list.num_channels == 0) {
            return RETURN_ERR;
        }

        channels[0] = args->channel_list.channels_list[0];
        for(i=0;i<args->channel_list.num_channels;i++)
        {
            if (mon_data->last_scanned_channel[args->radio_index] == args->channel_list.channels_list[i]) {
                if ((i+1) >= args->channel_list.num_channels) {
                    channels[0] = args->channel_list.channels_list[0];
                } else {
                    channels[0] = args->channel_list.channels_list[i+1];
                }
                //skip current channel
                if ((unsigned int)channels[0] == radioOperation->channel) {
                    if ((i+2) >= args->channel_list.num_channels) {
                        channels[0] = args->channel_list.channels_list[0];
                    } else {
                        channels[0] = args->channel_list.channels_list[i+2];
                    }
                }
            }
        }
        num_channels = 1;
        mon_data->last_scanned_channel[args->radio_index] = channels[0];
    }

    if (num_channels == 0) {
        wifi_util_error_print(WIFI_MON, "%s:%d invalid number of channels\n",__func__,__LINE__);
        return RETURN_ERR;
    }

    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL) {
        dwell_time = args->dwell_time;
        if (radioOperation->band == WIFI_FREQUENCY_6_BAND) {
            if (args->dwell_time < 110) {
                dwell_time = 110;
            }
        }
    } else {
        dwell_time = args->dwell_time;
        if (dwell_time ==  0) {
            dwell_time = 10;
        }
    }

    channel_buff = (char *) malloc(sizeof(char)*num_channels*5);
    if (channel_buff != NULL) {
        for (count = 0; count < num_channels; count++) {
            bytes_written +=  snprintf(&channel_buff[bytes_written], (sizeof(channel_buff)-bytes_written), "%d,", channels[count]);
        }
        channel_buff[bytes_written-1] = '\0';
    }
    wifi_util_dbg_print(WIFI_MON, "%s:%d Start scan. Radio_index : %d scan_mode : %d dwell_time : %d num_channels : %d  channels : %s\n",__func__,__LINE__, args->radio_index,
                                            args->scan_mode, dwell_time, num_channels, (channel_buff!=NULL ? channel_buff : "NULL"));

    if (channel_buff != NULL) {
        free(channel_buff);
    }
    mon_data->scan_status[args->radio_index] = 1;
    mon_data->scan_results_retries[args->radio_index] = 0;
#if CCSP_WIFI_HAL
    int private_vap_index = getPrivateApFromRadioIndex(args->radio_index);
    ret = wifi_startNeighborScan(private_vap_index, args->scan_mode, dwell_time, num_channels, (unsigned int *)channels);
#endif
    if (ret != RETURN_OK) {
        wifi_util_error_print(WIFI_MON, "%s : %d  Failed to trigger scan for radio index %d\n",__func__,__LINE__, args->radio_index);
        return RETURN_ERR;
    }
    scheduler_add_timer_task(mon_data->sched, FALSE, NULL, check_scan_complete, args,
            RADIO_SCAN_RESULT_INTERVAL, 1, FALSE);

    return RETURN_OK;
}

int update_radio_scan_collector_args(void *ce)
{
    wifi_radio_operationParam_t* radioOperation = NULL;
    wifi_radio_capabilities_t *wifi_cap = NULL;
    int   num_channels = 0;
    int channels[64] = {0};
    unsigned int is_used[64] = {0};
    int i, j;
    wifi_mon_provider_element_t *provider_elem = NULL;
    wifi_mon_collector_element_t *collector_elem = (wifi_mon_collector_element_t *) ce;

    if (collector_elem == NULL || collector_elem->args == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL arguments \n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (collector_elem->args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
        return RETURN_OK;
    }
    radioOperation = getRadioOperationParam(collector_elem->args->radio_index);
    if (radioOperation == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL radioOperation pointer for radio : %d\n", __func__, __LINE__, collector_elem->args->radio_index);
        return RETURN_ERR;
    }

    wifi_cap = getRadioCapability(collector_elem->args->radio_index);

    if (get_allowed_channels(radioOperation->band, wifi_cap, channels, &num_channels, radioOperation->DfsEnabled) != RETURN_OK) {
        wifi_util_error_print(WIFI_MON, "%s:%d get allowed channels failed for the radio : %d\n",__func__,__LINE__, collector_elem->args->radio_index);
        return RETURN_ERR;
    }
    
    //Traverse through the providers
    provider_elem = hash_map_get_first(collector_elem->provider_list);
    while (provider_elem != NULL) {
        for (i=0; i<provider_elem->mon_stats_config->args.channel_list.num_channels; i++) {
            for (j=0; j<num_channels; j++) {
                if (provider_elem->mon_stats_config->args.channel_list.channels_list[i] == channels[j]) {
                    is_used[j] = 1;
                    break;
                }
            }
        }
        provider_elem = hash_map_get_next(collector_elem->provider_list, provider_elem);
    }

    i = 0;
    for (j=0; j<num_channels; j++) {
        if (is_used[j] == 1) {
            collector_elem->args->channel_list.channels_list[i] =  channels[j];
            i++;
            break;
        }
    }
    collector_elem->args->channel_list.num_channels = i;
    return RETURN_OK;
}


