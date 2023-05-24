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

#include <stdbool.h>
#include "wifi_hal.h"
#include "wifi_mgr.h"
#include "wifi_sm.h"
#include "const.h"

#define DCA_TO_APP 1
#define APP_TO_DCA 2

typedef struct {
    wifi_associated_dev3_t      assoc_dev_diag[MAX_ASSOCIATED_WIFI_DEVS];
    wifi_associated_dev_stats_t assoc_dev_stats[MAX_ASSOCIATED_WIFI_DEVS];
} client_assoc_data_t;

typedef struct {
    client_assoc_data_t client_assoc_data[MAX_NUM_VAP_PER_RADIO];
    unsigned int    diag_vap_presence_mask;
    unsigned int    stats_vap_presence_mask;
    unsigned int    req_stats_vap_mask;
} client_assoc_stats_t;

client_assoc_stats_t client_assoc_stats[MAX_NUM_RADIOS];

int sm_survey_type_conversion(wifi_neighborScanMode_t *halw_scan_type, survey_type_t *app_stat_type, unsigned int conv_type)
{
    //is RADIO_SCAN_TYPE_NONE is required? as None survey type is not present
    unsigned int i = 0;
    wifi_neighborScanMode_t halw_scan_enum[] = {WIFI_RADIO_SCAN_MODE_FULL, WIFI_RADIO_SCAN_MODE_ONCHAN, WIFI_RADIO_SCAN_MODE_OFFCHAN};
    survey_type_t app_stat_enum[] = {survey_type_full, survey_type_on_channel, survey_type_off_channel};

    if ((halw_scan_type == NULL) || (app_stat_type == NULL)) {
        return RETURN_ERR;
    }

    if (conv_type == APP_TO_DCA) {
        for (i = 0; i < ARRAY_SIZE(app_stat_enum); i++) {
            if (*app_stat_type == app_stat_enum[i]) {
                *halw_scan_type = halw_scan_enum[i];
                return RETURN_OK;
            }
        }
    } else if (conv_type == DCA_TO_APP) {
        for (i = 0; i < ARRAY_SIZE(halw_scan_enum); i++) {
            if (*halw_scan_type == halw_scan_enum[i]) {
                *app_stat_type = app_stat_enum[i];
                return RETURN_OK;
            }
        }
    }

    return RETURN_ERR;
}

int sm_route(wifi_event_route_t *route)
{
    memset(route, 0, sizeof(wifi_event_route_t));
    route->dst = wifi_sub_component_mon;
    route->u.inst_bit_map = wifi_app_inst_sm;
    return RETURN_OK;
}

int neighbor_response(wifi_dca_response_t *dca_response)
{
    unsigned int radio_index = 0;
    radio_index = dca_response->args.radio_index;
    unsigned int count = 0;
    wifi_neighbor_ap2_t *neighbor_ap = NULL;

    neighbor_ap =  (wifi_neighbor_ap2_t *)dca_response->u.neigh_ap;

    wifi_util_dbg_print(WIFI_APPS,"%s:%d: radio_index : %d stats_array_size : %d\r\n",__func__, __LINE__, radio_index, dca_response->stat_array_size);

    for (count = 0; count < dca_response->stat_array_size; count++) {
        wifi_util_dbg_print(WIFI_APPS,"%s:%d: count : %d ap_SSID : %s\r\n",__func__, __LINE__, count, neighbor_ap[count].ap_SSID);
    }
    return RETURN_OK;
}

int survey_response(wifi_dca_response_t *dca_response)
{
    unsigned int radio_index = 0;
    unsigned int count = 0;
    radio_index = dca_response->args.radio_index;
    wifi_channelStats_t *channelStats = NULL;

    wifi_util_dbg_print(WIFI_APPS,"%s:%d: radio_index : %d stats_array_size : %d\r\n",__func__, __LINE__, radio_index, dca_response->stat_array_size);

    channelStats = dca_response->u.chan_stats;
    for (count = 0; count < dca_response->stat_array_size; count++) {
        wifi_util_dbg_print(WIFI_APPS,"%s:%d: radio_index : %d channel_num : %d\r\n",__func__, __LINE__, radio_index, channelStats[count].ch_number);
    }

    return RETURN_OK;
}


int assoc_client_response(wifi_dca_response_t *dca_response)
{
    unsigned int radio_index = 0;
    unsigned int vap_index = 0;
    int vap_array_index = 0;
    radio_index = dca_response->args.radio_index;
    vap_index = dca_response->args.vap_index;
    wifi_mgr_t *wifi_mgr = get_wifimgr_obj();
    char vap_name[32];

    if (convert_vap_index_to_name(&wifi_mgr->hal_cap.wifi_prop, vap_index, vap_name) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d: convert_vap_index_to_name failed for vap_index : %d\r\n",__func__, __LINE__, vap_index);
        return RETURN_ERR;
    }

    vap_array_index = convert_vap_name_to_array_index(&wifi_mgr->hal_cap.wifi_prop, vap_name);
    if (vap_array_index == -1) {
        wifi_util_error_print(WIFI_APPS,"%s:%d: convert_vap_name_to_array_index failed for vap_name: %s\r\n",__func__, __LINE__, vap_name);
        return RETURN_ERR;
    }


    if (dca_response->data_type == dca_associated_device_diag) {
        memset(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_diag, 0, sizeof(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_diag));
        memcpy(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_diag, dca_response->u.assoc_dev_diag, sizeof(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_diag));
        client_assoc_stats[radio_index].diag_vap_presence_mask |= (1 << vap_index);
    } else if (dca_response->data_type == dca_associated_device_stats) {
        memset(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_stats, 0, sizeof(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_stats ));
        memcpy(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_stats, dca_response->u.assoc_dev_stats, sizeof(client_assoc_stats[radio_index].client_assoc_data[vap_array_index].assoc_dev_stats));
        client_assoc_stats[radio_index].stats_vap_presence_mask |= (1 << vap_index);
    }

    if ((client_assoc_stats[radio_index].diag_vap_presence_mask == client_assoc_stats[radio_index].req_stats_vap_mask) && (client_assoc_stats[radio_index].stats_vap_presence_mask == client_assoc_stats[radio_index].req_stats_vap_mask)) {
        client_assoc_stats[radio_index].diag_vap_presence_mask = 0;
        client_assoc_stats[radio_index].stats_vap_presence_mask = 0;
        wifi_util_dbg_print(WIFI_APPS,"%s:%d: push to dpp for radio_index : %d \r\n",__func__, __LINE__, radio_index);
    }

    return RETURN_OK;
}


int capacity_response(wifi_dca_response_t *dca_response)
{
    unsigned int radio_index = 0;
    radio_index = dca_response->args.radio_index;
    wifi_channelStats_t *channelStats = NULL;
    unsigned int count = 0;

    wifi_util_dbg_print(WIFI_APPS,"%s:%d: radio_index : %d stats_array_size : %d\r\n",__func__, __LINE__, radio_index, dca_response->stat_array_size);
    channelStats = dca_response->u.chan_stats;
    for (count = 0; count < dca_response->stat_array_size; count++) {
        wifi_util_dbg_print(WIFI_APPS,"%s:%d: radio_index : %d channel_num : %d\r\n",__func__, __LINE__, radio_index, channelStats[count].ch_number);
    }

    return RETURN_OK;
}


int handle_monitor_data_collection_response(wifi_app_t *app, wifi_event_t *event)
{
    wifi_dca_response_t    *dca_response;
    dca_response = (wifi_dca_response_t *)event->u.dca_response;
    int ret = RETURN_ERR;

    if (dca_response == NULL) {
        wifi_util_error_print(WIFI_APPS,"%s:%d: input event is NULL\r\n",__func__, __LINE__);
        return ret;
    }

    switch (dca_response->data_type) {
        case dca_neighbor_stats:
            ret = neighbor_response(dca_response);
        break;
        case dca_radio_channel_stats:
            if (dca_response->args.is_type_capacity == true) {
                ret = capacity_response(dca_response);
            } else {
                ret = survey_response(dca_response);
            }
        break;
        case dca_associated_device_diag:
        case dca_associated_device_stats:
            ret = assoc_client_response(dca_response);
        break;
        default:
            wifi_util_error_print(WIFI_APPS,"%s:%d: event not handle[%d]\r\n",__func__, __LINE__, dca_response->data_type);
    }

    return ret;
}


int monitor_event_sm(wifi_app_t *app, wifi_event_t *event)
{
    int ret = RETURN_ERR;

    if (event == NULL) {
        wifi_util_error_print(WIFI_APPS,"%s:%d: input event is NULL\r\n",__func__, __LINE__);
        return ret;
    }

    switch (event->sub_type) {
        case wifi_event_monitor_data_collection_response:
            ret = handle_monitor_data_collection_response(app, event);
        break;
        default:
            wifi_util_error_print(WIFI_APPS,"%s:%d: event not handle[%d]\r\n",__func__, __LINE__, event->sub_type);
        break;
    }

    return ret;
}

int generate_vap_mask_for_radio_index(unsigned int radio_index)
{
   rdk_wifi_vap_map_t *rdk_vap_map = NULL;
   unsigned int count = 0;
   rdk_vap_map = getRdkWifiVap(radio_index);
   if (rdk_vap_map == NULL) {
       wifi_util_error_print(WIFI_APPS,"%s:%d: getRdkWifiVap failed for radio_index : %d\r\n",__func__, __LINE__, radio_index);
       return RETURN_ERR;
   }
   for (count = 0; count < rdk_vap_map->num_vaps; count++) {
       client_assoc_stats[radio_index].req_stats_vap_mask |= (1 << rdk_vap_map->rdk_vap_array[count].vap_index);
   }

    return RETURN_OK;
}


/*
 * Not handled variables
 * report_type_t   report_type;
 * unsigned int    reporting_interval;
 * unsigned int    reporting_count;
 * unsigned int    threshold_util;
 * unsigned int    threshold_max_delay;
 *
 */
int sm_common_config_to_monitor_queue(wifi_monitor_data_t *data, stats_config_t *stat_config_entry)
{
    data->u.dca.inst = wifi_app_inst_sm;

    if (convert_freq_band_to_radio_index(stat_config_entry->radio_type, (int *)&data->u.dca.args.radio_index) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d: convert freq_band %d  to radio_index failed \r\n",__func__, __LINE__, stat_config_entry->radio_type);
        return RETURN_ERR;
    }

    data->u.dca.interval_ms =  stat_config_entry->sampling_interval*1000; //converting seconds to ms
    data->u.dca.repetitions = 0; //0 means no stop

    return RETURN_OK;
}

int neighbor_config_to_monitor_queue(wifi_monitor_data_t *data, stats_config_t *stat_config_entry)
{
    int i = 0;
    wifi_event_route_t route;
    sm_route(&route);

    if (sm_common_config_to_monitor_queue(data, stat_config_entry) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d SM Config creation failed %d\r\n", __func__, __LINE__, stat_config_entry->stats_type);
        return RETURN_ERR;
    }

    if (sm_survey_type_conversion(&data->u.dca.args.scan_mode, &stat_config_entry->survey_type, APP_TO_DCA) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d Invalid survey type %d\r\n", __func__, __LINE__, stat_config_entry->survey_type);
        return RETURN_ERR;
    }

    data->u.dca.data_type = dca_neighbor_stats;
    data->u.dca.args.dwell_time = stat_config_entry->survey_interval; //its in ms

    if (stat_config_entry->survey_type == survey_type_on_channel) {
        data->u.dca.args.channel_list.num_channels = 0;
    } else {
        data->u.dca.args.channel_list.num_channels = stat_config_entry->channels_list.num_channels;
        for (i = 0;i < stat_config_entry->channels_list.num_channels; i++) {
            data->u.dca.args.channel_list.channels_list[i] = stat_config_entry->channels_list.channels_list[i];
        }
    }

    if (data->u.dca.interval_ms == 0) {
        data->u.dca.interval_ms = stat_config_entry->reporting_interval * 1000; //converting seconds to ms
    }

    push_event_to_monitor_queue(data, wifi_event_monitor_data_collection_config, &route);

    return RETURN_OK;
}

int survey_config_to_monitor_queue(wifi_monitor_data_t *data, stats_config_t *stat_config_entry)
{
    int i = 0;
    wifi_event_route_t route;
    sm_route(&route);
    if (sm_common_config_to_monitor_queue(data, stat_config_entry) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d SM Config creation failed %d\r\n", __func__, __LINE__, stat_config_entry->stats_type);
        return RETURN_ERR;
    }

    if (sm_survey_type_conversion(&data->u.dca.args.scan_mode, &stat_config_entry->survey_type, APP_TO_DCA) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d Invalid survey type %d\r\n", __func__, __LINE__, stat_config_entry->survey_type);
        return RETURN_ERR;
    }

    data->u.dca.data_type = dca_radio_channel_stats;
//    data->u.dca.args.dwell_time = stat_config_entry->survey_interval; //its in ms

    if (stat_config_entry->survey_type == survey_type_on_channel) {
        data->u.dca.args.channel_list.num_channels = 0;
    } else {
        data->u.dca.args.channel_list.num_channels = stat_config_entry->channels_list.num_channels;
        for (i = 0;i < stat_config_entry->channels_list.num_channels; i++) {
            data->u.dca.args.channel_list.channels_list[i] = stat_config_entry->channels_list.channels_list[i];
        }
    }
    data->u.dca.args.is_type_capacity=false;

    push_event_to_monitor_queue(data, wifi_event_monitor_data_collection_config, &route);
    return RETURN_OK;
}


int client_diag_config_to_monitor_queue(wifi_monitor_data_t *data, stats_config_t *stat_config_entry)
{
    unsigned int vapArrayIndex = 0;
    wifi_mgr_t *wifi_mgr = get_wifimgr_obj();
    wifi_event_route_t route;
    sm_route(&route);
    if (sm_common_config_to_monitor_queue(data, stat_config_entry) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d SM Config creation failed %d\r\n", __func__, __LINE__, stat_config_entry->stats_type);
        return RETURN_ERR;
    }

    data->u.dca.data_type = dca_associated_device_diag;

    if (client_assoc_stats[data->u.dca.args.radio_index].req_stats_vap_mask == 0) {
        if(generate_vap_mask_for_radio_index(data->u.dca.args.radio_index) == RETURN_ERR) {
            wifi_util_error_print(WIFI_APPS,"%s:%d generate_vap_mask_for_radio_index failed \r\n", __func__, __LINE__);
            return RETURN_ERR;
        }
    }

    //for each vap push the event to monitor queue
    for (vapArrayIndex = 0; vapArrayIndex < getNumberVAPsPerRadio(data->u.dca.args.radio_index); vapArrayIndex++) {
        data->u.dca.args.vap_index = wifi_mgr->radio_config[data->u.dca.args.radio_index].vaps.rdk_vap_array[vapArrayIndex].vap_index;
        push_event_to_monitor_queue(data, wifi_event_monitor_data_collection_config, &route);
    }

    return RETURN_OK;
}


int client_stats_config_to_monitor_queue(wifi_monitor_data_t *data, stats_config_t *stat_config_entry)
{
    unsigned int vapArrayIndex = 0;
    wifi_event_route_t route;
    sm_route(&route);
    wifi_mgr_t *wifi_mgr = get_wifimgr_obj();
    if (sm_common_config_to_monitor_queue(data, stat_config_entry) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d SM Config creation failed %d\r\n", __func__, __LINE__, stat_config_entry->stats_type);
        return RETURN_ERR;
    }

    data->u.dca.data_type = dca_associated_device_stats;

    if (client_assoc_stats[data->u.dca.args.radio_index].req_stats_vap_mask == 0) {
        if(generate_vap_mask_for_radio_index(data->u.dca.args.radio_index) == RETURN_ERR) {
            wifi_util_error_print(WIFI_APPS,"%s:%d generate_vap_mask_for_radio_index failed \r\n", __func__, __LINE__);
            return RETURN_ERR;
        }
    }

    //for each vap push the event to monitor queue
    for (vapArrayIndex = 0; vapArrayIndex < getNumberVAPsPerRadio(data->u.dca.args.radio_index); vapArrayIndex++) {
        data->u.dca.args.vap_index = wifi_mgr->radio_config[data->u.dca.args.radio_index].vaps.rdk_vap_array[vapArrayIndex].vap_index;
        push_event_to_monitor_queue(data, wifi_event_monitor_data_collection_config, &route);
    }

    return RETURN_OK;
}

int capacity_config_to_monitor_queue(wifi_monitor_data_t *data, stats_config_t *stat_config_entry)
{
    wifi_event_route_t route;
    sm_route(&route);
    if (sm_common_config_to_monitor_queue(data, stat_config_entry) != RETURN_OK) {
        wifi_util_error_print(WIFI_APPS,"%s:%d SM Config creation failed %d\r\n", __func__, __LINE__, stat_config_entry->stats_type);
        return RETURN_ERR;
    }

    data->u.dca.data_type = dca_radio_channel_stats;
    //for capacity its on channel
    data->u.dca.args.channel_list.num_channels = 0;
    data->u.dca.args.is_type_capacity=true;

    push_event_to_monitor_queue(data, wifi_event_monitor_data_collection_config, &route);
    return RETURN_OK;
}


int push_sm_config_event_to_monitor_queue(wifi_app_t *app, wifi_monitor_dca_request_state_t state, stats_config_t *stat_config_entry)
{
    wifi_monitor_data_t *data;
    int ret = RETURN_ERR;

    if (stat_config_entry == NULL) {
        wifi_util_error_print(WIFI_APPS,"%s:%d input config entry is NULL\r\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    if (data == NULL) {
        wifi_util_error_print(WIFI_APPS,"%s:%d data allocation failed\r\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    memset(data, 0, sizeof(wifi_monitor_data_t));

    data->u.dca.req_state = state;

    switch (stat_config_entry->stats_type) {
        case stats_type_neighbor:
            ret = neighbor_config_to_monitor_queue(data, stat_config_entry);
        break;
        case stats_type_survey:
            ret = survey_config_to_monitor_queue(data, stat_config_entry);
        break;
        case stats_type_client:
            ret = client_diag_config_to_monitor_queue(data, stat_config_entry); // wifi_getApAssociatedDeviceDiagnosticResult3
            if (ret != RETURN_OK) {
                break;
            }
            ret = client_stats_config_to_monitor_queue(data, stat_config_entry); // wifi_getApAssociatedDeviceStats
            if (ret != RETURN_OK) {
                break;
            }
        break;
        case stats_type_capacity:
            ret = capacity_config_to_monitor_queue(data, stat_config_entry);
        break;
        default:
            wifi_util_error_print(WIFI_APPS,"%s:%d: stats_type not handled[%d]\r\n",__func__, __LINE__, stat_config_entry->stats_type);
            free(data);
            return RETURN_ERR;
    }

    if (ret == RETURN_ERR) {
        wifi_util_error_print(WIFI_APPS,"%s:%d Event trigger failed for %d\r\n", __func__, __LINE__, stat_config_entry->stats_type);
        free(data);
        return RETURN_ERR;
    }

    //push_event_to_monitor_queue(data, wifi_event_monitor_data_collection_config, NULL);
    free(data);

    return RETURN_OK;
}

int handle_sm_webconfig_event(wifi_app_t *app, wifi_event_t *event)
{

    webconfig_subdoc_data_t *webconfig_data = NULL;
    if (event == NULL) {
        wifi_util_dbg_print(WIFI_APPS,"%s %d input arguements are NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    webconfig_data = event->u.webconfig_data;
    if (webconfig_data == NULL) {
        wifi_util_dbg_print(WIFI_APPS,"%s %d webconfig_data is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (webconfig_data->type != webconfig_subdoc_type_stats_config) {
        wifi_util_dbg_print(WIFI_APPS,"%s %d invalid type : %d\n", __func__, __LINE__, webconfig_data->type);
        return RETURN_ERR;
    }


    hash_map_t *new_ctrl_stats_cfg_map = webconfig_data->u.decoded.stats_config_map;
    hash_map_t *cur_app_stats_cfg_map = app->data.u.sm_data.sm_stats_config_map;
    stats_config_t *cur_stats_cfg, *new_stats_cfg;
    stats_config_t *temp_stats_config;
    char key[64] = {0};

    if (new_ctrl_stats_cfg_map == NULL) {
        wifi_util_dbg_print(WIFI_APPS,"%s %d input ctrl stats map is null, Nothing to update\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    //search for the deleted elements if any in new_ctrl_stats_cfg
    if (cur_app_stats_cfg_map != NULL) {
        cur_stats_cfg = hash_map_get_first(cur_app_stats_cfg_map);
        while (cur_stats_cfg != NULL) {
            if (hash_map_get(new_ctrl_stats_cfg_map, cur_stats_cfg->stats_cfg_id) == NULL) {
                //send the delete and remove elem from cur_stats_cfg
                memset(key, 0, sizeof(key));
                snprintf(key, sizeof(key), "%s", cur_stats_cfg->stats_cfg_id);
                cur_stats_cfg = hash_map_get_next(cur_app_stats_cfg_map, cur_stats_cfg);

                push_sm_config_event_to_monitor_queue(app, dca_request_state_stop, cur_stats_cfg);

                //Temporary removal, need to uncomment it
                temp_stats_config = hash_map_remove(cur_app_stats_cfg_map, key);
                if (temp_stats_config != NULL) {
                    free(temp_stats_config);
                }
            } else {
                cur_stats_cfg = hash_map_get_next(cur_app_stats_cfg_map, cur_stats_cfg);

            }
        }
    }

    //search for the newly added/updated elements
    if (new_ctrl_stats_cfg_map != NULL) {
        new_stats_cfg = hash_map_get_first(new_ctrl_stats_cfg_map);
        while (new_stats_cfg != NULL) {
            cur_stats_cfg = hash_map_get(cur_app_stats_cfg_map, new_stats_cfg->stats_cfg_id);
            if (cur_stats_cfg == NULL) {
                cur_stats_cfg = (stats_config_t *)malloc(sizeof(stats_config_t));
                if (cur_stats_cfg == NULL) {
                    wifi_util_error_print(WIFI_APPS,"%s %d NULL pointer \n", __func__, __LINE__);
                    return RETURN_ERR;
                }
                memset(cur_stats_cfg, 0, sizeof(stats_config_t));
                memcpy(cur_stats_cfg, new_stats_cfg, sizeof(stats_config_t));
                hash_map_put(cur_app_stats_cfg_map, strdup(cur_stats_cfg->stats_cfg_id), cur_stats_cfg);
                //Notification for new entry.

                push_sm_config_event_to_monitor_queue(app, dca_request_state_start, cur_stats_cfg);

            } else {
                if (memcmp(cur_stats_cfg, new_stats_cfg, sizeof(stats_config_t)) != 0) {
                    memcpy(cur_stats_cfg, new_stats_cfg, sizeof(stats_config_t));
                    //Notification for update entry.
                    push_sm_config_event_to_monitor_queue(app, dca_request_state_start, cur_stats_cfg);
                }
            }

            new_stats_cfg = hash_map_get_next(new_ctrl_stats_cfg_map, new_stats_cfg);
        }
    }

    return RETURN_OK;
}

int sm_event(wifi_app_t *app, wifi_event_t *event)
{
//TBD :  SM_APPS_INTEGRATED is not defined and and sm_apps will not process the events
////1. When SM-Apps is fully implemented, Remove ifdef SM_APPS_INTEGRATED
////2. check for SM_APPS_INTEGRATED in source/core/wifi_ctrl_webconfig.c and do the changes as requested.
#ifdef SM_APPS_INTEGRATED
    switch (event->event_type) {
        case wifi_event_type_webconfig:
            handle_sm_webconfig_event(app, event);
        break;
        case wifi_event_type_monitor:
            monitor_event_sm(app, event);
        break;
        default:
        break;
    }
#endif

    return RETURN_OK;
}


int sm_init(wifi_app_t *app, unsigned int create_flag)
{
    if (app_init(app, create_flag) != 0) {
        return RETURN_ERR;
    }

    app->data.u.sm_data.sm_stats_config_map  = hash_map_create();

    memset(client_assoc_stats, 0, sizeof(client_assoc_stats));

    return RETURN_OK;
}

int free_sm_stats_config_map(wifi_app_t *app)
{
    stats_config_t *stats_config = NULL, *temp_stats_config = NULL;
    char key[64] = {0};

    if (app->data.u.sm_data.sm_stats_config_map != NULL) {
        stats_config = hash_map_get_first(app->data.u.sm_data.sm_stats_config_map);
        while (stats_config != NULL) {
            memset(key, 0, sizeof(key));
            snprintf(key, sizeof(key), "%s", stats_config->stats_cfg_id);
            stats_config = hash_map_get_next(app->data.u.sm_data.sm_stats_config_map, stats_config);
            temp_stats_config = hash_map_remove(app->data.u.sm_data.sm_stats_config_map, key);
            if (temp_stats_config != NULL) {
                free(temp_stats_config);
            }
        }
        hash_map_destroy(app->data.u.sm_data.sm_stats_config_map);
        app->data.u.sm_data.sm_stats_config_map = NULL;
    }
    return RETURN_OK;
}


int sm_deinit(wifi_app_t *app)
{
    free_sm_stats_config_map(app);
    return RETURN_OK;
}
