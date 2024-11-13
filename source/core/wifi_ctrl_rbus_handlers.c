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

#include "const.h"
#include "log.h"
#include "wifi_passpoint.h"
#include "wifi_hal.h"
#include "wifi_ctrl.h"
#include "wifi_mgr.h"
#include "wifi_util.h"
#include "msgpack.h"
#include "wifi_monitor.h"
#include "wifi_webconfig.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_EVENT_NAME_SIZE     200

int webconfig_client_notify_apply(wifi_ctrl_t *ctrl, webconfig_subdoc_encoded_data_t *data)
{
    bus_error_t rc;
    raw_data_t rdata;

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data->raw;

    rc = get_bus_descriptor()->bus_event_publish_fn(&ctrl->handle, WIFI_WEBCONFIG_GET_ASSOC,
        &rdata);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus_event_publish_fn event failed %d\n", __func__,
            __LINE__, rc);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int webconfig_null_subdoc_notify_apply(wifi_ctrl_t *ctrl, webconfig_subdoc_encoded_data_t *data)
{
    bus_error_t rc;
    raw_data_t rdata;

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data->raw;

    rc = get_bus_descriptor()->bus_event_publish_fn(&ctrl->handle, WIFI_WEBCONFIG_GET_NULL_SUBDOC,
        &rdata);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus_event_publish_fn event failed %d\n", __func__,
            __LINE__, rc);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int notify_associated_entries(wifi_ctrl_t *ctrl, int ap_index, ULONG new_count, ULONG old_count)
{
    bus_error_t rc;
    char str[2048];
    memset(str, 0, 2048);

    if (ctrl == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return RETURN_ERR;
    }

    snprintf(str, sizeof(str),
        "Device.WiFi.AccessPoint.%d.AssociatedDeviceNumberOfEntries,%d,%lu,%lu,%d", ap_index + 1, 0,
        new_count, old_count, 2);
    rc = get_bus_descriptor()->bus_set_string_fn(&ctrl->handle, WIFI_NOTIFY_ASSOCIATED_ENTRIES,
        str);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus: bus_set_string_fn Failed %d\n", __func__,
            __LINE__, rc);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int notify_force_disassociation(wifi_ctrl_t *ctrl, int ap_index, char *threshold,
    mac_addr_str_t mac, int threshold_val, int client_val)
{
    bus_error_t rc;
    char str[2048];
    wifi_vap_info_t *vap_info = NULL;
    memset(str, 0, 2048);

    vap_info = getVapInfo(ap_index);

    if (ctrl == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return RETURN_ERR;
    }

    snprintf(str, sizeof(str), "%d,%s,%s,%d,%d", (ap_index + 1), threshold, mac, threshold_val,
        client_val);

    if (vap_info != NULL) {
        strncpy(vap_info->u.bss_info.postassoc.client_force_disassoc_info, str,
            sizeof(vap_info->u.bss_info.postassoc.client_force_disassoc_info));
    }

    rc = get_bus_descriptor()->bus_set_string_fn(&ctrl->handle, WIFI_NOTIFY_FORCE_DISASSOCIATION,
        str);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus: bus_set_string_fn Failed %d\n", __func__,
            __LINE__, rc);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int notify_deny_association(wifi_ctrl_t *ctrl, int ap_index, char *threshold, mac_addr_str_t mac,
    int threshold_val, int client_val)
{
    bus_error_t rc;
    char str[2048];
    wifi_vap_info_t *vap_info = NULL;

    memset(str, 0, 2048);

    if (ctrl == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return RETURN_ERR;
    }

    vap_info = getVapInfo(ap_index);

    snprintf(str, sizeof(str), "%d,%s,%s,%d,%d", (ap_index + 1), threshold, mac, threshold_val,
        client_val);

    if (vap_info != NULL) {
        strncpy(vap_info->u.bss_info.preassoc.client_deny_assoc_info, str,
            sizeof(vap_info->u.bss_info.preassoc.client_deny_assoc_info));
    }

    rc = get_bus_descriptor()->bus_set_string_fn(&ctrl->handle, WIFI_NOTIFY_DENY_ASSOCIATION, str);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus: bus_set_string_fn Failed %d\n", __func__,
            __LINE__, rc);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int notify_hotspot(wifi_ctrl_t *ctrl, assoc_dev_data_t *assoc_device)
{
    bus_error_t rc;
    char str[2048];
    mac_addr_str_t mac_str;
    memset(str, 0, 2048);

    if (ctrl == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return RETURN_ERR;
    }

    to_mac_str(assoc_device->dev_stats.cli_MACAddress, mac_str);
    snprintf(str, sizeof(str), "%d|%d|%d|%s", assoc_device->dev_stats.cli_Active,
        assoc_device->ap_index + 1, assoc_device->dev_stats.cli_RSSI, mac_str);

    rc = get_bus_descriptor()->bus_set_string_fn(&ctrl->handle, WIFI_HOTSPOT_NOTIFY, str);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus: bus_set_string_fn Failed %d\n", __func__,
            __LINE__, rc);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int notify_LM_Lite(wifi_ctrl_t *ctrl, LM_wifi_hosts_t *phosts, bool sync)
{
    int itr;
    bus_error_t rc;
    char str[2048];
    memset(str, 0, 2048);

    if (ctrl == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: NULL Pointer \n", __func__, __LINE__);
        return RETURN_ERR;
    }

    if (sync) {
        snprintf(str, sizeof(str), "%s,%s,%s,%d,%d", (char *)phosts->host[0].phyAddr,
            ('\0' != phosts->host[0].AssociatedDevice[0]) ?
                (char *)phosts->host[0].AssociatedDevice :
                "NULL",
            ('\0' != phosts->host[0].ssid[0]) ? (char *)phosts->host[0].ssid : "NULL",
            phosts->host[0].RSSI, (phosts->host[0].Status == TRUE) ? 1 : 0);

        rc = get_bus_descriptor()->bus_set_string_fn(&ctrl->handle, WIFI_LMLITE_NOTIFY, str);
        if (rc != bus_error_success) {
            wifi_util_error_print(WIFI_CTRL, "%s:%d: bus: Write Failed %d\n", __func__, __LINE__,
                rc);
            return RETURN_ERR;
        }
    } else {
        for (itr = 0; itr < phosts->count; itr++) {
            snprintf(str, sizeof(str), "%s,%s,%s,%d,%d", (char *)phosts->host[itr].phyAddr,
                ('\0' != phosts->host[itr].AssociatedDevice[0]) ?
                    (char *)phosts->host[itr].AssociatedDevice :
                    "NULL",
                ('\0' != phosts->host[itr].ssid[0]) ? (char *)phosts->host[0].ssid : "NULL",
                phosts->host[itr].RSSI, (phosts->host[itr].Status == TRUE) ? 1 : 0);

            rc = get_bus_descriptor()->bus_set_string_fn(&ctrl->handle, WIFI_LMLITE_NOTIFY, str);
            if (rc != bus_error_success) {
                wifi_util_error_print(WIFI_CTRL, "%s:%d: bus: Write Failed %d\n", __func__,
                    __LINE__, rc);
                return RETURN_ERR;
            }
        }
    }
    return RETURN_OK;
}

int webconfig_bus_apply_for_dml_thread_update(wifi_ctrl_t *ctrl,
    webconfig_subdoc_encoded_data_t *data)
{
    bus_error_t rc;
    raw_data_t rdata;

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data->raw;

    rc = get_bus_descriptor()->bus_event_publish_fn(&ctrl->handle, WIFI_WEBCONFIG_INIT_DML_DATA,
        &rdata);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus_event_publish_fn Event failed %d\n", __func__,
            __LINE__, rc);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int webconfig_bus_apply(wifi_ctrl_t *ctrl, webconfig_subdoc_encoded_data_t *data)
{
    bus_error_t rc;
    raw_data_t rdata;

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data->raw;

    rc = get_bus_descriptor()->bus_event_publish_fn(&ctrl->handle, WIFI_WEBCONFIG_DOC_DATA_NORTH,
        &rdata);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus_event_publish_fn Event failed %d\n", __func__,
            __LINE__, rc);

        return RETURN_ERR;
    }

    return RETURN_OK;
}

int get_managed_guest_bridge(char *brval, unsigned long length)
{
    bus_error_t rc;
    char *token = NULL;
    char *brname;
    wifi_mgr_t *g_wifi_mgr = get_wifimgr_obj();
    raw_data_t data;
    memset(&data, 0, sizeof(raw_data_t));

    rc = get_bus_descriptor()->bus_data_get_fn(&g_wifi_mgr->ctrl.handle, MANAGED_WIFI_BRIDGE,
        &data);
    if (data.data_type != bus_data_type_string) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d '%s' bus_data_get_fn failed with data_type:0x%x, rc:%\n", __func__, __LINE__,
            MANAGED_WIFI_BRIDGE, data.data_type, rc);
        get_bus_descriptor()->bus_data_free_fn(&data);
        return rc;
    }

    if (rc == bus_error_success) {
        brname = (char *)data.raw_data.bytes;
        wifi_util_dbg_print(WIFI_CTRL, "Managed_wifi bridge name is %s\n", brname);
        token = strrchr(brname, ':');
        snprintf(brval, length, token + 1);
        wifi_util_info_print(WIFI_CTRL, "Managed_wifi bridge val is %s\n", brval);
        get_bus_descriptor()->bus_data_free_fn(&data);
        return RETURN_OK;
    }
    /* Just in case if 'rc' value is not handled  correctly */
    get_bus_descriptor()->bus_data_free_fn(&data);
    return RETURN_ERR;
}

int set_managed_guest_interfaces(char *interface_name)
{
    bus_error_t rc;
    wifi_mgr_t *g_wifi_mgr = get_wifimgr_obj();
    rc = get_bus_descriptor()->bus_set_string_fn(&g_wifi_mgr->ctrl.handle, MANAGED_WIFI_INTERFACE,
        interface_name);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "Failed to set %s with %s \n", MANAGED_WIFI_INTERFACE,
            interface_name);
        return RETURN_ERR;
    } else {
        wifi_util_dbg_print(WIFI_CTRL, "Successfuly set %s with %s \n", MANAGED_WIFI_INTERFACE,
            interface_name);
    }

    return RETURN_OK;
}

bus_error_t webconfig_init_data_get_subdoc(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    bus_error_t status = bus_error_success;
    webconfig_subdoc_data_t data;
    wifi_mgr_t *mgr = (wifi_mgr_t *)get_wifimgr_obj();
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    unsigned int num_of_radios = getNumberRadios();
#define MAX_ACSD_SYNC_TIME_WAIT 12
    static int sync_retries = 0;
    raw_data_t rdata;

    if (!ctrl->ctrl_initialized) {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: Ctrl not initialized skip request.\n", __FUNCTION__,
            __LINE__);
        return bus_error_invalid_operation;
    }
    if (ctrl->network_mode == rdk_dev_mode_type_gw) {
        if ((sync_retries < MAX_ACSD_SYNC_TIME_WAIT)) {
            if ((is_acs_channel_updated(num_of_radios) == false) ||
                (check_wifi_radio_sched_timeout_active_status(ctrl) == true)) {
                sync_retries++;
                wifi_util_info_print(WIFI_CTRL,
                    "%s:%d: sync_retries=%d wifidb and global radio config not updated\n",
                    __FUNCTION__, __LINE__, sync_retries);
                return bus_error_invalid_operation;
            }
        }
        wifi_util_info_print(WIFI_CTRL,
            "%s:%d: sync_retries=%d wifidb and global radio config updated\n", __FUNCTION__,
            __LINE__, sync_retries);
        for (unsigned int index = 0; index < num_of_radios; index++) {
            if (ctrl->acs_pending[index] == true) {
                ctrl->acs_pending[index] = false;
            }
        }
        sync_retries = MAX_ACSD_SYNC_TIME_WAIT;
        memset(&data, 0, sizeof(webconfig_subdoc_data_t));
        memcpy((unsigned char *)&data.u.decoded.radios, (unsigned char *)&mgr->radio_config,
            num_of_radios * sizeof(rdk_wifi_radio_t));
        memcpy((unsigned char *)&data.u.decoded.config, (unsigned char *)&mgr->global_config,
            sizeof(wifi_global_config_t));
        memcpy((unsigned char *)&data.u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
            sizeof(wifi_hal_capability_t));
        data.u.decoded.num_radios = num_of_radios;
        // tell webconfig to encode
        webconfig_encode(&ctrl->webconfig, &data, webconfig_subdoc_type_dml);

        memset(&rdata, 0, sizeof(raw_data_t));

        rdata.data_type = bus_data_type_string;
        rdata.raw_data.bytes = (void *)data.u.encoded.raw;

        status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);
        webconfig_data_free(&data);
    } else if (ctrl->network_mode == rdk_dev_mode_type_ext) {
        if (check_wifi_radio_sched_timeout_active_status(ctrl) == true) {
            wifi_util_dbg_print(WIFI_CTRL, "%s wifidb and cache are not synced!\n", __FUNCTION__);
            return bus_error_invalid_operation;
        }
        memset(&data, 0, sizeof(webconfig_subdoc_data_t));
        memcpy((unsigned char *)&data.u.decoded.radios, (unsigned char *)&mgr->radio_config,
            num_of_radios * sizeof(rdk_wifi_radio_t));
        memcpy((unsigned char *)&data.u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
            sizeof(wifi_hal_capability_t));
        data.u.decoded.num_radios = num_of_radios;
        // tell webconfig to encode
        webconfig_encode(&ctrl->webconfig, &data, webconfig_subdoc_type_mesh_sta);

        memset(&rdata, 0, sizeof(raw_data_t));
        rdata.data_type = bus_data_type_string;
        rdata.raw_data.bytes = (void *)data.u.encoded.raw;

        status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);

        webconfig_data_free(&data);
    }
    return status;
}

bus_error_t webconfig_get_dml_subdoc(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    bus_error_t status = bus_error_success;
    webconfig_subdoc_data_t data;
    wifi_mgr_t *mgr = (wifi_mgr_t *)get_wifimgr_obj();
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    raw_data_t rdata;

    memset(&data, 0, sizeof(webconfig_subdoc_data_t));
    memcpy((unsigned char *)&data.u.decoded.radios, (unsigned char *)&mgr->radio_config,
        getNumberRadios() * sizeof(rdk_wifi_radio_t));
    memcpy((unsigned char *)&data.u.decoded.config, (unsigned char *)&mgr->global_config,
        sizeof(wifi_global_config_t));
    memcpy((unsigned char *)&data.u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
        sizeof(wifi_hal_capability_t));
    data.u.decoded.num_radios = getNumberRadios();
    // tell webconfig to encode
    if (webconfig_encode(&ctrl->webconfig, &data, webconfig_subdoc_type_dml) !=
        webconfig_error_none) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d webconfig encode failed\n", __func__, __LINE__);
        return bus_error_general;
    }

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data.u.encoded.raw;

    status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);

    webconfig_data_free(&data);
    return status;
}

bus_error_t webconfig_set_subdoc(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t options)
{
    char *pTmp = NULL;
    bus_error_t status;
    raw_data_t data;

    memset(&data, 0, sizeof(raw_data_t));

    status = get_bus_descriptor()->bus_property_data_get_fn(handle, property, options, &data);
    if (data.data_type != bus_data_type_string) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d bus_property_data_get_fn failed with data_type:%d, status:%d\n", __func__,
            __LINE__, data.data_type, status);
        return status;
    }

    pTmp = (char *)data.raw_data.bytes;

    if (status != bus_error_success || pTmp == NULL) {
        return status;
    }

    push_event_to_ctrl_queue((cJSON *)pTmp, (strlen(pTmp) + 1), wifi_event_type_webconfig,
        wifi_event_webconfig_set_data_ovsm, NULL);
    return status;
}

static void MarkerListConfigHandler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    marker_list_t list_type;
    const char *pTmp = NULL;
    raw_data_t data;

    if (!event) {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: Invalid Event Received %s", __func__, __LINE__,
            subscription->event_name);
        return;
    }

    if (strcmp(subscription->event_name, WIFI_NORMALIZED_RSSI_LIST) == 0) {
        list_type = wifi_event_type_normalized_rssi;

    } else if (strcmp(subscription->event_name, WIFI_SNR_LIST) == 0) {
        list_type = wifi_event_type_snr;

    } else if (strcmp(subscription->event_name, WIFI_CLI_STAT_LIST) == 0) {
        list_type = wifi_event_type_cli_stat;

    } else if (strcmp(subscription->event_name, WIFI_TxRx_RATE_LIST) == 0) {
        list_type = wifi_event_type_txrx_rate;

    } else {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: Invalid Event Received %s", __func__, __LINE__,
            subscription->event_name);
        return;
    }

    memset(&data, 0, sizeof(raw_data_t));

    data.data_type = bus_data_type_string;

    bus_error_t status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data,
        "value");

    pTmp = (char *)data.raw_data.bytes;
    if ((status != bus_error_success) || (pTmp == NULL)) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s", __func__,
            __LINE__, subscription->event_name);
        return;
    }
    wifi_util_dbg_print(WIFI_CTRL, "%s:%d: telemetry type:%d value:%s\n", __func__, __LINE__,
        list_type, pTmp);
    push_event_to_ctrl_queue(pTmp, (strlen(pTmp) + 1), wifi_event_type_command, list_type, NULL);
}

#if defined(GATEWAY_FAILOVER_SUPPORTED)
static void activeGatewayCheckHandler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    // int csi_session;
    bool other_gateway_present = false;
    raw_data_t data;
    bus_error_t status;

    if (!event || (strcmp(subscription->event_name, WIFI_ACTIVE_GATEWAY_CHECK) != 0)) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: Invalid Event Received %s", __func__, __LINE__,
            subscription->event_name);
        return;
    }

    memset(&data, 0, sizeof(raw_data_t));

    data.data_type = bus_data_type_boolean;

    status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data, NULL);

    other_gateway_present = data.raw_data.b;

    if (status != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s", __func__,
            __LINE__, subscription->event_name);
        return;
    }

    push_event_to_ctrl_queue(&other_gateway_present, sizeof(other_gateway_present),
        wifi_event_type_command, wifi_event_type_active_gw_check, NULL);
}
#endif
static void wan_failover_handler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    bool data_value = false;
    bus_error_t status;
    raw_data_t data;

    if (!event || (strcmp(subscription->event_name, WIFI_WAN_FAILOVER_TEST) != 0)) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: Invalid Event Received %s", __func__, __LINE__,
            subscription->event_name);
        return;
    }

    memset(&data, 0, sizeof(raw_data_t));

    data.data_type = bus_data_type_boolean;

    status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data,
        (char *)subscription->event_name);

    data_value = data.raw_data.b;

    if (status != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s", __func__,
            __LINE__, subscription->event_name);
        return;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d: recv data:%d\r\n", __func__, __LINE__, data_value);
}

static void hotspotTunnelHandler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    char *pTmp;
    bus_error_t status;
    raw_data_t data;

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d Recvd Event\n", __func__, __LINE__);
    if (!event) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    if (strcmp(subscription->event_name, "TunnelStatus") != 0) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d Not Tunnel event, %s\n", __func__, __LINE__,
            subscription->event_name);
        return;
    }

    memset(&data, 0, sizeof(raw_data_t));

    data.data_type = bus_data_type_string;

    status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data,
        (char *)subscription->event_name);

    pTmp = (char *)data.raw_data.bytes;

    if (status != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s", __func__,
            __LINE__, subscription->event_name);
        return;
    }

    bool tunnel_status = false;
    if (strcmp(pTmp, "TUNNEL_UP") == 0) {
        tunnel_status = true;
    }

    wifi_event_subtype_t ces_t = tunnel_status ? wifi_event_type_xfinity_tunnel_up :
                                                 wifi_event_type_xfinity_tunnel_down;
    push_event_to_ctrl_queue(&tunnel_status, sizeof(tunnel_status), wifi_event_type_command, ces_t,
        NULL);
}

bus_error_t get_assoc_clients_data(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    webconfig_subdoc_data_t data;
    raw_data_t rdata;
    bus_error_t status;
#if DML_SUPPORT
    assoc_dev_data_t *assoc_dev_data;
    int itr, itrj;
#endif
    wifi_mgr_t *mgr = (wifi_mgr_t *)get_wifimgr_obj();
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    if ((mgr == NULL) || (ctrl == NULL)) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d NULL pointers\n", __func__, __LINE__);
        return bus_error_invalid_operation;
    }

#if DML_SUPPORT
    pthread_mutex_lock(&ctrl->lock);
    for (itr = 0; itr < MAX_NUM_RADIOS; itr++) {
        for (itrj = 0; itrj < MAX_NUM_VAP_PER_RADIO; itrj++) {
            if (mgr->radio_config[itr].vaps.rdk_vap_array[itrj].associated_devices_map != NULL) {
                assoc_dev_data = hash_map_get_first(
                    mgr->radio_config[itr].vaps.rdk_vap_array[itrj].associated_devices_map);
                while (assoc_dev_data != NULL) {
                    get_sta_stats_info(assoc_dev_data);
                    assoc_dev_data = hash_map_get_next(
                        mgr->radio_config[itr].vaps.rdk_vap_array[itrj].associated_devices_map,
                        assoc_dev_data);
                }
            }
        }
    }
    pthread_mutex_unlock(&ctrl->lock);
#endif
    memset(&data, 0, sizeof(webconfig_subdoc_data_t));

    memcpy((unsigned char *)&data.u.decoded.radios, (unsigned char *)&mgr->radio_config,
        getNumberRadios() * sizeof(rdk_wifi_radio_t));
    memcpy((unsigned char *)&data.u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
        sizeof(wifi_hal_capability_t));

    data.u.decoded.num_radios = getNumberRadios();
    data.u.decoded.assoclist_notifier_type = assoclist_notifier_full;
    webconfig_encode(&ctrl->webconfig, &data, webconfig_subdoc_type_associated_clients);

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data.u.encoded.raw;

    status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);

    webconfig_data_free(&data);

    return status;
}

bus_error_t get_null_subdoc_data(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    bus_error_t status;
    char *name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    webconfig_subdoc_data_t data;
    raw_data_t rdata;
    wifi_mgr_t *mgr = (wifi_mgr_t *)get_wifimgr_obj();
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    if ((mgr == NULL) || (ctrl == NULL) || (name == NULL)) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d NULL pointers\n", __func__, __LINE__);
        return bus_error_invalid_input;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s bus: bus property=%s\n", __FUNCTION__, name);

    if (strcmp(name, WIFI_WEBCONFIG_GET_NULL_SUBDOC) != 0) {
        wifi_util_error_print(WIFI_CTRL, "%s bus: bus property invalid '%s'\n", __FUNCTION__, name);
        return bus_error_invalid_input;
    }

    memset(&data, 0, sizeof(webconfig_subdoc_data_t));

    memcpy((unsigned char *)&data.u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
        sizeof(wifi_hal_capability_t));

    data.u.decoded.num_radios = getNumberRadios();
    webconfig_encode(&ctrl->webconfig, &data, webconfig_subdoc_type_null);

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data.u.encoded.raw;

    status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);

    webconfig_data_free(&data);

    return status;
}

bus_error_t get_sta_disconnection(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    char *name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    bus_error_t status = bus_error_success;
    raw_data_t rdata;

    if (name == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d property name is not found\r\n", __FUNCTION__,
            __LINE__);
        return bus_error_invalid_input;
    }

    if (strcmp(name, WIFI_STA_TRIGGER_DISCONNECTION) == 0) {
        uint32_t output_data = 0;

        memset(&rdata, 0, sizeof(raw_data_t));
        rdata.data_type = bus_data_type_uint32;
        rdata.raw_data.u32 = output_data;
        rdata.raw_data_len = sizeof(output_data);

        status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);
    }

    return status;
}

bus_error_t set_sta_disconnection(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t options)
{
    unsigned int disconnection_type = 0;
    bus_error_t status;
    raw_data_t data;

    memset(&data, 0, sizeof(raw_data_t));

    status = get_bus_descriptor()->bus_property_data_get_fn(handle, property, options, &data);
    if (data.data_type != bus_data_type_uint32) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d bus_property_data_get_fn failed with data_type:%d, status:%d\n", __func__,
            __LINE__, data.data_type, status);
        return status;
    }

    disconnection_type = (unsigned int)data.raw_data.u32;

    if (status != bus_error_success) {
        return status;
    }

    // 0 - no action
    // 1 - disconnection
    // 2 - disconnection + ignore current radio on next scan
    wifi_util_dbg_print(WIFI_CTRL, "%s bus: bus set %d\n", __FUNCTION__, disconnection_type);
    push_event_to_ctrl_queue(&disconnection_type, sizeof(disconnection_type),
        wifi_event_type_command, wifi_event_type_trigger_disconnection, NULL);

    return bus_error_success;
}

bus_error_t set_kickassoc_command(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t options)
{
    char *pTmp;
    bus_error_t status;
    raw_data_t data;

    memset(&data, 0, sizeof(raw_data_t));

    status = get_bus_descriptor()->bus_property_data_get_fn(handle, property, options, &data);
    if (data.data_type != bus_data_type_string) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d bus_property_data_get_fn failed with data_type:%d, status:%d\n", __func__,
            __LINE__, data.data_type, status);
        return status;
    }

    pTmp = (char *)data.raw_data.bytes;

    if (status != bus_error_success || pTmp == NULL) {
        return status;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s bus set string %s\n", __FUNCTION__, pTmp);
    push_event_to_ctrl_queue(pTmp, (strlen(pTmp) + 1), wifi_event_type_command,
        wifi_event_type_command_kick_assoc_devices, NULL);

    return status;
}

bus_error_t set_wifiapi_command(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t options)
{
    bus_error_t status;
    unsigned int len = 0;
    char *pTmp = NULL;
    raw_data_t data;

    memset(&data, 0, sizeof(raw_data_t));

    status = get_bus_descriptor()->bus_property_data_get_fn(handle, property, options, &data);
    if (data.data_type != bus_data_type_string) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d bus_property_data_get_fn failed with data_type:%d, status:%d\n", __func__,
            __LINE__, data.data_type, status);
        return status;
    }

    len = data.raw_data_len;
    pTmp = (char *)data.raw_data.bytes;

    if (status != bus_error_success || pTmp == NULL) {
        return status;
    }
    wifi_util_dbg_print(WIFI_CTRL, "%s bus set string string=%s, len=%d\n", __FUNCTION__, pTmp,
        len);
    push_event_to_ctrl_queue((char *)pTmp, (strlen(pTmp) + 1), wifi_event_type_wifiapi,
        wifi_event_type_wifiapi_execution, NULL);
    return status;
}

bus_error_t wifiapi_event_handler(bus_handle_t *handle, bus_event_sub_action_t action,
    char const *eventName, bus_filter_t filter, int32_t interval, bool *autoPublish)
{
    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(filter);

    wifi_util_dbg_print(WIFI_CTRL,
        "%s:%d called: action=%s\n eventName=%s autoPublish:%d interval:%d\n", __func__, __LINE__,
        action == bus_event_action_subscribe ? "subscribe" : "unsubscribe", eventName, *autoPublish,
        interval);

    return bus_error_success;
}

bus_error_t hotspot_event_handler(bus_handle_t *handle, bus_event_sub_action_t action,
    char const *eventName, bus_filter_t filter, int32_t interval, bool *autoPublish)
{
    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(filter);

    wifi_util_dbg_print(WIFI_CTRL,
        "%s:%d called: action=%s\n eventName=%s autoPublish:%d interval:%d\n", __func__, __LINE__,
        action == bus_event_action_subscribe ? "subscribe" : "unsubscribe", eventName, *autoPublish,
        interval);

    return bus_error_success;
}

int wifiapi_result_publish(void)
{
    bus_error_t rc;
    int len;
    bus_error_t status = bus_error_success;
    char data[128];
    raw_data_t rdata;

    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    if (ctrl == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d NULL pointers\n", __func__, __LINE__);
        status = bus_error_invalid_input;
        return status;
    }
    pthread_mutex_lock(&ctrl->lock);

    if (ctrl->wifiapi.result == NULL) {
        len = strlen("Result not avaiable");
        strncpy(data, "Result not avaiable", len);
    } else {
        len = strlen(ctrl->wifiapi.result);
        strncpy(data, ctrl->wifiapi.result, len);
    }

    memset(&rdata, 0, sizeof(raw_data_t));
    rdata.data_type = bus_data_type_string;
    rdata.raw_data.bytes = (void *)data;
    rdata.raw_data_len = len;

    rc = get_bus_descriptor()->bus_event_publish_fn(&ctrl->handle, WIFI_BUS_WIFIAPI_RESULT, &rdata);
    pthread_mutex_unlock(&ctrl->lock);

    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d bus_event_publish_fn %s failed: %d\n", __func__,
            WIFI_BUS_WIFIAPI_RESULT, __LINE__, rc);
    }
    return rc;
}

// Function used till the bus_data_get_fn invalid context issue is resolved
/* The function returns a pointer to allocated memory or NULL in case of error */
char *get_assoc_devices_blob()
{
    char *str = NULL;
    webconfig_subdoc_data_t *pdata = NULL;
#if DML_SUPPORT
    assoc_dev_data_t *assoc_dev_data;
    int itr, itrj;
#endif
    wifi_mgr_t *mgr = (wifi_mgr_t *)get_wifimgr_obj();
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    if ((mgr == NULL) || (ctrl == NULL)) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d NULL pointers\n", __func__, __LINE__);
        return NULL;
    }

#if DML_SUPPORT
    pthread_mutex_lock(&ctrl->lock);
    for (itr = 0; itr < MAX_NUM_RADIOS; itr++) {
        for (itrj = 0; itrj < MAX_NUM_VAP_PER_RADIO; itrj++) {
            if (mgr->radio_config[itr].vaps.rdk_vap_array[itrj].associated_devices_map != NULL) {
                assoc_dev_data = hash_map_get_first(
                    mgr->radio_config[itr].vaps.rdk_vap_array[itrj].associated_devices_map);
                while (assoc_dev_data != NULL) {
                    get_sta_stats_info(assoc_dev_data);
                    assoc_dev_data = hash_map_get_next(
                        mgr->radio_config[itr].vaps.rdk_vap_array[itrj].associated_devices_map,
                        assoc_dev_data);
                }
            }
        }
    }
    pthread_mutex_unlock(&ctrl->lock);

#endif
    pdata = (webconfig_subdoc_data_t *)malloc(sizeof(webconfig_subdoc_data_t));
    if (pdata == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d Failed to allocate memory.\n", __func__, __LINE__);
        return NULL;
    }
    memset(pdata, 0, sizeof(webconfig_subdoc_data_t));
    memcpy((unsigned char *)&pdata->u.decoded.radios, (unsigned char *)&mgr->radio_config,
        getNumberRadios() * sizeof(rdk_wifi_radio_t));
    memcpy((unsigned char *)&pdata->u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
        sizeof(wifi_hal_capability_t));

    pdata->u.decoded.num_radios = getNumberRadios();
    pdata->u.decoded.assoclist_notifier_type = assoclist_notifier_full;

    webconfig_encode(&ctrl->webconfig, pdata, webconfig_subdoc_type_associated_clients);

    str = (char *)calloc(strlen(pdata->u.encoded.raw) + 1, sizeof(char));
    if (str == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d Failed to allocate memory.\n", __func__, __LINE__);
        free(pdata);
        return NULL;
    }

    memcpy(str, pdata->u.encoded.raw, strlen(pdata->u.encoded.raw));

    webconfig_data_free(pdata);
    free(pdata);

    return str;
}

bus_error_t get_acl_device_data(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t opts)
{
    char *name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    webconfig_subdoc_data_t data;
    wifi_mgr_t *mgr = (wifi_mgr_t *)get_wifimgr_obj();
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    bus_error_t status = bus_error_success;
    raw_data_t rdata;

    if ((mgr == NULL) || (ctrl == NULL) || (name == NULL)) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d NULL pointers\n", __func__, __LINE__);
        return bus_error_invalid_input;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s bus property=%s\n", __FUNCTION__, name);

    if (strncmp(name, WIFI_WEBCONFIG_GET_ACL, strlen(WIFI_WEBCONFIG_GET_ACL) + 1) != 0) {
        wifi_util_error_print(WIFI_CTRL, "%s bus property invalid '%s'\n", __FUNCTION__, name);
        return bus_error_invalid_input;
    }

    memset(&data, 0, sizeof(webconfig_subdoc_data_t));
    memcpy((unsigned char *)&data.u.decoded.radios, (unsigned char *)&mgr->radio_config,
        getNumberRadios() * sizeof(rdk_wifi_radio_t));
    memcpy((unsigned char *)&data.u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
        sizeof(wifi_hal_capability_t));

    data.u.decoded.num_radios = getNumberRadios();

    if (webconfig_encode(&ctrl->webconfig, &data, webconfig_subdoc_type_mac_filter) ==
        webconfig_error_none) {

        memset(&rdata, 0, sizeof(raw_data_t));
        rdata.data_type = bus_data_type_string;
        rdata.raw_data.bytes = (void *)data.u.encoded.raw;

        status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, opts, &rdata);

        wifi_util_info_print(WIFI_DMCLI, "%s: ACL DML cache encoded successfully  \n",
            __FUNCTION__);
    } else {
        wifi_util_error_print(WIFI_DMCLI, "%s: ACL DML cache encode failed  \n", __FUNCTION__);
    }

    webconfig_data_free(&data);

    return status;
}

extern void webconf_process_private_vap(const char *enb);
bus_error_t get_private_vap(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t options)
{
    unsigned int len = 0;
    char *pTmp = NULL;
    bus_error_t status = bus_error_success;
    raw_data_t data;

    memset(&data, 0, sizeof(raw_data_t));

    status = get_bus_descriptor()->bus_property_data_get_fn(handle, property, options, &data);
    if (data.data_type != bus_data_type_string) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d bus_property_data_get_fn failed with data_type:%d, status:%d\n", __func__,
            __LINE__, data.data_type, status);
        return status;
    }

    pTmp = (char *)data.raw_data.bytes;
    len = data.raw_data_len;

    if (status != bus_error_success || pTmp == NULL) {
        return status;
    }
    wifi_util_dbg_print(WIFI_CTRL, "%s bus set string len=%d, str: \n%s\n", __FUNCTION__, len,
        pTmp);

    wifidb_print("%s:%d [Start] Current time:[%llu]\r\n", __func__, __LINE__,
        get_current_ms_time());
    webconf_process_private_vap(pTmp);

    return status;
}
extern void webconf_process_home_vap(const char *enb);
bus_error_t get_home_vap(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t options)
{
    char *pTmp = NULL;
    bus_error_t status = bus_error_success;
    raw_data_t data;

    memset(&data, 0, sizeof(raw_data_t));

    status = get_bus_descriptor()->bus_property_data_get_fn(handle, property, options, &data);
    if (data.data_type != bus_data_type_string) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d bus_property_data_get_fn failed with data_type:%d, status:%d\n", __func__,
            __LINE__, data.data_type, status);
        return status;
    }

    pTmp = (char *)data.raw_data.bytes;

    if (status != bus_error_success || pTmp == NULL) {
        return status;
    }

    wifidb_print("%s:%d [Start] Current time:[%llu]\r\n", __func__, __LINE__,
        get_current_ms_time());
    webconf_process_home_vap(pTmp);

    return status;
}

#if defined(RDKB_EXTENDER_ENABLED) || defined(WAN_FAILOVER_SUPPORTED)
static void deviceModeHandler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    int device_mode;
    bus_error_t status = bus_error_success;
    raw_data_t data;

    if (!event) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d recvd event:%s\n", __func__, __LINE__, event->name);

    if (strcmp(event->name, WIFI_DEVICE_MODE) == 0) {

        memset(&data, 0, sizeof(raw_data_t));

        data.data_type = bus_data_type_uint32;

        status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data, NULL);

        device_mode = data.raw_data.u32;

        if (status != bus_error_success) {
            wifi_util_error_print(WIFI_CTRL,
                "%s:%d: bus object data get failed for Event %s, status:%d", __func__, __LINE__,
                subscription->event_name, status);
            return;
        }
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: event:%s: value:%d, status:%d\n", __func__, __LINE__,
            event->name, device_mode, status);
        push_event_to_ctrl_queue(&device_mode, sizeof(device_mode), wifi_event_type_command,
            wifi_event_type_device_network_mode, NULL);

    } else {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: Unsupported event:%s\n", __func__, __LINE__,
            event->name);
    }
}
#endif
static void testDeviceModeHandler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    int device_mode = rdk_dev_mode_type_gw;
    bus_error_t status = bus_error_success;
    raw_data_t data;

    if (!event) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d recvd event:%s\n", __func__, __LINE__, event->name);

    if (strcmp(event->name, TEST_WIFI_DEVICE_MODE) == 0) {

        memset(&data, 0, sizeof(raw_data_t));

        data.data_type = bus_data_type_uint32;

        status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data,
            (char *)subscription->event_name);

        device_mode = data.raw_data.u32;

        if (status != bus_error_success) {
            wifi_util_error_print(WIFI_CTRL,
                "%s:%d: bus object data get failed for Event %s, status:%d", __func__, __LINE__,
                subscription->event_name, status);
            return;
        }
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: event:%s: value:%d, status:%d\n", __func__, __LINE__,
            event->name, device_mode, status);
        push_event_to_ctrl_queue(&device_mode, sizeof(device_mode), wifi_event_type_command,
            wifi_event_type_device_network_mode, NULL);
    } else {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: Unsupported event:%s\n", __func__, __LINE__,
            event->name);
    }
}

static void meshStatusHandler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    raw_data_t data;
    bool mesh_status = false;
    bus_error_t status = bus_error_success;

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d Recvd Event\n", __func__, __LINE__);
    if (!event) {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    if (strcmp(subscription->event_name, MESH_STATUS) != 0) {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d Invalid event received, %s\n", __func__, __LINE__,
            subscription->event_name);
        return;
    }

    memset(&data, 0, sizeof(raw_data_t));

    data.data_type = bus_data_type_boolean;

    status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data, NULL);

    mesh_status = data.raw_data.b;

    if (status != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s", __func__,
            __LINE__, subscription->event_name);
        return;
    }
    push_event_to_ctrl_queue(&mesh_status, sizeof(mesh_status), wifi_event_type_command,
        wifi_event_type_command_mesh_status, NULL);
}

static void eventReceiveHandler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    bool tunnel_status = false;
    char *pTmp = NULL;
    bus_error_t status = bus_error_success;
    raw_data_t data;

    wifi_util_dbg_print(WIFI_CTRL, " %s:%d Recvd Event\n", __func__, __LINE__);
    if (!event) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    if (strcmp(event->name, WIFI_DEVICE_TUNNEL_STATUS) == 0) {

        memset(&data, 0, sizeof(raw_data_t));

        data.data_type = bus_data_type_string;

        status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data, NULL);

        pTmp = (char *)data.raw_data.bytes;

        if (status != bus_error_success) {
            wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s",
                __func__, __LINE__, subscription->event_name);
            return;
        }
        if (pTmp == NULL) {
            wifi_util_error_print(WIFI_CTRL, "%s:%d: Unable to get  value in event:%s\n", __func__,
                __LINE__);
            return;
        }
        if (strcmp(pTmp, "Up") == 0) {
            tunnel_status = true;
        } else if (strcmp(pTmp, "Down") == 0) {
            tunnel_status = false;
        } else {
            wifi_util_error_print(WIFI_CTRL, "%s:%d: Received Unsupported value\n", __func__,
                __LINE__);
            return;
        }
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: event:%s: value:%d\n", __func__, __LINE__,
            event->name, tunnel_status);
    } else {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: Unsupported event:%s\n", __func__, __LINE__,
            event->name);
        return;
    }
    wifi_event_subtype_t ces_t = tunnel_status ? wifi_event_type_xfinity_tunnel_up :
                                                 wifi_event_type_xfinity_tunnel_down;
    push_event_to_ctrl_queue(&tunnel_status, sizeof(tunnel_status), wifi_event_type_command, ces_t,
        NULL);
}

static void frame_802_11_injector_Handler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    bus_error_t status = bus_error_success;
    frame_data_t *data_ptr;
    unsigned char *bus_data;
    unsigned int len = 0;
    frame_data_t frame_data;
    raw_data_t data = { 0 };
    memset(&frame_data, 0, sizeof(frame_data));

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d Recvd Event\n", __func__, __LINE__);
    if (!event) {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    data.data_type = bus_data_type_bytes;
    status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data,
        (char *)subscription->event_name);

    bus_data = (unsigned char *)data.raw_data.bytes;
    len = data.raw_data_len;

    if (status != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s", __func__,
            __LINE__, subscription->event_name);
        return;
    }
    data_ptr = (frame_data_t *)bus_data;
    if (data_ptr != NULL && len != 0) {
        memcpy((uint8_t *)&frame_data.frame.sta_mac, (uint8_t *)&data_ptr->frame.sta_mac,
            sizeof(mac_address_t));
        frame_data.frame.ap_index = data_ptr->frame.ap_index;
        frame_data.frame.len = data_ptr->frame.len;
        frame_data.frame.type = data_ptr->frame.type;
        frame_data.frame.dir = data_ptr->frame.dir;
        frame_data.frame.sig_dbm = data_ptr->frame.sig_dbm;
        frame_data.frame.phy_rate = data_ptr->frame.phy_rate;
        frame_data.frame.data = data_ptr->frame.data;

        memcpy(&frame_data.data, data_ptr->data, data_ptr->frame.len);
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: vap_index:%d len:%d frame_byte:%d\r\n", __func__,
            __LINE__, frame_data.frame.ap_index, len, frame_data.frame.len);
        wifi_util_dbg_print(WIFI_CTRL,
            "%s:%d: frame_data.type:%d frame_data.dir:%d frame_data.sig_dbm:%d phy_rate:%d\r\n",
            __func__, __LINE__, frame_data.frame.type, frame_data.frame.dir,
            frame_data.frame.sig_dbm, frame_data.frame.phy_rate);
#ifdef WIFI_HAL_VERSION_3_PHASE2
        mgmt_wifi_frame_recv(frame_data.frame.ap_index, &frame_data.frame);
#else
#if defined(_XB7_PRODUCT_REQ_)
        mgmt_wifi_frame_recv(frame_data.frame.ap_index, frame_data.frame.sta_mac, frame_data.data,
            frame_data.frame.len, frame_data.frame.type, frame_data.frame.dir,
            frame_data.frame.sig_dbm, frame_data.frame.phy_rate);
#else
        mgmt_wifi_frame_recv(frame_data.frame.ap_index, frame_data.frame.sta_mac, frame_data.data,
            frame_data.frame.len, frame_data.frame.type, frame_data.frame.dir);
#endif
#endif
    }
}

static void wps_test_event_receive_handler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    bus_error_t status = bus_error_success;
    uint32_t vap_index = 0;
    raw_data_t data;

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d Recvd Event\n", __func__, __LINE__);

    if (!event) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d bus event name=%s, status:%d\n", __func__, __LINE__,
        event->name, status);

    memset(&data, 0, sizeof(raw_data_t));

    data.data_type = bus_data_type_uint32;

    status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data,
        (char *)subscription->event_name);
    vap_index = data.raw_data.u32;

    if (status != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s", __func__,
            __LINE__, subscription->event_name);
        return;
    }
    if (wifi_util_is_vap_index_valid(&((wifi_mgr_t *)get_wifimgr_obj())->hal_cap.wifi_prop,
            (int)vap_index)) {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d wifi wps test vap_index:%d\n", __func__, __LINE__,
            vap_index);
        push_event_to_ctrl_queue(&vap_index, sizeof(vap_index), wifi_event_type_command,
            wifi_event_type_command_wps, NULL);
    } else {
        uint32_t max_vaps = MAX_NUM_VAP_PER_RADIO * getNumberRadios();
        wifi_util_error_print(WIFI_CTRL, "%s:%d wifi wps test invalid vap_index:%d max_vap:%d\n",
            __func__, __LINE__, vap_index, max_vaps);
    }
}

#if defined(RDKB_EXTENDER_ENABLED)
static void eth_bh_status_handler(bus_handle_t handle, bus_event_t *event,
    bus_event_sub_t *subscription)
{
    bus_error_t status;
    bool eth_bh_status;
    raw_data_t data;

    if (!event) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d null event\n", __func__, __LINE__);
        return;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d recvd event:%s\n", __func__, __LINE__, event->name);

    if (strcmp(event->name, ETH_BH_STATUS) == 0) {

        memset(&data, 0, sizeof(raw_data_t));

        data.data_type = bus_data_type_boolean;

        status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data, NULL);
        eth_bh_status = data.raw_data.b;

        if (status != bus_error_success) {
            wifi_util_error_print(WIFI_CTRL, "%s:%d: bus object data get failed for Event %s",
                __func__, __LINE__, subscription->event_name);
            return;
        }
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d: event: %s value: %d\n", __func__, __LINE__,
            event->name, eth_bh_status);
        push_event_to_ctrl_queue(&eth_bh_status, sizeof(eth_bh_status), wifi_event_type_command,
            wifi_event_type_eth_bh_status, NULL);
    } else {
        wifi_util_error_print(WIFI_CTRL, "%s:%d: unsupported event: %s\n", __func__, __LINE__,
            event->name);
    }
}

static int eth_bh_status_notify()
{
    bool eth_bh_status;
    wifi_ctrl_t *ctrl;
    int rc = bus_error_success;
    raw_data_t data;
    memset(&data, 0, sizeof(raw_data_t));

    ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    rc = get_bus_descriptor()->bus_data_get_fn(&ctrl->handle, ETH_BH_STATUS, &data);
    if (data.data_type != bus_data_type_boolean) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d '%s' bus_data_get_fn failed with data_type:0x%x, rc:%\n", __func__, __LINE__,
            LAST_REBOOT_REASON_NAMESPACE, data.data_type, rc);
        return rc;
    }

    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d bus_geti_fn failed for [%s] with error [%d]\n",
            __func__, __LINE__, ETH_BH_STATUS, rc);
        return RETURN_ERR;
    }

    eth_bh_status = data.raw_data.b;
    wifi_util_dbg_print(WIFI_CTRL, "%s:%d: event: %s value: %d\n", __func__, __LINE__,
        ETH_BH_STATUS, eth_bh_status);
    push_event_to_ctrl_queue(&eth_bh_status, sizeof(eth_bh_status), wifi_event_type_command,
        wifi_event_type_eth_bh_status, NULL);

    return RETURN_OK;
}
#endif

void speed_test_handler(bus_handle_t handle, bus_event_t *event, bus_event_sub_t *subscription)
{
#if DML_SUPPORT
    speed_test_data_t speed_test_data = { 0 };
    bus_error_t status;
    raw_data_t data;

    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    if (!event) {
        wifi_util_dbg_print(1, "%s: %d Invalid event received\n", __func__, __LINE__);
        return;
    }

    memset(&data, 0, sizeof(raw_data_t));
    data.data_type = bus_data_type_uint32;
    status = get_bus_descriptor()->bus_object_data_get_fn(&handle, event->data, &data, "value");

    if ((status != bus_error_success)) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d: bus object data get failed for Event %s, status:%d", __func__, __LINE__,
            subscription->event_name, status);
        return;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s: %d event name : [%s] Data received : [%u], status:%d\n",
        __func__, __LINE__, event->name, data.raw_data.u32, status);

    if ((strcmp(subscription->event_name, SPEEDTEST_STATUS)) == 0) {
        ctrl->speed_test_running = data.raw_data.u32;
    } else if ((strcmp(subscription->event_name, SPEEDTEST_SUBSCRIBE)) == 0) {
        ctrl->speed_test_timeout = data.raw_data.u32;
    }
    speed_test_data.speed_test_running = ctrl->speed_test_running;
    speed_test_data.speed_test_timeout = ctrl->speed_test_timeout;
    push_event_to_ctrl_queue(&speed_test_data, sizeof(speed_test_data_t),
        wifi_event_type_speed_test, 0, NULL);
#endif
    return;
}

void update_speedtest_tout_value()
{
    char const *name = SPEEDTEST_SUBSCRIBE;
    int rc = bus_error_success;
    raw_data_t data;
    memset(&data, 0, sizeof(raw_data_t));

    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    if (ctrl == NULL) {
        wifi_util_dbg_print(WIFI_CTRL, "%s:%d NULL Pointer \n", __func__, __LINE__);
        return;
    }

    rc = get_bus_descriptor()->bus_data_get_fn(&ctrl->handle, name, &data);
    if (data.data_type != bus_data_type_uint32) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d '%s' bus_data_get_fn failed with data_type:0x%x, rc:%\n", __func__, __LINE__,
            LAST_REBOOT_REASON_NAMESPACE, data.data_type, rc);
        return;
    }

    if (rc != bus_error_success) {
        wifi_util_dbg_print(WIFI_CTRL, "%s: %d bus_data_get_fn failed for %s with error %d\n",
            __func__, __LINE__, name, rc);
        return;
    }

    ctrl->speed_test_timeout = (int)data.raw_data.u32;
    wifi_util_dbg_print(WIFI_CTRL, "%s: %d Init time speedtest timeout  : %d\n", __func__, __LINE__,
        ctrl->speed_test_timeout);
}

void event_receive_subscription_handler(bus_handle_t handle, bus_event_sub_t *sub,
    bus_error_t error)
{
    (void)handle;
    if (sub) {
        wifi_util_dbg_print(WIFI_CTRL, "%s: %d event name (%s) subscribe %s\n", __func__, __LINE__,
            sub->event_name, error == bus_error_success ? "success" : "failed");
        if ((error == bus_error_success) && ((strcmp(sub->event_name, SPEEDTEST_SUBSCRIBE)) == 0)) {
            update_speedtest_tout_value();
        }
    }
}

void bus_subscribe_events(wifi_ctrl_t *ctrl)
{
    wifi_bus_desc_t *bus_desc = get_bus_descriptor();

    bus_event_sub_t bus_marker_events[] = {
        { WIFI_NORMALIZED_RSSI_LIST, NULL, 0, 0, MarkerListConfigHandler, NULL, NULL, NULL, false },
        { WIFI_SNR_LIST,             NULL, 0, 0, MarkerListConfigHandler, NULL, NULL, NULL, false },
        { WIFI_CLI_STAT_LIST,        NULL, 0, 0, MarkerListConfigHandler, NULL, NULL, NULL, false },
        { WIFI_TxRx_RATE_LIST,       NULL, 0, 0, MarkerListConfigHandler, NULL, NULL, NULL, false },
        { SPEEDTEST_STATUS,          NULL, 0, 0, speed_test_handler,      NULL, NULL, NULL, false },
        { SPEEDTEST_SUBSCRIBE,       NULL, 0, 0, speed_test_handler,      NULL, NULL, NULL, false },
    };

    int consumer_app_file = -1;
    char file_name[512] = "/tmp/wifi_webconfig_consumer_app";
    consumer_app_file = access(file_name, F_OK);

    if (consumer_app_file == 0 && ctrl->bus_events_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, WIFI_WAN_FAILOVER_TEST, wan_failover_handler,
                NULL, 0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe
            // failed\n",__FUNCTION__, __LINE__, WIFI_WAN_FAILOVER_TEST);
        } else {
            ctrl->bus_events_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, WIFI_WAN_FAILOVER_TEST);
        }
    }

    if (ctrl->marker_list_config_subscribed == false) {
        if (bus_desc->bus_event_subs_ex_async_fn(&ctrl->handle, bus_marker_events,
                ARRAY_SIZE(bus_marker_events), event_receive_subscription_handler,
                0) != bus_error_success) {
        } else {
            ctrl->marker_list_config_subscribed = true;
            wifi_util_dbg_print(WIFI_CTRL, "%s:%d bus: bus event subscribe success\n", __FUNCTION__,
                __LINE__);
        }
    }

#if defined(GATEWAY_FAILOVER_SUPPORTED)
    if (ctrl->active_gateway_check_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, WIFI_ACTIVE_GATEWAY_CHECK,
                activeGatewayCheckHandler, NULL, 0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe
            // failed\n",__FUNCTION__, __LINE__, WIFI_ACTIVE_GATEWAY_CHECK);
        } else {
            ctrl->active_gateway_check_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, WIFI_ACTIVE_GATEWAY_CHECK);
        }
    }
#endif

    if (consumer_app_file == 0 && ctrl->tunnel_events_subscribed == false) {
        // TODO - what's the namespace for the event
        int rc = bus_desc->bus_event_subs_fn(&ctrl->handle, "TunnelStatus", hotspotTunnelHandler,
            NULL, 0);
        if (rc != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL,"%s:%d TunnelStatus subscribe Failed, rc:
            // %d\n",__FUNCTION__, __LINE__, rc);
        } else {
            ctrl->tunnel_events_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d TunnelStatus subscribe success, rc: %d\n",
                __FUNCTION__, __LINE__, rc);
        }
    }

    if (ctrl->mesh_status_subscribed == false) {
        int rc = bus_desc->bus_event_subs_fn(&ctrl->handle, MESH_STATUS, meshStatusHandler, NULL,
            0);
        if (rc != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL,"%s:%d MeshStatus subscribe Failed, rc:
            // %d\n",__FUNCTION__, __LINE__, rc);
        } else {
            ctrl->mesh_status_subscribed = true;
            wifi_util_dbg_print(WIFI_CTRL, "%s:%d MeshStatus subscribe success, rc: %d\n",
                __FUNCTION__, __LINE__, rc);
        }
    }

#if defined(RDKB_EXTENDER_ENABLED) || defined(WAN_FAILOVER_SUPPORTED)
    if (ctrl->device_mode_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, WIFI_DEVICE_MODE, deviceModeHandler, NULL,
                0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL,"%s:%d bus: bus event:%s subscribe
            // failed\n",__FUNCTION__, __LINE__, WIFI_DEVICE_MODE);
        } else {
            ctrl->device_mode_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, WIFI_DEVICE_MODE);
        }
    }
#endif

    if (ctrl->device_tunnel_status_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, WIFI_DEVICE_TUNNEL_STATUS,
                eventReceiveHandler, NULL, 0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL,"%s:%d bus: bus event:%s subscribe
            // failed\n",__FUNCTION__, __LINE__, WIFI_DEVICE_TUNNEL_STATUS);
        } else {
            ctrl->device_tunnel_status_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, WIFI_DEVICE_TUNNEL_STATUS);
        }
    }

    if (consumer_app_file == 0 && ctrl->device_wps_test_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, BUS_WIFI_WPS_PIN_START,
                wps_test_event_receive_handler, NULL, 0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL,"%s:%d bus: bus event:%s subscribe
            // failed\n",__FUNCTION__, __LINE__, BUS_WIFI_WPS_PIN_START);
        } else {
            ctrl->device_wps_test_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, BUS_WIFI_WPS_PIN_START);
        }
    }

    if (consumer_app_file == 0 && ctrl->test_device_mode_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, TEST_WIFI_DEVICE_MODE, testDeviceModeHandler,
                NULL, 0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL,"%s:%d bus: bus event:%s subscribe
            // failed\n",__FUNCTION__, __LINE__, TEST_WIFI_DEVICE_MODE);
        } else {
            ctrl->test_device_mode_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, TEST_WIFI_DEVICE_MODE);
        }
    }

    if (consumer_app_file == 0 && ctrl->frame_802_11_injector_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, WIFI_FRAME_INJECTOR_TO_ONEWIFI,
                frame_802_11_injector_Handler, NULL, 0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL,"%s:%d bus: bus event:%s subscribe
            // failed\n",__FUNCTION__, __LINE__, WIFI_FRAME_INJECTOR_TO_ONEWIFI);
        } else {
            ctrl->frame_802_11_injector_subscribed = true;
            wifi_util_dbg_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, WIFI_FRAME_INJECTOR_TO_ONEWIFI);
        }
    }

#if defined(RDKB_EXTENDER_ENABLED)
    if (ctrl->eth_bh_status_subscribed == false) {
        if (bus_desc->bus_event_subs_fn(&ctrl->handle, ETH_BH_STATUS, eth_bh_status_handler, NULL,
                0) != bus_error_success) {
            // wifi_util_dbg_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe failed\n",
            // __FUNCTION__,
            //     __LINE__, ETH_BH_STATUS);
        } else {
            ctrl->eth_bh_status_subscribed = true;
            wifi_util_info_print(WIFI_CTRL, "%s:%d bus: bus event:%s subscribe success\n",
                __FUNCTION__, __LINE__, ETH_BH_STATUS);
            eth_bh_status_notify();
        }
    }
#endif
}

bus_error_t get_sta_connection_timeout(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    bus_error_t status = bus_error_success;
    vap_svc_t *ext_svc;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    char *name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    raw_data_t rdata;

    if (name == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d property name is not found\r\n", __FUNCTION__,
            __LINE__);
        return bus_error_invalid_input;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s bus: bus property=%s\n", __FUNCTION__, name);

    ext_svc = get_svc_by_type(ctrl, vap_svc_type_mesh_ext);
    if (ext_svc != NULL) {
        if (strcmp(name, WIFI_STA_SELFHEAL_CONNECTION_TIMEOUT) == 0) {

            memset(&rdata, 0, sizeof(raw_data_t));
            rdata.data_type = bus_data_type_boolean;
            rdata.raw_data.b = ext_svc->u.ext.selfheal_status;

            status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options,
                &rdata);
        }
    }

    return status;
}

bus_error_t get_sta_attribs(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    bus_error_t status = bus_error_success;
    unsigned int index, vap_index = 0, i;
    char extension[64] = { 0 };
    wifi_mgr_t *mgr = (wifi_mgr_t *)get_wifimgr_obj();
    wifi_vap_info_map_t *vap_map;
    wifi_sta_conn_info_t sta_conn_info;
    memset(&sta_conn_info, 0, sizeof(wifi_sta_conn_info_t));
    wifi_interface_name_t *l_interface_name;
    mac_address_t l_bssid = { 0 };
    memset(l_bssid, 0, sizeof(l_bssid));
    raw_data_t rdata;

    char *name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    if (name == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d property name is not found\r\n", __FUNCTION__,
            __LINE__);
        return bus_error_invalid_input;
    }
    wifi_util_dbg_print(WIFI_CTRL, "%s bus property=%s\n", __FUNCTION__, name);

    sscanf(name, "Device.WiFi.STA.%d.%s", &index, extension);
    if (index > getNumberRadios()) {
        wifi_util_error_print(WIFI_CTRL, "%s Invalid index %d\n", __FUNCTION__, index);
        return bus_error_invalid_operation;
    }

    vap_map = &mgr->radio_config[(index - 1)].vaps.vap_map;
    vap_index = get_sta_vap_index_for_radio(&mgr->hal_cap.wifi_prop, index - 1);

    if (strcmp(extension, "Connection.Status") == 0) {
        for (i = 0; i < vap_map->num_vaps; i++) {
            if (vap_map->vap_array[i].vap_index == vap_index) {
                sta_conn_info.connect_status = vap_map->vap_array[i].u.sta_info.conn_status;
                memcpy(sta_conn_info.bssid, vap_map->vap_array[i].u.sta_info.bssid,
                    sizeof(vap_map->vap_array[i].u.sta_info.bssid));
                break;
            }
        }

        memset(&rdata, 0, sizeof(raw_data_t));
        rdata.data_type = bus_data_type_bytes;
        rdata.raw_data.bytes = (void *)&sta_conn_info;
        rdata.raw_data_len = sizeof(wifi_sta_conn_info_t);

        status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);

    } else if (strcmp(extension, "Bssid") == 0) {
        for (i = 0; i < vap_map->num_vaps; i++) {
            if (vap_map->vap_array[i].vap_index == vap_index) {
                memcpy(l_bssid, vap_map->vap_array[i].u.sta_info.bssid, sizeof(l_bssid));
                break;
            }
        }

        memset(&rdata, 0, sizeof(raw_data_t));
        rdata.data_type = bus_data_type_bytes;
        rdata.raw_data.bytes = (void *)l_bssid;
        rdata.raw_data_len = sizeof(mac_address_t);

        status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);

    } else if (strcmp(extension, "InterfaceName") == 0) {
        l_interface_name = get_interface_name_for_vap_index(vap_index, &mgr->hal_cap.wifi_prop);

        memset(&rdata, 0, sizeof(raw_data_t));
        rdata.data_type = bus_data_type_string;
        rdata.raw_data.bytes = (void *)l_interface_name;

        status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options, &rdata);
    }

    return status;
}

bus_error_t set_sta_attribs(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t opts)
{
    char *name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    if (name == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d property name is not found\r\n", __FUNCTION__,
            __LINE__);
        return bus_error_invalid_input;
    }

    (void)handle;
    (void)opts;

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d handler\r\n", __FUNCTION__, __LINE__);
    wifi_util_dbg_print(WIFI_CTRL, "%s:%d: setHandler1 called: property=%s\n", __func__, __LINE__,
        name);
    return bus_error_success;
}

bus_error_t events_STAtable_removerowhandler(bus_handle_t *handle, char const *rowName)
{
    UNREFERENCED_PARAMETER(handle);

    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    ctrl->sta_tree_instance_num--;

    wifi_util_info_print(WIFI_CTRL, "%s() called:\n\t rowName=%s: instance_num:%d\n", __func__,
        rowName, ctrl->sta_tree_instance_num);

    return bus_error_success;
}

bus_error_t events_STAtable_addrowhandler(bus_handle_t *handle, char const *tableName,
    char const *aliasName, uint32_t *instNum)
{
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d: tableAddRowHandler1 called. tableName=%s, aliasName=%s\n",
        __FUNCTION__, __LINE__, tableName, aliasName);

    *instNum = ++ctrl->sta_tree_instance_num;
    wifi_util_dbg_print(WIFI_CTRL,"%s:%d instance_num:%d\r\n",__func__, __LINE__, ctrl->sta_tree_instance_num);

    return bus_error_success;
}

static event_bus_element_t *events_getEventElement(char *eventName)
{
    int i;
    event_bus_element_t *event;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    int count = queue_count(ctrl->events_bus_data.events_bus_queue);

    if (count == 0) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        event = queue_peek(ctrl->events_bus_data.events_bus_queue, i);
        if ((event != NULL) && (strncmp(event->name, eventName, MAX_EVENT_NAME_SIZE) == 0)) {
            return event;
        }
    }
    return NULL;
}

bus_error_t eventSubHandler(bus_handle_t *handle, bus_event_sub_action_t action,
    char const *eventName, bus_filter_t filter, int32_t interval, bool *autoPublish)
{
    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(filter);

    *autoPublish = false;
    wifi_util_dbg_print(WIFI_CTRL,
        "%s:%d eventSubHandler called: action=%s\n eventName=%s autoPublish:%d interval:%d\n",
        __func__, __LINE__, action == bus_event_action_subscribe ? "subscribe" : "unsubscribe",
        eventName, *autoPublish, interval);

    unsigned int idx = 0;
    int ret = 0;
    event_bus_element_t *event;
    char *telemetry_start = NULL;
    char *telemetry_cancel = NULL;
    char tmp[128] = { 0 };
    unsigned int vap_array_index;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    events_bus_data_t *events_bus_data = &(ctrl->events_bus_data);
    const char *wifi_log = "/rdklogs/logs/WiFilog.txt.0";

    pthread_mutex_lock(&events_bus_data->events_bus_lock);
    event = events_getEventElement((char *)eventName);
    if (event != NULL) {
        switch (event->type) {
        case wifi_event_monitor_diagnostics:
            idx = event->idx;
            getVAPArrayIndexFromVAPIndex((unsigned int)idx - 1, &vap_array_index);
            if (action == bus_event_action_subscribe) {
                if (interval < MIN_DIAG_INTERVAL) {
                    get_formatted_time(tmp);
                    wifi_util_dbg_print(WIFI_CTRL, "WiFi_DiagData_SubscriptionFailed %d\n", idx);
                    write_to_file(wifi_log, "%s WiFi_DiagData_SubscriptionFailed %d\n", tmp, idx);

                    pthread_mutex_unlock(&events_bus_data->events_bus_lock);
                    wifi_util_dbg_print(WIFI_CTRL, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                    return bus_error_general;
                }
                if (events_bus_data->diag_events_json_buffer[vap_array_index] == NULL) {
                    memset(tmp, 0, sizeof(tmp));
                    get_formatted_time(tmp);
                    events_bus_data->diag_events_json_buffer[vap_array_index] = (char *)malloc(
                        CLIENTDIAG_JSON_BUFFER_SIZE * (sizeof(char)) * BSS_MAX_NUM_STATIONS);
                    if (events_bus_data->diag_events_json_buffer[vap_array_index] == NULL)
                        if (events_bus_data->diag_events_json_buffer[vap_array_index] == NULL) {
                            wifi_util_dbg_print(WIFI_CTRL, "WiFi_DiagData_SubscriptionFailed %d\n",
                                idx);
                            write_to_file(wifi_log, "%s WiFi_DiagData_SubscriptionFailed %d\n", tmp,
                                idx);
                            pthread_mutex_unlock(&events_bus_data->events_bus_lock);
                            wifi_util_dbg_print(WIFI_CTRL, "Exit %s: Event %s\n", __FUNCTION__,
                                eventName);
                            return bus_error_general;
                        }
                    memset(events_bus_data->diag_events_json_buffer[vap_array_index], 0,
                        (CLIENTDIAG_JSON_BUFFER_SIZE * (sizeof(char)) * BSS_MAX_NUM_STATIONS));
                    snprintf(events_bus_data->diag_events_json_buffer[vap_array_index],
                        CLIENTDIAG_JSON_BUFFER_SIZE * (sizeof(char)) * BSS_MAX_NUM_STATIONS,
                        "{"
                        "\"Version\":\"1.0\","
                        "\"AssociatedClientsDiagnostics\":["
                        "{"
                        "\"VapIndex\":\"%d\","
                        "\"AssociatedClientDiagnostics\":[]"
                        "}"
                        "]"
                        "}",
                        idx);
                }
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                wifi_util_dbg_print(WIFI_CTRL, "WiFi_DiagData_SubscriptionStarted %d\n", idx);
                write_to_file(wifi_log, "%s WiFi_DiagData_SubscriptionStarted %d\n", tmp, idx);

                event->num_subscribers++;
                event->subscribed = TRUE;

                // unlock event mutex before updating monitor data to avoid deadlock
                pthread_mutex_unlock(&events_bus_data->events_bus_lock);

                ret = diagdata_set_interval(interval, idx - 1);

                if (ret == RETURN_ERR) {
                    wifi_util_error_print(WIFI_CTRL, "%s:%d Failed to send event %s\n",
                        __FUNCTION__, __LINE__, eventName);
                    return bus_error_general;
                }

                wifi_util_dbg_print(WIFI_CTRL, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                return bus_error_success;
            } else {
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                wifi_util_dbg_print(WIFI_CTRL, "WiFi_DiagData_SubscriptionCancelled %d\n", idx);
                write_to_file(wifi_log, "%s WiFi_DiagData_SubscriptionCancelled %d\n", tmp, idx);

                event->num_subscribers--;
                if (event->num_subscribers == 0) {
                    event->subscribed = FALSE;
                    if (events_bus_data->diag_events_json_buffer[vap_array_index] != NULL) {
                        free(events_bus_data->diag_events_json_buffer[vap_array_index]);
                        events_bus_data->diag_events_json_buffer[vap_array_index] = NULL;
                    }
                    // unlock event mutex before updating monitor data to avoid deadlock
                    pthread_mutex_unlock(&events_bus_data->events_bus_lock);

                    ret = diagdata_set_interval(0, idx - 1);

                    if (ret == RETURN_ERR) {
                        wifi_util_error_print(WIFI_CTRL, "%s:%d Failed to send event %s\n",
                            __FUNCTION__, __LINE__, eventName);
                        return bus_error_general;
                    }
                    wifi_util_dbg_print(WIFI_CTRL, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                    return bus_error_success;
                }
            }
            break;

        case wifi_event_monitor_connect:
        case wifi_event_monitor_disconnect:
        case wifi_event_monitor_deauthenticate:
            idx = event->idx;
            if (event->type == wifi_event_monitor_connect) {
                telemetry_start = "WiFi_deviceConnected_SubscriptionStarted";
                telemetry_cancel = "WiFi_deviceConnected_SubscriptionCancelled";
            } else if (event->type == wifi_event_monitor_disconnect) {
                telemetry_start = "WiFi_deviceDisconnected_SubscriptionStarted";
                telemetry_cancel = "WiFi_deviceDisconnected_SubscriptionCancelled";
            } else {
                telemetry_start = "WiFi_deviceDeauthenticated_SubscriptionStarted";
                telemetry_cancel = "WiFi_deviceDeauthenticated_SubscriptionCancelled";
            }
            if (action == bus_event_action_subscribe) {
                event->num_subscribers++;
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_log, "%s %s %d\n", tmp, telemetry_start, idx);
                wifi_util_dbg_print(WIFI_CTRL, "%s %d\n", telemetry_start, idx);
                event->subscribed = TRUE;
            } else {
                wifi_util_dbg_print(WIFI_CTRL, "%s  %d\n", telemetry_cancel, idx);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_log, "%s %s %d\n", tmp, telemetry_cancel, idx);
                event->num_subscribers--;
                if (event->num_subscribers == 0) {
                    event->subscribed = FALSE;
                }

            }
            break;
        
	default:
            wifi_util_dbg_print(WIFI_CTRL, "%s(): Invalid event type\n", __FUNCTION__);
            break;
        }
    }
    pthread_mutex_unlock(&events_bus_data->events_bus_lock);
    wifi_util_dbg_print(WIFI_CTRL, "Exit %s: Event %s\n", __FUNCTION__, eventName);

    return bus_error_success;
}

bus_error_t ap_get_handler(bus_handle_t *handle, bus_property_t property,
    bus_get_handler_options_t options)
{
    char const *name;
    unsigned int idx = 0;
    int ret;
    bus_error_t status = bus_error_success;
    unsigned int vap_array_index;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    events_bus_data_t *events_bus_data = &(ctrl->events_bus_data);
    raw_data_t rdata;

    name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    if (!name) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d property name is not found\r\n", __FUNCTION__,
            __LINE__);
        return bus_error_invalid_input;
    }

    pthread_mutex_lock(&events_bus_data->events_bus_lock);
    wifi_util_dbg_print(WIFI_CTRL, "%s(): %s\n", __FUNCTION__, name);

    ret = sscanf(name, "Device.WiFi.AccessPoint.%d.X_RDK_DiagData", &idx);
    if (ret == 1 && idx > 0 && idx <= MAX_VAP) {
        getVAPArrayIndexFromVAPIndex((unsigned int)idx - 1, &vap_array_index);
        if (events_bus_data->diag_events_json_buffer[vap_array_index] != NULL) {
            memset(&rdata, 0, sizeof(raw_data_t));
            rdata.data_type = bus_data_type_string;
            rdata.raw_data.bytes =
                (void *)events_bus_data->diag_events_json_buffer[vap_array_index];

            status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options,
                &rdata);
        } else {
            // unlock event mutex before updating monitor data to avoid deadlock
            pthread_mutex_unlock(&events_bus_data->events_bus_lock);
            char *harvester_buf[MAX_VAP];
            harvester_buf[vap_array_index] = (char *)malloc(
                CLIENTDIAG_JSON_BUFFER_SIZE * (sizeof(char)) * BSS_MAX_NUM_STATIONS);
            if (harvester_buf[vap_array_index] == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d Memory allocation failed\n", __func__,
                    __LINE__);
                return bus_error_general;
            }
            wifi_util_error_print(WIFI_CTRL, "%s %d vap index : %u\n", __func__, __LINE__,
                vap_array_index);
            int res = harvester_get_associated_device_info(vap_array_index, harvester_buf);
            if (res < 0) {
                wifi_util_error_print(WIFI_CTRL, "%s %d Associated Device Info collection failed\n",
                    __func__, __LINE__);
                if (harvester_buf[vap_array_index] != NULL) {
                    wifi_util_error_print(WIFI_CTRL, "%s %d Freeing Harvester Memory\n", __func__,
                        __LINE__);
                    free(harvester_buf[vap_array_index]);
                    harvester_buf[vap_array_index] = NULL;
                }
                return bus_error_general;
            }
            pthread_mutex_lock(&events_bus_data->events_bus_lock);
            memset(&rdata, 0, sizeof(raw_data_t));
            rdata.data_type = bus_data_type_string;
            rdata.raw_data.bytes = (void *)harvester_buf[vap_array_index];

            status = get_bus_descriptor()->bus_property_data_set_fn(handle, property, options,
                &rdata);
            if (harvester_buf[vap_array_index] != NULL) {
                free(harvester_buf[vap_array_index]);
                harvester_buf[vap_array_index] = NULL;
            }
        }

        pthread_mutex_unlock(&events_bus_data->events_bus_lock);
        return status;
    }

    pthread_mutex_unlock(&events_bus_data->events_bus_lock);
    return bus_error_invalid_input;
}

bus_error_t ap_table_addrowhandler(bus_handle_t *handle, char const *tableName,
    char const *aliasName, uint32_t *instNum)
{
    static int instanceCounter = 1;
    wifi_mgr_t *mgr = get_wifimgr_obj();

    event_bus_element_t *event;
    unsigned int vap_index;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    vap_index = VAP_INDEX(mgr->hal_cap, (instanceCounter - 1)) + 1;
    *instNum = vap_index;
    instanceCounter++;

    wifi_util_dbg_print(WIFI_CTRL, "%s(): %s %d\n", __FUNCTION__, tableName, *instNum);

    pthread_mutex_lock(&ctrl->events_bus_data.events_bus_lock);

    // Device.WiFi.AccessPoint.{i}.X_RDK_deviceConnected
    event = (event_bus_element_t *)malloc(sizeof(event_bus_element_t));
    if (event != NULL) {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_deviceConnected", *instNum);
        event->idx = vap_index;
        event->type = wifi_event_monitor_connect;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(ctrl->events_bus_data.events_bus_queue, event);
    }

    // Device.WiFi.AccessPoint.{i}.X_RDK_deviceDisconnected
    event = (event_bus_element_t *)malloc(sizeof(event_bus_element_t));
    if (event != NULL) {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDisconnected", *instNum);
        event->idx = vap_index;
        event->type = wifi_event_monitor_disconnect;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(ctrl->events_bus_data.events_bus_queue, event);
    }
    // Device.WiFi.AccessPoint.{i}.X_RDK_deviceDeauthenticated
    event = (event_bus_element_t *)malloc(sizeof(event_bus_element_t));
    if (event != NULL) {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDeauthenticated", *instNum);
        event->idx = vap_index;
        event->type = wifi_event_monitor_deauthenticate;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(ctrl->events_bus_data.events_bus_queue, event);
    }

    // Device.WiFi.AccessPoint.{i}.X_RDK_DiagData
    event = (event_bus_element_t *)malloc(sizeof(event_bus_element_t));
    if (event != NULL) {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_DiagData", *instNum);
        event->idx = vap_index;
        event->type = wifi_event_monitor_diagnostics;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(ctrl->events_bus_data.events_bus_queue, event);
    }

    pthread_mutex_unlock(&ctrl->events_bus_data.events_bus_lock);
    wifi_util_dbg_print(WIFI_CTRL, "%s(): exit\n", __FUNCTION__);

    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(aliasName);
    return bus_error_success;
}

bus_error_t ap_table_removerowhandler(bus_handle_t *handle, char const *rowName)
{
    int i = 0;
    event_bus_element_t *event;
    bus_error_t ret = bus_error_success;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    int count = queue_count(ctrl->events_bus_data.events_bus_queue);

    wifi_util_dbg_print(WIFI_CTRL, "%s(): %s\n", __FUNCTION__, rowName);

    pthread_mutex_lock(&ctrl->events_bus_data.events_bus_lock);

    while (i < count) {
        event = queue_peek(ctrl->events_bus_data.events_bus_queue, i);
        if ((event != NULL) && (strstr(event->name, rowName) != NULL)) {
            wifi_util_dbg_print(WIFI_CTRL, "%s():event remove from queue %s\n", __FUNCTION__,
                event->name);
            event = queue_remove(ctrl->events_bus_data.events_bus_queue, i);
            if (event) {
                free(event);
            }
            count--;
        } else {
            i++;
        }
    }

    pthread_mutex_unlock(&ctrl->events_bus_data.events_bus_lock);

    return ret;
}

static BOOL events_getSubscribed(char *eventName)
{
    int i;
    event_bus_element_t *event;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    int count = queue_count(ctrl->events_bus_data.events_bus_queue);

    if (count == 0) {
        return FALSE;
    }

    for (i = 0; i < count; i++) {
        event = queue_peek(ctrl->events_bus_data.events_bus_queue, i);
        if ((event != NULL) && (strncmp(event->name, eventName, MAX_EVENT_NAME_SIZE) == 0)) {
            return event->subscribed;
        }
    }
    return FALSE;
}

int events_bus_publish(wifi_event_t *evt)
{
    char eventName[MAX_EVENT_NAME_SIZE];
    int rc;
    unsigned int vap_array_index;
    uint32_t len = 0;
    raw_data_t data;

    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    if (evt == NULL) {
        wifi_util_info_print(WIFI_CTRL, "%s(): Input arguements is NULL\n", __FUNCTION__);
        return 0;
    }

    if (evt->sub_type != wifi_event_monitor_csi) {
        wifi_util_info_print(WIFI_CTRL, "%s(): bus_event_publish_fn Event %d\n", __FUNCTION__,
            evt->sub_type);
    }

    switch (evt->sub_type) {
    case wifi_event_monitor_diagnostics:
        sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_DiagData",
            evt->u.mon_data->ap_index + 1);
        getVAPArrayIndexFromVAPIndex((unsigned int)evt->u.mon_data->ap_index, &vap_array_index);
        if (ctrl->events_bus_data.diag_events_json_buffer[vap_array_index] != NULL) {
            pthread_mutex_lock(&ctrl->events_bus_data.events_bus_lock);
            len = strlen(ctrl->events_bus_data.diag_events_json_buffer[vap_array_index]);
            memset(&data, 0, sizeof(raw_data_t));
            data.data_type = bus_data_type_string;
            data.raw_data.bytes =
                (void *)ctrl->events_bus_data.diag_events_json_buffer[vap_array_index];
            data.raw_data_len = len;

            rc = get_bus_descriptor()->bus_event_publish_fn(&ctrl->handle, eventName, &data);
            pthread_mutex_unlock(&ctrl->events_bus_data.events_bus_lock);
            if (rc != bus_error_success) {
                wifi_util_error_print(WIFI_CTRL, "%s(): bus_event_publish_fn Event failed: %d\n",
                    __FUNCTION__, rc);
            } else {
                wifi_util_dbg_print(WIFI_CTRL, "%s(): device_diagnostics Event %d %s \n",
                    __FUNCTION__, evt->sub_type, eventName);
            }
        }
        break;
    case wifi_event_monitor_connect:
    case wifi_event_monitor_disconnect:
    case wifi_event_monitor_deauthenticate:
        if (evt->sub_type == wifi_event_monitor_connect) {
            sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_deviceConnected",
                evt->u.mon_data->ap_index + 1);
        } else if (evt->sub_type == wifi_event_monitor_disconnect) {
            sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDisconnected",
                evt->u.mon_data->ap_index + 1);
        } else {
            sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDeauthenticated",
                evt->u.mon_data->ap_index + 1);
        }
        if (events_getSubscribed(eventName) == TRUE) {
            pthread_mutex_lock(&ctrl->events_bus_data.events_bus_lock);
            memset(&data, 0, sizeof(raw_data_t));
            data.data_type = bus_data_type_uint8;
            data.raw_data.u8 = evt->u.mon_data->u.dev.sta_mac[0];
            data.raw_data_len = sizeof(evt->u.mon_data->u.dev.sta_mac);

            rc = get_bus_descriptor()->bus_event_publish_fn(&ctrl->handle, eventName, &data);
            pthread_mutex_unlock(&ctrl->events_bus_data.events_bus_lock);
            if (rc != bus_error_success) {
                wifi_util_error_print(WIFI_CTRL, "%s(): bus_event_publish_fn Event failed: %d\n",
                    __FUNCTION__, rc);
            } else {
                wifi_util_dbg_print(WIFI_CTRL, "%s(): Event - %d %s \n", __FUNCTION__,
                    evt->sub_type, eventName);
            }
        }
        break;
    default:
        wifi_util_dbg_print(WIFI_CTRL, "%s(): Invalid event type\n", __FUNCTION__);
        break;
    }

    return 0;
}

bus_error_t get_client_assoc_request_multi(bus_handle_t *handle, char const *methodName,
    bus_object_t inParams, bus_object_t outParams, bus_method_async_handle_t asyncHandle)
{
    bus_error_t status = bus_error_success;
    sta_data_t *sta;
    unsigned int vap_index = 0;
    frame_data_t tmp_data;
    frame_data_t *l_data;
    unsigned int len;
    char vapname[32] = { 0 };
    bm_client_assoc_req mac_addr;
    wifi_platform_property_t *prop = NULL;
    raw_data_t data;

    unsigned char *pTmp;
    memset(&data, 0, sizeof(raw_data_t));
    data.data_type = bus_data_type_bytes;
    status = get_bus_descriptor()->bus_object_data_get_fn(handle, inParams, &data, NULL);
    if (data.data_type != bus_data_type_bytes) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d '%s' bus_object_data_get_fn failed with data_type:0x%x, statuc:%\n", __func__, __LINE__,
            MANAGED_WIFI_BRIDGE, data.data_type, status);
        return status;
    }
    pTmp = data.raw_data.bytes;
    len = data.raw_data_len;

    if (status != bus_error_success) {
        return status;
    }

    if (pTmp == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d hash_map object not found for vap_index:\r\n",
            __func__, __LINE__);
        return bus_error_destination_not_found;
    }
    memcpy(&mac_addr, pTmp, len);
    memset(&tmp_data, 0, sizeof(tmp_data));
    prop = (wifi_platform_property_t *)get_wifi_hal_cap_prop();
    convert_ifname_to_vapname(prop, mac_addr.if_name, vapname, sizeof(vapname));
    vap_index = convert_vap_name_to_index(prop, vapname);

    hash_map_t *sta_map = get_sta_data_map(vap_index);

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d %s,%svap_index:%d\r\n", __func__, __LINE__,
        mac_addr.mac_addr, mac_addr.if_name, vap_index);
    if (sta_map != NULL) {
        sta = (sta_data_t *)hash_map_get(sta_map, mac_addr.mac_addr);
    } else {
        wifi_util_info_print(WIFI_CTRL, "%s:%d , sta_map is null  \n", __func__, __LINE__);
        return bus_error_invalid_input;
    }
    if (sta != NULL) {
        if (sta->assoc_frame_data.msg_data.frame.len != 0) {
            wifi_util_dbg_print(WIFI_CTRL,
                "%s:%d bus_namespace_publish event:%s for vap_index:%d\r\n", __func__, __LINE__,
                ACCESSPOINT_ASSOC_REQ_EVENT, vap_index);
            memcpy(&tmp_data, &sta->assoc_frame_data.msg_data, sizeof(frame_data_t));
            l_data = &tmp_data;

        } else {
            wifi_util_info_print(WIFI_CTRL,
                "%s:%d assoc req frame not found for vap_index:%d: sta_mac:%s time:%ld\r\n",
                __func__, __LINE__, vap_index, mac_addr.mac_addr,
                sta->assoc_frame_data.frame_timestamp);
            return bus_error_invalid_input;
        }
    } else {
        wifi_util_info_print(WIFI_CTRL, "%s:%d , sta is null  \n", __func__, __LINE__);
        return bus_error_invalid_input;
    }

    memset(&data, 0, sizeof(raw_data_t));
    data.data_type = bus_data_type_bytes;
    data.raw_data.bytes = (void *)l_data;
    data.raw_data_len = (sizeof(l_data->frame) + l_data->frame.len);

    status = get_bus_descriptor()->bus_object_data_set_fn(handle, outParams,
        WIFI_CLIENT_GET_ASSOC_REQ, &data);
    return status;
}

bus_error_t set_force_vap_apply(bus_handle_t *handle, bus_property_t property,
    bus_set_handler_options_t options)
{
    unsigned int idx = 0;
    int ret;
    bus_error_t status;
    bool force_apply = false;
    webconfig_subdoc_data_t *data;
    char const *name;
    wifi_mgr_t *mgr = get_wifimgr_obj();
    unsigned int num_of_radios = getNumberRadios();
    unsigned int vap_array_index;
    unsigned int radio_index;
    int subdoc_type;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    raw_data_t rdata;

    name = get_bus_descriptor()->bus_property_get_name_fn(handle, property);
    if (!name) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d property name is not found\r\n", __FUNCTION__,
            __LINE__);
        return bus_error_invalid_input;
    }

    memset(&rdata, 0, sizeof(raw_data_t));

    status = get_bus_descriptor()->bus_property_data_get_fn(handle, property, options, &rdata);
    if (rdata.data_type != bus_data_type_boolean) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d '%s' bus_property_data_get_fn failed with data_type:%d, status:%d\n", __func__,
            __LINE__, name, rdata.data_type, status);
        return status;
    }

    force_apply = rdata.raw_data.b;

    if (status != bus_error_success) {
        return status;
    }

    if (force_apply == false) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d Invalid force apply option\r\n", __func__,
            __LINE__);
        return bus_error_invalid_input;
    }

    ret = sscanf(name, "Device.WiFi.AccessPoint.%d.ForceApply", &idx);
    if (ret == 1 && idx > 0 && idx <= num_of_radios * MAX_NUM_VAP_PER_RADIO) {
        data = (webconfig_subdoc_data_t *)malloc(sizeof(webconfig_subdoc_data_t));
        if (data == NULL) {
            wifi_util_error_print(WIFI_CTRL, "%s:%d Malloc failed for name %s\n", __func__,
                __LINE__, name);
            return bus_error_invalid_input;
        }

        memset(data, 0, sizeof(webconfig_subdoc_data_t));
        memcpy((unsigned char *)&data->u.decoded.radios, (unsigned char *)&mgr->radio_config,
            getNumberRadios() * sizeof(rdk_wifi_radio_t));
        memcpy((unsigned char *)&data->u.decoded.config, (unsigned char *)&mgr->global_config,
            sizeof(wifi_global_config_t));
        memcpy((unsigned char *)&data->u.decoded.hal_cap, (unsigned char *)&mgr->hal_cap,
            sizeof(wifi_hal_capability_t));
        data->u.decoded.num_radios = num_of_radios;

        vap_array_index = convert_vap_index_to_vap_array_index(&mgr->hal_cap.wifi_prop,
            (unsigned int)idx - 1);

        radio_index = getRadioIndexFromAp((unsigned int)idx - 1);

        data->u.decoded.radios[radio_index].vaps.rdk_vap_array[vap_array_index].force_apply =
            force_apply;

        get_subdoc_name_from_vap_index(idx - 1, &subdoc_type);

        if (webconfig_encode(&ctrl->webconfig, data, subdoc_type) != webconfig_error_none) {
            wifi_util_error_print(WIFI_CTRL, "%s:%d Error in encoding radio stats\n", __func__,
                __LINE__);
            free(data);
            return bus_error_invalid_input;
        }

        push_event_to_ctrl_queue((const cJSON *)data->u.encoded.raw,
            (strlen(data->u.encoded.raw) + 1), wifi_event_type_webconfig,
            wifi_event_webconfig_set_data_force_apply, NULL);
        free(data);
        return bus_error_success;
    }
    wifi_util_error_print(WIFI_CTRL, "%s:%d Invalid name : %s\r\n", __func__, __LINE__, name);

    return bus_error_invalid_input;
}

void bus_register_handlers(wifi_ctrl_t *ctrl)
{
    int rc = bus_error_success;
    char *component_name = "WifiCtrl";
    int num_of_radio = getNumberRadios();
    int num_of_vaps = getTotalNumberVAPs(NULL);
    int num_elements;
    bus_data_element_t dataElements[] = {
        { WIFI_WEBCONFIG_DOC_DATA_SOUTH,                  bus_element_type_method,
         { NULL, webconfig_set_subdoc, NULL, NULL, NULL, NULL },                                                             slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_DOC_DATA_NORTH,                  bus_element_type_method,
         { NULL, NULL, NULL, NULL, NULL, NULL },                                                                             slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_INIT_DATA,                       bus_element_type_method,
         { webconfig_init_data_get_subdoc, NULL, NULL, NULL, NULL, NULL },                                                   slow_speed,
         ZERO_TABLE                                                                                                                                   },
        { WIFI_WEBCONFIG_INIT_DML_DATA,                   bus_element_type_method,
         { webconfig_get_dml_subdoc, NULL, NULL, NULL, NULL, NULL },                                                         slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_GET_ASSOC,                       bus_element_type_method,
         { get_assoc_clients_data, NULL, NULL, NULL, NULL, NULL },                                                           slow_speed, ZERO_TABLE   },
        { WIFI_STA_NAMESPACE,                             bus_element_type_table,
         { NULL, NULL, events_STAtable_addrowhandler, events_STAtable_removerowhandler,
                eventSubHandler, NULL },
         slow_speed,                                                                                                                     num_of_radio },
        { WIFI_STA_CONNECT_STATUS,                        bus_element_type_property,
         { get_sta_attribs, set_sta_attribs, NULL, NULL, eventSubHandler, NULL },                                            slow_speed,
         ZERO_TABLE                                                                                                                                   },
        { WIFI_STA_INTERFACE_NAME,                        bus_element_type_property,
         { get_sta_attribs, set_sta_attribs, NULL, NULL, eventSubHandler, NULL },                                            slow_speed,
         ZERO_TABLE                                                                                                                                   },
        { WIFI_STA_CONNECTED_GW_BSSID,                    bus_element_type_property,
         { get_sta_attribs, set_sta_attribs, NULL, NULL, eventSubHandler, NULL },                                            slow_speed,
         ZERO_TABLE                                                                                                                                   },
        { WIFI_BUS_WIFIAPI_COMMAND,                       bus_element_type_method,
         { NULL, set_wifiapi_command, NULL, NULL, NULL, NULL },                                                              slow_speed, ZERO_TABLE   },
        { WIFI_BUS_WIFIAPI_RESULT,                        bus_element_type_event,
         { NULL, NULL, NULL, NULL, wifiapi_event_handler, NULL },                                                            slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_GET_CSI,                         bus_element_type_method,   { NULL, NULL, NULL, NULL, NULL, NULL },
         slow_speed,                                                                                                                     ZERO_TABLE   },
        { WIFI_WEBCONFIG_GET_ACL,                         bus_element_type_method,
         { get_acl_device_data, NULL, NULL, NULL, NULL, NULL },                                                              slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_PRIVATE_VAP,                     bus_element_type_method,
         { NULL, get_private_vap, NULL, NULL, NULL, NULL },                                                                  slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_HOME_VAP,                        bus_element_type_method,
         { NULL, get_home_vap, NULL, NULL, NULL, NULL },                                                                     slow_speed, ZERO_TABLE   },
        { WIFI_BUS_HOTSPOT_UP,                            bus_element_type_event,
         { NULL, NULL, NULL, NULL, hotspot_event_handler, NULL },                                                            slow_speed, ZERO_TABLE   },
        { WIFI_BUS_HOTSPOT_DOWN,                          bus_element_type_event,
         { NULL, NULL, NULL, NULL, hotspot_event_handler, NULL },                                                            slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_KICK_MAC,                        bus_element_type_method,
         { NULL, set_kickassoc_command, NULL, NULL, NULL, NULL },                                                            slow_speed, ZERO_TABLE   },
        { WIFI_WEBCONFIG_GET_NULL_SUBDOC,                 bus_element_type_method,
         { get_null_subdoc_data, NULL, NULL, NULL, NULL, NULL },                                                             slow_speed, ZERO_TABLE   },
        { WIFI_STA_TRIGGER_DISCONNECTION,                 bus_element_type_method,
         { get_sta_disconnection, set_sta_disconnection, NULL, NULL, NULL, NULL },                                           slow_speed,
         ZERO_TABLE                                                                                                                                   },
        { WIFI_STA_SELFHEAL_CONNECTION_TIMEOUT,           bus_element_type_event,
         { get_sta_connection_timeout, NULL, NULL, NULL, NULL, NULL },                                                       slow_speed, ZERO_TABLE   },
        { WIFI_ACCESSPOINT_TABLE,                         bus_element_type_table,
         { NULL, NULL, ap_table_addrowhandler, ap_table_removerowhandler, NULL, NULL },
         slow_speed,                                                                                                                     num_of_vaps  },
        { WIFI_ACCESSPOINT_DEV_CONNECTED,                 bus_element_type_event,
         { NULL, NULL, NULL, NULL, eventSubHandler, NULL },                                                                  slow_speed, ZERO_TABLE   },
        { WIFI_ACCESSPOINT_DEV_DISCONNECTED,              bus_element_type_event,
         { NULL, NULL, NULL, NULL, eventSubHandler, NULL },                                                                  slow_speed, ZERO_TABLE   },
        { WIFI_ACCESSPOINT_DEV_DEAUTH,                    bus_element_type_event,
         { NULL, NULL, NULL, NULL, eventSubHandler, NULL },                                                                  slow_speed, ZERO_TABLE   },
        { WIFI_ACCESSPOINT_DIAGDATA,                      bus_element_type_event,
         { ap_get_handler, NULL, NULL, NULL, eventSubHandler, NULL },                                                        slow_speed, ZERO_TABLE   },
        { WIFI_ACCESSPOINT_FORCE_APPLY,                   bus_element_type_method,
         { NULL, set_force_vap_apply, NULL, NULL, NULL, NULL },                                                              slow_speed, ZERO_TABLE   },
        { ACCESSPOINT_ASSOC_REQ_EVENT,                    bus_element_type_method,
         { NULL, NULL, NULL, NULL, NULL, NULL },                                                                             slow_speed, ZERO_TABLE   },
        { WIFI_CLIENT_GET_ASSOC_REQ,                      bus_element_type_method,
         { NULL, NULL, NULL, NULL, NULL, get_client_assoc_request_multi },                                                    slow_speed, ZERO_TABLE  },
    };
    rc = get_bus_descriptor()->bus_open_fn(&ctrl->handle, component_name);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d bus: bus_open_fn open failed for component:%s, rc:%d\n",
	 __func__, __LINE__, component_name, rc);
        return;
    }

    num_elements = (sizeof(dataElements) / sizeof(bus_data_element_t));
    rc = get_bus_descriptor()->bus_reg_data_element_fn(&ctrl->handle, dataElements, num_elements);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_CTRL, "%s bus: bus_regDataElements failed\n", __FUNCTION__);
    }

    wifi_util_info_print(WIFI_CTRL, "%s bus: bus event register:[%s]:%s\r\n", __FUNCTION__,
        WIFI_STA_2G_VAP_CONNECT_STATUS, WIFI_STA_5G_VAP_CONNECT_STATUS);
    return;
}
