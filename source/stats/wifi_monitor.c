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

#ifdef CCSP_COMMON
#include <telemetry_busmessage_sender.h>
#include "cosa_wifi_apis.h"
#include "ccsp_psm_helper.h"
#endif // CCSP_COMMON
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "collection.h"
#include "wifi_hal.h"
#include "wifi_mgr.h"
#include "wifi_util.h"
#include "wifi_monitor.h"
#include "wifi_blaster.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <time.h>
#include <sys/un.h>
#include <assert.h>
#include <limits.h>
#ifdef CCSP_COMMON
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include "ccsp_base_api.h"
#include "harvester.h"
#include "wifi_passpoint.h"
#include "ccsp_trace.h"
#include "safec_lib_common.h"
#include "ccsp_WifiLog_wrapper.h"
#endif // CCSP_COMMON
#include <sched.h>
#include "scheduler.h"

#ifdef CCSP_COMMON
#include <netinet/tcp.h>    //Provides declarations for tcp header
#include <netinet/ip.h> //Provides declarations for ip header
#include <arpa/inet.h> // inet_addr
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/stat.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include "wifi_events.h"
#include "common/ieee802_11_defs.h"
#ifdef HALW_IMPLEMENTATION
#include <opensync/wifi_halw_api.h>
#endif
#endif // CCSP_COMMON
#include "const.h"
#include "pktgen.h"

#ifndef  UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(_p_)         (void)(_p_)
#endif

#define MIN_MAC_LEN 12
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_FMT_TRIMMED "%02x%02x%02x%02x%02x%02x"
#define MAC_ARG(arg) \
    arg[0], \
    arg[1], \
    arg[2], \
    arg[3], \
    arg[4], \
    arg[5]
#define RADIO_STATS_INTERVAL_MS 30000 //30 seconds

#ifdef CCSP_COMMON
#define NDA_RTA(r) \
  ((struct rtattr *)(((char *)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))

static events_monitor_t g_events_monitor;
static struct timeval csi_prune_timer;

int harvester_get_associated_device_info(int vap_index, char **harvester_buf);

extern void* bus_handle;
extern char g_Subsystem[32];
#define SINGLE_CLIENT_WIFI_AVRO_FILENAME "WifiSingleClient.avsc"
#define DEFAULT_INSTANT_POLL_TIME 5
#define DEFAULT_INSTANT_REPORT_TIME 0
#define MAX_NEIGHBOURS 250

#define DEFAULT_CHANUTIL_LOG_INTERVAL 900
#define RADIO_HEALTH_TELEMETRY_INTERVAL_MS 900000 //15 minutes
#define REFRESH_TASK_INTERVAL_MS 5*60*1000 //5 minutes
#define ASSOCIATED_DEVICE_DIAG_INTERVAL_MS 5000 // 5 seconds
#define CAPTURE_VAP_STATUS_INTERVAL_MS 5000 // 5 seconds
#define UPLOAD_AP_TELEMETRY_INTERVAL_MS 24*60*60*1000 // 24 Hours
#define NEIGHBOR_SCAN_INTERVAL 60*60*1000 //1 Hr
#define NEIGHBOR_SCAN_RESULT_INTERVAL 5000 //5 seconds

#define MIN_TO_MILLISEC 60000
#define SEC_TO_MILLISEC 1000

char *instSchemaIdBuffer = "8b27dafc-0c4d-40a1-b62c-f24a34074914/4388e585dd7c0d32ac47e71f634b579b";
#endif // CCSP_COMMON

static wifi_monitor_t g_monitor_module;
static wifi_actvie_msmt_t g_active_msmt;

#ifdef CCSP_COMMON
static unsigned msg_id = 1000;
static const char *wifi_health_log = "/rdklogs/logs/wifihealth.txt";
static unsigned int vap_up_arr[MAX_VAP]={0};
static unsigned int vap_iteration=0;
static unsigned char vap_nas_status[MAX_VAP]={0};
#if defined (DUAL_CORE_XB3)
static unsigned char erouterIpAddrStr[32];
unsigned char wifi_pushSecureHotSpotNASIP(int apIndex, unsigned char erouterIpAddrStr[]);
#endif
int radio_stats_monitor = 0;
ULONG chan_util_upload_period = 0;
ULONG lastupdatedtime = 0;
ULONG chutil_last_updated_time = 0;
time_t lastpolledtime = 0;

int device_deauthenticated(int apIndex, char *mac, int reason);
int device_associated(int apIndex, wifi_associated_dev_t *associated_dev);
int vapstatus_callback(int apIndex, wifi_vapstatus_t status);
unsigned int get_upload_period  (int);
long get_sys_uptime();
void process_disconnect    (unsigned int ap_index, auth_deauth_dev_t *dev);
static void get_device_flag(char flag[], char *list_name);
static void logVAPUpStatus();
//extern BOOL sWiFiDmlvApStatsFeatureEnableCfg;
BOOL sWiFiDmlvApStatsFeatureEnableCfg = TRUE;//ONE_WIFI
BOOL sWiFiDmlApStatsEnableCfg[WIFI_INDEX_MAX];//ONE_WIFI
INT assocCountThreshold = 0; 
INT assocMonitorDuration = 0;
INT assocGateTime = 0;

INT deauthCountThreshold = 0;
INT deauthMonitorDuration = 0;
INT deauthGateTime = 0;//ONE_WIFI

static int neighscan_task_id = -1;

#if defined (_XB7_PRODUCT_REQ_)
#define FEATURE_CSI_CALLBACK 1
#endif

#if defined (FEATURE_CSI_CALLBACK)
INT process_csi(mac_address_t mac_addr, wifi_csi_data_t  *csi_data);
#endif

void associated_client_diagnostics();
void process_instant_msmt_stop (unsigned int ap_index, instant_msmt_t *msmt);
void process_instant_msmt_start        (unsigned int ap_index, instant_msmt_t *msmt);
void get_self_bss_chan_statistics (int radiocnt , UINT *Tx_perc, UINT  *Rx_perc);
int get_chan_util_upload_period(void);
int process_instant_msmt_monitor(void *arg);
static int refresh_task_period(void *arg);
int upload_radio_chan_util_telemetry(void *arg); 
int associated_device_diagnostics_send_event(void *arg);
static void scheduler_telemetry_tasks(void);
int csi_getCSIData(void * arg);
int csi_sendPingData(void * arg);
static void csi_refresh_session(void);
static int csi_sheduler_enable(void);
static int clientdiag_sheduler_enable(int ap_index);
static void csi_vap_down_update(int ap_idx);
static void csi_disable_client(csi_session_t *r_csi);
int associated_devices_diagnostics(void *arg);
static void upload_client_debug_stats_acs_stats(int apIndex);
static void upload_client_debug_stats_sta_fa_info(int apIndex, sta_data_t *sta);
static void upload_client_debug_stats_sta_fa_lmac_data_stats(int apindex, sta_data_t *sta);
static void upload_client_debug_stats_sta_fa_lmac_mgmt_stats(int apIndex, sta_data_t *sta);
static void upload_client_debug_stats_sta_vap_activity_stats(int apIndex);
static void upload_client_debug_stats_transmit_power_stats(int apIndex);
static void upload_client_debug_stats_chan_stats(int apIndex);
#endif // CCSP_COMMON

#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
static int off_chan_scan_init (void *args);
void off_chan_print_scan_data (unsigned int radio_index, wifi_neighbor_ap2_t *neighbor_result, int array_size);
#define MAX_5G_CHANNELS 25
#define DFS_START 52
#define DFS_END 144
#define OFFCHAN_DEFAULT_NSCAN_IN_SEC 10800
#endif //FEATURE_OFF_CHANNEL_SCAN_5G

#define LINUX_PROC_MEMINFO_FILE  "/proc/meminfo"
#define LINUX_PROC_LOADAVG_FILE  "/proc/loadavg"
#define LINUX_PROC_STAT_FILE     "/proc/stat"

#define WIFI_BLASTER_CPU_THRESHOLD     90   // percentage
#define WIFI_BLASTER_MEM_THRESHOLD     8096 // Kb
#define WIFI_BLASTER_CPU_CALC_PERIOD   1    // seconds
#define WIFI_BLASTER_POST_STEP_TIMEOUT 100  // ms
#ifndef CCSP_COMMON
#define WIFI_BLASTER_WAKEUP_LIMIT 2
#endif // CCSP_COMMON

void deinit_wifi_monitor(void);
void SetBlasterMqttTopic(char *mqtt_topic);
int radio_diagnostics(void *arg);

pktGenFrameCountSamples  *frameCountSample = NULL;

static inline char *to_sta_key    (mac_addr_t mac, sta_key_t key) 
{
    snprintf(key, STA_KEY_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (char *)key;
}

#ifdef CCSP_COMMON
BOOL IsWiFiApStatsEnable(UINT uvAPIndex)
{
    return ((sWiFiDmlApStatsEnableCfg[uvAPIndex]) ? TRUE : FALSE);
}

int harvester_get_associated_device_info(int vap_index, char **harvester_buf)
{
    unsigned int pos = 0, tr_pos = 0;
    sta_data_t *sta_data = NULL;
    if (harvester_buf[vap_index] == NULL) {
        wifi_util_dbg_print(WIFI_MON, "%s %d Harvester Buffer is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    pos = snprintf(harvester_buf[vap_index],
                CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS,
                "{"
                "\"Version\":\"1.0\","
                "\"AssociatedClientsDiagnostics\":["
                "{"
                "\"VapIndex\":\"%d\","
                "\"AssociatedClientDiagnostics\":[",
                (vap_index+1));
    pthread_mutex_lock(&g_monitor_module.data_lock);
    sta_data = hash_map_get_first(g_monitor_module.bssid_data[vap_index].sta_map);
    while (sta_data != NULL) {
        pos += snprintf(&harvester_buf[vap_index][pos],
                (CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS)-pos, "{"
                        "\"MAC\":\"%02x%02x%02x%02x%02x%02x\","
                        "\"DownlinkDataRate\":\"%d\","
                        "\"UplinkDataRate\":\"%d\","
                        "\"BytesSent\":\"%lu\","
                        "\"BytesReceived\":\"%lu\","
                        "\"PacketsSent\":\"%lu\","
                        "\"PacketsRecieved\":\"%lu\","
                        "\"Errors\":\"%lu\","
                        "\"RetransCount\":\"%lu\","
                        "\"Acknowledgements\":\"%lu\","
                        "\"SignalStrength\":\"%d\","
                        "\"SNR\":\"%d\","
                        "\"OperatingStandard\":\"%s\","
                        "\"OperatingChannelBandwidth\":\"%s\","
                        "\"AuthenticationFailures\":\"%d\","
                        "\"AuthenticationState\":\"%d\","
                        "\"Active\":\"%d\","
                        "\"InterferenceSources\":\"%s\","
                        "\"DataFramesSentNoAck\":\"%lu\","
                        "\"RSSI\":\"%d\","
                        "\"MinRSSI\":\"%d\","
                        "\"MaxRSSI\":\"%d\","
                        "\"Disassociations\":\"%u\","
                        "\"Retransmissions\":\"%u\""
                        "},",
                        sta_data->dev_stats.cli_MACAddress[0],
                        sta_data->dev_stats.cli_MACAddress[1],
                        sta_data->dev_stats.cli_MACAddress[2],
                        sta_data->dev_stats.cli_MACAddress[3],
                        sta_data->dev_stats.cli_MACAddress[4],
                        sta_data->dev_stats.cli_MACAddress[5],
                        sta_data->dev_stats.cli_MaxDownlinkRate,
                        sta_data->dev_stats.cli_MaxUplinkRate,
                        sta_data->dev_stats.cli_BytesSent,
                        sta_data->dev_stats.cli_BytesReceived,
                        sta_data->dev_stats.cli_PacketsSent,
                        sta_data->dev_stats.cli_PacketsReceived,
                        sta_data->dev_stats.cli_ErrorsSent,
                        sta_data->dev_stats.cli_RetransCount,
                        sta_data->dev_stats.cli_DataFramesSentAck,
                        sta_data->dev_stats.cli_SignalStrength,
                        sta_data->dev_stats.cli_SNR,
                        sta_data->dev_stats.cli_OperatingStandard,
                        sta_data->dev_stats.cli_OperatingChannelBandwidth,
                        sta_data->dev_stats.cli_AuthenticationFailures,
                        sta_data->dev_stats.cli_AuthenticationState,
                        sta_data->dev_stats.cli_Active,
                        sta_data->dev_stats.cli_InterferenceSources,
                        sta_data->dev_stats.cli_DataFramesSentNoAck,
                        sta_data->dev_stats.cli_RSSI,
                        sta_data->dev_stats.cli_MinRSSI,
                        sta_data->dev_stats.cli_MaxRSSI,
                        sta_data->dev_stats.cli_Disassociations,
                        sta_data->dev_stats.cli_Retransmissions);


        sta_data = hash_map_get_next(g_monitor_module.bssid_data[vap_index].sta_map, sta_data);

    }
    pthread_mutex_unlock(&g_monitor_module.data_lock);
    tr_pos = pos-1;

    snprintf(&harvester_buf[vap_index][tr_pos], (
             CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS)-tr_pos,"]"
             "}"
             "]"
             "}");

    wifi_util_dbg_print(WIFI_MON, "%s %d pos : %u tr_pos : %u Buffer for vap %d updated as %s\n", __func__, __LINE__, pos, tr_pos, vap_index, harvester_buf[vap_index]);
    return RETURN_OK;
}



/* get_self_bss_chan_statistics () will get channel statistics from driver and calculate self bss channel utilization */
void get_self_bss_chan_statistics (int radiocnt , UINT *Tx_perc, UINT  *Rx_perc)
{
    ULONG timediff = 0;
    wifi_channelStats_t chan_stats = {0};
    ULLONG Tx_count = 0, Rx_count = 0;
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    ULONG currentTime = tv_now.tv_sec;
    ULLONG bss_total = 0;
    *Tx_perc = 0;
    *Rx_perc = 0;
    wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(radiocnt);
    if (radioOperation != NULL) {
        chan_stats.ch_number = radioOperation->channel;
        chan_stats.ch_in_pool= TRUE;
        if (wifi_getRadioChannelStats(radiocnt, &chan_stats, 1) == RETURN_OK) {
            timediff = currentTime - g_monitor_module.radio_channel_data[radiocnt].LastUpdatedTime;
            /* if the last poll was within 5 seconds skip the  calculation*/
            if  (timediff > 5 ) {
                g_monitor_module.radio_channel_data[radiocnt].LastUpdatedTime = currentTime;
                if ((g_monitor_module.radio_channel_data[radiocnt].ch_number == chan_stats.ch_number) && 
                        (chan_stats.ch_utilization_busy_tx > g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_tx)) {
                    Tx_count = chan_stats.ch_utilization_busy_tx - g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_tx;
                }
                else {
                    Tx_count = chan_stats.ch_utilization_busy_tx;
                }

                if ((g_monitor_module.radio_channel_data[radiocnt].ch_number == chan_stats.ch_number) && 
                        (chan_stats.ch_utilization_busy_self > g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_self)) {
                    Rx_count =  chan_stats.ch_utilization_busy_self - g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_self;
                }
                else {
                    Rx_count = chan_stats.ch_utilization_busy_self;

                }
            }
            wifi_util_dbg_print(WIFI_MON, "%s: %d Radio %d Current channel %d new stats Tx_self : %llu Rx_self: %llu  \n"
                    ,__FUNCTION__,__LINE__,radiocnt,chan_stats.ch_number,chan_stats.ch_utilization_busy_tx
                    , chan_stats.ch_utilization_busy_self);

            bss_total = Tx_count + Rx_count;
            if (bss_total) {
                *Tx_perc = (UINT)round( (float) Tx_count / bss_total * 100 );
                *Rx_perc = (UINT)round( (float) Rx_count / bss_total * 100 );
            }
            wifi_util_dbg_print(WIFI_MON,"%s: %d Radio %d channel stats Tx_count : %llu Rx_count: %llu Tx_perc: %d Rx_perc: %d\n"
                    ,__FUNCTION__,__LINE__,radiocnt,Tx_count, Rx_count, *Tx_perc, *Rx_perc);
            /* Update prev var for next call */
            g_monitor_module.radio_channel_data[radiocnt].ch_in_pool = chan_stats.ch_in_pool;
            g_monitor_module.radio_channel_data[radiocnt].ch_radar_noise = chan_stats.ch_radar_noise;
            g_monitor_module.radio_channel_data[radiocnt].ch_number = chan_stats.ch_number;
            g_monitor_module.radio_channel_data[radiocnt].ch_noise = chan_stats.ch_noise;
            g_monitor_module.radio_channel_data[radiocnt].ch_max_80211_rssi = chan_stats.ch_max_80211_rssi;
            g_monitor_module.radio_channel_data[radiocnt].ch_non_80211_noise = chan_stats.ch_non_80211_noise;
            g_monitor_module.radio_channel_data[radiocnt].ch_utilization = chan_stats.ch_utilization;
            g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_tx = chan_stats.ch_utilization_busy_tx;
            g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_self = chan_stats.ch_utilization_busy_self;
            g_monitor_module.radio_channel_data[radiocnt].ch_utilization_total = chan_stats.ch_utilization_total;
            g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy = chan_stats.ch_utilization_busy;
            g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_rx = chan_stats.ch_utilization_busy_rx;
            g_monitor_module.radio_channel_data[radiocnt].ch_utilization_busy_ext = chan_stats.ch_utilization_busy_ext;
        }
        else {
            wifi_util_error_print(WIFI_MON, "%s : %d wifi_getRadioChannelStats failed for rdx : %d\n",__func__,__LINE__,radiocnt);
        }
    }
    return;
}

int get_radio_channel_stats(int radio_index, 
                            wifi_channelStats_t *channel_stats_array, 
                            int *array_size)
{
    wifi_channelStats_t chan_stats = {0};

    pthread_mutex_lock(&g_monitor_module.data_lock);
    *array_size = 1;
    chan_stats.ch_in_pool = g_monitor_module.radio_channel_data[radio_index].ch_in_pool;
    chan_stats.ch_radar_noise = g_monitor_module.radio_channel_data[radio_index].ch_radar_noise;
    chan_stats.ch_number = g_monitor_module.radio_channel_data[radio_index].ch_number;
    chan_stats.ch_noise = g_monitor_module.radio_channel_data[radio_index].ch_noise;
    chan_stats.ch_max_80211_rssi = g_monitor_module.radio_channel_data[radio_index].ch_max_80211_rssi;
    chan_stats.ch_non_80211_noise = g_monitor_module.radio_channel_data[radio_index].ch_non_80211_noise;
    chan_stats.ch_utilization = g_monitor_module.radio_channel_data[radio_index].ch_utilization;
    chan_stats.ch_utilization_busy_tx = g_monitor_module.radio_channel_data[radio_index].ch_utilization_busy_tx;
    chan_stats.ch_utilization_busy_self = g_monitor_module.radio_channel_data[radio_index].ch_utilization_busy_self;
    chan_stats.ch_utilization_total = g_monitor_module.radio_channel_data[radio_index].ch_utilization_total;
    chan_stats.ch_utilization_busy = g_monitor_module.radio_channel_data[radio_index].ch_utilization_busy;
    chan_stats.ch_utilization_busy_rx = g_monitor_module.radio_channel_data[radio_index].ch_utilization_busy_rx;
    chan_stats.ch_utilization_busy_ext = g_monitor_module.radio_channel_data[radio_index].ch_utilization_busy_ext;
    memcpy(channel_stats_array, &chan_stats, sizeof(wifi_channelStats_t));
    pthread_mutex_unlock(&g_monitor_module.data_lock);

    return 0;
}

// upload_radio_chan_util_telemetry()  will update the channel stats in telemetry marker
int upload_radio_chan_util_telemetry(void *arg)
{
    static int radiocnt = 0;
    static int total_radiocnt = 0;
    static int new_chan_util_period = 0;
    UINT  Tx_perc = 0, Rx_perc = 0;
    UINT bss_Tx_cu = 0 , bss_Rx_cu = 0;
    char tmp[128] = {0};
    char log_buf[1024] = {0};
    char telemetry_buf[1024] = {0};
    errno_t rc = -1;

    if (total_radiocnt == 0) {
        total_radiocnt = (int)getNumberRadios();
    }
    wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(radiocnt);
    if (radioOperation != NULL) {
        if (radioOperation->enable) {
            get_self_bss_chan_statistics(radiocnt, &Tx_perc, &Rx_perc);

            /* calculate Self bss Tx and Rx channel utilization */

            bss_Tx_cu = (UINT)round( (float) g_monitor_module.radio_data[radiocnt].RadioActivityFactor * Tx_perc / 100 );
            bss_Rx_cu = (UINT)round( (float) g_monitor_module.radio_data[radiocnt].RadioActivityFactor * Rx_perc / 100 );

            wifi_util_dbg_print(WIFI_MON,"%s: channel Statistics results for Radio %d: Activity: %d AFTX : %d AFRX : %d ChanUtil: %d CSTE: %d\n"
                    ,__func__, radiocnt, g_monitor_module.radio_data[radiocnt].RadioActivityFactor
                    ,bss_Tx_cu,bss_Rx_cu,g_monitor_module.radio_data[radiocnt].channelUtil
                    ,g_monitor_module.radio_data[radiocnt].CarrierSenseThreshold_Exceeded);

            // Telemetry:
            // "header":  "CHUTIL_1_split"
            // "content": "CHUTIL_1_split:"
            // "type": "wifihealth.txt",
            rc = sprintf_s(telemetry_buf, sizeof(telemetry_buf), "%d,%d,%d", bss_Tx_cu, bss_Rx_cu, g_monitor_module.radio_data[radiocnt].CarrierSenseThreshold_Exceeded);
            if(rc < EOK) {
                ERR_CHK(rc);
            }
            get_formatted_time(tmp);
            rc = sprintf_s(log_buf, sizeof(log_buf), "%s CHUTIL_%d_split:%s\n", tmp, getPrivateApFromRadioIndex(radiocnt)+1, telemetry_buf);
            if(rc < EOK) {
                ERR_CHK(rc);
            }
            write_to_file(wifi_health_log, log_buf);
            wifi_util_dbg_print(WIFI_MON, "%s", log_buf);

            memset(tmp, 0, sizeof(tmp));
            sprintf(tmp, "CHUTIL_%d_split", getPrivateApFromRadioIndex(radiocnt)+1);
            t2_event_s(tmp, telemetry_buf);
        }
        else {
            wifi_util_dbg_print(WIFI_MON, "%s : %d Radio : %d is not enabled\n",__func__,__LINE__,radiocnt);
        }
    }
    else {
        wifi_util_error_print(WIFI_MON, "%s : %d Failed to get getRadioOperationParam for rdx : %d\n",__func__,__LINE__,radiocnt);
    }

    radiocnt++;
    if (radiocnt >= total_radiocnt) {
        radiocnt = 0;
        new_chan_util_period = get_chan_util_upload_period();
        if((g_monitor_module.curr_chan_util_period != new_chan_util_period) 
                && (new_chan_util_period != 0)) {
            scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.chutil_id, new_chan_util_period*1000);
            g_monitor_module.curr_chan_util_period = new_chan_util_period;
        }
        return TIMER_TASK_COMPLETE;
    }
    return TIMER_TASK_CONTINUE;
}

int radio_health_telemetry_logger(void *arg)
{
    int output_percentage = 0;
    unsigned int i = 0;
    char buff[256] = {0}, tmp[128] = {0}, telemetry_buf[64] = {0}, t_string[5] = {0};
    unsigned long int itr = 0;
    char *t_str = NULL;
    for (i = 0; i < getNumberRadios(); i++) {
        if (g_monitor_module.radio_presence[i] == false) {
           continue;
        }
        memset(buff, 0, sizeof(buff));
        memset(tmp, 0, sizeof(tmp));
        get_formatted_time(tmp);
        wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(i);
        if (radioOperation != NULL) {
        //Printing the utilization of Radio if and only if the radio is enabled
            if (radioOperation->enable) {
                get_radio_channel_utilization(i, &output_percentage);
                snprintf(buff, 256, "%s WIFI_BANDUTILIZATION_%d:%d\n", tmp, i + 1, output_percentage);
                memset(tmp, 0, sizeof(tmp));
                t_str = convert_radio_index_to_band_str_g(i);
                if (t_str != NULL) {
                    strncpy(t_string, t_str, sizeof(t_string) - 1);
                    for (itr=0; itr<strlen(t_string); itr++) {
                        t_string[itr] = toupper(t_string[itr]);
                    }
                    snprintf(tmp, sizeof(tmp), "Wifi_%s_utilization_split", t_string);
                } else {
                    wifi_util_dbg_print(WIFI_MON, "%s-%d Failed to get band for radio Index %d\n", __func__, __LINE__, i);
                    continue; 
                }
                //updating T2 Marker here
                memset(telemetry_buf, 0, sizeof(telemetry_buf));
                snprintf(telemetry_buf, sizeof(telemetry_buf), "%d", output_percentage);
                t2_event_s(tmp, telemetry_buf);
            } else {
                snprintf(buff, 256, "%s Radio_%d is down, so not printing WIFI_BANDUTILIZATION marker", tmp, i + 1);
            }
            write_to_file(wifi_health_log, buff);
        }
    }
    return TIMER_TASK_COMPLETE;
}



int upload_ap_telemetry_data(void *arg)
{
    char buff[1024];
    char tmp[128];
    unsigned int i;
    for (i = 0; i < getNumberRadios(); i++) {
        if (g_monitor_module.radio_presence[i] == false) {
           continue;
        }
        wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(i);
        if (radioOperation != NULL) {
            if (radioOperation->enable) {
                get_formatted_time(tmp);
                snprintf(buff, 1024, "%s WIFI_NOISE_FLOOR_%d:%d\n", tmp, i + 1, g_monitor_module.radio_data[i].NoiseFloor);
                write_to_file(wifi_health_log, buff);
                wifi_util_dbg_print(WIFI_MON, "%s", buff);
            }
        }
    }
    return TIMER_TASK_COMPLETE;
}

BOOL client_fast_reconnect(unsigned int apIndex, char *mac)
{
    extern int assocCountThreshold;
    extern int assocMonitorDuration;
    extern int assocGateTime;
    sta_data_t  *sta;
    hash_map_t  *sta_map;
    struct timeval tv_now;
    unsigned int vap_array_index;

    gettimeofday(&tv_now, NULL);

    if(!assocMonitorDuration) {
        wifi_util_error_print(WIFI_MON, "%s: Client fast reconnection check disabled, assocMonitorDuration:%d \n", __func__, assocMonitorDuration);
        return FALSE;
    }

    wifi_util_dbg_print(WIFI_MON, "%s: Checking for client:%s connection on ap:%d\n", __func__, mac, apIndex);
    getVAPArrayIndexFromVAPIndex(apIndex, &vap_array_index);

    pthread_mutex_lock(&g_monitor_module.data_lock);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;
    str_tolower(mac);
    sta = (sta_data_t *)hash_map_get(sta_map, mac);
    if (sta == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Client:%s could not be found on sta map of ap:%d\n", __func__, mac, apIndex);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return FALSE;
    }

    if(sta->gate_time && (tv_now.tv_sec < sta->gate_time)) {
        wifi_util_dbg_print(WIFI_MON, "%s: Blocking burst client connections for few more seconds\n", __func__);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return TRUE;
    } else {
        wifi_util_dbg_print(WIFI_MON, "%s: processing further\n", __func__);
    }

    wifi_util_dbg_print(WIFI_MON, "%s: assocCountThreshold:%d assocMonitorDuration:%d assocGateTime:%d \n", __func__, assocCountThreshold, assocMonitorDuration, assocGateTime);

    if((tv_now.tv_sec - sta->assoc_monitor_start_time) < assocMonitorDuration) {
        sta->reconnect_count++;
        wifi_util_dbg_print(WIFI_MON, "%s: reconnect_count:%d \n", __func__, sta->reconnect_count);
        if(sta->reconnect_count > (UINT)assocCountThreshold) {
            wifi_util_dbg_print(WIFI_MON, "%s: Blocking client connections for assocGateTime:%d \n", __func__, assocGateTime);
            t2_event_d("SYS_INFO_ClientConnBlock", 1);
            sta->reconnect_count = 0;
            sta->gate_time = tv_now.tv_sec + assocGateTime;
            pthread_mutex_unlock(&g_monitor_module.data_lock);
            return TRUE;
        }
    } else {
        sta->assoc_monitor_start_time = tv_now.tv_sec;
        sta->reconnect_count = 0;
        sta->gate_time = 0;
        wifi_util_dbg_print(WIFI_MON, "%s: resetting reconnect_count and assoc_monitor_start_time \n", __func__);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return FALSE;
    }
    pthread_mutex_unlock(&g_monitor_module.data_lock);
    return FALSE;
}

BOOL client_fast_redeauth(unsigned int apIndex, char *mac)
{
    extern int deauthMonitorDuration;
    extern int deauthGateTime;
    sta_data_t  *sta;
    hash_map_t  *sta_map;
    unsigned int vap_array_index;
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    if(!deauthMonitorDuration) {
        wifi_util_error_print(WIFI_MON, "%s: Client fast deauth check disabled, deauthMonitorDuration:%d \n", __func__, deauthMonitorDuration);
        return FALSE;
    }

    wifi_util_dbg_print(WIFI_MON, "%s: Checking for client:%s deauth on ap:%d\n", __func__, mac, apIndex);

    pthread_mutex_lock(&g_monitor_module.data_lock);
    getVAPArrayIndexFromVAPIndex(apIndex, &vap_array_index);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;
    str_tolower(mac);
    sta = (sta_data_t *)hash_map_get(sta_map, mac);

    if (sta == NULL  ) {
        wifi_util_dbg_print(WIFI_MON, "%s: Client:%s could not be found on sta map of ap:%d,  Blocking client deauth notification\n", __func__, mac, apIndex);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return TRUE;
    }

    if(sta->deauth_gate_time && (tv_now.tv_sec < sta->deauth_gate_time)) {
        wifi_util_dbg_print(WIFI_MON, "%s: Blocking burst client deauth for few more seconds\n", __func__);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return TRUE;
    } else {
        wifi_util_dbg_print(WIFI_MON, "%s: processing further\n", __func__);
    }

    wifi_util_dbg_print(WIFI_MON, "%s: deauthCountThreshold:%d deauthMonitorDuration:%d deauthGateTime:%d \n", __func__, deauthCountThreshold, deauthMonitorDuration, deauthGateTime);

    if((tv_now.tv_sec - sta->deauth_monitor_start_time) < deauthMonitorDuration) {
        sta->redeauth_count++;
        wifi_util_dbg_print(WIFI_MON, "%s: redeauth_count:%d \n", __func__, sta->redeauth_count);
        if(sta->redeauth_count > (UINT)deauthCountThreshold) {
            wifi_util_dbg_print(WIFI_MON, "%s: Blocking client deauth for deauthGateTime:%d \n", __func__, deauthGateTime);
            sta->redeauth_count = 0;
            sta->deauth_gate_time = tv_now.tv_sec + deauthGateTime;
            pthread_mutex_unlock(&g_monitor_module.data_lock);
            return TRUE;
        }
    } else {
        sta->deauth_monitor_start_time = tv_now.tv_sec;
        sta->redeauth_count = 0;
        sta->deauth_gate_time = 0;
        wifi_util_dbg_print(WIFI_MON, "%s: resetting redeauth_count and deauth_monitor_start_time \n", __func__);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return FALSE;
    }
    pthread_mutex_unlock(&g_monitor_module.data_lock);
    return FALSE;
}

#define MAX_BUFFER 4096
#define TELEMETRY_MAX_BUFFER 4096
int upload_client_telemetry_data(void *arg)
{
    hash_map_t     *sta_map;
    sta_key_t    sta_key;
    unsigned int num_devs;
    sta_data_t *sta;
    char buff[MAX_BUFFER];
    char telemetryBuff[TELEMETRY_MAX_BUFFER] = { '\0' };
    char tmp[128];
    int rssi;
    BOOL sendIndication = false;
    static char trflag[MAX_VAP] = {0};
    static char nrflag[MAX_VAP] = {0};
    static char stflag[MAX_VAP] = {0};
    static char snflag[MAX_VAP] = {0};
    static unsigned int phase = 0;
    static unsigned int i = 0;
    CHAR eventName[32] = {0};
    unsigned int itr = 0;
    char *t_str =  NULL;
    char t_string[5] = {0};
    wifi_mgr_t *mgr = get_wifimgr_obj();
    UINT vap_index = VAP_INDEX(mgr->hal_cap, i);

    if (phase == 0) {
        // IsCosaDmlWiFivAPStatsFeatureEnabled needs to be set to get telemetry of some stats, the TR object is 
        // Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable
        get_device_flag(trflag, WIFI_TxRx_RATE_LIST);
        get_device_flag(nrflag, WIFI_NORMALIZED_RSSI_LIST);
        get_device_flag(stflag, WIFI_CLI_STAT_LIST);
        // see if list has changed
        BOOL enableRadioDetailStats[MAX_NUM_RADIOS] = {FALSE};
        if (strncmp(stflag, g_monitor_module.cliStatsList, MAX_VAP) != 0) {
            strncpy(g_monitor_module.cliStatsList, stflag, MAX_VAP);
            // check if we should enable of disable detailed client stats collection on XB3
            UINT radioIndex = 0; 
            for (itr = 0; itr < (UINT)getTotalNumberVAPs(); itr++)  {
                UINT vap_index = VAP_INDEX(mgr->hal_cap, itr);
                UINT radio = RADIO_INDEX(mgr->hal_cap, itr);
                if (g_monitor_module.radio_presence[radio] == false) {
                   continue;
                }
                if (stflag[itr] == 1) {
                    radioIndex = getRadioIndexFromAp(vap_index);
                    enableRadioDetailStats[radioIndex] = TRUE;
                }
            }
            for (radioIndex = 0; radioIndex < getNumberRadios(); ++radioIndex) {
                if (g_monitor_module.radio_presence[radioIndex] == false) {
                    continue;
                }
                wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(radioIndex);
                if (radioOperation == NULL) {
                    CcspTraceWarning(("%s : failed to getRadioOperationParam with radio index \n", __FUNCTION__));
                    phase = 0;
                    return TIMER_TASK_COMPLETE;
                }
                switch (radioOperation->band)
                {
                    case WIFI_FREQUENCY_2_4_BAND:
                        wifi_util_dbg_print(WIFI_MON, "%s:%d: client detailed stats collection for 2.4GHz radio set to %s\n", __func__, __LINE__, 
                                (enableRadioDetailStats[radioIndex] == TRUE)?"enabled":"disabled");
                    break;
                    case WIFI_FREQUENCY_5_BAND:
                        wifi_util_dbg_print(WIFI_MON, "%s:%d: client detailed stats collection for 5GHz radio set to %s\n", __func__, __LINE__, 
                                (enableRadioDetailStats[radioIndex] == TRUE)?"enabled":"disabled");
                    break;
                    case WIFI_FREQUENCY_5L_BAND:
                        wifi_util_dbg_print(WIFI_MON, "%s:%d: client detailed stats collection for 5GHz Low radio set to %s\n", __func__, __LINE__,
                                (enableRadioDetailStats[radioIndex] == TRUE)?"enabled":"disabled");
                    break;
                    case WIFI_FREQUENCY_5H_BAND:
                        wifi_util_dbg_print(WIFI_MON, "%s:%d: client detailed stats collection for 5GHz High radio set to %s\n", __func__, __LINE__,
                                (enableRadioDetailStats[radioIndex] == TRUE)?"enabled":"disabled");
                    break;
                    case WIFI_FREQUENCY_6_BAND:
                        wifi_util_dbg_print(WIFI_MON, "%s:%d: client detailed stats collection for 6GHz radio set to %s\n", __func__, __LINE__, 
                                (enableRadioDetailStats[radioIndex] == TRUE)?"enabled":"disabled");
                    break;
                    default:
                    break;
                }
            }
        }
        get_device_flag(snflag, WIFI_SNR_LIST);
        memset(buff, 0, MAX_BUFFER);
        phase++;
        return TIMER_TASK_CONTINUE;
    }
    if (phase == 1) {
        UINT radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            i++;
            if (i >= getTotalNumberVAPs()) {
                i = 0;
                phase++;
            }
            return TIMER_TASK_CONTINUE;
        }
        sta_map = g_monitor_module.bssid_data[i].sta_map;
        memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
        get_formatted_time(tmp);
        snprintf(buff, 2048, "%s WIFI_MAC_%d:", tmp, vap_index + 1);
        num_devs = 0;
        sta = hash_map_get_first(sta_map);
        while (sta != NULL) {
            if (sta->dev_stats.cli_Active == true) {
                snprintf(tmp, 32, "%s,", to_sta_key(sta->sta_mac, sta_key));
                strncat(buff, tmp, MAX_BUFFER - strlen(buff) - 1);
                strncat(telemetryBuff, tmp, TELEMETRY_MAX_BUFFER - strlen(buff) - 1);
                num_devs++;
            }
            sta = hash_map_get_next(sta_map, sta);
        }
        strncat(buff, "\n", MAX_BUFFER - strlen(buff) - 1);
        // RDKB-28827 don't print this marker if there is no client connected
        if(0 != num_devs) {
            write_to_file(wifi_health_log, buff);
        }
        /*
          "header": "2GclientMac_split", "content": "WIFI_MAC_1:", "type": "wifihealth.txt",
          "header": "5GclientMac_split", "content": "WIFI_MAC_2:", "type": "wifihealth.txt",
          "header": "xh_mac_3_split",    "content": "WIFI_MAC_3:", "type": "wifihealth.txt",
          "header": "xh_mac_4_split",    "content": "WIFI_MAC_4:", "type": "wifihealth.txt",
          */
        t_str = convert_radio_index_to_band_str_g(getRadioIndexFromAp(vap_index));
        if (t_str != NULL) {
            strncpy(t_string, t_str, sizeof(t_string) - 1);
            for (itr=1; itr<strlen(t_string); itr++) {
                t_string[itr] = toupper(t_string[itr]);
            }
            if (isVapPrivate(vap_index)) {
                snprintf(eventName, sizeof(eventName), "%sclientMac_split", t_string);
                t2_event_s(eventName, telemetryBuff);
            } else if (isVapXhs(vap_index)) {
                snprintf(eventName, sizeof(eventName), "xh_mac_%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_MAC_%d_TOTAL_COUNT:%d\n", tmp, vap_index + 1, num_devs);
            write_to_file(wifi_health_log, buff);
            //    "header": "Total_2G_clients_split", "content": "WIFI_MAC_1_TOTAL_COUNT:", "type": "wifihealth.txt",
            //    "header": "Total_5G_clients_split", "content": "WIFI_MAC_2_TOTAL_COUNT:","type": "wifihealth.txt",
            //    "header": "xh_cnt_1_split","content": "WIFI_MAC_3_TOTAL_COUNT:","type": "wifihealth.txt",
            //    "header": "xh_cnt_2_split","content": "WIFI_MAC_4_TOTAL_COUNT:","type": "wifihealth.txt",
            if (isVapPrivate(vap_index)) {
                if (0 == num_devs) {
                    snprintf(eventName, sizeof(eventName), "WIFI_INFO_Zero_%s_Clients", t_string);
                    t2_event_d(eventName, 1);
                } else {
                    snprintf(eventName, sizeof(eventName), "Total_%s_clients_split", t_string);
                    t2_event_d(eventName, num_devs);
                }
            } else if (isVapXhs(vap_index)) {
                snprintf(eventName, sizeof(eventName), "xh_cnt_%s_split",
                    convert_radio_index_to_band_str(getRadioIndexFromAp(vap_index)));
                t2_event_d(eventName, num_devs);
            } else if (isVapMesh(vap_index)) {
                snprintf(eventName, sizeof(eventName), "Total_%s_PodClients_split", t_string);
                t2_event_d(eventName, num_devs);
            }
        } else {
            wifi_util_dbg_print(WIFI_MON, "%s-%d Failed to get band for radio Index %d\n", __func__,
                __LINE__, getRadioIndexFromAp(vap_index));
        }
        wifi_util_dbg_print(WIFI_MON, "%s", buff);
        get_formatted_time(tmp);
        memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
#if !defined(_XB7_PRODUCT_REQ_) && !defined(_PLATFORM_TURRIS_) && !defined(_HUB4_PRODUCT_REQ_) && !defined(_WNXL11BWL_PRODUCT_REQ_)
        wifi_VAPTelemetry_t telemetry;
        char vap_status[16];
        memset(vap_status,0,16);
        wifi_getApStatus(vap_index, vap_status);
        wifi_getVAPTelemetry(vap_index, &telemetry);
        if(strncmp(vap_status,"Up",2)==0) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WiFi_TX_Overflow_SSID_%d:%u\n", tmp, vap_index + 1, telemetry.txOverflow);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
#endif
        // RDKB-28827 no need for markers of client details if there is no client connected
        if (0 == num_devs) {
            i++;
            if (i >= getTotalNumberVAPs()) {
                i = 0;
                phase++;
            }
            return TIMER_TASK_CONTINUE;
        }
        snprintf(buff, 2048, "%s WIFI_RSSI_%d:", tmp, vap_index + 1);
        sta = hash_map_get_first(sta_map);
        while (sta != NULL) {
            if (sta->dev_stats.cli_Active == true) {
                snprintf(tmp, 32, "%d,", sta->dev_stats.cli_RSSI);
                strncat(buff, tmp, 128);
                strncat(telemetryBuff, tmp, 128);
            }
            sta = hash_map_get_next(sta_map, sta);
        }
        strncat(buff, "\n", 2);
        write_to_file(wifi_health_log, buff);
        if (isVapPrivate(vap_index)) {
            t_str = convert_radio_index_to_band_str_g(getRadioIndexFromAp(vap_index));
            if (t_str != NULL) {
                strncpy(t_string, t_str, sizeof(t_string) - 1);
                for (itr=1; itr<strlen(t_string); itr++) {
                    t_string[itr] = toupper(t_string[itr]);

                }
                snprintf(eventName, sizeof(eventName), "%sRSSI_split", t_string);
                t2_event_s(eventName, telemetryBuff);
            } else {
                wifi_util_dbg_print(WIFI_MON, "%s-%d Failed to get band for radio Index %d\n", __func__, __LINE__, getRadioIndexFromAp(vap_index));
            }
        } else if (isVapXhs(vap_index)) {
            snprintf(eventName, sizeof(eventName), "xh_rssi_%u_split", vap_index + 1);
            t2_event_s(eventName, telemetryBuff);
        }
        wifi_util_dbg_print(WIFI_MON, "%s", buff);
        get_formatted_time(tmp);
        memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
        snprintf(buff, 2048, "%s WIFI_CHANNEL_WIDTH_%d:", tmp, vap_index + 1);
        sta = hash_map_get_first(sta_map);
        while (sta != NULL) {
            if (sta->dev_stats.cli_Active == true) {
                snprintf(tmp, 64, "%s,", sta->dev_stats.cli_OperatingChannelBandwidth);
                strncat(buff, tmp, 128);
                strncat(telemetryBuff, tmp, 128);
            }
            sta = hash_map_get_next(sta_map, sta);
        }
        strncat(buff, "\n", 2);
        write_to_file(wifi_health_log, buff);
        if (isVapPrivate(vap_index)) {
            snprintf(eventName, sizeof(eventName), "WIFI_CW_%d_split", vap_index + 1);
            t2_event_s(eventName, telemetryBuff);
        }
        wifi_util_dbg_print(WIFI_MON, "%s", buff);
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && nrflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_NORMALIZED_RSSI_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%d,", sta->dev_stats.cli_SignalStrength);
                    strncat(buff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }	
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && snflag[i]) {
            get_formatted_time(tmp);
            memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
            snprintf(buff, 2048, "%s WIFI_SNR_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%d,", sta->dev_stats.cli_SNR);
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            if (isVapPrivate(vap_index)) {
                snprintf(eventName, sizeof(eventName), "WIFI_SNR_%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        get_formatted_time(tmp);
        memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
        snprintf(buff, 2048, "%s WIFI_TXCLIENTS_%d:", tmp, vap_index + 1);
        sta = hash_map_get_first(sta_map);
        while (sta != NULL) {
            if (sta->dev_stats.cli_Active == true) {
                snprintf(tmp, 32, "%d,", sta->dev_stats.cli_LastDataDownlinkRate);
                strncat(buff, tmp, 128);
                strncat(telemetryBuff, tmp, 128);
            }

            sta = hash_map_get_next(sta_map, sta);
        }
        strncat(buff, "\n", 2);
        write_to_file(wifi_health_log, buff);
        if (isVapPrivate(vap_index)) {
            snprintf(eventName, sizeof(eventName), "WIFI_TX_%d_split", vap_index + 1);
            t2_event_s(eventName, telemetryBuff);
        }
        wifi_util_dbg_print(WIFI_MON, "%s", buff);
        get_formatted_time(tmp);
        memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
        snprintf(buff, 2048, "%s WIFI_RXCLIENTS_%d:", tmp, vap_index + 1);
        sta = hash_map_get_first(sta_map);
        while (sta != NULL) {
            if (sta->dev_stats.cli_Active == true) {
                snprintf(tmp, 32, "%d,", sta->dev_stats.cli_LastDataUplinkRate);
                strncat(buff, tmp, 128);
                strncat(telemetryBuff, tmp, 128);
            }

            sta = hash_map_get_next(sta_map, sta);
        }
        strncat(buff, "\n", 2);
        write_to_file(wifi_health_log, buff);
        //  "header": "WIFI_RX_1_split", "content": "WIFI_RXCLIENTS_1:", "type": "wifihealth.txt",
        if (isVapPrivate(vap_index)) {
            snprintf(eventName, sizeof(eventName), "WIFI_RX_%d_split", vap_index + 1);
            t2_event_s(eventName, telemetryBuff);
        }
        wifi_util_dbg_print(WIFI_MON, "%s", buff);

        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i]) {
            get_formatted_time(tmp);
            memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
            snprintf(buff, 2048, "%s WIFI_MAX_TXCLIENTS_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%u,", sta->dev_stats.cli_MaxDownlinkRate);
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            if (isVapPrivate(vap_index)) {
                snprintf(eventName, sizeof(eventName), "MAXTX_%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i]) {
            get_formatted_time(tmp);
            memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
            snprintf(buff, 2048, "%s WIFI_MAX_RXCLIENTS_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%u,", sta->dev_stats.cli_MaxUplinkRate);
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            if (isVapPrivate(vap_index)) {
                snprintf(eventName, sizeof(eventName), "MAXRX_%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_RXTXCLIENTDELTA_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%u,", (sta->dev_stats.cli_LastDataDownlinkRate - sta->dev_stats.cli_LastDataUplinkRate));
                    strncat(buff, tmp, 128);
                }
                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_BYTESSENTCLIENTS_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_BytesSent - sta->dev_stats_last.cli_BytesSent);
                    sta->dev_stats_last.cli_BytesSent = sta->dev_stats.cli_BytesSent;
                    strncat(buff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_BYTESRECEIVEDCLIENTS_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_BytesReceived - sta->dev_stats_last.cli_BytesReceived);
                    sta->dev_stats_last.cli_BytesReceived = sta->dev_stats.cli_BytesReceived;
                    strncat(buff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
            snprintf(buff, 2048, "%s WIFI_PACKETSSENTCLIENTS_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_PacketsSent - sta->dev_stats_last.cli_PacketsSent);
                    sta->dev_stats_last.cli_PacketsSent = sta->dev_stats.cli_PacketsSent;
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            if (isVapPrivate(vap_index)) {
                snprintf(eventName, sizeof(eventName), "WIFI_PACKETSSENTCLIENTS_%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_PACKETSRECEIVEDCLIENTS_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_PacketsReceived - sta->dev_stats_last.cli_PacketsReceived);
                    sta->dev_stats_last.cli_PacketsReceived = sta->dev_stats.cli_PacketsReceived;
                    strncat(buff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
            snprintf(buff, 2048, "%s WIFI_ERRORSSENT_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_ErrorsSent - sta->dev_stats_last.cli_ErrorsSent);
                    sta->dev_stats_last.cli_ErrorsSent = sta->dev_stats.cli_ErrorsSent;       
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            if (isVapPrivate(vap_index))
            {
                snprintf(eventName, sizeof(eventName), "WIFI_ERRORSSENT_%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
            snprintf(buff, 2048, "%s WIFI_RETRANSCOUNT_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_RetransCount - sta->dev_stats_last.cli_RetransCount);
                    sta->dev_stats_last.cli_RetransCount = sta->dev_stats.cli_RetransCount;
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            if (isVapPrivate(vap_index))
            {
                snprintf(eventName, sizeof(eventName), "WIFIRetransCount%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_FAILEDRETRANSCOUNT_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_FailedRetransCount - sta->dev_stats_last.cli_FailedRetransCount);
                    sta->dev_stats_last.cli_FailedRetransCount = sta->dev_stats.cli_FailedRetransCount;
                    strncat(buff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_RETRYCOUNT_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_RetryCount - sta->dev_stats_last.cli_RetryCount);
                    sta->dev_stats_last.cli_RetryCount = sta->dev_stats.cli_RetryCount;
                    strncat(buff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
            get_formatted_time(tmp);
            snprintf(buff, 2048, "%s WIFI_MULTIPLERETRYCOUNT_%d:", tmp, vap_index + 1);
            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_MultipleRetryCount - sta->dev_stats_last.cli_MultipleRetryCount);
                    sta->dev_stats_last.cli_MultipleRetryCount = sta->dev_stats.cli_MultipleRetryCount;
                    strncat(buff, tmp, 128);
                }

                sta = hash_map_get_next(sta_map, sta);
            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            wifi_util_dbg_print(WIFI_MON, "%s", buff);
        }
        // Every hour, for private SSID(s) we need to calculate the good rssi time and bad rssi time 
        // and write into wifi log in following format
        // WIFI_GOODBADRSSI_$apindex: $MAC,$GoodRssiTime,$BadRssiTime; $MAC,$GoodRssiTime,$BadRssiTime; ....
        if (i < (UINT)getTotalNumberVAPs()) {
            get_formatted_time(tmp);
            memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
            snprintf(buff, 2048, "%s WIFI_GOODBADRSSI_%d:", tmp, vap_index + 1);

            sta = hash_map_get_first(sta_map);
            while (sta != NULL) {
                sta->total_connected_time += sta->connected_time;
                sta->connected_time = 0;
                sta->total_disconnected_time += sta->disconnected_time;
                sta->disconnected_time = 0;
                if (sta->dev_stats.cli_Active == true) {
                    snprintf(tmp, 128, "%s,%d,%d;", to_sta_key(sta->sta_mac, sta_key), (sta->good_rssi_time)/60, (sta->bad_rssi_time)/60);
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);
                }
                sta->good_rssi_time = 0;
                sta->bad_rssi_time = 0;
                sta = hash_map_get_next(sta_map, sta);

            }
            strncat(buff, "\n", 2);
            write_to_file(wifi_health_log, buff);
            if (isVapPrivate(vap_index)) {
                snprintf(eventName, sizeof(eventName), "GB_RSSI_%d_split", vap_index + 1);
                t2_event_s(eventName, telemetryBuff);
            }
            wifi_util_dbg_print(WIFI_MON, "%s", buff);		
        }
        // check if failure indication is enabled in TR swicth
        wifi_front_haul_bss_t *vap_bss_info = Get_wifi_object_bss_parameter(vap_index);
        if(vap_bss_info != NULL) {
            sendIndication = vap_bss_info->rapidReconnectEnable;
            wifi_util_dbg_print(WIFI_MON, "%s: sendIndication:%d vapIndex:%d \n", __FUNCTION__, sendIndication, vap_index);
        } else {
            wifi_util_dbg_print(WIFI_MON, "%s: wrong vapIndex:%d \n", __FUNCTION__, vap_index);
        }
        if (sendIndication == true) {
            BOOLEAN bReconnectCountEnable = 0;
            // check whether Reconnect Count is enabled or not fro individual vAP
            get_multi_vap_dml_parameters(vap_index, RECONNECT_COUNT_STATUS, &bReconnectCountEnable);
            if (bReconnectCountEnable == true)
            {
                get_formatted_time(tmp);
                memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
                snprintf(buff, 2048, "%s WIFI_RECONNECT_%d:", tmp, vap_index + 1);
                sta = hash_map_get_first(sta_map);
                while (sta != NULL) {

                    snprintf(tmp, 128, "%s,%d;", to_sta_key(sta->sta_mac, sta_key), sta->rapid_reconnects);
                    strncat(buff, tmp, 128);
                    strncat(telemetryBuff, tmp, 128);

                    sta->rapid_reconnects = 0;

                    sta = hash_map_get_next(sta_map, sta);

                }
                strncat(buff, "\n", 2);
                write_to_file(wifi_health_log, buff);
                if (isVapPrivate(vap_index))
                {
                    snprintf(eventName, sizeof(eventName), "WIFI_REC_%d_split", vap_index + 1);
                    t2_event_s(eventName, telemetryBuff);
                }
                wifi_util_dbg_print(WIFI_MON, "%s", buff);
            }
        }
        i++;
        if(i >= getTotalNumberVAPs()) {
            i = 0;
            phase++;
        }
        return TIMER_TASK_CONTINUE;
    }

    // update thresholds if changed
    if (get_vap_dml_parameters(RSSI_THRESHOLD, &rssi) == ANSC_STATUS_SUCCESS) {
        g_monitor_module.sta_health_rssi_threshold = rssi;
    }

    for (i = 0; i < getTotalNumberVAPs(); i++) {
        UINT vap_index;
        UINT radio;

        vap_index = VAP_INDEX(mgr->hal_cap, i);
        radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        // update rapid reconnect time limit if changed
        wifi_front_haul_bss_t *vap_bss_info = Get_wifi_object_bss_parameter(vap_index);
        if(vap_bss_info != NULL) {
            g_monitor_module.bssid_data[i].ap_params.rapid_reconnect_threshold = vap_bss_info->rapidReconnThreshold;
            wifi_util_dbg_print(WIFI_MON, "%s:rapidReconnThreshold:%d vapIndex:%d \n", __FUNCTION__, vap_bss_info->rapidReconnThreshold, vap_index);
        } else {
            wifi_util_error_print(WIFI_MON, "%s: wrong vapIndex:%d \n", __FUNCTION__, vap_index);
        }

    }    
    logVAPUpStatus();
    i = 0;
    phase = 0;
    return TIMER_TASK_COMPLETE;
}
#endif // CCSP_COMMON


static char*
macbytes_to_string(mac_address_t mac, unsigned char* string)
{
    sprintf((char *)string, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0] & 0xff,
            mac[1] & 0xff,
            mac[2] & 0xff,
            mac[3] & 0xff,
            mac[4] & 0xff,
            mac[5] & 0xff);
    return (char *)string;
}

#ifdef CCSP_COMMON
static void
reset_client_stats_info(unsigned int apIndex)
{
    sta_data_t      *sta = NULL;
    hash_map_t      *sta_map;
    unsigned int    vap_array_index;

    getVAPArrayIndexFromVAPIndex(apIndex, &vap_array_index);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;

    sta = hash_map_get_first(sta_map);
    while (sta != NULL) {
        memset((unsigned char *)&sta->dev_stats_last, 0, sizeof(wifi_associated_dev3_t));
        memset((unsigned char *)&sta->dev_stats, 0,  sizeof(wifi_associated_dev3_t));
        sta = hash_map_get_next(sta_map, sta);
    }

}

void get_device_flag(char flag[], char *list_name)
{
    int ret = RETURN_ERR;
    char buf[MAX_BUF_SIZE] = {0};

    ret = get_device_config_list(buf, MAX_BUF_SIZE, list_name);
    wifi_util_dbg_print(WIFI_MON, "\n %s line %d get_device_config_list for %s is %s\n",__func__, __LINE__,list_name, buf);

    if ((ret == RETURN_OK) && (strlen(buf)) ) {
        int buf_int[16] = {0}, i = 0, j = 0;

        for (i = 0; buf[i] != '\0'; i++)
        {
            if (buf[i] == ',')
            {
                j++;
            } else if (buf[i] == '"') {

                continue;
            }
            else
            {
                buf_int[j] = buf_int[j] * 10 + (buf[i] - 48);
            }
        }
        int len = sizeof(buf_int)/sizeof(buf_int[0]);
        for(i = 0;  i < len; i ++)
        {
            if((buf_int[i] <= MAX_VAP) && (buf_int[i] > 0))
            {
                flag[i] = 1;
            }
        }
    } else {
        flag[0] = 1;
        flag[1] = 1;
    }
    return;
}


static void upload_client_debug_stats_chan_stats(INT apIndex) 
{
    char tmp[128] = {0};
    ULONG channel = 0;
    CHAR eventName[32] = {0};
    wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(getRadioIndexFromAp(apIndex));
    if (radioOperation != NULL) {
        channel = radioOperation->channel;
        memset(tmp, 0, sizeof(tmp));
        get_formatted_time(tmp);
        write_to_file(wifi_health_log, "\n%s WIFI_CHANNEL_%d:%lu\n", tmp, apIndex+1, channel);
        if (isVapPrivate(apIndex))
        {
            snprintf(eventName, sizeof(eventName), "WIFI_CH_%d_split", apIndex + 1 );
            t2_event_d(eventName, channel);
            if (getRadioIndexFromAp(apIndex) == 1)
            {
                if( 1 == channel )
                {
                    //         "header": "WIFI_INFO_UNI3_channel", "content": "WIFI_CHANNEL_2:1", "type": "wifihealth.txt",
                    t2_event_d("WIFI_INFO_UNI3_channel", 1);
                } else if (( 3 == channel || 4 == channel)) \
                {
                    t2_event_d("WIFI_INFO_UNII_channel", 1);
                }
            }
        }
    } else {
        wifi_util_error_print(WIFI_MON, "%s :Failed to get channel from global db",__func__);
    }
}
static void  upload_client_debug_stats_transmit_power_stats(INT apIndex)
{
    char tmp[128] = {0};
    ULONG txpower = 0;
    ULONG txpwr_pcntg = 0;
    CHAR eventName[32] = {0};
    if (isVapPrivate(apIndex))
    {
        txpower = 0;
        /* adding transmit power and countrycode */
        wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(getRadioIndexFromAp(apIndex));
        if (radioOperation != NULL) {
            memset(tmp, 0, sizeof(tmp));
            get_formatted_time(tmp);
            write_to_file(wifi_health_log, "%s WIFI_COUNTRY_CODE_%d:%s\n", tmp, apIndex+1, wifiCountryMap[radioOperation->countryCode].countryStr);
            wifi_getRadioTransmitPower(getRadioIndexFromAp(apIndex), &txpower);
            memset(tmp, 0, sizeof(tmp));
            get_formatted_time(tmp);
            write_to_file(wifi_health_log, "%s WIFI_TX_PWR_dBm_%d:%lu\n", tmp, apIndex+1, txpower);
            //    "header": "WIFI_TXPWR_1_split",   "content": "WIFI_TX_PWR_dBm_1:", "type": "wifihealth.txt",
            //    "header": "WIFI_TXPWR_2_split",   "content": "WIFI_TX_PWR_dBm_2:", "type": "wifihealth.txt",
            snprintf(eventName, sizeof(eventName), "WIFI_TXPWR_%d_split", apIndex + 1 );
            t2_event_d(eventName, txpower);
            txpwr_pcntg = radioOperation->transmitPower;
            memset(tmp, 0, sizeof(tmp));
            get_formatted_time(tmp);
            write_to_file(wifi_health_log, "%s WIFI_TX_PWR_PERCENTAGE_%d:%lu\n", tmp, apIndex+1, txpwr_pcntg);
            snprintf(eventName, sizeof(eventName), "WIFI_TXPWR_PCNTG_%u_split", apIndex + 1 );
            t2_event_d("WIFI_TXPWR_PCNTG_1_split", txpwr_pcntg);
        } else {
            wifi_util_error_print(WIFI_MON, "%s: getRadioOperationParam failed for ApIdx %d\n", __FUNCTION__, getRadioIndexFromAp(apIndex)); 
        }
    }
}
static void upload_client_debug_stats_acs_stats(INT apIndex) 
{
    BOOL enable = false;
    char tmp[128] = {0};
    CHAR eventName[32] = {0};
    if (isVapPrivate(apIndex))
    {
        wifi_global_param_t *global_param = get_wifidb_wifi_global_param();
        if (global_param != NULL) {
            enable = global_param->bandsteering_enable;
        }
        memset(tmp, 0, sizeof(tmp));
        get_formatted_time(tmp);
        write_to_file(wifi_health_log, "%s WIFI_ACL_%d:%d\n", tmp, apIndex+1, enable);
        enable = false;
        wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(getRadioIndexFromAp(apIndex));
        if (radioOperation != NULL) {
            enable = radioOperation->autoChannelEnabled;
        }
        if (true == enable)
        {
            memset(tmp, 0, sizeof(tmp));
            get_formatted_time(tmp);
            write_to_file(wifi_health_log, "%s WIFI_ACS_%d:true\n", tmp, apIndex+1);
            // "header": "WIFI_ACS_1_split",  "content": "WIFI_ACS_1:", "type": "wifihealth.txt",
            // "header": "WIFI_ACS_2_split", "content": "WIFI_ACS_2:", "type": "wifihealth.txt",
            snprintf(eventName, sizeof(eventName), "WIFI_ACS_%d_split", apIndex + 1 );
            t2_event_s(eventName, "true");
        }
        else
        {
            memset(tmp, 0, sizeof(tmp));
            get_formatted_time(tmp);
            write_to_file(wifi_health_log, "%s WIFI_ACS_%d:false\n", tmp, apIndex+1);
            // "header": "WIFI_ACS_1_split",  "content": "WIFI_ACS_1:", "type": "wifihealth.txt",
            // "header": "WIFI_ACS_2_split", "content": "WIFI_ACS_2:", "type": "wifihealth.txt",
            snprintf(eventName, sizeof(eventName), "WIFI_ACS_%d_split", apIndex + 1 );
            t2_event_s(eventName,  "false");
        }
    }
}
static void upload_client_debug_stats_sta_fa_info(INT apIndex, sta_data_t *sta) 
{
    INT len = 0;
    char *value = NULL;
    char *saveptr = NULL;
    char *ptr = NULL;
    FILE *fp  = NULL;
    char tmp[128] = {0};
    sta_key_t sta_key;
    char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};

    memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
    if (sta != NULL) {
        fp = (FILE *)v_secure_popen("r", "dmesg | grep FA_INFO_%s | tail -1", to_sta_key(sta->sta_mac, sta_key));
        if (fp) {
            fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
            v_secure_pclose(fp);
            len = strlen(buf);
            if (len) {
                ptr = buf + len;
                while (len-- && ptr-- && *ptr != ':');
                ptr++;
                value = strtok_r(ptr, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_AID_%d:%s", tmp, apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_TIM_%d:%s", tmp, apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_BMP_SET_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_BMP_CLR_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_TX_PKTS_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_TX_DISCARDS_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "\n%s WIFI_UAPSD_%d:%s", tmp,
                        apIndex+1, value);
            }
        }
        else {
            wifi_util_error_print(WIFI_MON, " %s Failed to run popen command\n", __FUNCTION__);
        }
    }
    else {
        wifi_util_error_print(WIFI_MON, "%s NULL sta\n", __FUNCTION__);
    }
}
static void upload_client_debug_stats_sta_fa_lmac_data_stats(INT apIndex, sta_data_t *sta)
{
    INT len = 0;
    char *value = NULL;
    char *saveptr = NULL;
    char *ptr = NULL;
    FILE *fp  = NULL;
    char tmp[128] = {0};
    sta_key_t sta_key;
    char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
    memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
    if (sta != NULL) {
        fp = (FILE *)v_secure_popen("r", "dmesg | grep FA_LMAC_DATA_STATS_%s | tail -1", to_sta_key(sta->sta_mac, sta_key));
        if (fp) {
            fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
            v_secure_pclose(fp);
            len = strlen(buf);
            if (len) {
                ptr = buf + len;
                while (len-- && ptr-- && *ptr != ':');
                ptr++;
                value = strtok_r(ptr, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_DATA_QUEUED_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_DATA_DROPPED_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_DATA_DEQUED_TX_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_DATA_DEQUED_DROPPED_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_DATA_EXP_DROPPED_CNT_%d:%s", tmp,
                        apIndex+1, value);
            }
        }
        else {
            wifi_util_error_print(WIFI_MON, "%s Failed to run popen command\n", __FUNCTION__);
        }
    }
    else {
        wifi_util_error_print(WIFI_MON, "%s NULL sta\n", __FUNCTION__);
    }
}
static void upload_client_debug_stats_sta_fa_lmac_mgmt_stats(INT apIndex, sta_data_t *sta)
{
    INT len = 0;
    char *value = NULL;
    char *saveptr = NULL;
    char *ptr = NULL;
    FILE *fp  = NULL;
    sta_key_t sta_key;
    char tmp[128] = {0};
    char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
    memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
    if(sta != NULL) {
        fp = (FILE *)v_secure_popen("r", "dmesg | grep FA_LMAC_MGMT_STATS_%s | tail -1", to_sta_key(sta->sta_mac, sta_key));
        if (fp) {
            fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
            v_secure_pclose(fp);
            len = strlen(buf);
            if (len)
            {
                ptr = buf + len;
                while (len-- && ptr-- && *ptr != ':');
                ptr++;
                value = strtok_r(ptr, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_MGMT_QUEUED_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_MGMT_DROPPED_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_MGMT_DEQUED_TX_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_MGMT_DEQUED_DROPPED_CNT_%d:%s", tmp,
                        apIndex+1, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log,
                        "\n%s WIFI_MGMT_EXP_DROPPED_CNT_%d:%s", tmp,
                        apIndex+1, value);
            }
        }
        else {
            wifi_util_error_print(WIFI_MON, "%s Failed to run popen command\n", __FUNCTION__ );
        }
    }
    else {
        wifi_util_error_print(WIFI_MON, "%s NULL sta\n", __FUNCTION__);
    }
}
static void upload_client_debug_stats_sta_vap_activity_stats(INT apIndex)
{
    INT len = 0;
    char *value = NULL;
    char *saveptr = NULL;
    char *ptr = NULL;
    FILE *fp  = NULL;
    char tmp[128] = {0};
    char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
    if (0 == apIndex) {
        memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
        fp = (FILE *)v_secure_popen("r", "dmesg | grep VAP_ACTIVITY_ath0 | tail -1");
        if (fp)
        {
            fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
            v_secure_pclose(fp);
            len = strlen(buf);
            if (len)
            {
                ptr = buf + len;
                while (len-- && ptr-- && *ptr != ':');
                ptr += 3;
                value = strtok_r(ptr, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_1:%s\n", tmp, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_LEN_1:%s\n", tmp,
                        value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_1:%s\n", tmp,
                        value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_LEN_1:%s\n", tmp,
                        value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_COUNT_1:%s\n", tmp,
                        value);
            }
        }
        else {
            wifi_util_error_print(WIFI_MON, "%s Failed to run popen command\n", __FUNCTION__ );
        }
    }
    if (1 == apIndex) {
        memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
        fp = (FILE *)v_secure_popen("r", "dmesg | grep VAP_ACTIVITY_ath1 | tail -1");
        if (fp)
        {
            fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
            v_secure_pclose(fp);
            len = strlen(buf);
            if (len)
            {
                ptr = buf + len;
                while (len-- && ptr-- && *ptr != ':');
                ptr += 3;
                value = strtok_r(ptr, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_2:%s\n", tmp, value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_LEN_2:%s\n", tmp,
                        value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_2:%s\n", tmp,
                        value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_LEN_2:%s\n", tmp,
                        value);
                value = strtok_r(NULL, ",", &saveptr);
                memset(tmp, 0, sizeof(tmp));
                get_formatted_time(tmp);
                write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_COUNT_2:%s\n", tmp,
                        value);
            }
        }
        else
        {
            wifi_util_error_print(WIFI_MON, "%s Failed to run popen command\n", __FUNCTION__);
        }
    }
}
/*
 * This API will Create telemetry and data model for client activity stats
 * like BytesSent, BytesReceived, RetransCount, FailedRetransCount, etc...
*/
int upload_client_debug_stats(void *arg)
{
    static INT itr = 0 ;
    static UINT vap_index = 0;
    static unsigned int phase = 0;
    static int vap_status = 0;
    static hash_map_t     *sta_map;
    static sta_data_t *sta;
    static int phase_sta = 0;
    static int phase_fp = 0;
    wifi_mgr_t *mgr = get_wifimgr_obj();
    unsigned int radio;
    vap_index = VAP_INDEX(mgr->hal_cap, itr);
    radio = RADIO_INDEX(mgr->hal_cap, itr);

    if (g_monitor_module.radio_presence[radio] == false) {
        itr++;
        if (itr >= (int)getTotalNumberVAPs())
        {
            itr = 0;
            phase = 0;
            return TIMER_TASK_COMPLETE;
        }
       return TIMER_TASK_COMPLETE;
    }

    if  (false == sWiFiDmlvApStatsFeatureEnableCfg)
    {
        wifi_util_info_print(WIFI_MON, "%s Client activity stats feature is disabled\n", __FUNCTION__);
        phase_sta = 0;
        phase_fp = 0;
        itr = 0;
        phase = 0;
        return TIMER_TASK_COMPLETE;
    }
    if (phase == 0) {
        if (false == sWiFiDmlvApStatsFeatureEnableCfg)
        {
            wifi_util_dbg_print(WIFI_MON, "Stats feature is disabled for itr = %d\n",itr+1);
            itr++;
            if (itr >= (int)getTotalNumberVAPs())
            {
                itr = 0;
                phase = 0;
                return TIMER_TASK_COMPLETE;
            }
            return TIMER_TASK_CONTINUE;
        }
        vap_status = g_monitor_module.bssid_data[vap_index].ap_params.ap_status;
        phase++;
        return TIMER_TASK_CONTINUE;
    }

    if (vap_status) {
        if (phase == 1) {
            upload_client_debug_stats_chan_stats(vap_index);
            phase++;
            return TIMER_TASK_CONTINUE;
        } else if (phase == 2) {
            if(phase_sta == 0){
                sta_map = g_monitor_module.bssid_data[itr].sta_map;
                sta = hash_map_get_first(sta_map);
            }
            if (sta != NULL) {
                if (phase_fp == 0) {
                    upload_client_debug_stats_sta_fa_info(vap_index, sta);
                    phase_fp++;
                    return TIMER_TASK_CONTINUE;
                } else if (phase_fp == 1) {
                    upload_client_debug_stats_sta_fa_lmac_data_stats(vap_index, sta);
                    phase_fp++;
                    return TIMER_TASK_CONTINUE;
                } else if (phase_fp == 2) {
                    upload_client_debug_stats_sta_fa_lmac_mgmt_stats(vap_index, sta);
                    phase_fp++;
                    return TIMER_TASK_CONTINUE;
                } else if(phase_fp ==3) {
                    upload_client_debug_stats_sta_vap_activity_stats(vap_index);
                }
                sta = hash_map_get_next(sta_map, sta);
                phase_fp = 0;
                if(sta != NULL){
                    phase_sta++;
                    return TIMER_TASK_CONTINUE;
                }
            }
            phase++;
            phase_sta=0;
            return TIMER_TASK_CONTINUE;
        } else if (phase == 3) {
            upload_client_debug_stats_transmit_power_stats(vap_index);
            phase++;
            return TIMER_TASK_CONTINUE;
        } else if (phase == 4) {
            upload_client_debug_stats_acs_stats(vap_index);
        }
    }
    itr++;
    phase = 0;
    if (itr >= (int)getTotalNumberVAPs())
    {
        itr = 0;
        return TIMER_TASK_COMPLETE;
    }
    return TIMER_TASK_CONTINUE;
}


static void
process_stats_flag_changed(unsigned int ap_index, client_stats_enable_t *flag)
{
    wifi_mgr_t *mgr = get_wifimgr_obj();

    //Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable = 0
    if (0 == flag->type) {
        int idx;
        int vap_index;
        int radio;

        write_to_file(wifi_health_log, "WIFI_STATS_FEATURE_ENABLE:%s\n",
                (flag->enable) ? "true" : "false");
        for(idx = 0; idx < (int)getTotalNumberVAPs(); idx++) {
            vap_index = VAP_INDEX(mgr->hal_cap, idx);
            radio = RADIO_INDEX(mgr->hal_cap, idx);
            if (g_monitor_module.radio_presence[radio] == false) {
               continue;
            }
            reset_client_stats_info(vap_index);
        }
    } else if (1 == flag->type) { //Device.WiFi.AccessPoint.<vAP>.X_RDKCENTRAL-COM_StatsEnable = 1
        if (wifi_util_is_vap_index_valid(&mgr->hal_cap.wifi_prop, (int)ap_index)) {
            reset_client_stats_info(ap_index);
            write_to_file(wifi_health_log, "WIFI_STATS_ENABLE_%d:%s\n", ap_index+1,
                    (flag->enable) ? "true" : "false");
        }
    }
}

static void
radio_stats_flag_changed(unsigned int radio_index, client_stats_enable_t *flag)
{
    wifi_mgr_t *mgr = get_wifimgr_obj();
    for(UINT apIndex = 0; apIndex <= getTotalNumberVAPs(); apIndex++)
    {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, apIndex);
        UINT radio = RADIO_INDEX(mgr->hal_cap, apIndex);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        if (radio_index == getRadioIndexFromAp(vap_index))
        {
            reset_client_stats_info(apIndex);
        }
        write_to_file(wifi_health_log, "WIFI_RADIO_STATUS_ENABLE_%d:%s\n", radio_index+1,
                (flag->enable) ? "true" : "false");
    }
}

static void
vap_stats_flag_changed(unsigned int ap_index, client_stats_enable_t *flag)
{
    //Device.WiFi.SSID.<vAP>.Enable = 0
    reset_client_stats_info(ap_index);
    write_to_file(wifi_health_log, "WIFI_VAP_STATUS_ENABLE_%d:%s\n", ap_index+1,
            (flag->enable) ? "true" : "false");
}

static void
get_sub_string(char *bandwidth, char *dest)
{
    if (5 == strlen(bandwidth)) // 20MHz. Copy only the first 2 bytes
        strncpy(dest, bandwidth, 2);
    else
        strncpy(dest, bandwidth, 3); //160MHz Copy only the first 3 bytes
}

int upload_channel_width_telemetry(void *arg)
{
    char buffer[64] = {0};
    char bandwidth[4] = {0};
    char tmp[128] = {0};
    char buff[1024] = {0};
    char t_string[5] = {0};
    CHAR eventName[32] = {0};
    BOOL radioEnabled = FALSE;
    char *t_str = NULL;
    unsigned long int itr = 0;
    UINT numRadios = getNumberRadios();
    wifi_util_dbg_print(WIFI_MON, "Entering %s:%d \n", __FUNCTION__, __LINE__);
    for (UINT i = 0; i < numRadios; ++i) {
        if (g_monitor_module.radio_presence[i] == false) {
           continue;
        }
        wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(i);
        if (radioOperation == NULL) {
            CcspTraceWarning(("%s : failed to getRadioOperationParam with radio index:%d \n", __FUNCTION__, i));
            radioEnabled = FALSE;
        } else {
            radioEnabled = radioOperation->enable;
        }
        if (radioEnabled) {
            wifi_getRadioOperatingChannelBandwidth(i, buffer);
            get_sub_string(buffer, bandwidth);
            get_formatted_time(tmp);
            t_str = convert_radio_index_to_band_str_g(i);
            if (t_str != NULL) {
                strncpy(t_string, t_str, sizeof(t_string) - 1);
                for (itr=1; itr<strlen(t_string); itr++) {
                    t_string[itr] = toupper(t_string[itr]);
                }
                snprintf(buff, 1024, "%s WiFi_config_%s_chan_width_split:%s\n", tmp, t_string, bandwidth);
                write_to_file(wifi_health_log, buff);
            } else {
                wifi_util_dbg_print(WIFI_MON, "%s-%d Failed to get band for radio Index %d\n", __func__, __LINE__, i);
            }

            snprintf(eventName, sizeof(eventName), "WIFI_CWconfig_%d_split", i + 1 );
            t2_event_s(eventName, bandwidth);


            memset(buffer, 0, sizeof(buffer));
            memset(bandwidth, 0, sizeof(bandwidth));
            memset(tmp, 0, sizeof(tmp));
        }
    }

    return TIMER_TASK_COMPLETE;
}

int upload_ap_telemetry_pmf(void *arg)
{
    int i;
    bool bFeatureMFPConfig=false;
    char tmp[128]={0};
    char log_buf[1024]={0};
    char telemetry_buf[1024]={0};
    errno_t rc = -1;
    UINT vap_index, radio;
    wifi_mgr_t *mgr = get_wifimgr_obj();

    wifi_util_dbg_print(WIFI_MON, "Entering %s:%d \n", __FUNCTION__, __LINE__);
    // Telemetry:
    // "header":  "WIFI_INFO_PMF_ENABLE"
    // "content": "WiFi_INFO_PMF_enable:"
    // "type": "wifihealth.txt",
    get_vap_dml_parameters(MFP_FEATURE_STATUS, &bFeatureMFPConfig);
    rc = sprintf_s(telemetry_buf, sizeof(telemetry_buf), "%s", bFeatureMFPConfig?"true":"false");
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    get_formatted_time(tmp);
    rc = sprintf_s(log_buf, sizeof(log_buf), "%s WIFI_INFO_PMF_ENABLE:%s\n", tmp, (bFeatureMFPConfig?"true":"false"));
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    write_to_file(wifi_health_log, log_buf);
    wifi_util_dbg_print(WIFI_MON, "%s", log_buf);
    t2_event_s("WIFI_INFO_PMF_ENABLE", telemetry_buf);
    // Telemetry:
    // "header":  "WIFI_INFO_PMF_CONFIG_1"
    // "content": "WiFi_INFO_PMF_config_ath0:"
    // "type": "wifihealth.txt",
    for(i = 0; i < (int)getTotalNumberVAPs(); i++) 
    {
        vap_index = VAP_INDEX(mgr->hal_cap, i);
        radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        if (isVapPrivate(vap_index))
        {
            wifi_vap_security_t *vapSecurity = (wifi_vap_security_t *)Get_wifi_object_bss_security_parameter(vap_index);
            if (vapSecurity != NULL) {

                switch (vapSecurity->mfp)
                {
                    case wifi_mfp_cfg_disabled:
                        snprintf(telemetry_buf, sizeof(telemetry_buf), "Disabled");
                        break;
                    case wifi_mfp_cfg_optional:
                        snprintf(telemetry_buf, sizeof(telemetry_buf), "Optional");
                        break;
                    case wifi_mfp_cfg_required:
                        snprintf(telemetry_buf, sizeof(telemetry_buf), "Required");
                        break;
                    default:
                        wifi_util_dbg_print(WIFI_MON, "%s:%d: unable to find mfp config\n", __func__, __LINE__); 
                        break;
                }
                get_formatted_time(tmp);
                rc = sprintf_s(log_buf, sizeof(log_buf), "%s WIFI_INFO_PMF_CONFIG_%d:%s\n", tmp, i+1, telemetry_buf);
                if(rc < EOK)
                {
                    ERR_CHK(rc);
                }
                write_to_file(wifi_health_log, log_buf);
                wifi_util_dbg_print(WIFI_MON, "%s", log_buf);
                rc = sprintf_s(tmp, sizeof(tmp), "WIFI_INFO_PMF_CONFIG_%d", i+1);
                if(rc < EOK)
                {
                    ERR_CHK(rc);
                }
                t2_event_s(tmp, telemetry_buf);
            }
        }
    }
    wifi_util_dbg_print(WIFI_MON, "Exiting %s:%d \n", __FUNCTION__, __LINE__);
    return TIMER_TASK_COMPLETE;
}

/*
 * wifi_stats_flag_change()
 * ap_index vAP
 * enable   true/false
 * type     Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable= 0,
 Device.WiFi.AccessPoint.<vAP>.X_RDKCENTRAL-COM_StatsEnable = 1
 */
int wifi_stats_flag_change(int ap_index, bool enable, int type)
{
    wifi_monitor_data_t data;

    memset(&data, 0, sizeof(wifi_monitor_data_t));
    data.id = msg_id++;
    data.ap_index = ap_index;

    data.u.flag.type = type;
    data.u.flag.enable = enable;

    wifi_util_dbg_print(WIFI_MON, "%s:%d: flag changed apIndex=%d enable=%d type=%d\n",
            __func__, __LINE__, ap_index, enable, type);

    push_event_to_monitor_queue(&data, wifi_event_monitor_stats_flag_change, NULL);

    return 0;
}

/*
 * radio_stats_flag_change()
 * ap_index vAP
 * enable   true/false
 * type     Device.WiFi.Radio.<Index>.Enable = 1
 */
int radio_stats_flag_change(int radio_index, bool enable)
{
    wifi_monitor_data_t data;

    memset(&data, 0, sizeof(wifi_monitor_data_t));
    data.id = msg_id++;
    data.ap_index = radio_index;	//Radio_Index = 0, 1
    data.u.flag.enable = enable;

    wifi_util_dbg_print(WIFI_MON, "%s:%d: flag changed radioIndex=%d enable=%d\n",
            __func__, __LINE__, radio_index, enable);

    push_event_to_monitor_queue(&data, wifi_event_monitor_radio_stats_flag_change, NULL);

    return 0;
}

/*
 * vap_stats_flag_change()
 * ap_index vAP
 * enable   true/false
 * type     Device.WiFi.SSID.<vAP>.Enable = 0
 */
int vap_stats_flag_change(int ap_index, bool enable)
{
    wifi_monitor_data_t data;

    memset(&data, 0, sizeof(wifi_monitor_data_t));

    data.id = msg_id++;

    data.ap_index = ap_index;	//vap_Index

    data.u.flag.enable = enable;

    wifi_util_dbg_print(WIFI_MON, "%s:%d: flag changed vapIndex=%d enable=%d \n",
            __func__, __LINE__, ap_index, enable);

    if(enable == FALSE) {
        csi_vap_down_update(ap_index);
    }

    push_event_to_monitor_queue(&data, wifi_event_monitor_vap_stats_flag_change, NULL);


    return 0;
}

int get_sta_stats_info (assoc_dev_data_t *assoc_dev_data) {

    unsigned int vap_array_index;
    if (assoc_dev_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: NULL pointer\n", __func__, __LINE__);
        return -1;
    }

    hash_map_t *sta_map = NULL;
    sta_data_t *sta_data = NULL;
    sta_key_t sta_key;

    pthread_mutex_lock(&g_monitor_module.data_lock);

    getVAPArrayIndexFromVAPIndex((unsigned int)assoc_dev_data->ap_index, &vap_array_index);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;
    memset(sta_key, 0, STA_KEY_LEN);
    to_sta_key(assoc_dev_data->dev_stats.cli_MACAddress, sta_key);

    str_tolower(sta_key);

    sta_data = (sta_data_t *)hash_map_get(sta_map, sta_key);
    if (sta_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: NULL pointer\n", __func__, __LINE__);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return -1;
    }

    assoc_dev_data->dev_stats.cli_AuthenticationState = sta_data->dev_stats.cli_AuthenticationState;
    assoc_dev_data->dev_stats.cli_LastDataDownlinkRate = sta_data->dev_stats.cli_LastDataDownlinkRate;
    assoc_dev_data->dev_stats.cli_LastDataUplinkRate = sta_data->dev_stats.cli_LastDataUplinkRate;
    assoc_dev_data->dev_stats.cli_SignalStrength = sta_data->dev_stats.cli_SignalStrength;
    assoc_dev_data->dev_stats.cli_Retransmissions = sta_data->dev_stats.cli_Retransmissions;
    assoc_dev_data->dev_stats.cli_Active = sta_data->dev_stats.cli_Active;
    memcpy(assoc_dev_data->dev_stats.cli_OperatingStandard, sta_data->dev_stats.cli_OperatingStandard, sizeof(char)*64);
    memcpy(assoc_dev_data->dev_stats.cli_OperatingChannelBandwidth, sta_data->dev_stats.cli_OperatingChannelBandwidth, sizeof(char)*64);
    assoc_dev_data->dev_stats.cli_SNR = sta_data->dev_stats.cli_SNR;
    memcpy(assoc_dev_data->dev_stats.cli_InterferenceSources, sta_data->dev_stats.cli_InterferenceSources, sizeof(char)*64);
    assoc_dev_data->dev_stats.cli_DataFramesSentAck = sta_data->dev_stats.cli_DataFramesSentAck;
    assoc_dev_data->dev_stats.cli_DataFramesSentNoAck = sta_data->dev_stats.cli_DataFramesSentNoAck;
    assoc_dev_data->dev_stats.cli_BytesSent = sta_data->dev_stats.cli_BytesSent;
    assoc_dev_data->dev_stats.cli_BytesReceived = sta_data->dev_stats.cli_BytesReceived;
    assoc_dev_data->dev_stats.cli_RSSI = sta_data->dev_stats.cli_RSSI;
    assoc_dev_data->dev_stats.cli_MinRSSI = sta_data->dev_stats.cli_MinRSSI;
    assoc_dev_data->dev_stats.cli_MaxRSSI = sta_data->dev_stats.cli_MaxRSSI;
    assoc_dev_data->dev_stats.cli_Disassociations = sta_data->dev_stats.cli_Disassociations;
    assoc_dev_data->dev_stats.cli_AuthenticationFailures = sta_data->dev_stats.cli_AuthenticationFailures;
    assoc_dev_data->dev_stats.cli_PacketsSent = sta_data->dev_stats.cli_PacketsSent;
    assoc_dev_data->dev_stats.cli_PacketsReceived = sta_data->dev_stats.cli_PacketsReceived;
    assoc_dev_data->dev_stats.cli_ErrorsSent = sta_data->dev_stats.cli_ErrorsSent;
    assoc_dev_data->dev_stats.cli_RetransCount = sta_data->dev_stats.cli_RetransCount;
    assoc_dev_data->dev_stats.cli_FailedRetransCount = sta_data->dev_stats.cli_FailedRetransCount;
    assoc_dev_data->dev_stats.cli_RetryCount = sta_data->dev_stats.cli_RetryCount;
    assoc_dev_data->dev_stats.cli_MultipleRetryCount = sta_data->dev_stats.cli_MultipleRetryCount;

    pthread_mutex_unlock(&g_monitor_module.data_lock);
    return 0;
}

int get_sta_stats_for_vap(int ap_index, wifi_associated_dev3_t *assoc_dev_array,
                          unsigned int *output_array_size)
{
    unsigned int vap_array_index;
    unsigned int i = 0;
    hash_map_t *sta_map = NULL;
    sta_data_t *sta_data = NULL;

    pthread_mutex_lock(&g_monitor_module.data_lock);
    getVAPArrayIndexFromVAPIndex(ap_index, &vap_array_index);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;
    sta_data = hash_map_get_first(sta_map);

    if (sta_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: NULL pointer\n", __func__, __LINE__);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return -1;
    }

    while (sta_data != NULL && i < 2) { // i < 2 is temporary as HAL_IPC_MAX_STA_SUPPORT_NUM is hardcoded now to 2
        memcpy(&assoc_dev_array[i], &sta_data->dev_stats, sizeof(wifi_associated_dev3_t));
        i++;
        sta_data = hash_map_get_next(sta_map, sta_data);   
    }

    *output_array_size = i;

    pthread_mutex_unlock(&g_monitor_module.data_lock);

    return 0;
}

void send_wifi_disconnect_event_to_ctrl(mac_address_t mac_addr, unsigned int ap_index)
{
    assoc_dev_data_t assoc_data;
    memset(&assoc_data, 0, sizeof(assoc_data));

    memcpy(assoc_data.dev_stats.cli_MACAddress, mac_addr, sizeof(mac_address_t));
    assoc_data.ap_index = ap_index;
    assoc_data.reason = 0;
    push_event_to_ctrl_queue(&assoc_data, sizeof(assoc_data), wifi_event_type_hal_ind, wifi_event_hal_disassoc_device, NULL);
}

sta_data_t *create_sta_data_hash_map(hash_map_t *sta_map, mac_addr_t l_sta_mac)
{
    pthread_mutex_lock(&g_monitor_module.data_lock);
    mac_addr_str_t mac_str = { 0 };
    sta_data_t *sta = NULL;

    sta = (sta_data_t *)malloc(sizeof(sta_data_t));
    if (sta == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d malloc allocation failure\r\n", __func__, __LINE__);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return NULL;
    }
    memset(sta, 0, sizeof(sta_data_t));
    memcpy(sta->sta_mac, l_sta_mac, sizeof(mac_addr_t));
    hash_map_put(sta_map, strdup(to_mac_str(l_sta_mac, mac_str)), sta);
    pthread_mutex_unlock(&g_monitor_module.data_lock);
    return sta;
}

hash_map_t *get_sta_data_map(unsigned int vap_index)
{
    pthread_mutex_lock(&g_monitor_module.data_lock);
    unsigned int vap_array_index;
    char vap_name[16] ={ };
    convert_vap_index_to_name(&((wifi_mgr_t *)get_wifimgr_obj())->hal_cap.wifi_prop,vap_index,vap_name);
    if (strlen(vap_name) <= 0) {
        wifi_util_error_print(WIFI_MON,"%s:%d wrong vap_index:%d\r\n", __func__, __LINE__, vap_index);
        pthread_mutex_unlock(&g_monitor_module.data_lock);
        return NULL;
    }
    getVAPArrayIndexFromVAPIndex(vap_index, &vap_array_index);
    pthread_mutex_unlock(&g_monitor_module.data_lock);
    return g_monitor_module.bssid_data[vap_array_index].sta_map;
}

int set_assoc_req_frame_data(frame_data_t *msg)
{
    hash_map_t   *sta_map;
    sta_data_t   *sta;
    struct ieee80211_mgmt *frame;
    mac_addr_str_t mac_str = { 0 };
    char *str;

    frame = (struct ieee80211_mgmt *)msg->data;
    str = to_mac_str(frame->sa, mac_str);
    if (str == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d mac str convert failure\r\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    wifi_util_dbg_print(WIFI_MON,"%s:%d wifi mgmt frame message: ap_index:%d length:%d type:%d dir:%d src mac:%s rssi:%d\r\n", __func__, __LINE__, msg->frame.ap_index, msg->frame.len, msg->frame.type, msg->frame.dir, str, msg->frame.sig_dbm);

    sta_map = get_sta_data_map(msg->frame.ap_index);
    if (sta_map == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d sta_data map not found for vap_index:%d\r\n", __func__, __LINE__, msg->frame.ap_index);
        return RETURN_ERR;
    }

    sta = (sta_data_t *)hash_map_get(sta_map, mac_str);
    if (sta != NULL) {
        memset(&sta->assoc_frame_data, 0, sizeof(assoc_req_elem_t));
        memcpy(&sta->assoc_frame_data.msg_data, msg, sizeof(frame_data_t));
        time(&sta->assoc_frame_data.frame_timestamp);
    } else {
        sta = create_sta_data_hash_map(sta_map, frame->sa);
        if (sta != NULL) {
            memset(&sta->assoc_frame_data, 0, sizeof(assoc_req_elem_t));
            memcpy(&sta->assoc_frame_data.msg_data, msg, sizeof(frame_data_t));
            time(&sta->assoc_frame_data.frame_timestamp);
        } else {
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

int update_assoc_frame_data_entry(unsigned int vap_index)
{
    hash_map_t   *sta_map;
    sta_data_t   *sta;
    time_t       current_timestamp;
    mac_addr_str_t mac_str = { 0 };

    sta_map = get_sta_data_map(vap_index);
    if (sta_map == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d sta_data map not found for vap_index:%d\r\n", __func__, __LINE__, vap_index);
        return RETURN_ERR;
    }

    sta = hash_map_get_first(sta_map);
    while (sta != NULL) {
        if ((sta->connection_authorized == false) && (sta->assoc_frame_data.frame_timestamp != 0)) {
            time(&current_timestamp);

            wifi_util_dbg_print(WIFI_MON,"%s:%d assoc time:%ld, current time:%ld\r\n", __func__, __LINE__, sta->assoc_frame_data.frame_timestamp, current_timestamp);
            wifi_util_dbg_print(WIFI_MON,"%s:%d vap_index:%d sta_mac:%s\r\n", __func__, __LINE__, vap_index, to_mac_str(sta->sta_mac, mac_str));
            //If sta client disconnected and time diff more than 30 seconds then we need to reset client assoc frame data
            if ((current_timestamp - sta->assoc_frame_data.frame_timestamp) > MAX_ASSOC_FRAME_REFRESH_PERIOD) {
                wifi_util_dbg_print(WIFI_MON,"%s:%d assoc time diff:%d\r\n", __func__, __LINE__, (current_timestamp - sta->assoc_frame_data.frame_timestamp));
                memset(&sta->assoc_frame_data, 0, sizeof(assoc_req_elem_t));
            }
        }
        sta = hash_map_get_next(sta_map, sta);
    }

    return RETURN_OK;
}

void update_assoc_frame_all_vap_data_entry(void)
{
    unsigned int index,vap_index;

    wifi_mgr_t *mgr = get_wifimgr_obj();
    for (index = 0; index < getTotalNumberVAPs(); index++) {
        vap_index = VAP_INDEX(mgr->hal_cap, index);
        update_assoc_frame_data_entry(vap_index);
    }
}

static int refresh_assoc_frame_entry(void *arg)
{
    update_assoc_frame_all_vap_data_entry();
    return TIMER_TASK_COMPLETE;
}

void process_diagnostics	(unsigned int ap_index, wifi_associated_dev3_t *dev, unsigned int num_devs)
{
    hash_map_t     *sta_map = NULL;
    sta_data_t *sta = NULL, *tmp_sta = NULL;
    unsigned int i;
    wifi_associated_dev3_t	*hal_sta;
    sta_key_t	sta_key;
    char bssid[MIN_MAC_LEN+1];
    unsigned int vap_array_index;

    getVAPArrayIndexFromVAPIndex(ap_index, &vap_array_index);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;

    snprintf(bssid, MIN_MAC_LEN+1, "%02x%02x%02x%02x%02x%02x",
            g_monitor_module.bssid_data[vap_array_index].bssid[0], g_monitor_module.bssid_data[vap_array_index].bssid[1],
            g_monitor_module.bssid_data[vap_array_index].bssid[2], g_monitor_module.bssid_data[vap_array_index].bssid[3],
            g_monitor_module.bssid_data[vap_array_index].bssid[4], g_monitor_module.bssid_data[vap_array_index].bssid[5]);

    hal_sta = dev;
   memset(sta_key, 0, STA_KEY_LEN); 
    // update all sta(s) that are in the record retrieved from hal
    if (hal_sta != NULL) {
        for (i = 0; i < num_devs; i++) {
            to_sta_key(hal_sta->cli_MACAddress, sta_key);
            str_tolower(sta_key);
            sta = (sta_data_t *)hash_map_get(sta_map, sta_key);
            if (sta == NULL) {
                sta = (sta_data_t *)malloc(sizeof(sta_data_t));
                memset(sta, 0, sizeof(sta_data_t));
                memcpy(sta->sta_mac, hal_sta->cli_MACAddress, sizeof(mac_addr_t));
                hash_map_put(sta_map, strdup(sta_key), sta);
            }

            //wifi_util_dbg_print(WIFI_MON, "Current Stored for:%s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d at index:%d on vap:%d\n",
            //    to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
            //    sta->dev_stats.cli_PacketsSent, sta->dev_stats.cli_PacketsReceived, sta->dev_stats.cli_ErrorsSent,
            //    sta->dev_stats.cli_RetransCount, sta->dev_stats.cli_RetryCount, sta->dev_stats.cli_MultipleRetryCount, i, ap_index);

            memcpy((unsigned char *)&sta->dev_stats, (unsigned char *)hal_sta, sizeof(wifi_associated_dev3_t)); 

            //wifi_util_dbg_print(WIFI_MON, "Current Polled for:%s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d\n",
            //    to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
            //    hal_sta->cli_PacketsSent, hal_sta->cli_PacketsReceived, hal_sta->cli_ErrorsSent,
            //    hal_sta->cli_RetransCount, hal_sta->cli_RetryCount, hal_sta->cli_MultipleRetryCount);
            //wifi_util_dbg_print(WIFI_MON, "Current Last for: %s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d\n",
            //    to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
            //    sta->dev_stats_last.cli_PacketsSent, sta->dev_stats_last.cli_PacketsReceived, sta->dev_stats_last.cli_ErrorsSent,
            //    sta->dev_stats_last.cli_RetransCount, sta->dev_stats_last.cli_RetryCount, sta->dev_stats_last.cli_MultipleRetryCount);

            sta->updated = true;
            sta->dev_stats.cli_Active = true;
            sta->dev_stats.cli_SignalStrength = hal_sta->cli_SignalStrength;  //zqiu: use cli_SignalStrength as normalized rssi
            if (sta->dev_stats.cli_SignalStrength >= g_monitor_module.sta_health_rssi_threshold) {
                sta->good_rssi_time += g_monitor_module.poll_period;
            } else {
                sta->bad_rssi_time += g_monitor_module.poll_period;
            }

            sta->connected_time += g_monitor_module.poll_period;
            wifi_util_dbg_print(WIFI_MON, "Polled station info for, vap:%d bssid:%s ClientMac:%s Uplink rate:%d Downlink rate:%d Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d\n",
                  ap_index+1, bssid, to_sta_key(sta->dev_stats.cli_MACAddress, sta_key), sta->dev_stats.cli_LastDataUplinkRate, sta->dev_stats.cli_LastDataDownlinkRate,
                    sta->dev_stats.cli_PacketsSent, sta->dev_stats.cli_PacketsReceived, sta->dev_stats.cli_ErrorsSent, sta->dev_stats.cli_RetransCount);
            wifi_util_dbg_print(WIFI_MON, "Polled radio NF %d \n",g_monitor_module.radio_data[getRadioIndexFromAp(ap_index)].NoiseFloor);
            wifi_util_dbg_print(WIFI_MON, "Polled channel info for radio 2.4 : channel util:%d, channel interference:%d \n",
                    g_monitor_module.radio_data[0].channelUtil, g_monitor_module.radio_data[0].channelInterference);
            wifi_util_dbg_print(WIFI_MON, "Polled channel info for radio 5 : channel util:%d, channel interference:%d \n",
                    g_monitor_module.radio_data[1].channelUtil, g_monitor_module.radio_data[1].channelInterference);

            hal_sta++;

        }
    } else {
        wifi_util_dbg_print(WIFI_MON, "[%s:%d]Wi-Fi associated device map is NULL for vap_index:%d number of device:%d\r\n",
                    __func__, __LINE__, ap_index, num_devs);
    }

    // now update all sta(s) in cache that were not updated
    sta = hash_map_get_first(sta_map);
    while (sta != NULL) {

        if (sta->updated == true) {
            sta->updated = false;
        } else {
            // this was not present in hal record
            sta->disconnected_time += g_monitor_module.poll_period;
            sta->dev_stats.cli_Active = false;
            wifi_util_dbg_print(WIFI_MON, "Device:%s is disassociated from ap:%d, for %d amount of time, assoc status:%d\n",
                    to_sta_key(sta->sta_mac, sta_key), ap_index, sta->disconnected_time, sta->dev_stats.cli_Active);
            if ((sta->disconnected_time > g_monitor_module.bssid_data[vap_array_index].ap_params.rapid_reconnect_threshold) && (sta->dev_stats.cli_Active == false)) {
                tmp_sta = sta;
            }
        }

        sta = hash_map_get_next(sta_map, sta);

        if (tmp_sta != NULL) {
            wifi_util_info_print(WIFI_MON, "Device:%s being removed from map of ap:%d, and being deleted\n", to_sta_key(tmp_sta->sta_mac, sta_key), ap_index);
            wifi_util_info_print(WIFI_MON, "[%s:%d] Station info for, vap:%d bssid:%s ClientMac:%s\n", __func__, __LINE__,
                                                     (ap_index + 1), bssid, to_sta_key(tmp_sta->dev_stats.cli_MACAddress, sta_key));
            send_wifi_disconnect_event_to_ctrl(tmp_sta->sta_mac, ap_index);
            hash_map_remove(sta_map, to_sta_key(tmp_sta->sta_mac, sta_key));
            free(tmp_sta);
            tmp_sta = NULL;
        }        
    }

}

void process_deauthenticate_password_fail (unsigned int ap_index, auth_deauth_dev_t *dev)
{
    char buff[2048];
    char tmp[128];
    sta_key_t sta_key;

    wifi_util_info_print(WIFI_MON, "%s:%d Device:%s deauthenticated on ap:%d with reason : %d\n", __func__, __LINE__, to_sta_key(dev->sta_mac, sta_key), ap_index, dev->reason);

    /*Wrong password on private, Xfinity Home and LNF SSIDs*/
    if ((dev->reason == 2) && ( isVapPrivate(ap_index) || isVapXhs(ap_index) || isVapLnfPsk(ap_index) ) ) {
        get_formatted_time(tmp);

        snprintf(buff, 2048, "%s WIFI_PASSWORD_FAIL:%d,%s\n", tmp, ap_index + 1, to_sta_key(dev->sta_mac, sta_key));
        /* send telemetry of password failure */
        write_to_file(wifi_health_log, buff);
    }
    /*ARRISXB6-11979 Possible Wrong WPS key on private SSIDs*/
    if ((dev->reason == 2 || dev->reason == 14 || dev->reason == 19) && ( isVapPrivate(ap_index) ))  {
        get_formatted_time(tmp);

        snprintf(buff, 2048, "%s WIFI_POSSIBLE_WPS_PSK_FAIL:%d,%s,%d\n", tmp, ap_index + 1, to_sta_key(dev->sta_mac, sta_key), dev->reason);
        /* send telemetry of WPS failure */
        write_to_file(wifi_health_log, buff);
    }
}

void process_deauthenticate	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    char buff[2048];
    char tmp[128];
    sta_key_t sta_key;

    wifi_util_info_print(WIFI_MON, "%s:%d Device:%s deauthenticated on ap:%d with reason : %d\n", __func__, __LINE__, to_sta_key(dev->sta_mac, sta_key), ap_index, dev->reason);

    /*Wrong password on private, Xfinity Home and LNF SSIDs*/
    if ((dev->reason == 2) && ( isVapPrivate(ap_index) || isVapXhs(ap_index) || isVapLnfPsk(ap_index) ) ) {
        get_formatted_time(tmp);

        snprintf(buff, 2048, "%s WIFI_PASSWORD_FAIL:%d,%s\n", tmp, ap_index + 1, to_sta_key(dev->sta_mac, sta_key));
        /* send telemetry of password failure */
        write_to_file(wifi_health_log, buff);
    }
    /*ARRISXB6-11979 Possible Wrong WPS key on private SSIDs*/
    if ((dev->reason == 2 || dev->reason == 14 || dev->reason == 19) && ( isVapPrivate(ap_index) ))  {
        get_formatted_time(tmp);

        snprintf(buff, 2048, "%s WIFI_POSSIBLE_WPS_PSK_FAIL:%d,%s,%d\n", tmp, ap_index + 1, to_sta_key(dev->sta_mac, sta_key), dev->reason);
        /* send telemetry of WPS failure */
        write_to_file(wifi_health_log, buff);
    }
    /*Calling process_disconnect as station is disconncetd from vAP*/
    process_disconnect(ap_index, dev);
}
#endif // CCSP_COMMON

void process_connect(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
#ifdef CCSP_COMMON
    struct timeval tv_now;
    unsigned int i = 0;
    int vap_status = 0;
    wifi_mgr_t *mgr = get_wifimgr_obj();
#endif // CCSP_COMMON
    unsigned int vap_array_index;
    getVAPArrayIndexFromVAPIndex(ap_index, &vap_array_index);

    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;

    wifi_util_info_print(WIFI_MON, "sta map: %p Device:%s connected on ap:%d\n", sta_map, to_sta_key(dev->sta_mac, sta_key), ap_index);
    to_sta_key(dev->sta_mac, sta_key);
    str_tolower(sta_key);
    sta = (sta_data_t *)hash_map_get(sta_map, sta_key);
    if (sta == NULL) { /* new client */
        sta = (sta_data_t *)malloc(sizeof(sta_data_t));
        memset(sta, 0, sizeof(sta_data_t));
        memcpy(sta->sta_mac, dev->sta_mac, sizeof(mac_addr_t));
        memcpy(sta->dev_stats.cli_MACAddress, dev->sta_mac, sizeof(mac_addr_t));
        hash_map_put(sta_map, strdup(sta_key), sta);
    }

#ifdef CCSP_COMMON
    sta->total_disconnected_time += sta->disconnected_time;
    sta->disconnected_time = 0;

    gettimeofday(&tv_now, NULL);
    if(!sta->assoc_monitor_start_time)
        sta->assoc_monitor_start_time = tv_now.tv_sec;

    if ((UINT)(tv_now.tv_sec - sta->last_disconnected_time.tv_sec) <= g_monitor_module.bssid_data[i].ap_params.rapid_reconnect_threshold) {
        if (sta->dev_stats.cli_Active == false) {
            wifi_util_dbg_print(WIFI_MON, "Device:%s connected on ap:%d connected within rapid reconnect time\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
            sta->rapid_reconnects++;
        }
    }	
    else {
            wifi_util_dbg_print(WIFI_MON, "Device:%s connected on ap:%d received another connection event\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
        }

    sta->last_connected_time.tv_sec = tv_now.tv_sec;
    sta->last_connected_time.tv_usec = tv_now.tv_usec;

    /* reset stats of client */
    memset((unsigned char *)&sta->dev_stats_last, 0, sizeof(wifi_associated_dev3_t));
#endif // CCSP_COMMON
    memset((unsigned char *)&sta->dev_stats, 0, sizeof(wifi_associated_dev3_t));
    sta->dev_stats.cli_Active = true;
#ifdef CCSP_COMMON
    sta->connection_authorized = true;
    /*To avoid duplicate entries in hash map of different vAPs eg:RDKB-21582
      Also when clients moved away from a vAP and connect back to other vAP this will be usefull*/
    for (i = 0; i < getTotalNumberVAPs(); i++) {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
        UINT radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        if ( vap_index == ap_index)
            continue;
        vap_status = g_monitor_module.bssid_data[vap_index].ap_params.ap_status;
        if (vap_status) {
            sta_map = g_monitor_module.bssid_data[i].sta_map;
            to_sta_key(dev->sta_mac, sta_key);
            str_tolower(sta_key);
            sta = (sta_data_t *)hash_map_get(sta_map, sta_key);
            if ((sta != NULL) && (sta->dev_stats.cli_Active == true)) {
                sta->dev_stats.cli_Active = false;
            } else if ((sta != NULL) && (sta->connection_authorized == true)) {
                sta->connection_authorized = false;
            }
        }
    }
#endif // CCSP_COMMON
}

void process_disconnect	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
#ifdef CCSP_COMMON
    struct timeval tv_now;
    instant_msmt_t msmt;
#endif // CCSP_COMMON
    unsigned int vap_array_index;

    getVAPArrayIndexFromVAPIndex(ap_index, &vap_array_index);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;
    wifi_util_info_print(WIFI_MON, "Device:%s disconnected on ap:%d\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
    str_tolower(sta_key);
    sta = (sta_data_t *)hash_map_get(sta_map, sta_key);
    if (sta == NULL) {
        wifi_util_error_print(WIFI_MON, "Device:%s could not be found on sta map of ap:%d\n", sta_key, ap_index);
        return;
    }

#ifdef CCSP_COMMON
    sta->total_connected_time += sta->connected_time;
    sta->connected_time = 0;
    sta->dev_stats.cli_Active = false;
    sta->connection_authorized = false;
    gettimeofday(&tv_now, NULL);
    if(!sta->deauth_monitor_start_time)
        sta->deauth_monitor_start_time = tv_now.tv_sec;

    sta->last_disconnected_time.tv_sec = tv_now.tv_sec;
    sta->last_disconnected_time.tv_usec = tv_now.tv_usec;

    // stop instant measurements if its going on with this client device
    msmt.ap_index = ap_index;
    memcpy(msmt.sta_mac, dev->sta_mac, sizeof(mac_address_t));
    /* stop the instant measurement only if the client for which instant measuremnt
      is running got disconnected from AP
      */
    if (memcmp(g_monitor_module.inst_msmt.sta_mac, msmt.sta_mac, sizeof(mac_address_t)) == 0)
    {
        process_instant_msmt_stop(ap_index, &msmt);
    }
#else
    sta = hash_map_remove(sta_map, sta_key);
    if (sta != NULL) {
        free(sta);
    }
#endif // CCSP_COMMON
}

#ifdef CCSP_COMMON
void process_instant_msmt_start	(unsigned int ap_index, instant_msmt_t *msmt)
{
    memcpy(g_monitor_module.inst_msmt.sta_mac, msmt->sta_mac, sizeof(mac_address_t));
    g_monitor_module.inst_msmt.ap_index = ap_index;
    g_monitor_module.poll_period = g_monitor_module.instantPollPeriod;
    g_monitor_module.inst_msmt.active = g_monitor_module.instntMsmtenable;

    if((g_monitor_module.instantDefOverrideTTL == 0) || (g_monitor_module.instantPollPeriod == 0))
        g_monitor_module.maxCount = 0;
    else
        g_monitor_module.maxCount = g_monitor_module.instantDefOverrideTTL/g_monitor_module.instantPollPeriod;

    g_monitor_module.count = 0;
    wifi_util_dbg_print(WIFI_MON, "%s:%d: count:%d, maxCount:%d, TTL:%d, poll:%d\n",__func__, __LINE__, 
            g_monitor_module.count, g_monitor_module.maxCount, g_monitor_module.instantDefOverrideTTL, g_monitor_module.instantPollPeriod);

    //Stopping telemetry while running instant measurement.
    scheduler_telemetry_tasks();
    if (g_monitor_module.instantPollPeriod != 0) {
        scheduler_add_timer_task(g_monitor_module.sched, TRUE, &g_monitor_module.inst_msmt_id,
                process_instant_msmt_monitor, NULL, (g_monitor_module.instantPollPeriod*1000), 0);
    }
}
#endif // CCSP_COMMON

static void active_msmt_log_message(char *fmt, ...)
{
    va_list args;
    char msg[1024] = {};

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

#ifdef CCSP_COMMON
    CcspTraceDebug((msg));
#endif // CCSP_COMMON

    wifi_util_dbg_print(WIFI_MON, msg);
}

static char *active_msmt_status_to_str(active_msmt_status_t status)
{
    switch (status)
    {
        case ACTIVE_MSMT_STATUS_SUCCEED:
            return "SUCCEED";
        case ACTIVE_MSMT_STATUS_CANCELED:
            return "CANCELED";
        case ACTIVE_MSMT_STATUS_FAILED:
            return "FAILED";
        case ACTIVE_MSMT_STATUS_BUSY:
            return "BUSY";
        case ACTIVE_MSMT_STATUS_NO_CLIENT:
            return "NO_CLIENT";
        case ACTIVE_MSMT_STATUS_SLEEP_CLIENT:
            return "SLEEP_CLIENT";
        case ACTIVE_MSMT_STATUS_WRONG_ARG:
            return "WRONG_ARG";
        default:
            return "UNDEFINED";
    }
}

static void active_msmt_set_status_desc(const char *func, unsigned char *plan_id, unsigned int step_id,
    unsigned char *dst_mac, char *msg)
{
    char mac[MAC_ADDRESS_LENGTH] = {};

    memset(&g_active_msmt.status_desc, 0, sizeof(g_active_msmt.status_desc));

    if (strlen((char *)dst_mac) != MAC_ADDRESS_LENGTH - 1) {
        snprintf(mac, sizeof(mac), MAC_FMT_TRIMMED, MAC_ARG(dst_mac));
    } else {
        strncpy(mac, (char *)dst_mac, sizeof(mac) - 1);
    }

    snprintf(g_active_msmt.status_desc, sizeof(g_active_msmt.status_desc),
        "Plan[%s] Step[%d] Mac[%s] FW[] Node[] Status[%s] %s",
        (char *)plan_id ?: "", step_id, mac, active_msmt_status_to_str(g_active_msmt.status), msg ?: "");

    wifi_util_dbg_print(WIFI_MON, "%s: %s\n", func, g_active_msmt.status_desc);
}

static void active_msmt_report_error(const char *func, unsigned char *plan_id, active_msmt_step_t *step,
    char *msg, active_msmt_status_t status)
{
    SetActiveMsmtStatus(__func__, status);
    active_msmt_set_status_desc(func, plan_id, step->StepId, step->DestMac, msg);

    SetActiveMsmtPlanID((char *)plan_id);
    g_active_msmt.curStepData.ApIndex = 0;
    g_active_msmt.curStepData.StepId = step->StepId;
    stream_client_msmt_data(true);
}

static void active_msmt_set_step_status(const char *func, ULONG StepIns, active_msmt_step_status_t value)
{
    wifi_util_dbg_print(WIFI_MON, "%s: updating stepIns to %d step: %d\n", func, value, StepIns);

    g_active_msmt.active_msmt.StepInstance[StepIns] = value;
}

static void active_msmt_report_all_steps(active_msmt_t *cfg, char *msg, active_msmt_status_t status)
{
    // Send MQTT report for all configured steps
    for (unsigned int i = 0; i < MAX_STEP_COUNT; i++) {
        if (strlen((char *) cfg->Step[i].DestMac) != 0) {
            active_msmt_report_error(__func__, cfg->PlanId, &cfg->Step[i], msg, status);
            continue;
        }
    }
}

static bool DeviceMemory_DataGet(uint32_t *util_mem)
{
    const char *filename = LINUX_PROC_MEMINFO_FILE;
    FILE *proc_file = NULL;
    char buf[256] = {'\0'};
    uint32_t mem_total = 0;
    uint32_t mem_free = 0;

    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        wifi_util_dbg_print(WIFI_MON,"Failed opening file: %s\n", filename);
        return false;
    }

    while (fgets(buf, sizeof(buf), proc_file) != NULL)
    {
        if (strncmp(buf, "MemTotal:", strlen("MemTotal:")) == 0)
        {
            if (sscanf(buf, "MemTotal: %u", &mem_total) != 1)
                goto parse_error;
        } else if (strncmp(buf, "MemFree:", strlen("MemFree:")) == 0) {
            if (sscanf(buf, "MemFree: %u", &mem_free) != 1)
                goto parse_error;
        }
    }
    wifi_util_dbg_print(WIFI_MON," Returned MemTotal is %d and MemFree is %d\n", mem_total, mem_free);
    *util_mem = mem_total - mem_free;
    fclose(proc_file);
    return true;

parse_error:
    fclose(proc_file);
    wifi_util_dbg_print(WIFI_MON,"Error parsing %s.\n", filename);
    return false;
}

static bool DeviceLoad_DataGet(active_msmt_resources_t *res)
{
    int32_t     rc;
    const char  *filename = LINUX_PROC_LOADAVG_FILE;
    FILE        *proc_file = NULL;

    proc_file = fopen(filename, "r");
    if (proc_file == NULL)
    {
        wifi_util_dbg_print(WIFI_MON,"Parsing device stats (Failed to open %s)\n", filename);
        return false;
    }

    rc = fscanf(proc_file,
            "%lf %lf %lf",
            &res->cpu_one,
            &res->cpu_five,
            &res->cpu_fifteen);

    fclose(proc_file);

    wifi_util_dbg_print(WIFI_MON," Returned %d and Parsed device load %0.2f %0.2f %0.2f\n", rc,
            res->cpu_one,
            res->cpu_five,
            res->cpu_fifteen);

    return true;
}

static bool DeviceCpuUtil_DataGet(unsigned int *util_cpu)
{
    FILE *fp;
    char buff[256] = {};
    char token[] = "cpu ";
    uint64_t hz_user, hz_nice, hz_system, hz_idle;
    static uint64_t prev_hz_user, prev_hz_nice, prev_hz_system, prev_hz_idle;
    static bool init = true;
    uint64_t hz_total;
    double busy;

    if ((fp = fopen(LINUX_PROC_STAT_FILE, "r")) == NULL) {
        wifi_util_dbg_print(WIFI_MON, "Failed to open file: %s\n", LINUX_PROC_STAT_FILE);
        *util_cpu = 0;
        return false;
    }

    while (fgets(buff, sizeof(buff), fp) != NULL) {

        if (strncmp(buff, token, sizeof(token) - 1) != 0) {
            continue;
        }

        sscanf(buff, "cpu %llu %llu %llu %llu", &hz_user, &hz_nice, &hz_system, &hz_idle);

        if (init == true) {

            *util_cpu = 0;
            prev_hz_user = hz_user;
            prev_hz_nice = hz_nice;
            prev_hz_system = hz_system;
            prev_hz_idle = hz_idle;
            init = false;

            break;
        }

        hz_total = (hz_user - prev_hz_user) + (hz_nice - prev_hz_nice) + (hz_system - prev_hz_system) + (hz_idle - prev_hz_idle);
        busy = (1.0 - ((double)(hz_idle - prev_hz_idle) / (double)hz_total)) * 100.0;
        *util_cpu = (unsigned int) (busy + 0.5);

        prev_hz_user = hz_user;
        prev_hz_nice = hz_nice;
        prev_hz_system = hz_system;
        prev_hz_idle = hz_idle;

        break;
    }

    fclose(fp);
    return true;
}

/* Calculate CPU util during specified period */
static bool active_msmt_calc_cpu_util(unsigned int period, unsigned int *util_cpu)
{
    if (DeviceCpuUtil_DataGet(util_cpu) == false)
        return false;

    sleep(period);

    if (DeviceCpuUtil_DataGet(util_cpu) == false)
        return false;

    wifi_util_dbg_print(WIFI_MON, "%s:%d Calculated CPU util: %u%%\n", __func__, __LINE__, *util_cpu);

    return true;
}

static void active_msmt_queue_push(active_msmt_queue_t **q, active_msmt_t *data)
{
    active_msmt_queue_t **it = q;

    for (; it && *it; it = &(*it)->next);

    if ((*it = calloc(1, sizeof(active_msmt_queue_t))) == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d Unable to allocate memory!\n", __func__, __LINE__);
        return;
    }

    wifi_util_dbg_print(WIFI_MON, "%s:%d Pushing [%s] to queue\n", __func__, __LINE__, data->PlanId);
    memcpy(&(*it)->data, data, sizeof(active_msmt_t));
}

static void *active_msmt_worker(void *ctx)
{
    int oldcanceltype;
    active_msmt_t *cfg;
    active_msmt_queue_t *it = g_active_msmt.queue;
    active_msmt_t *act_msmt = &g_active_msmt.active_msmt;
    active_msmt_status_t status;
    wifi_mgr_t *mgr = get_wifimgr_obj();
    wifi_ctrl_t *ctrl = &mgr->ctrl;
    char msg[256] = {};
    bool report = false;

    prctl(PR_SET_NAME,  __func__, 0, 0, 0);

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldcanceltype);
    UNREFERENCED_PARAMETER(ctx);

    pthread_mutex_lock(&g_active_msmt.lock);
    g_active_msmt.is_running = true;
    pthread_mutex_unlock(&g_active_msmt.lock);

    while (true) {

        pthread_mutex_lock(&g_active_msmt.lock);

        if (it == NULL) {
            g_active_msmt.is_running = false;
            pthread_mutex_unlock(&g_active_msmt.lock);
            break;
        }

        cfg = &it->data;

        SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_SUCCEED);
        ResetActiveMsmtStepInstances();

        if (ctrl->network_mode == rdk_dev_mode_type_ext) {

            if (ActiveMsmtConfValidation(cfg) != RETURN_OK) {
                wifi_util_error_print(WIFI_MON, "%s:%d Active measurement conf is invalid!\n", __func__, __LINE__);
                mgr->ctrl.webconfig_state |= ctrl_webconfig_state_blaster_cfg_complete_rsp_pending;

                g_active_msmt.queue = it->next;
                free(it);
                it = g_active_msmt.queue;

                pthread_mutex_unlock(&g_active_msmt.lock);
                continue;
            }

            SetActiveMsmtSampleDuration(cfg->ActiveMsmtSampleDuration / cfg->ActiveMsmtNumberOfSamples);
            SetBlasterMqttTopic((char *)cfg->blaster_mqtt_topic);
        } else {
            SetActiveMsmtSampleDuration(cfg->ActiveMsmtSampleDuration);
        }

        SetActiveMsmtPktSize(cfg->ActiveMsmtPktSize);
        SetActiveMsmtNumberOfSamples(cfg->ActiveMsmtNumberOfSamples);
        SetActiveMsmtPlanID((char *)cfg->PlanId);

        for (unsigned int i = 0; i < MAX_STEP_COUNT; i++) {
            if(strlen((char *) cfg->Step[i].DestMac) != 0) {
                SetActiveMsmtStepID(cfg->Step[i].StepId, i);
                SetActiveMsmtStepDstMac((char *)cfg->Step[i].DestMac, i);
                SetActiveMsmtStepSrcMac((char *)cfg->Step[i].SrcMac, i);
            }
        }

        SetActiveMsmtEnable(cfg->ActiveMsmtEnable);

        if (DeviceMemory_DataGet(&act_msmt->ActiveMsmtResources.util_mem) != true ||
            DeviceLoad_DataGet(&act_msmt->ActiveMsmtResources) != true ||
            active_msmt_calc_cpu_util(WIFI_BLASTER_CPU_CALC_PERIOD, &act_msmt->ActiveMsmtResources.util_cpu) != true) {

            snprintf(msg, sizeof(msg), "Failed to fill in health stats");
            status = ACTIVE_MSMT_STATUS_FAILED;
            report = true;

        } else if (act_msmt->ActiveMsmtResources.util_cpu > WIFI_BLASTER_CPU_THRESHOLD) {

            snprintf(msg, sizeof(msg), "Skip because of high CPU usage [%u]%%. Threshold [%d]%%",
                act_msmt->ActiveMsmtResources.util_cpu, WIFI_BLASTER_CPU_THRESHOLD);
            status = ACTIVE_MSMT_STATUS_BUSY;
            report = true;

        } else if (act_msmt->ActiveMsmtResources.util_mem < WIFI_BLASTER_MEM_THRESHOLD) {

            snprintf(msg, sizeof(msg), "Skip because of low free RAM memory [%u]KB.  Threshold [%d]KB",
                act_msmt->ActiveMsmtResources.util_mem, WIFI_BLASTER_MEM_THRESHOLD);
            status = ACTIVE_MSMT_STATUS_BUSY;
            report = true;
        }

        if (report == true) {

            if (ctrl->network_mode == rdk_dev_mode_type_ext) {
                active_msmt_report_all_steps(cfg, msg, status);
            }

            active_msmt_log_message( "%s:%d %s\n" ,__FUNCTION__, __LINE__, msg);
            mgr->ctrl.webconfig_state |= ctrl_webconfig_state_blaster_cfg_complete_rsp_pending;

            g_active_msmt.queue = it->next;
            free(it);
            it = g_active_msmt.queue;
            report = false;

            pthread_mutex_unlock(&g_active_msmt.lock);
            continue;
        }

        pthread_mutex_unlock(&g_active_msmt.lock);

        active_msmt_log_message("%s:%d Starting to proceed PlanId [%s]\n", __func__, __LINE__, cfg->PlanId);
        WiFiBlastClient();

        pthread_mutex_lock(&g_active_msmt.lock);
        g_active_msmt.queue = it->next;
        free(it);
        it = g_active_msmt.queue;
        pthread_mutex_unlock(&g_active_msmt.lock);
    }

    active_msmt_log_message("%s:%d Exit\n", __func__, __LINE__);

    return NULL;
}

static unsigned int active_msmt_step_inst_get_available(unsigned int StepIns)
{
    active_msmt_t *cfg = &g_active_msmt.active_msmt;

    if (strlen((char *)cfg->Step[StepIns].DestMac) == 0) {
        return StepIns;
    }

    for (unsigned int i = StepIns + 1; i < MAX_STEP_COUNT; i++) {
        if (strlen((char *) cfg->Step[i].DestMac) == 0) {
            return i;
        }
    }

    return StepIns;
}

static bool active_msmt_step_inst_is_configured(char *dst_mac)
{
    active_msmt_t *cfg = &g_active_msmt.active_msmt;

    for (unsigned int i = 0; i < MAX_STEP_COUNT; i++) {
        if (strlen((char *)cfg->Step[i].DestMac) != 0) {
            char mac[MAC_ADDRESS_LENGTH] = {};

            snprintf(mac, sizeof(mac), MAC_FMT_TRIMMED, MAC_ARG(cfg->Step[i].DestMac));

            if (strcasecmp(dst_mac, mac) == 0) {
                active_msmt_log_message("%s:%d: MAC [%s] is configured at Step:%d\n", __func__, __LINE__, dst_mac, i);
                return true;
            }
        }
    }

    return false;
}

/* This function process the active measurement step info
  from the active_msmt_monitor thread and calls wifiblaster. 
  */

void process_active_msmt_step(active_msmt_t *cfg)
{
    pthread_t id;
    pthread_attr_t attr;
    pthread_attr_t *attrp = NULL;
    active_msmt_t *act_msmt = &g_active_msmt.active_msmt;
    wifi_ctrl_t *ctrl = get_wifictrl_obj();

    active_msmt_log_message("%s:%d: calling process_active_msmt_step\n",__func__, __LINE__);

    pthread_mutex_lock(&g_active_msmt.lock);

    if (g_active_msmt.is_running == true && !strncasecmp((char *)cfg->PlanId, (char *)act_msmt->PlanId, strlen((char *)cfg->PlanId))) {

        if (cfg->ActiveMsmtEnable == false) {
            act_msmt->ActiveMsmtEnable = false;

            active_msmt_log_message("%s:%d: Canceling %s\n",__func__, __LINE__, cfg->PlanId);
            pthread_mutex_unlock(&g_active_msmt.lock);

            return;
        }

        if (ctrl->network_mode == rdk_dev_mode_type_ext && ActiveMsmtConfValidation(cfg) != RETURN_OK) {

            wifi_util_error_print(WIFI_MON, "%s:%d Active measurement conf for %s is invalid!\n", __func__, __LINE__, cfg->PlanId);
            act_msmt->ActiveMsmtEnable = false;
            pthread_mutex_unlock(&g_active_msmt.lock);

            return;
        }

        active_msmt_log_message("%s:%d: Changing configuration for PlanId [%s]\n",__func__, __LINE__, cfg->PlanId);

        /* Cloud could update SampleDuration, PktSize, NumberOfSamples during 
         * sampling
         */

        if (ctrl->network_mode == rdk_dev_mode_type_ext) {
            SetActiveMsmtSampleDuration(cfg->ActiveMsmtSampleDuration / cfg->ActiveMsmtNumberOfSamples);
        } else {
            SetActiveMsmtSampleDuration(cfg->ActiveMsmtSampleDuration);
        }

        SetActiveMsmtPktSize(cfg->ActiveMsmtPktSize);
        SetActiveMsmtNumberOfSamples(cfg->ActiveMsmtNumberOfSamples);

        /* Cloud could only extend the previous PlanId request with new set of
         * steps. Old ones couldn't be overwritten
         */

        for (unsigned int i = 0; i < MAX_STEP_COUNT; i++) {

            if (strlen((char *) cfg->Step[i].DestMac) != 0 &&
                active_msmt_step_inst_is_configured((char *)cfg->Step[i].DestMac) == false) {

                unsigned int StepIns = active_msmt_step_inst_get_available(i);

                SetActiveMsmtStepID(cfg->Step[i].StepId, StepIns);
                SetActiveMsmtStepDstMac((char *)cfg->Step[i].DestMac, StepIns);
                SetActiveMsmtStepSrcMac((char *)cfg->Step[i].SrcMac, StepIns);
            }
        }

        pthread_mutex_unlock(&g_active_msmt.lock);

        return;
    }

    active_msmt_queue_push(&g_active_msmt.queue, cfg);

    if (g_active_msmt.is_running == false) {

        attrp = &attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&id, attrp, active_msmt_worker, NULL) != 0) {
            active_msmt_log_message("%s:%d: Fail to spawn 'active_msmt_worker' thread errno: %d - %s\n",
                __FUNCTION__, __LINE__, errno, strerror(errno));

            if(attrp != NULL) {
                pthread_attr_destroy(attrp);
            }

            pthread_mutex_unlock(&g_active_msmt.lock);
            return;
        }

        active_msmt_log_message("%s:%d: Sucessfully created thread for starting blast\n", __FUNCTION__, __LINE__);

        if(attrp != NULL) {
            pthread_attr_destroy(attrp);
        }
    }

    pthread_mutex_unlock(&g_active_msmt.lock);
    wifi_util_dbg_print(WIFI_MON, "%s:%d: exiting this function\n",__func__, __LINE__);
}

#ifdef CCSP_COMMON
void process_instant_msmt_stop  (unsigned int ap_index, instant_msmt_t *msmt)
{
    /*if ((g_monitor_module.inst_msmt.active == true) && (memcmp(g_monitor_module.inst_msmt.sta_mac, msmt->sta_mac, sizeof(mac_address_t)) == 0)) {
      g_monitor_module.inst_msmt.active = false;
      g_monitor_module.poll_period = DEFAULT_INSTANT_POLL_TIME;
      g_monitor_module.maxCount = g_monitor_module.instantDefReportPeriod/DEFAULT_INSTANT_POLL_TIME;
      g_monitor_module.count = 0;
      }*/
    UNREFERENCED_PARAMETER(msmt);
    g_monitor_module.inst_msmt.active = false;
    g_monitor_module.poll_period = DEFAULT_INSTANT_POLL_TIME;
    g_monitor_module.maxCount = 0;
    g_monitor_module.count = 0;

    //Restarting telemetry after stopping instant measurement.  
    scheduler_telemetry_tasks();
    if (g_monitor_module.inst_msmt_id != 0) {
        scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.inst_msmt_id);
        g_monitor_module.inst_msmt_id = 0;
    }
}

int process_instant_msmt_monitor(void *arg)
{
    if (g_monitor_module.count >= g_monitor_module.maxCount) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d: instant polling freq reached threshold\n", __func__, __LINE__);
        g_monitor_module.instantDefOverrideTTL = DEFAULT_INSTANT_REPORT_TIME;
        g_monitor_module.instntMsmtenable = false;
        process_instant_msmt_stop(g_monitor_module.inst_msmt.ap_index, &g_monitor_module.inst_msmt);
    } else {
        g_monitor_module.count += 1;
        wifi_util_dbg_print(WIFI_MON, "%s:%d: client %s on ap %d\n", __func__, __LINE__, g_monitor_module.instantMac, g_monitor_module.inst_msmt.ap_index);
        associated_client_diagnostics(); //for single client
        stream_client_msmt_data(false);
    }

    return TIMER_TASK_COMPLETE;
}

int get_neighbor_scan_results() 
{
    wifi_monitor_t *monitor_param = (wifi_monitor_t *)get_wifi_monitor();
    wifi_neighbor_ap2_t *NeighResult = NULL;
    wifi_neighbor_ap2_t *pTmp;
    UINT count = 0;

    monitor_param->neighbor_scan_cfg.ResultCount = 0;
    
    for(UINT rIdx = 0; rIdx < getNumberRadios(); rIdx++)
    {
        if (g_monitor_module.radio_presence[rIdx] == false) {
           continue;
        }
        if (wifi_getNeighboringWiFiStatus(rIdx, &NeighResult,&count) == RETURN_OK)
        {
            pTmp = monitor_param->neighbor_scan_cfg.pResult[rIdx];
            monitor_param->neighbor_scan_cfg.pResult[rIdx] = NeighResult;
            monitor_param->neighbor_scan_cfg.resultCountPerRadio[rIdx] = count;
            if(pTmp) {
                free(pTmp);
                pTmp = NULL;
            }
        }
        else if (NeighResult != NULL) {
            free(NeighResult);
            NeighResult = NULL;
        }
        monitor_param->neighbor_scan_cfg.ResultCount += monitor_param->neighbor_scan_cfg.resultCountPerRadio[rIdx];
    }
    monitor_param->neighbor_scan_cfg.ResultCount = (monitor_param->neighbor_scan_cfg.ResultCount > MAX_NEIGHBOURS) ? MAX_NEIGHBOURS : monitor_param->neighbor_scan_cfg.ResultCount;
    strcpy_s(monitor_param->neighbor_scan_cfg.DiagnosticsState, sizeof(monitor_param->neighbor_scan_cfg.DiagnosticsState) , "Completed");
    return TIMER_TASK_COMPLETE;
}

int get_neighbor_scan_cfg(int radio_index, 
                          wifi_neighbor_ap2_t *neighbor_results,
                          unsigned int *output_array_size)
{
    wifi_monitor_t *monitor_param = (wifi_monitor_t *)get_wifi_monitor();

    pthread_mutex_lock(&g_monitor_module.data_lock);
    *output_array_size = monitor_param->neighbor_scan_cfg.resultCountPerRadio[radio_index];
    memcpy(neighbor_results, monitor_param->neighbor_scan_cfg.pResult[radio_index], (*output_array_size)*sizeof(wifi_neighbor_ap2_t));
    pthread_mutex_unlock(&g_monitor_module.data_lock);

    return 0;
}

int process_periodical_neighbor_scan(void *arg)
{
    wifi_monitor_t *monitor_param = (wifi_monitor_t *)get_wifi_monitor();
    wifi_radio_operationParam_t *wifi_radio_oper_param = NULL;
    wifi_neighborScanMode_t scan_mode = WIFI_RADIO_SCAN_MODE_FULL;
    int dwell_time = 20;

    if(strcmp(monitor_param->neighbor_scan_cfg.DiagnosticsState, "Requested") == 0) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d: Scan already in Progress!!!\n", __func__, __LINE__);
    } else {
        strcpy_s(monitor_param->neighbor_scan_cfg.DiagnosticsState, sizeof(monitor_param->neighbor_scan_cfg.DiagnosticsState) , "Requested");

        for(UINT rIdx = 0; rIdx < getNumberRadios(); rIdx++)
        {
            if (g_monitor_module.radio_presence[rIdx] == false) {
                continue;
            }
            wifi_radio_oper_param = (wifi_radio_operationParam_t *)get_wifidb_radio_map(rIdx);
            wifi_startNeighborScan(rIdx, scan_mode, ((wifi_radio_oper_param->band == WIFI_FREQUENCY_6_BAND) ? (dwell_time=110) : dwell_time), 0, NULL);
        }
        scheduler_add_timer_task(g_monitor_module.sched, FALSE, &neighscan_task_id, get_neighbor_scan_results, NULL,
                    NEIGHBOR_SCAN_RESULT_INTERVAL, 1);
    }
    return TIMER_TASK_COMPLETE;
}
#endif // CCSP_COMMON

void *monitor_function  (void *data)
{
    wifi_monitor_t *proc_data;
    struct timespec time_to_wait;
    struct timeval tv_now;
    wifi_event_t *event;
    wifi_monitor_data_t        *event_data = NULL;
    int rc;
    struct timeval t_start;
    struct timeval interval;
    struct timeval timeout;
    timerclear(&t_start);

    prctl(PR_SET_NAME,  __func__, 0, 0, 0);

    proc_data = (wifi_monitor_t *)data;

    pthread_mutex_lock(&proc_data->queue_lock);
    while (proc_data->exit_monitor == false) {
        gettimeofday(&tv_now, NULL);

        interval.tv_sec = 0;
        interval.tv_usec = MONITOR_RUNNING_INTERVAL_IN_MILLISEC * 1000;
        timeradd(&t_start, &interval, &timeout);

        time_to_wait.tv_sec = timeout.tv_sec;
        time_to_wait.tv_nsec = timeout.tv_usec*1000;

        rc = 0;
        if (queue_count(proc_data->queue) == 0) {
            rc = pthread_cond_timedwait(&proc_data->cond, &proc_data->queue_lock, &time_to_wait);
        }

        if ((rc == 0) || (queue_count(proc_data->queue) != 0)) {
            // dequeue data
            while (queue_count(proc_data->queue)) {
                event = queue_pop(proc_data->queue);
                if (event == NULL) {
                    continue;
                }

                pthread_mutex_unlock(&proc_data->queue_lock);

                event_data = event->u.mon_data;

#ifdef CCSP_COMMON
                //Send data to wifi_events library
                events_rbus_publish(event);
#endif // CCSP_COMMON
                switch (event->sub_type) {
#ifdef CCSP_COMMON
                    case wifi_event_monitor_diagnostics:
                        //process_diagnostics(event_data->ap_index, &event_data->.devs);
                    break;
#endif // CCSP_COMMON

                    case wifi_event_monitor_connect:
                        process_connect(event_data->ap_index, &event_data->u.dev);
                    break;

                    case wifi_event_monitor_disconnect:
                        process_disconnect(event_data->ap_index, &event_data->u.dev);
                    break;
#ifdef CCSP_COMMON

                    case wifi_event_monitor_deauthenticate_password_fail:
                        process_deauthenticate_password_fail(event_data->ap_index, &event_data->u.dev);
                    break;

                    case wifi_event_monitor_deauthenticate:
                        process_deauthenticate(event_data->ap_index, &event_data->u.dev);
                    break;

                    case wifi_event_monitor_stop_inst_msmt:
                        process_instant_msmt_stop(event_data->ap_index, &event_data->u.imsmt);
                    break;

                    case wifi_event_monitor_start_inst_msmt:
                        process_instant_msmt_start(event_data->ap_index, &event_data->u.imsmt);
                    break;

                    case wifi_event_monitor_stats_flag_change:
                        process_stats_flag_changed(event_data->ap_index, &event_data->u.flag);
                    break;
                    case wifi_event_monitor_radio_stats_flag_change:
                        radio_stats_flag_changed(event_data->ap_index, &event_data->u.flag);
                    break;
                    case wifi_event_monitor_vap_stats_flag_change:
                        vap_stats_flag_changed(event_data->ap_index, &event_data->u.flag);
                    break;
#endif // CCSP_COMMON
                    case wifi_event_monitor_process_active_msmt:
                        process_active_msmt_step(&event_data->u.amsmt);
                    break;
#ifdef CCSP_COMMON
                    case wifi_event_monitor_csi_update_config:
                        csi_sheduler_enable();
                    break;
                    case wifi_event_monitor_clientdiag_update_config:
                        clientdiag_sheduler_enable(event_data->ap_index);
                    break;
                    case wifi_event_monitor_data_collection_config:
                        process_stats_data_collection_request(&event_data->u.dca);
                    break;
                    case wifi_event_monitor_assoc_req:
                        set_assoc_req_frame_data(&event_data->u.msg);
                    break;
#endif // CCSP_COMMON
                    default:
                    break;

                }

                destroy_wifi_event(event);

                gettimeofday(&proc_data->last_signalled_time, NULL);
                pthread_mutex_lock(&proc_data->queue_lock);
            }
        } else if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&proc_data->queue_lock);
            gettimeofday(&t_start, NULL);
            scheduler_execute(g_monitor_module.sched, t_start, interval.tv_usec/1000);
            pthread_mutex_lock(&proc_data->queue_lock);
        } else {
            wifi_util_error_print(WIFI_MON,"%s:%d Monitor Thread exited with rc - %d",__func__,__LINE__,rc);
            pthread_mutex_unlock(&proc_data->queue_lock);
            return NULL;
        }

    }
    pthread_mutex_unlock(&proc_data->queue_lock);


    return NULL;
}

#ifdef CCSP_COMMON
static int refresh_task_period(void *arg)
{
    unsigned int    new_upload_period;
    new_upload_period = get_upload_period(g_monitor_module.upload_period);
    if (new_upload_period != g_monitor_module.upload_period) {
        g_monitor_module.upload_period = new_upload_period;
        if (new_upload_period != 0) {
            if (g_monitor_module.client_telemetry_id == 0) {
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.client_telemetry_id,
                        upload_client_telemetry_data, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
            } else {
                scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.client_telemetry_id,
                        (g_monitor_module.upload_period * MIN_TO_MILLISEC));
            }
            if (g_monitor_module.client_debug_id == 0 ) {
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.client_debug_id,
                        upload_client_debug_stats, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
            } else {
                scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.client_debug_id,
                        (g_monitor_module.upload_period * MIN_TO_MILLISEC));
            }
            if (g_monitor_module.channel_width_telemetry_id == 0) {
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.channel_width_telemetry_id,
                        upload_channel_width_telemetry, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
            } else {
                scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.channel_width_telemetry_id,
                        (g_monitor_module.upload_period * MIN_TO_MILLISEC));
            }
            if (g_monitor_module.ap_telemetry_id == 0) {
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.ap_telemetry_id,
                        upload_ap_telemetry_data, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
            } else {
                scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.ap_telemetry_id,
                        (g_monitor_module.upload_period * MIN_TO_MILLISEC));
            }
        } else {
            if (g_monitor_module.client_telemetry_id != 0) {
                scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.client_telemetry_id);
                g_monitor_module.client_telemetry_id = 0;
            }
            if (g_monitor_module.client_debug_id != 0) {
                scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.client_debug_id);
                g_monitor_module.client_debug_id = 0;
            }
            if (g_monitor_module.channel_width_telemetry_id != 0 ) {
                scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.channel_width_telemetry_id);
                g_monitor_module.channel_width_telemetry_id = 0;
            }
            if (g_monitor_module.ap_telemetry_id != 0) {
                scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.ap_telemetry_id);
                g_monitor_module.ap_telemetry_id = 0;
            }
        }
    }
    return TIMER_TASK_COMPLETE;
}
#endif // CCSP_COMMON

bool is_device_associated(int ap_index, char *mac)
{
    mac_address_t bmac;
    sta_data_t *sta;
    hash_map_t     *sta_map;
    unsigned int vap_array_index;

    getVAPArrayIndexFromVAPIndex((unsigned int)ap_index, &vap_array_index);

    str_to_mac_bytes(mac, bmac);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;
    sta = hash_map_get_first(sta_map);
    while (sta != NULL) {
        if ((memcmp(sta->sta_mac, bmac, sizeof(mac_address_t)) == 0) && (sta->dev_stats.cli_Active == true)) {
            return true;
        }
        sta = hash_map_get_next(sta_map, sta);
    }
    return false;
}

#ifdef CCSP_COMMON
int timeval_subtract (struct timeval *result, struct timeval *end, struct timeval *start)
{
    if(result == NULL || end == NULL || start == NULL) {
        return 1;
    }
    /* Refer to https://www.gnu.org/software/libc/manual/html_node/Calculating-Elapsed-Time.html" */

    if (end->tv_usec < start->tv_usec) {
        int adjust_sec = (start->tv_usec - end->tv_usec) / 1000000 + 1;
        start->tv_usec -= 1000000 * adjust_sec;
        start->tv_sec += adjust_sec;
    }
    if (end->tv_usec - start->tv_usec > 1000000) {
        int adjust_sec = (end->tv_usec - start->tv_usec) / 1000000;
        start->tv_usec += 1000000 * adjust_sec;
        start->tv_sec -= adjust_sec;
    }


    result->tv_sec = end->tv_sec - start->tv_sec;
    result->tv_usec = end->tv_usec - start->tv_usec;

    return (end->tv_sec < start->tv_sec);
}
#endif // CCSP_COMMON

int  getApIndexfromClientMac(char *check_mac)
{
    unsigned int i=0;
    unsigned char tmpmac[18];
    wifi_mgr_t *mgr = get_wifimgr_obj();

    if(check_mac == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Null arguments %p \n",__func__, check_mac);
        return -1;
    }

    macbytes_to_string((unsigned char *)check_mac, tmpmac);
    for (i = 0; i < getTotalNumberVAPs(); i++) {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
        UINT radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }

        if (is_device_associated(vap_index, (char *)tmpmac) == true) {
            return vap_index;
        }
    }
    return -1;
}

#ifdef CCSP_COMMON
static void rtattr_parse(struct rtattr *table[], int max, struct rtattr *rta, int len)
{
    unsigned short type;
    if(table == NULL || rta == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Null arguments %p %p\n",__func__,table, rta);
        return;
    }
    memset(table, 0, sizeof(struct rtattr *) * (max + 1));
    while (RTA_OK(rta, len)) {
        type = rta->rta_type;
        if (type <= max)
            table[type] = rta;
        rta = RTA_NEXT(rta, len);
    }
}

int getlocalIPAddress(char *ifname, char *ip, bool af_family)
{
    struct {
        struct nlmsghdr n;
        struct ifaddrmsg r;
    } req;

    int status;
    char buf[16384];
    struct nlmsghdr *nlm;
    struct ifaddrmsg *rtmp;
    unsigned char family;

    struct rtattr * table[__IFA_MAX+1];
    int fd;
    char if_name[IFNAMSIZ] = {'\0'};

    if(ifname == NULL || ip == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Null arguments %p %p\n",__func__, ifname, ip);
        return -1;
    }

    fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if (fd < 0 ) {
        wifi_util_error_print(WIFI_MON, "Socket error\n");
        return -1;
    }

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT | NLM_F_REQUEST;
    req.n.nlmsg_type = RTM_GETADDR;
    if(af_family) {
        req.r.ifa_family = AF_INET;
        family = AF_INET;
    } else {
        req.r.ifa_family = AF_INET6;
        family = AF_INET6;
    }
    status = send(fd, &req, req.n.nlmsg_len, 0);
    if(status<0) {
        wifi_util_error_print(WIFI_MON, "Send error\n");
        close(fd);
        return -1;
    }

    status = recv(fd, buf, sizeof(buf), 0);
    if(status<0) {
        wifi_util_error_print(WIFI_MON, "receive error\n");
        close(fd);
        return -1;
    }

    for(nlm = (struct nlmsghdr *)buf; status > (int)sizeof(*nlm);){
        int len = nlm->nlmsg_len;
        int req_len = len - sizeof(*nlm);

        if (req_len<0 || len>status || !NLMSG_OK(nlm, status)) {
            wifi_util_error_print(WIFI_MON, "length error\n");
            close(fd);
            return -1;
        }
        rtmp = (struct ifaddrmsg *)NLMSG_DATA(nlm);
        rtattr_parse(table, IFA_MAX, IFA_RTA(rtmp), nlm->nlmsg_len - NLMSG_LENGTH(sizeof(*rtmp)));
        if(rtmp->ifa_index) {
            if_indextoname(rtmp->ifa_index, if_name);
            if(!strcasecmp(ifname, if_name) && table[IFA_ADDRESS]) {
                inet_ntop(family, RTA_DATA(table[IFA_ADDRESS]), ip, 64);
                close(fd);
                return 0;
            }
        }
        status -= NLMSG_ALIGN(len);
        nlm = (struct nlmsghdr*)((char*)nlm + NLMSG_ALIGN(len));
    }
    close(fd);
    return -1;
}

int csi_getInterfaceAddress(unsigned char *tmpmac, char *ip, char *interface, bool *af_family)
{
    int ret;
    unsigned char mac[18];

    if(tmpmac == NULL || ip == NULL || interface == NULL || af_family == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Null arguments %p %p %p %p\n",__func__, tmpmac, ip, interface, af_family);
        return -1;
    }
    macbytes_to_string((unsigned char*)tmpmac, mac);
    ret = csi_getClientIpAddress(mac, ip, interface, 1);
    if(ret < 0 ) {
        wifi_util_error_print(WIFI_MON, "Not able to find v4 address\n");
    }
    else {
        *af_family = TRUE;
        return 0;
    }
    ret = csi_getClientIpAddress(mac, ip, interface, 0);
    if(ret < 0) {
        *af_family = FALSE;
        wifi_util_error_print(WIFI_MON, "Not able to find v4 or v6 addresses\n");
        return -1;
    }
    return 0;
}

int csi_getClientIpAddress(char *mac, char *ip, char *interface, int check)
{
    struct {
        struct nlmsghdr n;
        struct ndmsg r;
    } req;

    int status;
    char buf[16384];
    struct nlmsghdr *nlm;
    struct ndmsg *rtmp;
    struct rtattr * table[NDA_MAX+1];
    int fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    char if_name[IFNAMSIZ] = {'\0'};
    unsigned char tmp_mac[17];
    unsigned char af_family;

    if(mac == NULL || ip == NULL || interface == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Null arguments %p %p %p\n",__func__, mac, ip, interface);
        return -1;
    }
    if (fd < 0 ) {
        wifi_util_error_print(WIFI_MON, "Socket error\n");
        return -1;
    }
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ndmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT | NLM_F_REQUEST;
    req.n.nlmsg_type = RTM_GETNEIGH;
    if(check)  {
        req.r.ndm_family = AF_INET;
        af_family =  AF_INET;
    } else {
        req.r.ndm_family = AF_INET6;
        af_family = AF_INET6;
    }

    status = send(fd, &req, req.n.nlmsg_len, 0);
    if (status < 0) {
        wifi_util_error_print(WIFI_MON, "Socket send error\n");
        close(fd);
        return -1;
    }

    status = recv(fd, buf, sizeof(buf), 0);
    if (status < 0) {
        wifi_util_error_print(WIFI_MON, "Socket receive error\n");
        close(fd);
        return -1;
    }

    for(nlm = (struct nlmsghdr *)buf; status > (int)sizeof(*nlm);){
        int len = nlm->nlmsg_len;
        int req_len = len - sizeof(*nlm);

        if (req_len<0 || len>status || !NLMSG_OK(nlm, status)) {
            wifi_util_error_print(WIFI_MON, "packet length error\n");
            close(fd);
            return -1;
        }

        rtmp = (struct ndmsg *)NLMSG_DATA(nlm);
        rtattr_parse(table, NDA_MAX, NDA_RTA(rtmp), nlm->nlmsg_len - NLMSG_LENGTH(sizeof(*rtmp)));

        if(rtmp->ndm_state & NUD_REACHABLE || rtmp->ndm_state & NUD_STALE) {
            if(table[NDA_LLADDR]) {
                unsigned char *addr =  RTA_DATA(table[NDA_LLADDR]);
                macbytes_to_string(addr,tmp_mac);
                if(!strcasecmp((char *)tmp_mac, mac)) {
                    if(table[NDA_DST] && rtmp->ndm_ifindex) {
                        inet_ntop(af_family, RTA_DATA(table[NDA_DST]), ip, 64);
                        if_indextoname(rtmp->ndm_ifindex, if_name);
                        strncpy(interface, if_name, IFNAMSIZ);
                        close(fd);
                        return 0;
                    }
                }
            }
        }
        status -= NLMSG_ALIGN(len);
        nlm = (struct nlmsghdr*)((char*)nlm + NLMSG_ALIGN(len));
    }
    close(fd);
    return -1;
}

unsigned short csum(unsigned short *ptr,int nbytes)
{
    register long sum;
    unsigned short oddbyte;
    register short answer;

    if(ptr == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Null arguments %p\n",__func__, ptr);
        return 0;
    }

    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }

    sum = (sum>>16)+(sum & 0xffff);
    sum = sum + (sum>>16);
    answer=(short)~sum;

    return(answer);
}

int frame_icmpv4_ping(char *buffer, char *dest_ip, char *source_ip)
{
    char *data;
    int buffer_size;
    //ip header
    struct iphdr *ip = (struct iphdr *) buffer;
    static int pingCount = 1;
    //ICMP header
    struct icmphdr *icmp = (struct icmphdr *) (buffer + sizeof (struct iphdr));
    if(buffer == NULL || dest_ip == NULL || source_ip == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: Null arguments %p %p %p\n",__func__, buffer, dest_ip, source_ip);
        return 0;
    }
    data = buffer + sizeof(struct iphdr) + sizeof(struct icmphdr);
    strcpy(data , "stats ping");
    buffer_size = sizeof (struct iphdr) + sizeof (struct icmphdr) + strlen(data);

    //ICMP_HEADER
    //
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.id = (unsigned short) getpid();
    icmp->un.echo.sequence = pingCount++;
    icmp->checksum = csum ((unsigned short *) (icmp), sizeof (struct icmphdr) + strlen(data));

    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons (sizeof (struct iphdr) + sizeof (struct icmphdr) + strlen(data));
    ip->ttl = 8;
    ip->frag_off = 0;
    ip->protocol = IPPROTO_ICMP;
    ip->saddr = inet_addr (source_ip);
    ip->daddr = inet_addr (dest_ip);
    ip->check = 0;
    ip->id = htonl (54321);
    ip->check = csum ((unsigned short *) (ip), sizeof(struct iphdr));

    return buffer_size;
}

int frame_icmpv6_ping(char *buffer, char *dest_ip, char *source_ip)
{
    char *data;
    int buffer_size;
    struct ip6_hdr* ip  = (struct ip6_hdr*) buffer;
    struct icmp6_hdr* icmp = (struct icmp6_hdr*)(buffer + sizeof(struct ip6_hdr));

    //v6 pseudo header for icmp6 checksum 
    struct ip6_pseu
    {
        struct in6_addr ip6e_src;
        struct in6_addr ip6e_dst;
        uint16_t ip6e_len;
        uint8_t  pad;
        uint8_t  ip6e_nxt;
    };
    char sample[1024] = {0};
    struct ip6_pseu* pseu = (struct ip6_pseu*)sample;

    data = (char *)(buffer + sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr));
    strcpy(data, "stats ping");
    buffer_size = sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr) + strlen(data);
    icmp->icmp6_type = ICMP6_ECHO_REQUEST;
    icmp->icmp6_code = 0;

    pseu->pad = 0x00;
    pseu->ip6e_nxt = IPPROTO_ICMPV6;

    ip->ip6_flow = htonl ((6 << 28) | (0 << 20) | 0);
    ip->ip6_plen = htons(sizeof(struct icmp6_hdr)+strlen(data));
    ip->ip6_nxt = IPPROTO_ICMPV6;
    ip->ip6_hlim = 255;

    pseu->ip6e_len = htons(sizeof(struct icmp6_hdr)+strlen(data));

    inet_pton(AF_INET6, source_ip, &(ip->ip6_src));
    inet_pton(AF_INET6, dest_ip, &(ip->ip6_dst));
    pseu->ip6e_src = ip->ip6_src;
    pseu->ip6e_dst = ip->ip6_dst;

    memcpy(sample+sizeof(struct ip6_pseu), icmp, sizeof(struct icmp6_hdr)+strlen(data));
    icmp->icmp6_cksum = 0;
    icmp->icmp6_cksum = csum ((unsigned short* )sample, sizeof(struct ip6_pseu)+sizeof(struct icmp6_hdr)+strlen(data));

    return buffer_size;
}

static bool isValidIpAddress(char *ipAddress, bool af_family)
{
    struct sockaddr_in sa;
    unsigned char family;
    if(ipAddress==NULL)    {
        return FALSE;
    }
    if(af_family)    {
        family = AF_INET;
    }    else    {
        family = AF_INET6;
    }
    int result = inet_pton(family, ipAddress, &(sa.sin_addr));
    return (result == 1);
}

static void send_ping_data(int ap_idx, unsigned char *mac, char *client_ip, char *vap_ip, long *client_ip_age, bool refresh)
{
    char        cli_interface_str[16];
    char        buffer[1024] = {0};
    int         frame_len;
    int rc = 0;
    bool af_family = TRUE;
    char        src_ip_str[IP_STR_LEN];
    char        cli_ip_str[IP_STR_LEN];

    if(mac == NULL ) {
        wifi_util_error_print(WIFI_MON, "%s: Mac is NULL\n",__func__);
        return;
    }

    memset (buffer, 0, sizeof(buffer));
    frame_len = 0;
    if(ap_idx < 0 || mac == NULL) {
        return;
    }
    wifi_util_info_print(WIFI_MON, "%s: Got the csi client for index  %02x..%02x\n",__func__,mac[0], mac[5]);
    if(refresh) {
        //Find the interface through which this client was seen
        rc = csi_getInterfaceAddress(mac, cli_ip_str, cli_interface_str, &af_family); //pass mac_addr_t
        if(rc<0)
        {
            wifi_util_error_print(WIFI_MON, "%s Failed to get ipv4 client address\n",__func__);
            return;
        } else {
            if(isValidIpAddress(cli_ip_str, af_family)) {
                *client_ip_age = 0;
                strncpy(client_ip, cli_ip_str, IP_STR_LEN);
                wifi_util_info_print(WIFI_MON, "%s Returned ipv4 client address is %s interface %s \n",__func__,  cli_ip_str, cli_interface_str );
            } else {
                wifi_util_error_print(WIFI_MON, "%s Was not a valid client ip string\n", __func__);
                return;
            }
        }
        //Get the ip address of the interface
        if(*vap_ip == '\0') {
            rc = getlocalIPAddress(cli_interface_str, src_ip_str, af_family);
            if(rc<0) {
                wifi_util_error_print(WIFI_MON, "%s Failed to get ipv4 address\n",__func__);
                return;
            } else {
                if(isValidIpAddress(src_ip_str, af_family)) {
                    strncpy(vap_ip, src_ip_str, IP_STR_LEN);
                    wifi_util_info_print(WIFI_MON, "%s Returned interface ip addr is %s\n", __func__,src_ip_str);
                } else {
                    wifi_util_error_print(WIFI_MON, "%s Was not a valid client ip string\n", __func__);
                    return;
                }
            }
        }
    } else {
        strncpy(src_ip_str, vap_ip, IP_STR_LEN);
        strncpy(cli_ip_str, client_ip, IP_STR_LEN);
    }
    //build a layer 3 packet , tcp ping
    if(af_family) {
        frame_len = frame_icmpv4_ping(buffer, (char *)&cli_ip_str, (char *)&src_ip_str);
        //send buffer
        if(frame_len) {
#if (defined (_XB7_PRODUCT_REQ_) && !defined (_COSA_BCM_ARM_))
            wifi_sendDataFrame(ap_idx,
                    (unsigned char*)mac,
                    (unsigned char*)buffer,
                    frame_len,
                    TRUE,
                    WIFI_ETH_TYPE_IP,
                    wifi_data_priority_be);
#else
            wifi_hal_sendDataFrame(ap_idx,
                    (unsigned char*)mac,
                    (unsigned char*)buffer,
                    frame_len,
                    TRUE,
                    WIFI_ETH_TYPE_IP,
                    wifi_data_priority_be);
#endif
        }
    } else {
        frame_len = frame_icmpv6_ping(buffer, (char *)&cli_ip_str, (char *)&src_ip_str);
        //send buffer
        if(frame_len) {
#if (defined (_XB7_PRODUCT_REQ_) && !defined (_COSA_BCM_ARM_))
            wifi_sendDataFrame(ap_idx,
                    (unsigned char*)mac,
                    (unsigned char*)buffer,
                    frame_len,
                    TRUE,
                    WIFI_ETH_TYPE_IP6,
                    wifi_data_priority_be);
#else
            wifi_hal_sendDataFrame(ap_idx,
                    (unsigned char*)mac,
                    (unsigned char*)buffer,
                    frame_len,
                    TRUE,
                    WIFI_ETH_TYPE_IP,
                    wifi_data_priority_be);
#endif
        }
    }
}


static csi_session_t* csi_get_session(bool create, int csi_session_number) {
    csi_session_t *csi = NULL;
    int count = 0, i = 0;

    count = queue_count(g_events_monitor.csi_queue);
    for(i = 0; i<count; i++) {
        csi = queue_peek(g_events_monitor.csi_queue, i);
        if(csi == NULL){
            continue;
        }
        if(csi->csi_sess_number == csi_session_number) {
            return csi;
        }
    }

    if(create == FALSE) {
        return NULL;
    }


    csi = (csi_session_t *) malloc(sizeof(csi_session_t));
    if(csi == NULL) {
        return NULL;
    }

    memset(csi, 0, sizeof(csi_session_t));
    csi->csi_time_interval = MIN_CSI_INTERVAL;
    csi->csi_sess_number = csi_session_number;
    csi->enable = FALSE;
    csi->subscribed = FALSE;
    memset(csi->client_ip, '\0', sizeof(csi->client_ip));
    gettimeofday(&csi->last_snapshot_time, NULL);

    queue_push(g_events_monitor.csi_queue, csi);
    return csi;
}


void csi_update_client_mac_status(mac_addr_t mac, bool connected, int ap_idx) {
    csi_session_t *csi = NULL;
    int count = 0;
    int i = 0, j = 0;
    bool client_csi_monitored = FALSE;
    pthread_mutex_lock(&g_events_monitor.lock);
    count = queue_count(g_events_monitor.csi_queue);
    wifi_util_info_print(WIFI_MON, "%s: Received Mac %d %d %02x %02x\n",__func__, connected, count, mac[0], mac[5]);
    for(i = 0; i<count; i++) {
        csi = queue_peek(g_events_monitor.csi_queue, i);
        if(csi == NULL){
            continue;
        }

        wifi_util_dbg_print(WIFI_MON, "%s: Received Mac  %d %d %d %02x %02x\n",__func__, connected, csi->subscribed, csi->enable, mac[0], mac[5]);
        for(j =0 ;j < csi->no_of_mac; j++) {
            wifi_util_dbg_print(WIFI_MON, "%s: checking with Mac  %d %d %d %02x %02x\n",__func__, connected, csi->subscribed, csi->enable, csi->mac_list[j][0], csi->mac_list[j][5]);
            if(memcmp(mac, csi->mac_list[j], sizeof(mac_addr_t)) == 0) {
                csi->mac_is_connected[j] = connected;
                if(csi->enable && csi->subscribed) {
                    client_csi_monitored = TRUE;
                }
                if(connected == FALSE) {
                    csi->ap_index[j] = -1;
                    memset(&csi->client_ip[j][0], '\0', IP_STR_LEN);
                    csi->client_ip_age[j] = 0;
                }
                else {
                    csi->ap_index[j] = ap_idx;
                }
                break;
            }
        }
    }
    if(client_csi_monitored) {
        wifi_util_dbg_print(WIFI_MON, "%s: Updating csi collection for Mac %02x %02x %d\n",__func__, mac[0], mac[5], connected);
        wifi_enableCSIEngine(ap_idx, (unsigned char*)mac, connected);
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
}

void csi_set_client_mac(char *r_mac_list, int csi_session_number)
{
    csi_session_t *csi = NULL;
    int ap_index = -1, mac_ctr = 0;
    int i = 0;
    char *mac_tok=NULL;
    char* rest = NULL;
    char mac_list[MAX_CSI_CLIENTMACLIST_STR] = {0};
    wifi_monitor_data_t data;
    struct timeval t_now;

    if(r_mac_list == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: mac_list is NULL \n",__func__);
        return;
    }

    pthread_mutex_lock(&g_events_monitor.lock);
    csi = csi_get_session(FALSE, csi_session_number);
    if(!csi) {
        wifi_util_error_print(WIFI_MON, "%s: csi session not present \n",__func__);
        pthread_mutex_unlock(&g_events_monitor.lock);
        return;
    }
    if(csi->no_of_mac > 0) {
        wifi_util_info_print(WIFI_MON, "%s: Mac already configured %d\n",__func__, csi->no_of_mac);
        csi_disable_client(csi);
        for(i = 0; i<csi->no_of_mac; i++) {
            csi->mac_is_connected[i] = FALSE;
            csi->ap_index[mac_ctr] = -1;
            memset(&csi->mac_list[i], 0, sizeof(mac_addr_t));
        }
        csi->no_of_mac = 0;
    }

    strncpy(mac_list, r_mac_list, MAX_CSI_CLIENTMACLIST_STR);
    csi->no_of_mac = (strlen(mac_list)+1) / (MIN_MAC_LEN + 1);
    wifi_util_info_print(WIFI_MON, "%s: Total mac's present -  %d %s\n",__func__, csi->no_of_mac, mac_list);
    rest = mac_list;
    if(csi->no_of_mac > 0)  {
        gettimeofday(&t_now, NULL);
        while((mac_tok = strtok_r(rest, ",", &rest))) {

            wifi_util_dbg_print(WIFI_MON, "%s: Mac %s\n",__func__, mac_tok);
            str_to_mac_bytes(mac_tok,(unsigned char*)&csi->mac_list[mac_ctr]);
            ap_index= getApIndexfromClientMac((char *)&csi->mac_list[mac_ctr]);
            if(ap_index >= 0) {
                csi->ap_index[mac_ctr] = ap_index;
                csi->mac_is_connected[mac_ctr] = TRUE;
                if(csi->enable && csi->subscribed) {
                    wifi_util_info_print(WIFI_MON, "%s: Enabling csi collection for Mac %s\n",__func__, mac_tok);
                    wifi_enableCSIEngine(ap_index, csi->mac_list[mac_ctr], TRUE);
                }
            } else {
                wifi_util_info_print(WIFI_MON, "%s: Not Enabling csi collection for Mac %s\n",__func__, mac_tok);
                csi->ap_index[mac_ctr] = -1;
                csi->mac_is_connected[mac_ctr] = FALSE;
            }
            mac_ctr++;
        }
        for(i = 0; i<csi->no_of_mac; i++) {
            memcpy(&csi->last_publish_time[i], &t_now, sizeof(struct timeval));
        }
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
    memset(&data, 0, sizeof(wifi_monitor_data_t));
    data.id = msg_id++;
    push_event_to_monitor_queue(&data, wifi_event_monitor_csi_update_config, NULL);
}


static void csi_enable_client(csi_session_t *csi)
{
    int i =0;
    int ap_index = -1;
    if((csi == NULL) || !(csi->enable && csi->subscribed)) {
        return;
    }

    for(i =0; i<csi->no_of_mac; i++) {
        if((csi->ap_index[i] != -1) && (csi->mac_is_connected[i] == TRUE)) {
            wifi_util_info_print(WIFI_MON, "%s: Enabling csi collection for Mac %02x..%02x\n",__func__, csi->mac_list[i][0] , csi->mac_list[i][5]  );
            wifi_enableCSIEngine(csi->ap_index[i], csi->mac_list[i], TRUE);
        }
        //check if client is connected now
        else {
            ap_index= getApIndexfromClientMac((char *)&csi->mac_list[i]);
            if(ap_index >= 0) {
                csi->ap_index[i] = ap_index;
                csi->mac_is_connected[i] = TRUE;
                wifi_util_info_print(WIFI_MON, "%s: Enabling csi collection for Mac %02x..%02x\n",__func__, csi->mac_list[i][0] , csi->mac_list[i][5]  );
                wifi_enableCSIEngine(csi->ap_index[i], csi->mac_list[i], TRUE);
            }
        }
    }
}

void csi_enable_session(bool enable, int csi_session_number)
{
    csi_session_t *csi = NULL;
    wifi_monitor_data_t data;

    pthread_mutex_lock(&g_events_monitor.lock);
    csi = csi_get_session(FALSE, csi_session_number);
    if(csi) {
        wifi_util_dbg_print(WIFI_MON, "%s: Enable session %d enable - %d\n",__func__, csi_session_number, enable);
        if(enable) {
            csi->enable = enable;
            csi_enable_client(csi);
        }  else {
            csi_disable_client(csi);
            csi->enable = enable;
        }
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
    memset(&data, 0, sizeof(wifi_monitor_data_t));
    data.id = msg_id++;
    push_event_to_monitor_queue(&data, wifi_event_monitor_csi_update_config, NULL);

}

static void csi_vap_down_update(int ap_idx)
{
    csi_session_t *csi = NULL;
    int count = 0, i = 0, j=0;

    pthread_mutex_lock(&g_events_monitor.lock);
    count = queue_count(g_events_monitor.csi_queue);
    for(i = 0; i<count; i++) {
        csi = queue_peek(g_events_monitor.csi_queue, i);
        if(csi == NULL){
            continue;
        }
        for(j =0; j<csi->no_of_mac; j++) {
            if(csi->ap_index[j] == ap_idx) {
                wifi_enableCSIEngine(csi->ap_index[j], csi->mac_list[j], FALSE);
                csi->ap_index[j] = -1;
                csi->mac_is_connected[j] = FALSE;
                memset(&csi->client_ip[j][0], '\0', IP_STR_LEN);
                csi->client_ip_age[j] = 0;
            }
        }
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
}

void csi_enable_subscription(bool subscribe, int csi_session_number)
{
    csi_session_t *csi = NULL;
    wifi_monitor_data_t data;

    pthread_mutex_lock(&g_events_monitor.lock);
    csi = csi_get_session(TRUE, csi_session_number);
    if(csi) {
        wifi_util_info_print(WIFI_MON, "%s: subscription for session %d\n",__func__, csi_session_number);
        if(subscribe) {
            csi->subscribed = subscribe;
            csi_enable_client(csi);
        } else {
            csi_disable_client(csi);
            csi->subscribed = subscribe;
        }
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
    memset(&data, 0, sizeof(wifi_monitor_data_t));

    data.id = msg_id++;

    push_event_to_monitor_queue(&data, wifi_event_monitor_csi_update_config, NULL);
}

void csi_set_interval(int interval, int csi_session_number)
{
    csi_session_t *csi = NULL;

    pthread_mutex_lock(&g_events_monitor.lock);
    csi = csi_get_session(FALSE, csi_session_number);
    if(csi) {
        csi->csi_time_interval = interval;
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
}

void csi_create_session(int csi_session_number)
{

    pthread_mutex_lock(&g_events_monitor.lock);
    csi_get_session(TRUE, csi_session_number);
    pthread_mutex_unlock(&g_events_monitor.lock);
}

static void csi_disable_client(csi_session_t *r_csi) 
{
    int count = 0;
    int i = 0, j = 0, k = 0;
    csi_session_t *csi = NULL;
    bool client_in_diff_subscriber = FALSE;

    if(r_csi == NULL) {
        wifi_util_error_print(WIFI_MON, "%s: r_csi is NULL\n",__func__);
        return;
    }

    count = queue_count(g_events_monitor.csi_queue);

    for(j =0 ; j< r_csi->no_of_mac; j++) {
        client_in_diff_subscriber = FALSE;
        for(i = 0; i<count; i++) {
            csi = queue_peek(g_events_monitor.csi_queue, i);

            if(csi == NULL || csi == r_csi){
                continue;
            }
            if(!(csi->enable && csi->subscribed)) {
                continue;
            }

            for(k = 0; k < csi->no_of_mac; k++) {
                if(memcmp(r_csi->mac_list[j], csi->mac_list[k], sizeof(mac_addr_t))== 0) {
                    //Client is also monitored by a different subscriber
                    wifi_util_info_print(WIFI_MON, "%s: Not Disabling csi for client mac %02x..%02x\n",__func__,r_csi->mac_list[j][0],r_csi->mac_list[j][5]);
                    client_in_diff_subscriber = TRUE;
                    break;
                }
            }
            if(client_in_diff_subscriber)
                break;
        }
        if((client_in_diff_subscriber == FALSE) && (r_csi->mac_is_connected[j] == TRUE)) {
            wifi_util_info_print(WIFI_MON, "%s: Disabling for client mac %02x..%02x\n",__func__,r_csi->mac_list[j][0],r_csi->mac_list[j][5]);
            wifi_enableCSIEngine(r_csi->ap_index[j], r_csi->mac_list[j], FALSE);
        }
    }
}

static int csi_timedout(struct timeval *time_diff, int *csi_time_interval)
{

    if(time_diff == NULL || csi_time_interval == NULL) {
        return 0;
    }

    int time_compare = *csi_time_interval;
    if(time_compare >= 1000) {
        if(time_diff->tv_sec >= (time_compare / 1000)) {
            return 1;
        } else {
            return 0;
        }
    } else {
        if(((time_diff->tv_usec) >=  (time_compare - 5 ) * MILLISEC_TO_MICROSEC) || time_diff->tv_sec > 0) {
            return 1;
        } else {
            return 0;
        }
    }
    return 0;
}

void csi_del_session(int csi_sess_number) 
{
    int count = 0;
    int i = 0;
    csi_session_t *csi = NULL;

    pthread_mutex_lock(&g_events_monitor.lock);
    count = queue_count(g_events_monitor.csi_queue);
    wifi_util_info_print(WIFI_MON, "%s: Deleting Element %d\n",__func__, csi_sess_number);
    for(i = 0; i<count; i++) {
        csi = queue_peek(g_events_monitor.csi_queue, i);

        if(csi == NULL){
            continue;
        }

        if(csi->csi_sess_number == csi_sess_number) {
            wifi_util_dbg_print(WIFI_MON, "%s: Found Element\n",__func__);
            queue_remove(g_events_monitor.csi_queue, i);

            csi_disable_client(csi);
            free(csi);
            break;
        }
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
}

static void csi_refresh_session ()
{
    int i = 0;
    int count = 0;
    struct timeval time_diff;
    csi_session_t *csi = NULL;

    pthread_mutex_lock(&g_events_monitor.lock);

    count = queue_count(g_events_monitor.csi_queue);

    while(i < count) {
        csi = queue_peek(g_events_monitor.csi_queue, i);
        i++;
        if(csi == NULL || !(csi->enable && csi->subscribed)) {
            continue;
        }
        if(!timeval_subtract(&time_diff, &csi_prune_timer, &csi->last_snapshot_time)) {
            if(csi_timedout(&time_diff, &csi->csi_time_interval)) {
                csi->last_snapshot_time = csi_prune_timer;
            }
        }
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
}

static int csi_sheduler_enable(void)
{
    unsigned int *interval_list;
    int i, found = 0, count;
    unsigned int csi_time_interval = MAX_CSI_INTERVAL;
    bool enable = FALSE;
    csi_session_t *csi = NULL;

    count = queue_count(g_events_monitor.csi_queue);
    if (count > 0) {
        interval_list = (unsigned int *) malloc(sizeof(unsigned int)*count);
        if (interval_list == NULL) {
            return -1;
        }
        pthread_mutex_lock(&g_events_monitor.lock);
        for (i=0; i < count; i++) {
            interval_list[i] = MAX_CSI_INTERVAL;
            csi = queue_peek(g_events_monitor.csi_queue, i);
            if (csi != NULL && csi->enable && csi->subscribed) {
                enable = TRUE;
                //make sure it is multiple of 100ms
                interval_list[i] = (csi->csi_time_interval/100)*100;
                //find shorter time interval
                if(csi_time_interval > interval_list[i]) {
                    csi_time_interval = interval_list[i];
                }
            }
        }
        pthread_mutex_unlock(&g_events_monitor.lock);
        if (enable == TRUE) {
            while (found == 0) {
                found = 1;
                for (int i=0; i < count; i++) {
                    if ((interval_list[i] % csi_time_interval) != 0 ) {
                        csi_time_interval = csi_time_interval - 100;
                        found = 0;
                        break;
                    }
                }
            }
        }
        free(interval_list);
    }
#if !defined(FEATURE_CSI_CALLBACK)
    if (enable == TRUE) {
        if (g_monitor_module.csi_sched_id == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, TRUE,
                    &(g_monitor_module.csi_sched_id), csi_getCSIData,
                    NULL, csi_time_interval, 0);
        } else {
            if (g_monitor_module.csi_sched_interval != csi_time_interval) {
                g_monitor_module.csi_sched_interval = csi_time_interval;
                scheduler_update_timer_task_interval(g_monitor_module.sched,
                        g_monitor_module.csi_sched_id, csi_time_interval);
            }
        }
    } else {
        if (g_monitor_module.csi_sched_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched,
                    g_monitor_module.csi_sched_id);
            g_monitor_module.csi_sched_id = 0;
        }
    }
#else
//Enabling Pinger only on CMXB7
#if (defined (_XB7_PRODUCT_REQ_) && !defined (_COSA_BCM_ARM_))
    if (enable == TRUE) {
        if (g_monitor_module.csi_sched_id == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, TRUE,
                        &(g_monitor_module.csi_sched_id), csi_sendPingData,
                        NULL, csi_time_interval, 0);
        } else {
            if (g_monitor_module.csi_sched_interval != csi_time_interval) {
                g_monitor_module.csi_sched_interval = csi_time_interval;
                scheduler_update_timer_task_interval(g_monitor_module.sched,
                                        g_monitor_module.csi_sched_id, csi_time_interval);
            }
        }
    } else {
        if (g_monitor_module.csi_sched_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched,
                                        g_monitor_module.csi_sched_id);
            g_monitor_module.csi_sched_id = 0;
        }
    }
#endif
#endif
    return 0;
}


static void csi_publish(wifi_event_t *event)
{
    int i = 0;
    int j = 0;
    int count = 0;
    struct timeval time_diff;
    csi_session_t *csi = NULL;
    wifi_monitor_data_t *evtData;

    if(event == NULL) {
        return;
    }

    evtData = event->u.mon_data;

    pthread_mutex_lock(&g_events_monitor.lock);
    count = queue_count(g_events_monitor.csi_queue);

    for(i=0; i<count; i++) {
        csi = queue_peek(g_events_monitor.csi_queue, i);

        if(csi == NULL || !(csi->enable && csi->subscribed)){
            continue;
        }
        /*this code is hit every MONITOR_RUNNING_INTERVAL_IN_MILLISEC,
          Rounding off by -5 to make sure  we do not miss an interval, as hit this path
          1 or 2 msec earlier at times*/
        if(!timeval_subtract(&time_diff, &csi_prune_timer, &csi->last_snapshot_time)) {
            if(csi_timedout(&time_diff, &csi->csi_time_interval)) {
                for(j = 0; j < csi->no_of_mac; j++) {
                    if(memcmp(evtData->u.csi.sta_mac, csi->mac_list[j],  sizeof(mac_address_t)) == 0) {
                        wifi_util_dbg_print(WIFI_MON, "%s: Publish CSI Event - MAC  %02x:%02x:%02x:%02x:%02x:%02x\n",__func__, evtData->u.csi.sta_mac[0], evtData->u.csi.sta_mac[1], evtData->u.csi.sta_mac[2], evtData->u.csi.sta_mac[3],
                                        evtData->u.csi.sta_mac[4], evtData->u.csi.sta_mac[5]);
                        evtData->csi_session = csi->csi_sess_number;
                        events_rbus_publish(event);
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&g_events_monitor.lock);
}

#if defined (FEATURE_CSI_CALLBACK)
bool csi_check_timeout(csi_session_t *csi, int client_idx, struct timeval* t_now)
{
    struct timeval interval;
    int  interval_ms_margin;
    struct timeval timeout;

    if (csi == NULL || t_now == NULL) {
        wifi_util_dbg_print(WIFI_MON, "%s: Invalid arguments. csi %p, t_now %p\n",__func__, csi, t_now);
        return FALSE;
    }
    //Need to support the fluctuation of csi interval coming from the driver
    interval_ms_margin = csi->csi_time_interval - (MIN_CSI_INTERVAL/2);

    interval.tv_sec = (interval_ms_margin / 1000);
    interval.tv_usec = (interval_ms_margin % 1000) * 1000;
    timeradd(&(csi->last_publish_time[client_idx]), &interval, &timeout);
    if (timercmp(t_now, &timeout, >)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

INT process_csi(mac_address_t mac_addr, wifi_csi_data_t  *csi_data)
{
    struct timeval t_now;
    int i, j;
    int csi_subscribers_count = 0;
    bool mac_found = FALSE;
    csi_session_t *csi = NULL;
    wifi_event_t *event = NULL;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    wifi_util_dbg_print(WIFI_MON, "%s: CSI data received - MAC  %02x:%02x:%02x:%02x:%02x:%02x\n",__func__, mac_addr[0], mac_addr[1],
                                                        mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    gettimeofday(&t_now, NULL);

    event = create_wifi_event(sizeof(wifi_monitor_data_t), wifi_event_type_monitor, wifi_event_monitor_csi);
    if (event == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: memory allocation for event failed.\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    memcpy(event->u.mon_data->u.csi.sta_mac, mac_addr, sizeof(mac_addr_t));
    memcpy(&event->u.mon_data->u.csi.csi, csi_data, sizeof(wifi_csi_data_t));
    pthread_mutex_lock(&g_events_monitor.lock);
    csi_subscribers_count = queue_count(g_events_monitor.csi_queue);

    for (i =0; i < csi_subscribers_count; i++) {
        mac_found = FALSE;
        csi = queue_peek(g_events_monitor.csi_queue, i);
        if (csi == NULL || !(csi->enable && csi->subscribed)) {
            continue;
        }
        for (j = 0; j < csi->no_of_mac; j++) {
            if (csi->mac_is_connected[j] == FALSE) {
                continue;
            }
            if (memcmp(mac_addr, csi->mac_list[j], sizeof(mac_addr_t)) == 0) {
                mac_found = TRUE;
                break;
            }
        }
        if (mac_found == TRUE) {
            event->u.mon_data->csi_session = csi->csi_sess_number;
            //check interval
            if (csi->csi_time_interval == MIN_CSI_INTERVAL || csi_check_timeout(csi, j, &t_now)) {
                event->u.mon_data->csi_session = csi->csi_sess_number;
                wifi_util_dbg_print(WIFI_MON, "%s: Publish CSI Event - MAC  %02x:%02x:%02x:%02x:%02x:%02x Session %d\n",__func__, mac_addr[0], mac_addr[1],
                                                        mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], csi->csi_sess_number);
                events_rbus_publish(event);
                csi->last_publish_time[j] = t_now;
            }
        }
    }
#if DML_SUPPORT
    // now forward the event to apps manager
    apps_mgr_event(&ctrl->apps_mgr, event);
#endif
    destroy_wifi_event(event);
    pthread_mutex_unlock(&g_events_monitor.lock);
    return 0;
}

int csi_sendPingData(void *arg)
{
    mac_addr_t  tmp_csiClientMac[MAX_CSI_CLIENTS_PER_SESSION];
    int count=0, i=0, j =0, k=0, m=0;
    int csi_subscribers_count = 0;
    csi_session_t *csi = NULL;
    bool mac_found = FALSE;
    bool refresh = FALSE;
    void* pCsiClientIpAge   = NULL;
    //Iterating through each VAP and collecting data
    for (i = 0; i < MAX_VAP; i++) {
        count=0;
        memset(tmp_csiClientMac, 0, sizeof(tmp_csiClientMac));
        pthread_mutex_lock(&g_events_monitor.lock);
        csi_subscribers_count = queue_count(g_events_monitor.csi_queue);

        for(k =0; k < csi_subscribers_count; k++) {
            mac_found = FALSE;
            csi = queue_peek(g_events_monitor.csi_queue, k);
            if(csi == NULL || !(csi->enable && csi->subscribed)) {
                continue;
            }

            for(j = 0; j < csi->no_of_mac; j++) {
                if((csi->mac_is_connected[j] == FALSE) || (csi->ap_index[j] != i)) {
                  continue;
                }
                for(m=0; m<count; m++) {
                  if(memcmp(tmp_csiClientMac[m], csi->mac_list[j], sizeof(mac_addr_t)) == 0) {
                    mac_found = TRUE;
                    break;
                  }
                }
                if(mac_found == TRUE) {		
                  wifi_util_dbg_print(WIFI_MON, "%s: Mac already present in CSI list %02x..%02x\n",__func__, csi->mac_list[j][0], csi->mac_list[j][5]);
                  continue;
                }
                wifi_util_dbg_print(WIFI_MON, "%s: Adding Mac for csi collection %02x..%02x ap_idx %d\n",__func__, csi->mac_list[j][0], csi->mac_list[j][5], i);
                memcpy(&tmp_csiClientMac[count], &csi->mac_list[j], sizeof(mac_addr_t));
                if((csi->client_ip[j][0] != '\0') && ((csi->client_ip_age[j]*csi->csi_time_interval)  <= IPREFRESH_PERIOD_IN_MILLISECONDS) && (g_events_monitor.vap_ip[j][0] != '\0')) {
                  refresh = FALSE;
                }
                else {
                  refresh = TRUE;
                }
                pCsiClientIpAge = &csi->client_ip_age[j];
                send_ping_data(csi->ap_index[j], (unsigned char *)&csi->mac_list[j][0],
                               &csi->client_ip[j][0], &g_events_monitor.vap_ip[j][0], pCsiClientIpAge,refresh);
                csi->client_ip_age[j]++;
                count++;
            }
        }
        pthread_mutex_unlock(&g_events_monitor.lock);
    }
    //csi_refresh_session();
    return TIMER_TASK_COMPLETE;
}

#endif

int csi_getCSIData(void *arg)
{
    mac_addr_t  tmp_csiClientMac[MAX_CSI_CLIENTS_PER_SESSION];
    int count=0, i=0, itrcsi=0, itrc=0, ret=RETURN_ERR, j =0, k=0, m=0;
    wifi_event_t *event;
    struct timeval time_diff;
    int csi_subscribers_count = 0;
    csi_session_t *csi = NULL;
    bool mac_found = FALSE;
    bool refresh = FALSE;
    int total_events = 0;
    int re_itr = 0;
    gettimeofday(&csi_prune_timer, NULL);
    wifi_associated_dev3_t *dev_array = NULL;
    wifi_mgr_t *mgr = get_wifimgr_obj();

    //Iterating through each VAP and collecting data
    for (i = 0; i < (int) getTotalNumberVAPs(); i++) {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
        UINT radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        count=0;
        memset(tmp_csiClientMac, 0, sizeof(tmp_csiClientMac));
        pthread_mutex_lock(&g_events_monitor.lock);
        csi_subscribers_count = queue_count(g_events_monitor.csi_queue);

        for(k =0; k < csi_subscribers_count; k++) {
            mac_found = FALSE;
            csi = queue_peek(g_events_monitor.csi_queue, k);
            if(csi == NULL || !(csi->enable && csi->subscribed)) {
                continue;
            }
            /*this code is hit every MONITOR_RUNNING_INTERVAL_IN_MILLISEC,
              Rounding off by -5 to make sure  we do not miss an interval, as hit this path
              1 or 2 msec earlier at times*/
            if(!timeval_subtract(&time_diff, &csi_prune_timer, &csi->last_snapshot_time)) {
                if(csi_timedout(&time_diff, &csi->csi_time_interval)) {
                    for(j = 0; j < csi->no_of_mac; j++) {
                        if((csi->mac_is_connected[j] == FALSE) || (csi->ap_index[j] != (int)vap_index)) {
                            continue;
                        }
                        for(m=0; m<count; m++) {
                            if(memcmp(tmp_csiClientMac[m], csi->mac_list[j], sizeof(mac_addr_t)) == 0) {
                                mac_found = TRUE;
                                break;
                            }
                        }
                        if(mac_found == TRUE) {
                            wifi_util_dbg_print(WIFI_MON, "%s: Mac already present in CSI list %02x..%02x\n",__func__, csi->mac_list[j][0], csi->mac_list[j][5]);
                            continue;
                        }
                        wifi_util_dbg_print(WIFI_MON, "%s: Adding Mac for csi collection %02x..%02x ap_idx %d\n",__func__, csi->mac_list[j][0], csi->mac_list[j][5], vap_index);
                        memcpy(&tmp_csiClientMac[count], &csi->mac_list[j], sizeof(mac_addr_t));
                        if((csi->client_ip[j][0] != '\0') && ((csi->client_ip_age[j]*csi->csi_time_interval)  <= IPREFRESH_PERIOD_IN_MILLISECONDS) && (g_events_monitor.vap_ip[j][0] != '\0')) {
                            refresh = FALSE;
                        }
                        else {
                            refresh = TRUE;
                        }
                        send_ping_data(csi->ap_index[j], (unsigned char *)&csi->mac_list[j][0],
                                &csi->client_ip[j][0], &g_events_monitor.vap_ip[j][0],&csi->client_ip_age[j],refresh);
                        csi->client_ip_age[j]++;
                        count++;
                    }
                }
            }
        }
        pthread_mutex_unlock(&g_events_monitor.lock);
        if (count>0) {
            dev_array = (wifi_associated_dev3_t *)malloc(sizeof(wifi_associated_dev3_t)*count);
            if (dev_array != NULL) {
                memset(dev_array, 0, (sizeof(wifi_associated_dev3_t)*count));
                for (itrc=0; itrc<count; itrc++) {
                    memcpy(dev_array[itrc].cli_MACAddress, tmp_csiClientMac[itrc], sizeof(mac_addr_t));
                }
                for (re_itr = 0; re_itr < 4; re_itr++) {
                    ret = wifi_getApAssociatedDeviceDiagnosticResult3(vap_index, &dev_array, (unsigned int *)&count);
                    if (ret == RETURN_OK) {
                        for (itrcsi=0; itrcsi < count; itrcsi++) {
                            if (dev_array[itrcsi].cli_CsiData != NULL) {
                                event = (wifi_event_t *)create_wifi_event(sizeof(wifi_monitor_data_t), wifi_event_type_monitor, wifi_event_monitor_csi);
                                if (event == NULL) {
                                    wifi_util_error_print(WIFI_MON, "%s:%d: memory allocation for event failed.\n", __func__, __LINE__);
                                    return 0;
                                }
                                memcpy(event->u.mon_data->u.csi.sta_mac, dev_array[itrcsi].cli_MACAddress, sizeof(mac_addr_t));
                                memcpy(&event->u.mon_data->u.csi.csi, dev_array[itrcsi].cli_CsiData, sizeof(wifi_csi_data_t));
                                csi_publish(event);
                                wifi_util_dbg_print(WIFI_MON, "%s Free CSI data for %02x..%02x\n",__func__,dev_array[itrcsi].cli_MACAddress[0],
                                        dev_array[itrcsi].cli_MACAddress[5]);
                                if (dev_array[itrcsi].cli_CsiData != NULL) {
                                    free(dev_array[itrcsi].cli_CsiData);
                                    dev_array[itrcsi].cli_CsiData = NULL;
                                }
                                total_events++;
                                destroy_wifi_event(event);
                            } else {
                                wifi_util_dbg_print(WIFI_MON, "%s: CSI data is NULL for %02x..%02x\n", __func__, dev_array[itrcsi].cli_MACAddress[0],
                                        dev_array[itrcsi].cli_MACAddress[5]);
                            }
                        }
                    } else {
                        wifi_util_error_print(WIFI_MON, "%s: wifi_getApAssociatedDeviceDiagnosticResult3 api returned error\n", __func__);
                    }
                    if (total_events == count) {
                        break;
                    }
                }
                free(dev_array);
            } else {
                wifi_util_error_print(WIFI_MON, "%s: Failed to allocate mem to dev_array\n",__func__);
            }
        }
    }
    csi_refresh_session();
    return TIMER_TASK_COMPLETE;
}


static int clientdiag_sheduler_enable(int ap_index)
{
    unsigned int clientdiag_interval;
    unsigned int vap_array_index;

    getVAPArrayIndexFromVAPIndex((unsigned int)ap_index, &vap_array_index);

    pthread_mutex_lock(&g_events_monitor.lock);
    clientdiag_interval = g_events_monitor.diag_session[vap_array_index].interval;
    pthread_mutex_unlock(&g_events_monitor.lock);

    if (clientdiag_interval != 0) {
        if (g_monitor_module.clientdiag_id[vap_array_index] == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, FALSE,
                    &(g_monitor_module.clientdiag_id[vap_array_index]), associated_device_diagnostics_send_event,
                    (void *)&(g_monitor_module.clientdiag_sched_arg[vap_array_index]), clientdiag_interval, 0);
        } else {
            if (g_monitor_module.clientdiag_sched_interval[vap_array_index] != clientdiag_interval) {
                g_monitor_module.clientdiag_sched_interval[vap_array_index] = clientdiag_interval;
                scheduler_update_timer_task_interval(g_monitor_module.sched,
                        g_monitor_module.clientdiag_id[vap_array_index], clientdiag_interval);
            }
        }
    } else {
        if (g_monitor_module.clientdiag_id[vap_array_index] != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched,
                    g_monitor_module.clientdiag_id[vap_array_index]);
            g_monitor_module.clientdiag_id[vap_array_index] = 0;
        }
    }
    return 0;
}

void diagdata_set_interval(int interval, unsigned int ap_idx)
{
    wifi_monitor_data_t data;
    unsigned int vap_array_index;

    if(ap_idx >= MAX_VAP) {
        wifi_util_error_print(WIFI_MON, "%s: ap_idx %d not valid\n",__func__, ap_idx);
    }

    getVAPArrayIndexFromVAPIndex(ap_idx, &vap_array_index);

    pthread_mutex_lock(&g_events_monitor.lock);
    g_events_monitor.diag_session[vap_array_index].interval = interval;
    wifi_util_dbg_print(WIFI_MON, "%s: ap_idx %d configuring inteval %d\n", __func__, ap_idx, interval);
    pthread_mutex_unlock(&g_events_monitor.lock);

    memset(&data, 0, sizeof(wifi_monitor_data_t));
    data.id = msg_id++;
    data.ap_index = ap_idx;
    push_event_to_monitor_queue(&data, wifi_event_monitor_clientdiag_update_config, NULL);
}

int associated_device_diagnostics_send_event(void* arg)
{
    int *ap_index;
    wifi_event_t *event = NULL;
    if (arg == NULL) {
        wifi_util_error_print(WIFI_MON, "%s(): Error arg NULL\n",__func__);
        return TIMER_TASK_ERROR;
    }

    event = create_wifi_event(sizeof(wifi_monitor_data_t), wifi_event_type_monitor, wifi_event_monitor_diagnostics);
    if (event == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: memory allocation for event failed.\n", __func__, __LINE__);
        return TIMER_TASK_ERROR;
    }

    ap_index = (int *) arg;
    event->u.mon_data->ap_index = *ap_index;

    events_rbus_publish(event);

    destroy_wifi_event(event);

    return TIMER_TASK_COMPLETE;
}

#if defined (DUAL_CORE_XB3)
static BOOL erouterGetIpAddress()
{
    FILE *f;
    char ptr[32];
    char *cmd = "deviceinfo.sh -eip";

    memset (ptr, 0, sizeof(ptr));

    if ((f = popen(cmd, "r")) == NULL) {
        return false;
    } else {
        *ptr = 0;
        fgets(ptr,32,f);
        pclose(f);
    }

    if ((ptr[0] >= '1') && (ptr[0] <= '9')) {
        memset(erouterIpAddrStr, 0, sizeof(erouterIpAddrStr));
        /*CID: 159695 BUFFER_SIZE_WARNING*/
        strncpy((char*)erouterIpAddrStr, ptr, sizeof(erouterIpAddrStr)-1);
        erouterIpAddrStr[sizeof(erouterIpAddrStr)-1] = '\0';
        return true;
    } else {
        return false;
    }
}
#endif

static unsigned char updateNasIpStatus (int apIndex)
{
#if defined (DUAL_CORE_XB3)

    static unsigned char erouterIpInitialized = 0;
    if(isVapHotspotSecure(apIndex)) {
        if (!erouterIpInitialized) {
            if (FALSE == erouterGetIpAddress()) {
                return 0;
            } else {
                erouterIpInitialized = 1;
                return wifi_pushSecureHotSpotNASIP(apIndex, erouterIpAddrStr);
            }
        } else {
            return wifi_pushSecureHotSpotNASIP(apIndex, erouterIpAddrStr);
        }
    } else {
        return 1;
    }
#else
    UNREFERENCED_PARAMETER(apIndex);
    return 1;
#endif
}

/* Log VAP status on percentage basis */
static void logVAPUpStatus()
{
    int i=0;
    int vapup_percentage=0;
    char log_buf[1024]={0};
    char telemetry_buf[1024]={0};
    char vap_buf[16]={0};
    char tmp[128]={0};
    errno_t rc = -1;
    wifi_mgr_t *mgr = get_wifimgr_obj();

    UINT vap_index = 0;

    wifi_util_dbg_print(WIFI_MON, "Entering %s:%d \n",__FUNCTION__,__LINE__);
    get_formatted_time(tmp);
    rc = sprintf_s(log_buf, sizeof(log_buf), "%s WIFI_VAP_PERCENT_UP:",tmp);
    if(rc < EOK) {
        ERR_CHK(rc);
    }

    for(i = 0; i < (int)getTotalNumberVAPs(); i++)
    {
        vap_index = VAP_INDEX(mgr->hal_cap, i);
        vapup_percentage=((int)(vap_up_arr[vap_index])*100)/(vap_iteration);
        char delimiter = (i+1) < ((int)getTotalNumberVAPs()+1) ?';':' ';
        rc = sprintf_s(vap_buf, sizeof(vap_buf), "%d,%d%c",(vap_index + 1),vapup_percentage, delimiter);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = strcat_s(log_buf, sizeof(log_buf), vap_buf);
        ERR_CHK(rc);
        rc = strcat_s(telemetry_buf, sizeof(telemetry_buf), vap_buf);
        ERR_CHK(rc);
    }
    rc = strcat_s(log_buf, sizeof(log_buf), "\n");
    ERR_CHK(rc);
    write_to_file(wifi_health_log,log_buf);
    wifi_util_dbg_print(WIFI_MON, "%s", log_buf);
    t2_event_s("WIFI_VAPPERC_split", telemetry_buf);
    vap_iteration=0;
    memset(vap_up_arr, 0,sizeof(vap_up_arr));
    wifi_util_dbg_print(WIFI_MON, "Exiting %s:%d \n",__FUNCTION__,__LINE__);

}
/* Capture the VAP status periodically */
int captureVAPUpStatus(void *arg)
{
    static unsigned int i = 0;
    int vap_status = 0;
    wifi_mgr_t *mgr = get_wifimgr_obj();

    UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
    wifi_util_dbg_print(WIFI_MON, "Entering %s:%d for VAP %d\n",__FUNCTION__,__LINE__, vap_index);
    if (vap_index > MAX_VAP) {
        wifi_util_error_print(WIFI_MON, "wrong vap_index:%d for i:%d\n", __func__, __LINE__, vap_index, i);
        i = 0;
        return TIMER_TASK_COMPLETE;
    }

    vap_status = g_monitor_module.bssid_data[vap_index].ap_params.ap_status;
    if (vap_status) {
        vap_up_arr[vap_index]=vap_up_arr[vap_index]+1;
        if (!vap_nas_status[vap_index]) {
            vap_nas_status[vap_index] = updateNasIpStatus(vap_index);
        }
    } else {
        vap_nas_status[vap_index] = 0;
    }
    i++;
    if(i >= getTotalNumberVAPs()) {
        i = 0;
        vap_iteration++;
        wifi_util_dbg_print(WIFI_MON, "Exiting %s:%d \n",__FUNCTION__,__LINE__);
        return TIMER_TASK_COMPLETE;
    }
    wifi_util_dbg_print(WIFI_MON, "Exiting %s:%d \n",__FUNCTION__,__LINE__);
    return TIMER_TASK_CONTINUE;

}

int get_chan_util_upload_period()
{
    int logInterval = DEFAULT_CHANUTIL_LOG_INTERVAL;//Default Value 15mins.

    get_vap_dml_parameters(CH_UTILITY_LOG_INTERVAL, &logInterval);
    wifi_util_dbg_print(WIFI_MON, " %s:%d logInterval %d \n",__FUNCTION__,__LINE__,logInterval);  
    return logInterval;
}

static int readLogInterval()
{
    int logInterval=60;//Default Value 60mins.

    wifi_util_dbg_print(WIFI_MON, "Entering %s:%d \n",__FUNCTION__,__LINE__);
    get_vap_dml_parameters(DEVICE_LOG_INTERVAL, &logInterval);
    wifi_util_dbg_print(WIFI_MON, "Exiting %s:%d \n",__FUNCTION__,__LINE__);
    return logInterval;
}

void associated_client_diagnostics ()
{
    wifi_associated_dev3_t dev_conn ;
    int radioIndex;
    int chan_util = 0;

    char s_mac[MIN_MAC_LEN+1];
    int index = g_monitor_module.inst_msmt.ap_index;

    memset(&dev_conn, 0, sizeof(wifi_associated_dev3_t));
    snprintf(s_mac, MIN_MAC_LEN+1, "%02x%02x%02x%02x%02x%02x", g_monitor_module.inst_msmt.sta_mac[0],
            g_monitor_module.inst_msmt.sta_mac[1],g_monitor_module.inst_msmt.sta_mac[2], g_monitor_module.inst_msmt.sta_mac[3],
            g_monitor_module.inst_msmt.sta_mac[4], g_monitor_module.inst_msmt.sta_mac[5]);
    radioIndex = getRadioIndexFromAp(index);

    wifi_util_dbg_print(WIFI_MON, "%s:%d: get radio NF %d\n", __func__, __LINE__, g_monitor_module.radio_data[radioIndex].NoiseFloor);

    /* ToDo: We can get channel_util percentage now, channel_ineterference percentage is 0 */
    if (get_radio_channel_utilization(radioIndex, &chan_util) == RETURN_OK) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d: get channel stats for radio %d\n", __func__, __LINE__, radioIndex);
        g_monitor_module.radio_data[radioIndex].channelUtil = chan_util;
        g_monitor_module.radio_data[radioIndex].channelInterference = 0;
        g_monitor_module.radio_data[getRadioIndexFromAp(radioIndex + 1)].channelUtil = 0;
        g_monitor_module.radio_data[getRadioIndexFromAp(radioIndex + 1)].channelInterference = 0;
    }

    wifi_util_dbg_print(WIFI_MON, "%s:%d: get single connected client %s stats\n", __func__, __LINE__, s_mac);
#if !defined(_CBR_PRODUCT_REQ_)
    wifi_util_dbg_print(WIFI_MON, "WIFI_HAL enabled, calling wifi_getApAssociatedClientDiagnosticResult\n");
    if (wifi_getApAssociatedClientDiagnosticResult(index, s_mac, &dev_conn) == RETURN_OK) {
        process_diagnostics(index, &dev_conn, 1);
    }
#else
    wifi_util_dbg_print(WIFI_MON, "WIFI_HAL Not enabled. Using wifi default values\n");
    process_diagnostics(index, &dev_conn, 1);
#endif
}
#endif // CCSP_COMMON


int get_radio_data(int radio_index, wifi_radioTrafficStats2_t *radio_traffic_stats)
{
    pthread_mutex_lock(&g_monitor_module.data_lock);
    radio_traffic_stats->radio_NoiseFloor = g_monitor_module.radio_data[radio_index].NoiseFloor;
    radio_traffic_stats->radio_ActivityFactor = g_monitor_module.radio_data[radio_index].RadioActivityFactor;
    radio_traffic_stats->radio_CarrierSenseThreshold_Exceeded = g_monitor_module.radio_data[radio_index].CarrierSenseThreshold_Exceeded;
    radio_traffic_stats->radio_ChannelUtilization = g_monitor_module.radio_data[radio_index].channelUtil;
    radio_traffic_stats->radio_BytesSent = g_monitor_module.radio_data[radio_index].radio_BytesSent;
    radio_traffic_stats->radio_BytesReceived = g_monitor_module.radio_data[radio_index].radio_BytesReceived;
    radio_traffic_stats->radio_PacketsSent = g_monitor_module.radio_data[radio_index].radio_PacketsSent;
    radio_traffic_stats->radio_PacketsReceived = g_monitor_module.radio_data[radio_index].radio_PacketsReceived;
    radio_traffic_stats->radio_ErrorsSent = g_monitor_module.radio_data[radio_index].radio_ErrorsSent;
    radio_traffic_stats->radio_ErrorsReceived = g_monitor_module.radio_data[radio_index].radio_ErrorsReceived;
    radio_traffic_stats->radio_DiscardPacketsSent = g_monitor_module.radio_data[radio_index].radio_DiscardPacketsSent;
    radio_traffic_stats->radio_DiscardPacketsReceived = g_monitor_module.radio_data[radio_index].radio_DiscardPacketsReceived;
    radio_traffic_stats->radio_RetransmissionMetirc = g_monitor_module.radio_data[radio_index].radio_RetransmissionMetirc;
    radio_traffic_stats->radio_PLCPErrorCount = g_monitor_module.radio_data[radio_index].radio_PLCPErrorCount;
    radio_traffic_stats->radio_FCSErrorCount = g_monitor_module.radio_data[radio_index].radio_FCSErrorCount;
    radio_traffic_stats->radio_MaximumNoiseFloorOnChannel = g_monitor_module.radio_data[radio_index].radio_MaximumNoiseFloorOnChannel;
    radio_traffic_stats->radio_MinimumNoiseFloorOnChannel = g_monitor_module.radio_data[radio_index].radio_MinimumNoiseFloorOnChannel;
    radio_traffic_stats->radio_MedianNoiseFloorOnChannel = g_monitor_module.radio_data[radio_index].radio_MedianNoiseFloorOnChannel;
    radio_traffic_stats->radio_StatisticsStartTime = g_monitor_module.radio_data[radio_index].radio_StatisticsStartTime;
    pthread_mutex_unlock(&g_monitor_module.data_lock);

    return 0;
}

int radio_diagnostics(void *arg)
{
    wifi_radioTrafficStats2_t radioTrafficStats;
    //char            ChannelsInUse[256] = {0};
    char            RadioFreqBand[64] = {0};
#ifdef CCSP_COMMON
    char            RadioChanBand[64] = {0};
#endif // CCSP_COMMON
    static unsigned int radiocnt = 0;
    wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(radiocnt);

    pthread_mutex_lock(&g_active_msmt.lock);
    if (g_active_msmt.is_running == true) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d Active Measurement is running, skipping radio diagnostics...\n",__func__,__LINE__);
        pthread_mutex_unlock(&g_active_msmt.lock);
        return TIMER_TASK_COMPLETE;
    }
    pthread_mutex_unlock(&g_active_msmt.lock);

    wifi_util_dbg_print(WIFI_MON, "%s : %d getting radio Traffic stats for Radio %d\n",__func__,__LINE__, radiocnt);
    memset(&radioTrafficStats, 0, sizeof(wifi_radioTrafficStats2_t));
    memset(&g_monitor_module.radio_data[radiocnt], 0, sizeof(radio_data_t));

    if (radioOperation != NULL) {
        if(radioOperation->enable) {

#ifdef CCSP_COMMON
            if (wifi_getRadioTrafficStats2(radiocnt, &radioTrafficStats) == RETURN_OK) {
#else
            if (ow_mesh_ext_get_radio_stats(radiocnt, &radioTrafficStats) == RETURN_OK) {
#endif // CCSP_COMMON
                /* update the g_active_msmt with the radio data */
                g_monitor_module.radio_data[radiocnt].NoiseFloor = radioTrafficStats.radio_NoiseFloor;
                g_monitor_module.radio_data[radiocnt].RadioActivityFactor = radioTrafficStats.radio_ActivityFactor;
                g_monitor_module.radio_data[radiocnt].CarrierSenseThreshold_Exceeded = radioTrafficStats.radio_CarrierSenseThreshold_Exceeded;
                g_monitor_module.radio_data[radiocnt].channelUtil = radioTrafficStats.radio_ChannelUtilization;
                g_monitor_module.radio_data[radiocnt].radio_BytesSent = radioTrafficStats.radio_BytesSent;
                g_monitor_module.radio_data[radiocnt].radio_BytesReceived = radioTrafficStats.radio_BytesReceived;
                g_monitor_module.radio_data[radiocnt].radio_PacketsSent = radioTrafficStats.radio_PacketsSent;
                g_monitor_module.radio_data[radiocnt].radio_PacketsReceived = radioTrafficStats.radio_PacketsReceived;
                g_monitor_module.radio_data[radiocnt].radio_ErrorsSent = radioTrafficStats.radio_ErrorsSent;
                g_monitor_module.radio_data[radiocnt].radio_ErrorsReceived = radioTrafficStats.radio_ErrorsReceived;
                g_monitor_module.radio_data[radiocnt].radio_DiscardPacketsSent = radioTrafficStats.radio_DiscardPacketsSent;
                g_monitor_module.radio_data[radiocnt].radio_DiscardPacketsReceived = radioTrafficStats.radio_DiscardPacketsReceived;
                g_monitor_module.radio_data[radiocnt].radio_InvalidMACCount = radioTrafficStats.radio_InvalidMACCount;
                g_monitor_module.radio_data[radiocnt].radio_PacketsOtherReceived = radioTrafficStats.radio_PacketsOtherReceived;
                g_monitor_module.radio_data[radiocnt].radio_RetransmissionMetirc = radioTrafficStats.radio_RetransmissionMetirc;
                g_monitor_module.radio_data[radiocnt].radio_PLCPErrorCount = radioTrafficStats.radio_PLCPErrorCount;
                g_monitor_module.radio_data[radiocnt].radio_FCSErrorCount = radioTrafficStats.radio_FCSErrorCount;
                g_monitor_module.radio_data[radiocnt].radio_MaximumNoiseFloorOnChannel = radioTrafficStats.radio_MaximumNoiseFloorOnChannel;
                g_monitor_module.radio_data[radiocnt].radio_MinimumNoiseFloorOnChannel = radioTrafficStats.radio_MinimumNoiseFloorOnChannel;
                g_monitor_module.radio_data[radiocnt].radio_MedianNoiseFloorOnChannel = radioTrafficStats.radio_MedianNoiseFloorOnChannel;
                g_monitor_module.radio_data[radiocnt].radio_StatisticsStartTime = radioTrafficStats.radio_StatisticsStartTime;
#if 0
                /* When we trigger below API then Broadcom driver internally trigger offchannel scan.
                *  We don't want this offchannel scan at every 30 seconds. So, for resolution of
                *  this issue we commented out below API.
                */
                wifi_getRadioChannelsInUse (radiocnt, ChannelsInUse);
                strncpy((char *)&g_monitor_module.radio_data[radiocnt].ChannelsInUse, ChannelsInUse,sizeof(ChannelsInUse));
#endif
                g_monitor_module.radio_data[radiocnt].primary_radio_channel = radioOperation->channel;

                if (freq_band_conversion((wifi_freq_bands_t *)&radioOperation->band, (char *)RadioFreqBand, sizeof(RadioFreqBand), ENUM_TO_STRING) != RETURN_OK)
                {
                    wifi_util_error_print(WIFI_MON,"%s:%d: frequency band conversion failed\n", __func__, __LINE__);
                } else {
                    strncpy((char *)&g_monitor_module.radio_data[radiocnt].frequency_band, RadioFreqBand, sizeof(RadioFreqBand));
                    g_monitor_module.radio_data[radiocnt].frequency_band[sizeof(g_monitor_module.radio_data[radiocnt].frequency_band)-1] = '\0';
                    wifi_util_dbg_print(WIFI_MON, "%s:%d: Frequency band is  %s\n", __func__, __LINE__, RadioFreqBand);
                }

#ifdef CCSP_COMMON
                wifi_getRadioOperatingChannelBandwidth(radiocnt,RadioChanBand);
                strncpy((char *)&g_monitor_module.radio_data[radiocnt].channel_bandwidth, RadioChanBand,sizeof(RadioChanBand));
                wifi_util_dbg_print(WIFI_MON, "%s:%d: channelbandwidth is  %s\n", __func__, __LINE__, RadioChanBand);
#endif // CCSP_COMMON
            } else {
#ifdef CCSP_COMMON
                wifi_util_error_print(WIFI_MON, "%s : %d wifi_getRadioTrafficStats2 failed for rdx : %d\n",__func__,__LINE__,radiocnt);
#else
                wifi_util_error_print(WIFI_MON, "%s : %d ow_mesh_ext_get_radio_stats failed for rdx : %d\n",__func__,__LINE__,radiocnt);
#endif // CCSP_COMMON
            }
        } else {
            wifi_util_dbg_print(WIFI_MON, "%s : %d Radio : %d is not enabled\n",__func__,__LINE__,radiocnt);
        }
    } else {
        wifi_util_error_print(WIFI_MON, "%s : %d Failed to get getRadioOperationParam for rdx : %d\n",__func__,__LINE__,radiocnt);
    }
    radiocnt++;
    if (radiocnt >= getNumberRadios()) {
        radiocnt = 0;
        return TIMER_TASK_COMPLETE;
    }
    return TIMER_TASK_CONTINUE;
}

#ifdef CCSP_COMMON
int associated_devices_diagnostics(void *arg)
{
    static unsigned int idx = 0;
    static unsigned int phase = 0;
    static unsigned int vap_index = 0;
    static wifi_associated_dev3_t *dev_array = NULL;
    static unsigned int num_devs = 0;
    wifi_front_haul_bss_t *bss_param;

    wifi_mgr_t *mgr = get_wifimgr_obj();

    UINT radio = RADIO_INDEX(mgr->hal_cap, idx);
    if (g_monitor_module.radio_presence[radio] == false) {
        goto exit_task;
    }
    vap_index = VAP_INDEX(mgr->hal_cap, idx);

    if (isVapSTAMesh(vap_index)) {
        goto exit_task;
    }

    bss_param = Get_wifi_object_bss_parameter(vap_index);
    if (bss_param == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d Failed to get bss info for vap index %d\n",
            __func__, __LINE__, vap_index);
        goto exit_task;
    }

    if (bss_param->enabled == false) {
        goto exit_task;
    }

    if (phase == 0) { /* phase 0: collect diag data */
        if (dev_array == NULL) {
            num_devs = 0;
            if (wifi_getApAssociatedDeviceDiagnosticResult3(vap_index, &dev_array, &num_devs) != RETURN_OK) {
                wifi_util_error_print(WIFI_MON, "[%s:%d]Wi-Fi hal get Associated Device failure dev_array:[%p] for vap_index:%d number of device:%d\r\n",
                            __func__, __LINE__, dev_array, vap_index, num_devs);
                if (dev_array) {
                    free(dev_array);
                    dev_array = NULL;
                }
                goto exit_task;
            }
            phase = 1;
            return TIMER_TASK_CONTINUE;
        }
    }

    if (phase == 1) { /* phase 1: process diag data */
        events_update_clientdiagdata(num_devs, vap_index, dev_array);
        process_diagnostics(vap_index, dev_array, num_devs);

        if (dev_array) {
            free(dev_array);
            dev_array = NULL;
        }
    }

exit_task:
    idx++;
    phase = 0;
    if (idx >= getTotalNumberVAPs()) {
        idx = 0;
        return TIMER_TASK_COMPLETE;
    }
    return TIMER_TASK_CONTINUE;
}


bool active_sta_connection_status(int ap_index, char *mac)
{
    sta_data_t  *sta;
    hash_map_t  *sta_map;
    unsigned int vap_array_index;

    if (mac == NULL) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d input mac adrress is NULL for ap_index:%d\n", __func__, __LINE__, ap_index);
        return false;
    }

    getVAPArrayIndexFromVAPIndex(ap_index, &vap_array_index);

    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;
    if (sta_map == NULL) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d: return, stamap is NULL for vap:%d\n", __func__, __LINE__, ap_index);
        return false;
    }
    str_tolower(mac);
    sta = (sta_data_t *)hash_map_get(sta_map, mac);

    if (sta == NULL) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d: return, sta:%s is not part of hashmap on vap:%d\n", __func__, __LINE__, mac, ap_index);
        return false;
    } else if(sta->connection_authorized != true) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d: return, sta:%s is not ACTIVE on vap:%d\n", __func__, __LINE__, mac, ap_index);
        return false;
    }
    return true;
}

int device_disassociated(int ap_index, char *mac, int reason)
{
    wifi_monitor_data_t data;
    assoc_dev_data_t assoc_data;
    greylist_data_t greylist_data;
    unsigned int mac_addr[MAC_ADDR_LEN];
    mac_address_t grey_list_mac;

    if (mac == NULL) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d input mac adrress is NULL for ap_index:%d reason:%d\n", __func__, __LINE__, ap_index, reason);
        return -1;
    }

    if (reason == WLAN_RADIUS_GREYLIST_REJECT) {
        wifi_util_dbg_print(WIFI_MON,"Device disassociated due to Greylist\n");
        greylist_data.reason = reason;

        str_to_mac_bytes(mac, grey_list_mac);
        memcpy(greylist_data.sta_mac, &grey_list_mac, sizeof(mac_address_t));
        wifi_util_dbg_print(WIFI_MON," sending Greylist mac to  ctrl queue %s\n",mac);
        push_event_to_ctrl_queue(&greylist_data, sizeof(greylist_data), wifi_event_type_hal_ind, wifi_event_radius_greylist, NULL);

    }
    if (active_sta_connection_status(ap_index, mac) == false) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d: sta[%s] not connected with ap:[%d]\r\n", __func__, __LINE__, mac, ap_index);
        return 0;
    }

    memset(&data, 0, sizeof(wifi_monitor_data_t));
    data.id = msg_id++;

    data.ap_index = ap_index;
    sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
            &mac_addr[0], &mac_addr[1], &mac_addr[2],
            &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    data.u.dev.sta_mac[0] = mac_addr[0]; data.u.dev.sta_mac[1] = mac_addr[1]; data.u.dev.sta_mac[2] = mac_addr[2];
    data.u.dev.sta_mac[3] = mac_addr[3]; data.u.dev.sta_mac[4] = mac_addr[4]; data.u.dev.sta_mac[5] = mac_addr[5];
    data.u.dev.reason = reason;

    wifi_util_info_print(WIFI_MON, "%s:%d:Device diaassociated on interface:%d mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
            __func__, __LINE__, ap_index,
            data.u.dev.sta_mac[0], data.u.dev.sta_mac[1], data.u.dev.sta_mac[2],
            data.u.dev.sta_mac[3], data.u.dev.sta_mac[4], data.u.dev.sta_mac[5]);
    csi_update_client_mac_status(data.u.dev.sta_mac, FALSE, ap_index);

    memcpy(assoc_data.dev_stats.cli_MACAddress, data.u.dev.sta_mac, sizeof(mac_address_t));
    assoc_data.ap_index = data.ap_index;
    assoc_data.reason = reason;
    push_event_to_ctrl_queue(&assoc_data, sizeof(assoc_data), wifi_event_type_hal_ind, wifi_event_hal_disassoc_device, NULL);

    push_event_to_monitor_queue(&data, wifi_event_monitor_disconnect, NULL);

    return 0;
}

int vapstatus_callback(int apIndex, wifi_vapstatus_t status)
{
    wifi_util_dbg_print(WIFI_MON,"%s called for %d and status %d \n",__func__, apIndex, status);
    g_monitor_module.bssid_data[apIndex].ap_params.ap_status = status;
    return 0;
}

int device_deauthenticated(int ap_index, char *mac, int reason)
{
    wifi_monitor_data_t data;
    unsigned int mac_addr[MAC_ADDR_LEN];
    greylist_data_t greylist_data;
    assoc_dev_data_t assoc_data;
    mac_address_t grey_list_mac;

    if (mac == NULL) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d input mac adrress is NULL for ap_index:%d reason:%d\n", __func__, __LINE__, ap_index, reason);
        return -1;
    }

    if (reason == WLAN_RADIUS_GREYLIST_REJECT) {
        str_to_mac_bytes(mac, grey_list_mac);
        wifi_util_dbg_print(WIFI_MON,"Device disassociated due to Greylist\n");
        greylist_data.reason = reason;
        memcpy(greylist_data.sta_mac, &grey_list_mac, sizeof(mac_address_t));
        wifi_util_dbg_print(WIFI_MON,"Sending Greylist mac to ctrl queue %s\n",mac);
        push_event_to_ctrl_queue(&greylist_data, sizeof(greylist_data), wifi_event_type_hal_ind, wifi_event_radius_greylist, NULL);

    }
    if (active_sta_connection_status(ap_index, mac) == false) {
        wifi_util_dbg_print(WIFI_MON,"%s:%d: sta[%s] not connected with ap:[%d]\r\n", __func__, __LINE__, mac, ap_index);
        if ((reason == 2 || reason == 14 || reason == 19))
        {
            memset(&data, 0, sizeof(wifi_monitor_data_t));
            data.id = msg_id++;
            data.ap_index = ap_index;
            data.u.dev.reason = reason;
            sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                    &mac_addr[0], &mac_addr[1], &mac_addr[2],
                    &mac_addr[3], &mac_addr[4], &mac_addr[5]);
            data.u.dev.sta_mac[0] = mac_addr[0]; data.u.dev.sta_mac[1] = mac_addr[1]; data.u.dev.sta_mac[2] = mac_addr[2];
            data.u.dev.sta_mac[3] = mac_addr[3]; data.u.dev.sta_mac[4] = mac_addr[4]; data.u.dev.sta_mac[5] = mac_addr[5];
            wifi_util_info_print(WIFI_MON, "%s:%d   Device deauthenticated on interface:%d mac:%02x:%02x:%02x:%02x:%02x:%02x with reason %d\n",
                                 __func__, __LINE__, ap_index,
                                 data.u.dev.sta_mac[0], data.u.dev.sta_mac[1], data.u.dev.sta_mac[2],
                                 data.u.dev.sta_mac[3], data.u.dev.sta_mac[4], data.u.dev.sta_mac[5], reason);
            push_event_to_monitor_queue(&data, wifi_event_monitor_deauthenticate_password_fail, NULL);
        }
         return 0;
    }

    memset(&data, 0, sizeof(wifi_monitor_data_t));
    data.id = msg_id++;

    data.ap_index = ap_index;
    sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
            &mac_addr[0], &mac_addr[1], &mac_addr[2],
            &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    data.u.dev.sta_mac[0] = mac_addr[0]; data.u.dev.sta_mac[1] = mac_addr[1]; data.u.dev.sta_mac[2] = mac_addr[2];
    data.u.dev.sta_mac[3] = mac_addr[3]; data.u.dev.sta_mac[4] = mac_addr[4]; data.u.dev.sta_mac[5] = mac_addr[5];
    data.u.dev.reason = reason;

    wifi_util_info_print(WIFI_MON, "%s:%d   Device deauthenticated on interface:%d mac:%02x:%02x:%02x:%02x:%02x:%02x with reason %d\n",
            __func__, __LINE__, ap_index,
            data.u.dev.sta_mac[0], data.u.dev.sta_mac[1], data.u.dev.sta_mac[2],
            data.u.dev.sta_mac[3], data.u.dev.sta_mac[4], data.u.dev.sta_mac[5], reason);
    csi_update_client_mac_status(data.u.dev.sta_mac, FALSE, ap_index);


    memcpy(assoc_data.dev_stats.cli_MACAddress, data.u.dev.sta_mac, sizeof(mac_address_t));
    assoc_data.ap_index = data.ap_index;
    assoc_data.reason = reason;
    push_event_to_ctrl_queue(&assoc_data, sizeof(assoc_data), wifi_event_type_hal_ind, wifi_event_hal_disassoc_device, NULL);

    push_event_to_monitor_queue(&data, wifi_event_monitor_deauthenticate, NULL);

    return 0;
}

int device_associated(int ap_index, wifi_associated_dev_t *associated_dev)
{
    wifi_monitor_data_t data;
    assoc_dev_data_t assoc_data;
    wifi_radioTrafficStats2_t chan_stats;
    int radio_index;
    char vap_name[32];

    memset(&assoc_data, 0, sizeof(assoc_data));
    memset(&data, 0, sizeof(wifi_monitor_data_t));

    data.id = msg_id++;

    data.ap_index = ap_index;
    //data->u.dev.reason = reason;

    data.u.dev.sta_mac[0] = associated_dev->cli_MACAddress[0]; data.u.dev.sta_mac[1] = associated_dev->cli_MACAddress[1];
    data.u.dev.sta_mac[2] = associated_dev->cli_MACAddress[2]; data.u.dev.sta_mac[3] = associated_dev->cli_MACAddress[3];
    data.u.dev.sta_mac[4] = associated_dev->cli_MACAddress[4]; data.u.dev.sta_mac[5] = associated_dev->cli_MACAddress[5];

    wifi_util_info_print(WIFI_MON, "%s:%d:Device associated on interface:%d mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
            __func__, __LINE__, ap_index,
            data.u.dev.sta_mac[0], data.u.dev.sta_mac[1], data.u.dev.sta_mac[2],
            data.u.dev.sta_mac[3], data.u.dev.sta_mac[4], data.u.dev.sta_mac[5]);

    csi_update_client_mac_status(data.u.dev.sta_mac, TRUE, ap_index);

    convert_vap_index_to_name(&((wifi_mgr_t *)get_wifimgr_obj())->hal_cap.wifi_prop, ap_index, vap_name);
    radio_index = convert_vap_name_to_radio_array_index(&((wifi_mgr_t *)get_wifimgr_obj())->hal_cap.wifi_prop, vap_name);
    get_radio_data(radio_index, &chan_stats);

    memcpy(assoc_data.dev_stats.cli_MACAddress, data.u.dev.sta_mac, sizeof(mac_address_t));
    assoc_data.dev_stats.cli_SignalStrength = associated_dev->cli_SignalStrength;
    assoc_data.dev_stats.cli_RSSI = associated_dev->cli_RSSI;
    assoc_data.dev_stats.cli_AuthenticationState = associated_dev->cli_AuthenticationState;
    assoc_data.dev_stats.cli_LastDataDownlinkRate = associated_dev->cli_LastDataDownlinkRate;

    assoc_data.dev_stats.cli_LastDataUplinkRate = associated_dev->cli_LastDataUplinkRate;
    assoc_data.dev_stats.cli_SignalStrength = associated_dev->cli_SignalStrength;
    assoc_data.dev_stats.cli_Retransmissions = associated_dev->cli_Retransmissions;
    assoc_data.dev_stats.cli_Active = associated_dev->cli_Active;

    if (associated_dev->cli_SNR != 0) {
        assoc_data.dev_stats.cli_SNR = associated_dev->cli_SNR;
    } else {
        assoc_data.dev_stats.cli_SNR = associated_dev->cli_RSSI - chan_stats.radio_NoiseFloor;
    }

    assoc_data.dev_stats.cli_DataFramesSentAck = associated_dev->cli_DataFramesSentAck;
    assoc_data.dev_stats.cli_DataFramesSentNoAck = associated_dev->cli_DataFramesSentNoAck;
    assoc_data.dev_stats.cli_BytesSent = associated_dev->cli_BytesSent;
    assoc_data.dev_stats.cli_BytesReceived = associated_dev->cli_BytesReceived;
    assoc_data.dev_stats.cli_MinRSSI = associated_dev->cli_MinRSSI;
    assoc_data.dev_stats.cli_MaxRSSI = associated_dev->cli_MaxRSSI;
    assoc_data.dev_stats.cli_Disassociations = associated_dev->cli_Disassociations;
    assoc_data.dev_stats.cli_AuthenticationFailures = associated_dev->cli_AuthenticationFailures;
    snprintf(assoc_data.dev_stats.cli_OperatingStandard, sizeof(assoc_data.dev_stats.cli_OperatingStandard),"%s", associated_dev->cli_OperatingStandard);
    snprintf(assoc_data.dev_stats.cli_OperatingChannelBandwidth, sizeof(assoc_data.dev_stats.cli_OperatingChannelBandwidth),"%s", associated_dev->cli_OperatingChannelBandwidth);
    snprintf(assoc_data.dev_stats.cli_InterferenceSources, sizeof(assoc_data.dev_stats.cli_InterferenceSources),"%s", associated_dev->cli_InterferenceSources);


    assoc_data.ap_index = data.ap_index;
    push_event_to_ctrl_queue(&assoc_data, sizeof(assoc_data), wifi_event_type_hal_ind, wifi_event_hal_assoc_device, NULL);

    push_event_to_monitor_queue(&data, wifi_event_monitor_connect, NULL);

    return 0;
}

static void scheduler_telemetry_tasks(void)
{
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
    wifi_mgr_t *mgr = get_wifimgr_obj();
    unsigned int total_radios = getNumberRadios();
    unsigned int rad_index = 0;
#endif //FEATURE_OFF_CHANNEL_SCAN_5G
    if (!g_monitor_module.instntMsmtenable) {
        g_monitor_module.curr_chan_util_period = get_chan_util_upload_period();
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
        for (rad_index = 0; rad_index < total_radios; rad_index++)
        {
            if (is_radio_band_5G(mgr->radio_config[rad_index].oper.band))
            {
                wifi_util_dbg_print(WIFI_MON,"Off_channel_scan Nscan: %lu\n", g_monitor_module.off_channel_cfg[rad_index].NscanSec);
                if (g_monitor_module.off_channel_cfg[rad_index].NscanSec == 0) {
                    /*If Nscan is 0 at boot up, running scheduler at default value*/
                    g_monitor_module.off_channel_cfg[rad_index].NscanSec = OFFCHAN_DEFAULT_NSCAN_IN_SEC;
                }
                g_monitor_module.off_channel_cfg[rad_index].curr_off_channel_scan_period = g_monitor_module.off_channel_cfg[rad_index].NscanSec;
            }
        }
#endif //FEATURE_OFF_CHANNEL_SCAN_5G

        //5 minutes
        if (g_monitor_module.refresh_task_id == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.refresh_task_id, refresh_task_period,
                    NULL, REFRESH_TASK_INTERVAL_MS, 0);
        }
        if (g_monitor_module.associated_devices_id == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.associated_devices_id, associated_devices_diagnostics,
                    NULL, ASSOCIATED_DEVICE_DIAG_INTERVAL_MS, 0);
        }
        if (g_monitor_module.vap_status_id == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.vap_status_id, captureVAPUpStatus,
                    NULL, CAPTURE_VAP_STATUS_INTERVAL_MS, 0);
        }
        if (g_monitor_module.radio_diagnostics_id == 0) {
            //RADIO_STATS_INTERVAL - 30 seconds
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.radio_diagnostics_id, radio_diagnostics, NULL,
                    RADIO_STATS_INTERVAL_MS, 0);
        }
        if (g_monitor_module.chutil_id == 0) {
            //get_chan_util_upload_period - configurable on PSM
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.chutil_id, upload_radio_chan_util_telemetry,
                    NULL, get_chan_util_upload_period()*SEC_TO_MILLISEC, 0);
        }

#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
        for (rad_index = 0; rad_index < total_radios; rad_index++)
        {
            if (g_monitor_module.off_channel_scan_id[rad_index] == 0) {
                if (is_radio_band_5G(mgr->radio_config[rad_index].oper.band)) {
                    scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.off_channel_scan_id[rad_index], off_chan_scan_init,
                            &g_monitor_module.off_channel_cfg[rad_index].radio_index, (((int)g_monitor_module.off_channel_cfg[rad_index].NscanSec + g_monitor_module.off_channel_cfg[rad_index].TidleSec)*SEC_TO_MILLISEC), 0);
                }
            }
        }
#endif //FEATURE_OFF_CHANNEL_SCAN_5G

        //upload_period - configurable from file
        if (g_monitor_module.upload_period != 0) {
            if (g_monitor_module.client_telemetry_id == 0) {
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.client_telemetry_id,
                        upload_client_telemetry_data, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
            }
            if (g_monitor_module.client_debug_id == 0) {
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.client_debug_id,
                        upload_client_debug_stats, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
            }
            if (g_monitor_module.channel_width_telemetry_id == 0) {
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.channel_width_telemetry_id,
                        upload_channel_width_telemetry, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
            }
            if (g_monitor_module.ap_telemetry_id == 0)
                scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.ap_telemetry_id,
                        upload_ap_telemetry_data, NULL, (g_monitor_module.upload_period * MIN_TO_MILLISEC), 0);
        }

        if (g_monitor_module.radio_health_telemetry_logger_id == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.radio_health_telemetry_logger_id, radio_health_telemetry_logger, NULL,
                    RADIO_HEALTH_TELEMETRY_INTERVAL_MS, 0);
        }
        if (g_monitor_module.upload_ap_telemetry_pmf_id == 0) {
            //24h
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.upload_ap_telemetry_pmf_id, upload_ap_telemetry_pmf, NULL,
                    UPLOAD_AP_TELEMETRY_INTERVAL_MS, 0);
        }
        if (g_monitor_module.neighbor_scan_id == 0) {
            scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.neighbor_scan_id, process_periodical_neighbor_scan, NULL,
                    NEIGHBOR_SCAN_INTERVAL, 0);
        }
        
    } else {
        if (g_monitor_module.refresh_task_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.refresh_task_id);
            g_monitor_module.refresh_task_id = 0;
        }
        if (g_monitor_module.associated_devices_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.associated_devices_id);
            g_monitor_module.associated_devices_id = 0;
        }
        if (g_monitor_module.vap_status_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.vap_status_id);
            g_monitor_module.vap_status_id = 0;
        }
        if (g_monitor_module.radio_diagnostics_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.radio_diagnostics_id);
            g_monitor_module.radio_diagnostics_id = 0;
        }
        if (g_monitor_module.chutil_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.chutil_id);
            g_monitor_module.chutil_id = 0;
        }
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
        for (rad_index = 0; rad_index < total_radios; rad_index++)
        {
            if (g_monitor_module.off_channel_scan_id[rad_index] != 0) {
                if (is_radio_band_5G(mgr->radio_config[rad_index].oper.band)) {
                    scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.off_channel_scan_id[rad_index]);
                    g_monitor_module.off_channel_scan_id[rad_index] = 0;
                }
            }
        }
#endif //FEATURE_OFF_CHANNEL_SCAN_5G
        if (g_monitor_module.client_telemetry_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.client_telemetry_id);
            g_monitor_module.client_telemetry_id = 0;
        }
        if (g_monitor_module.client_debug_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.client_debug_id);
            g_monitor_module.client_debug_id = 0;
        }
        if (g_monitor_module.channel_width_telemetry_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.channel_width_telemetry_id);
            g_monitor_module.channel_width_telemetry_id = 0;
        }
        if (g_monitor_module.ap_telemetry_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.ap_telemetry_id);
            g_monitor_module.ap_telemetry_id = 0;
        }
        if (g_monitor_module.radio_health_telemetry_logger_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.radio_health_telemetry_logger_id);
            g_monitor_module.radio_health_telemetry_logger_id = 0;
        }
        if (g_monitor_module.upload_ap_telemetry_pmf_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.upload_ap_telemetry_pmf_id);
            g_monitor_module.upload_ap_telemetry_pmf_id = 0;
        }
        if (g_monitor_module.neighbor_scan_id != 0) {
            scheduler_cancel_timer_task(g_monitor_module.sched, g_monitor_module.neighbor_scan_id);
            g_monitor_module.neighbor_scan_id= 0;
        }
    }
}
#endif // CCSP_COMMON

void update_ecomode_radios()
{
    unsigned int radio;
    wifi_mgr_t *mgr = get_wifimgr_obj();

    for (radio = 0; radio < getNumberRadios(); radio++)
    {
        g_monitor_module.radio_presence[radio] = mgr->hal_cap.wifi_prop.radio_presence[radio];
    }
}

int init_wifi_monitor()
{
    unsigned int i = 0;
    unsigned int rad_ind = 0;
    wifi_mgr_t *mgr = get_wifimgr_obj();
    unsigned int total_radios = getNumberRadios();
#ifdef CCSP_COMMON
    unsigned int uptimeval = 0;
    int rssi;
    UINT vap_index, radio;
#endif // CCSP_COMMON

    update_ecomode_radios();
    for (rad_ind = 0; rad_ind < total_radios; rad_ind++)
    {
        g_monitor_module.off_channel_cfg[rad_ind].radio_index = rad_ind;
        if(!(is_radio_band_5G(mgr->radio_config[rad_ind].oper.band))) {
            if(SetOffChanParams(rad_ind,0,0,0) != RETURN_OK) {
                wifi_util_error_print(WIFI_MON,"%s:%d: Unable to set Offchannel Params\n", __func__, __LINE__);
            }
            continue;
        }
        if(SetOffChanParams(rad_ind,mgr->radio_config[rad_ind].feature.OffChanTscanInMsec,mgr->radio_config[rad_ind].feature.OffChanNscanInSec,mgr->radio_config[rad_ind].feature.OffChanTidleInSec) != RETURN_OK) {
            wifi_util_error_print(WIFI_MON,"%s:%d: Unable to set Offchannel Params\n", __func__, __LINE__);
        }
    }
#if CCSP_COMMON
    memset(g_monitor_module.cliStatsList, 0, MAX_VAP);
    g_monitor_module.poll_period = 5;
    g_monitor_module.upload_period = get_upload_period(60);//Default value 60
    uptimeval=get_sys_uptime();
    chan_util_upload_period = get_chan_util_upload_period();
    wifi_util_dbg_print(WIFI_MON, "%s:%d system uptime val is %ld and upload period is %d in secs\n",
             __FUNCTION__,__LINE__,uptimeval,(g_monitor_module.upload_period*60));
    /* If uptime is less than the upload period then we should calculate the current
      VAP iteration for measuring correct VAP UP percentatage. Becaues we should show
      the uptime value as VAP down percentatage.
      */
    if(uptimeval<(g_monitor_module.upload_period*60)) {
        vap_iteration=(int)uptimeval/60;
        g_monitor_module.current_poll_iter = vap_iteration;
        wifi_util_dbg_print(WIFI_MON, "%s:%d Current VAP UP check iteration  %d \n",__FUNCTION__,__LINE__,vap_iteration);
    } else {
        vap_iteration=0;
        g_monitor_module.current_poll_iter = 0;
        wifi_util_dbg_print(WIFI_MON, "%s:%d Upload period is already crossed \n",__FUNCTION__,__LINE__);
    }
    if (get_vap_dml_parameters(RSSI_THRESHOLD, &rssi) != ANSC_STATUS_SUCCESS) {
        g_monitor_module.sta_health_rssi_threshold = -65;
    } else {
        g_monitor_module.sta_health_rssi_threshold = rssi;
    }
    for (i = 0; i < getTotalNumberVAPs(); i++) {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
        radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        // update rapid reconnect time limit if changed
        wifi_front_haul_bss_t *vap_bss_info = Get_wifi_object_bss_parameter(vap_index);
        if(vap_bss_info != NULL) {
            g_monitor_module.bssid_data[i].ap_params.rapid_reconnect_threshold = vap_bss_info->rapidReconnThreshold;
            wifi_util_dbg_print(WIFI_MON, "%s:..rapidReconnThreshold:%d vapIndex:%d \n", __FUNCTION__, vap_bss_info->rapidReconnThreshold, i);
        } else {
            wifi_util_dbg_print(WIFI_MON, "%s: wrong vapIndex:%d \n", __FUNCTION__, i);
        }
    }
#endif // CCSP_COMMON

    gettimeofday(&g_monitor_module.last_signalled_time, NULL);
#ifdef CCSP_COMMON
    gettimeofday(&g_monitor_module.last_polled_time, NULL);
#endif // CCSP_COMMON
    pthread_cond_init(&g_monitor_module.cond, NULL);
    pthread_mutex_init(&g_monitor_module.queue_lock, NULL);
    pthread_mutex_init(&g_monitor_module.data_lock, NULL);

    for (i = 0; i < getTotalNumberVAPs(); i++) {
        g_monitor_module.bssid_data[i].sta_map = hash_map_create();
        if (g_monitor_module.bssid_data[i].sta_map == NULL) {
            deinit_wifi_monitor();
            wifi_util_error_print(WIFI_MON, "sta map create error\n");
            return -1;
        }
    }

    g_monitor_module.queue = queue_create();
    if (g_monitor_module.queue == NULL) {
        deinit_wifi_monitor();
        wifi_util_error_print(WIFI_MON, "monitor queue create error\n");
        return -1;
    }

    g_monitor_module.sched = scheduler_init();
    if (g_monitor_module.sched == NULL) {
        deinit_wifi_monitor();
        wifi_util_error_print(WIFI_MON, "monitor scheduler init error\n");
        return -1;
    }

#ifdef CCSP_COMMON
    g_monitor_module.dca_list = hash_map_create();
    if (g_monitor_module.dca_list == NULL) {
        deinit_wifi_monitor();
        wifi_util_error_print(WIFI_MON, "dca map create error\n");
        return -1;
    }
    g_monitor_module.chutil_id = 0;
    g_monitor_module.client_telemetry_id = 0;
    g_monitor_module.client_debug_id = 0;
    g_monitor_module.channel_width_telemetry_id = 0;
    g_monitor_module.ap_telemetry_id = 0;
    g_monitor_module.refresh_task_id = 0;
    g_monitor_module.associated_devices_id = 0;
    g_monitor_module.vap_status_id = 0;
#endif // CCSP_COMMON
    g_monitor_module.radio_diagnostics_id = 0;
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
    for (rad_ind = 0; rad_ind < total_radios; rad_ind++)
    {
        wifi_util_dbg_print(WIFI_MON,"%s:%d radio_index: %u, MAX_RADIOS: %d\n", __func__,__LINE__, rad_ind, total_radios);
        if(is_radio_band_5G(mgr->radio_config[rad_ind].oper.band)) {
            g_monitor_module.off_channel_scan_id[rad_ind] = 0;
        }
    }
#endif //FEATURE_OFF_CHANNEL_SCAN_5G
#ifdef CCSP_COMMON
    g_monitor_module.radio_health_telemetry_logger_id = 0;
    g_monitor_module.upload_ap_telemetry_pmf_id = 0;
    g_monitor_module.csi_sched_id = 0;
    g_monitor_module.csi_sched_interval = 0;
    g_monitor_module.neighbor_scan_id = 0;
    for (i = 0; i < getTotalNumberVAPs(); i++) {
        vap_index = VAP_INDEX(mgr->hal_cap, i);
        radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        g_monitor_module.clientdiag_id[i] = 0;
        g_monitor_module.clientdiag_sched_arg[i] = vap_index;
        g_monitor_module.clientdiag_sched_interval[i] = 0;
    }
#endif // CCSP_COMMON

#ifdef CCSP_COMMON
    scheduler_telemetry_tasks();
#else
    //RADIO_STATS_INTERVAL - 30 seconds
    scheduler_add_timer_task(g_monitor_module.sched, FALSE, &g_monitor_module.radio_diagnostics_id, radio_diagnostics, NULL,
        RADIO_STATS_INTERVAL_MS, 0);
#endif // CCSP_COMMON

#ifdef CCSP_COMMON
    memset(g_events_monitor.vap_ip, '\0', sizeof(g_events_monitor.vap_ip));
    pthread_mutex_init(&g_events_monitor.lock, NULL);

    g_events_monitor.csi_queue = queue_create();
    if (g_events_monitor.csi_queue == NULL) {
        deinit_wifi_monitor();
        wifi_util_error_print(WIFI_MON, "monitor csi queue create error\n");
        return -1;
    }
#endif // CCSP_COMMON

    g_monitor_module.exit_monitor = false;
    /* Initializing the lock for active measurement g_active_msmt.lock */
    pthread_mutex_init(&g_active_msmt.lock, NULL);

#if CCSP_COMMON
    wifi_hal_newApAssociatedDevice_callback_register(device_associated);
    wifi_vapstatus_callback_register(vapstatus_callback);
#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
    wifi_hal_apDeAuthEvent_callback_register(device_deauthenticated);
    wifi_hal_apDisassociatedDevice_callback_register(device_disassociated);
#endif
#if defined(FEATURE_CSI_CALLBACK)
    wifi_csi_callback_register(process_csi);
#endif
    scheduler_add_timer_task(g_monitor_module.sched, FALSE, NULL, refresh_assoc_frame_entry, NULL, (MAX_ASSOC_FRAME_REFRESH_PERIOD * 1000), 0);
#endif // CCSP_COMMON

    if (onewifi_pktgen_init() != RETURN_OK) {
        deinit_wifi_monitor();
        wifi_util_error_print(WIFI_MON, "%s:%d Pktgen support is missed!\n", __func__, __LINE__);
        return -1;
    }

    wifi_util_dbg_print(WIFI_MON, "%s:%d Wi-Fi monitor is initialized successfully\n", __func__, __LINE__);

    return 0;
}

int start_wifi_monitor ()
{
    unsigned int i;
    UINT vap_index, radio;
#ifdef CCSP_COMMON
    //ONEWIFI To avoid the st
        //Cleanup all CSI clients configured in driver
    unsigned char def_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif // CCSP_COMMON
    wifi_mgr_t *mgr = get_wifimgr_obj();

    onewifi_pktgen_uninit();

    for (i = 0; i < getTotalNumberVAPs(); i++) {
        /*TODO CID: 110946 Out-of-bounds access - Fix in QTN code*/
        vap_index = VAP_INDEX(mgr->hal_cap, i);
        radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        wifi_front_haul_bss_t *vap_bss_info = Get_wifi_object_bss_parameter(vap_index);
        if (vap_bss_info != NULL) {
            mac_addr_str_t mac_str;
            memcpy(g_monitor_module.bssid_data[i].bssid, vap_bss_info->bssid, sizeof(mac_address_t));
            wifi_util_dbg_print(WIFI_MON, "%s:%d vap_bss_info->bssid is %s for vap %d", __func__,__LINE__,to_mac_str(g_monitor_module.bssid_data[i].bssid, mac_str), vap_index);
        }

#ifdef CCSP_COMMON
        //ONEWIFI To avoid the segmentation Fault
        //Cleanup all CSI clients configured in driver
        wifi_enableCSIEngine(vap_index, def_mac, FALSE);
#endif // CCSP_COMMON
    }

    pthread_attr_t attr;
    pthread_attr_t *attrp = NULL;

    attrp = &attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
    if (pthread_create(&g_monitor_module.id, attrp, monitor_function, &g_monitor_module) != 0) {
        if(attrp != NULL)
            pthread_attr_destroy( attrp );
        deinit_wifi_monitor();
        wifi_util_error_print(WIFI_MON, "monitor thread create error\n");
        return -1;
    }

    wifi_util_dbg_print(WIFI_MON, "%s:%d Monitor thread is started successfully\n", __func__, __LINE__);

    if(attrp != NULL)
        pthread_attr_destroy( attrp );

#ifdef CCSP_COMMON
    g_monitor_module.sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "wifiMonitor", &g_monitor_module.sysevent_token);
    if (g_monitor_module.sysevent_fd < 0) {
        wifi_util_error_print(WIFI_MON, "%s:%d: Failed to open sysevent\n", __func__, __LINE__);
    } else {
        wifi_util_info_print(WIFI_MON, "%s:%d: Opened sysevent\n", __func__, __LINE__);
    }
    if (initparodusTask() == -1) {
        //wifi_util_dbg_print(WIFI_MON, "%s:%d: Failed to initialize paroduc task\n", __func__, __LINE__);

    }
#endif // CCSP_COMMON

    return 0;
}

void deinit_wifi_monitor()
{
    unsigned int i;
    sta_data_t *sta, *temp_sta;
    char key[64] = {0};

#ifdef CCSP_COMMON
    sysevent_close(g_monitor_module.sysevent_fd, g_monitor_module.sysevent_token);
#endif // CCSP_COMMON
    if(g_monitor_module.queue != NULL)
        queue_destroy(g_monitor_module.queue);

    scheduler_deinit(&(g_monitor_module.sched));
#ifdef CCSP_COMMON
    pthread_mutex_destroy(&g_events_monitor.lock);
    if(g_events_monitor.csi_queue != NULL) {
        queue_destroy(g_events_monitor.csi_queue);
    }
    if (g_monitor_module.dca_list != NULL) {
        hash_map_destroy(g_monitor_module.dca_list);
    }
#endif // CCSP_COMMON
    for (i = 0; i < getTotalNumberVAPs(); i++) {
        if(g_monitor_module.bssid_data[i].sta_map != NULL) {
            sta = hash_map_get_first(g_monitor_module.bssid_data[i].sta_map);
            while (sta != NULL) {
                memset(key, 0, sizeof(key));
                to_sta_key(sta->sta_mac, key);
                sta = hash_map_get_next(g_monitor_module.bssid_data[i].sta_map, sta);
                temp_sta = hash_map_remove(g_monitor_module.bssid_data[i].sta_map, key);
                if (temp_sta != NULL) {
                    free(temp_sta);
                }
            }
            hash_map_destroy(g_monitor_module.bssid_data[i].sta_map);
        }
    }
    pthread_mutex_destroy(&g_monitor_module.queue_lock);
    pthread_mutex_destroy(&g_monitor_module.data_lock);
    pthread_cond_destroy(&g_monitor_module.cond);

    /* destory the active measurement g_active_msmt.lock */
    pthread_mutex_destroy(&g_active_msmt.lock);
}

#ifdef CCSP_COMMON
unsigned int get_poll_period 	()
{
    return g_monitor_module.poll_period;
}

unsigned int GetINSTOverrideTTL()
{
    return g_monitor_module.instantDefOverrideTTL;
}

void SetINSTOverrideTTL(int defTTL)
{
    g_monitor_module.instantDefOverrideTTL = defTTL;
}

unsigned int GetINSTDefReportingPeriod()
{
    return g_monitor_module.instantDefReportPeriod;
}

void SetINSTDefReportingPeriod(int defPeriod)
{
    g_monitor_module.instantDefReportPeriod = defPeriod;
}

void SetINSTReportingPeriod(unsigned long pollPeriod)
{
    g_monitor_module.instantPollPeriod = pollPeriod;
}

unsigned int GetINSTPollingPeriod()
{
    return g_monitor_module.instantPollPeriod;
}

void SetINSTMacAddress(char *mac_addr)
{
    strncpy(g_monitor_module.instantMac, mac_addr, MIN_MAC_LEN);
}

char* GetInstAssocDevSchemaIdBuffer()
{
    return instSchemaIdBuffer;
}

int GetInstAssocDevSchemaIdBufferSize()
{
    int len = 0;
    if(instSchemaIdBuffer) {
        len = strlen(instSchemaIdBuffer);
    }

    return len;
}

void instant_msmt_reporting_period(int pollPeriod)
{
    int timeSpent = 0;
    int timeLeft = 0;

    wifi_util_dbg_print(WIFI_MON, "%s:%d: reporting period changed\n", __func__, __LINE__);
    pthread_mutex_lock(&g_monitor_module.queue_lock);

    if(pollPeriod == 0){
        g_monitor_module.maxCount = 0;
        g_monitor_module.count = 0;
    }else{
        timeSpent = g_monitor_module.count * g_monitor_module.instantPollPeriod ;
        timeLeft = g_monitor_module.instantDefOverrideTTL - timeSpent;
        g_monitor_module.maxCount = timeLeft/pollPeriod;
        g_monitor_module.poll_period = pollPeriod;

        if(g_monitor_module.count > g_monitor_module.maxCount)
            g_monitor_module.count = 0;
    }
    g_monitor_module.instantPollPeriod = pollPeriod;
    if(g_monitor_module.instntMsmtenable == true) {
        pthread_cond_signal(&g_monitor_module.cond);
    }
    if (g_monitor_module.inst_msmt_id != 0) {
        scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.inst_msmt_id,
                (g_monitor_module.instantPollPeriod*1000));
    }
    pthread_mutex_unlock(&g_monitor_module.queue_lock);
}

void instant_msmt_def_period(int defPeriod)
{
    int curCount = 0;
    int newCount = 0;

    wifi_util_dbg_print(WIFI_MON, "%s:%d: def period changed\n", __func__, __LINE__);
    g_monitor_module.instantDefReportPeriod = defPeriod;

    if(g_monitor_module.instntMsmtenable == false) {
        pthread_mutex_lock(&g_monitor_module.queue_lock);

        curCount = g_monitor_module.count;
        newCount = g_monitor_module.instantDefReportPeriod / DEFAULT_INSTANT_POLL_TIME;

        if(newCount > curCount){
            g_monitor_module.maxCount = newCount - curCount;
            g_monitor_module.count = 0;
        }else{
            wifi_util_dbg_print(WIFI_MON, "%s:%d:created max non instant report, stop polling now\n", __func__, __LINE__);
            g_monitor_module.maxCount = 0;
        }
        pthread_cond_signal(&g_monitor_module.cond);
        pthread_mutex_unlock(&g_monitor_module.queue_lock);
    }
}

void instant_msmt_ttl(int overrideTTL)
{
    int curCount = 0;
    int newCount = 0;

    wifi_util_dbg_print(WIFI_MON, "%s:%d: TTL changed\n", __func__, __LINE__);
    g_monitor_module.instantDefOverrideTTL = overrideTTL;

    if(g_monitor_module.instantPollPeriod == 0)
        return;

    pthread_mutex_lock(&g_monitor_module.queue_lock);

    if(overrideTTL == 0){
        g_monitor_module.maxCount = 0;
        g_monitor_module.count = 0;
    } else {
        curCount = g_monitor_module.count;
        newCount = g_monitor_module.instantDefOverrideTTL/g_monitor_module.instantPollPeriod;
        if(newCount > curCount){
            g_monitor_module.maxCount = newCount - curCount;
            g_monitor_module.count = 0;
        }else{
            wifi_util_dbg_print(WIFI_MON, "%s:%d:already created maxCount report, stop polling now\n", __func__, __LINE__);
            g_monitor_module.maxCount = 0;
        }
    }
    if(g_monitor_module.instntMsmtenable == true) {
        pthread_cond_signal(&g_monitor_module.cond);
    }
    pthread_mutex_unlock(&g_monitor_module.queue_lock);
}

void instant_msmt_macAddr(char *mac_addr)
{
    mac_address_t bmac;
    int i;
    wifi_mgr_t *mgr = get_wifimgr_obj();

    wifi_util_dbg_print(WIFI_MON, "%s:%d: get new client %s stats\n", __func__, __LINE__, mac_addr);
    strncpy(g_monitor_module.instantMac, mac_addr, MIN_MAC_LEN);

    str_to_mac_bytes(mac_addr, bmac);
    for (i = 0; i < (int)getTotalNumberVAPs(); i++) {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
        UINT radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        if( is_device_associated(vap_index, mac_addr)  == true) {
            wifi_util_dbg_print(WIFI_MON, "%s:%d: found client %s on ap %d\n", __func__, __LINE__, mac_addr, vap_index);
            pthread_mutex_lock(&g_monitor_module.queue_lock);
            g_monitor_module.inst_msmt.ap_index = vap_index;
            memcpy(g_monitor_module.inst_msmt.sta_mac, bmac, sizeof(mac_address_t));

            pthread_cond_signal(&g_monitor_module.cond);
            pthread_mutex_unlock(&g_monitor_module.queue_lock);

            break;
        }
    }
}

void monitor_enable_instant_msmt(mac_address_t sta_mac, bool enable)
{
    mac_addr_str_t sta;
    unsigned int i;
    wifi_mgr_t *mgr = get_wifimgr_obj();
    wifi_monitor_data_t data;

    to_sta_key(sta_mac, sta);
    wifi_util_dbg_print(WIFI_MON, "%s:%d: instant measurements %s for sta:%s\n", __func__, __LINE__, (enable == true)?"start":"stop", sta);

    g_monitor_module.instntMsmtenable = enable;
    pthread_mutex_lock(&g_monitor_module.queue_lock);

    if (g_monitor_module.inst_msmt.active == true) {
        if (enable == false) {
            if (memcmp(g_monitor_module.inst_msmt.sta_mac, sta_mac, sizeof(mac_address_t)) == 0) {
                wifi_util_dbg_print(WIFI_MON, "%s:%d: instant measurements active for sta:%s, should stop\n", __func__, __LINE__, sta);
                g_monitor_module.instantDefOverrideTTL = DEFAULT_INSTANT_REPORT_TIME;

                memset(&data, 0, sizeof(wifi_monitor_data_t));
                memcpy(data.u.imsmt.sta_mac, sta_mac, sizeof(mac_address_t));

                data.u.imsmt.ap_index = g_monitor_module.inst_msmt.ap_index;
                data.ap_index = g_monitor_module.inst_msmt.ap_index;
                pthread_mutex_unlock(&g_monitor_module.queue_lock);

                push_event_to_monitor_queue(&data, wifi_event_monitor_stop_inst_msmt, NULL);

            }

        } else {
            // must return
            pthread_mutex_unlock(&g_monitor_module.queue_lock);
            wifi_util_dbg_print(WIFI_MON, "%s:%d: instant measurements active for sta:%s, should just return\n", __func__, __LINE__, sta);
        }


        return;

    }

    wifi_util_dbg_print(WIFI_MON, "%s:%d: instant measurements not active should look for sta:%s\n", __func__, __LINE__, sta);

    for (i = 0; i < getTotalNumberVAPs(); i++) {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
        UINT radio = RADIO_INDEX(mgr->hal_cap, i);
        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }
        if ( is_device_associated(vap_index, sta) == true ) {
            wifi_util_dbg_print(WIFI_MON, "%s:%d: found sta:%s on ap index:%d starting instant measurements\n", __func__, __LINE__, sta, vap_index);
            memset(&data, 0, sizeof(wifi_monitor_data_t));

            memcpy(data.u.imsmt.sta_mac, sta_mac, sizeof(mac_address_t));

            data.u.imsmt.ap_index = vap_index;
            data.ap_index = vap_index;

            pthread_mutex_unlock(&g_monitor_module.queue_lock);
            push_event_to_monitor_queue(&data, wifi_event_monitor_stats_flag_change, NULL);

            return;
        }
    }

    pthread_mutex_unlock(&g_monitor_module.queue_lock);
}

bool monitor_is_instant_msmt_enabled()
{
    return g_monitor_module.instntMsmtenable;
}
#endif // CCSP_COMMON 


/* Active Measurement GET Calls */

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : monitor_is_active_msmt_enabled                                */
/*                                                                               */
/* DESCRIPTION   : This function returns the status of the Active Measurement    */
/*                                                                               */
/* INPUT         : NONE                                                          */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : TRUE / FALSE                                                  */
/*                                                                               */
/*********************************************************************************/

bool monitor_is_active_msmt_enabled()
{
    return g_active_msmt.active_msmt.ActiveMsmtEnable;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : GetActiveMsmtPktSize                                          */
/*                                                                               */
/* DESCRIPTION   : This function returns the size of the packet configured       */
/*                 for Active Measurement                                        */
/*                                                                               */
/* INPUT         : NONE                                                          */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : size of the packet                                            */
/*                                                                               */
/*********************************************************************************/

unsigned int GetActiveMsmtPktSize()
{
    return g_active_msmt.active_msmt.ActiveMsmtPktSize;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : GetActiveMsmtSampleDuration                                   */
/*                                                                               */
/* DESCRIPTION   : This function returns the duration between the samples        */
/*                 configured for Active Measurement                             */
/*                                                                               */
/* INPUT         : NONE                                                          */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : duration between samples                                      */
/*                                                                               */
/*********************************************************************************/

unsigned int GetActiveMsmtSampleDuration()
{
    return g_active_msmt.active_msmt.ActiveMsmtSampleDuration;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : GetActiveMsmtNumberOfSamples                                  */
/*                                                                               */
/* DESCRIPTION   : This function returns the count of samples configured         */
/*                 for Active Measurement                                        */
/*                                                                               */
/* INPUT         : NONE                                                          */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : Sample count                                                  */
/*                                                                               */
/*********************************************************************************/

unsigned int GetActiveMsmtNumberOfSamples()
{
    return g_active_msmt.active_msmt.ActiveMsmtNumberOfSamples;
}
/* Active Measurement Step & Plan GET calls */

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : GetActiveMsmtStepID                                           */
/*                                                                               */
/* DESCRIPTION   : This function returns the Step Identifier configured          */
/*                 for Active Measurement                                        */
/*                                                                               */
/* INPUT         : NONE                                                          */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : Step Identifier                                               */
/*                                                                               */
/*********************************************************************************/
unsigned int GetActiveMsmtStepID()
{
    return g_active_msmt.curStepData.StepId;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : GetActiveMsmtPlanID                                           */
/*                                                                               */
/* DESCRIPTION   : This function returns the Plan Id configured for              */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : pPlanId                                                       */
/*                                                                               */
/* OUTPUT        : Plan ID                                                       */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/
void GetActiveMsmtPlanID(unsigned int *pPlanID)
{
    if (pPlanID != NULL) {
        memcpy(pPlanID, g_active_msmt.active_msmt.PlanId, strlen((char *)g_active_msmt.active_msmt.PlanId));
    }
    return;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : GetActiveMsmtStepSrcMac                                       */
/*                                                                               */
/* DESCRIPTION   : This function returns the Step Source Mac configured for      */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : pStepSrcMac                                                   */
/*                                                                               */
/* OUTPUT        : Step Source Mac                                               */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/
void GetActiveMsmtStepSrcMac(mac_address_t pStepSrcMac)
{
    if (pStepSrcMac != NULL) {
        memcpy(pStepSrcMac, g_active_msmt.curStepData.SrcMac, sizeof(mac_address_t));
    }
    return;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : GetActiveMsmtStepDestMac                                      */
/*                                                                               */
/* DESCRIPTION   : This function returns the Step Destination Mac configured for */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : pStepDstMac                                                   */
/*                                                                               */
/* OUTPUT        : Step Destination Mac                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/
void GetActiveMsmtStepDestMac(mac_address_t pStepDstMac)
{
    if (pStepDstMac != NULL) {
        memcpy(pStepDstMac, g_active_msmt.curStepData.DestMac, sizeof(mac_address_t));
    }
    return;
}


/* Active Measurement SET Calls */

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtEnable                                           */
/*                                                                               */
/* DESCRIPTION   : This function set the status of Active Measurement            */
/*                                                                               */
/* INPUT         : enable - flag to enable/ disable Active Measurement           */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtEnable(bool enable)
{
    active_msmt_log_message("%s:%d: changing the Active Measurement Flag to %s\n", __func__, __LINE__,(enable ? "true" : "false"));
    g_active_msmt.active_msmt.ActiveMsmtEnable = enable;
}
/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtPktSize                                          */
/*                                                                               */
/* DESCRIPTION   : This function set the size of packet configured for           */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : PktSize - size of packet                                      */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtPktSize(unsigned int PktSize)
{
    wifi_util_dbg_print(WIFI_MON, "%s:%d: Active Measurement Packet Size Changed to %d \n", __func__, __LINE__,PktSize);
    g_active_msmt.active_msmt.ActiveMsmtPktSize = PktSize;
}
/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtSampleDuration                                   */
/*                                                                               */
/* DESCRIPTION   : This function set the sample duration configured for          */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : Duration - duration between samples                           */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtSampleDuration(unsigned int Duration)
{
    wifi_util_dbg_print(WIFI_MON, "%s:%d: Active Measurement Sample Duration Changed to %d \n", __func__, __LINE__,Duration);
    g_active_msmt.active_msmt.ActiveMsmtSampleDuration = Duration;
}
/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtNumberOfSamples                                  */
/*                                                                               */
/* DESCRIPTION   : This function set the count of sample configured for          */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : NoOfSamples - count of samples                                */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtNumberOfSamples(unsigned int NoOfSamples)
{
    wifi_util_dbg_print(WIFI_MON, "%s:%d: Active Measurement Number of Samples Changed %d \n", __func__, __LINE__,NoOfSamples);
    g_active_msmt.active_msmt.ActiveMsmtNumberOfSamples = NoOfSamples;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtPlanID                                           */
/*                                                                               */
/* DESCRIPTION   : This function set the Plan Identifier configured for          */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : pPlanID - Plan Idetifier                                      */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtPlanID(char *pPlanID)
{
    if (pPlanID == NULL) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d pPlanID is NULL\n", __func__, __LINE__);
        return;
    }

    unsigned int planid_len = 0;
    planid_len = strlen(pPlanID);
    if (planid_len > PLAN_ID_LENGTH) { 
        wifi_util_error_print(WIFI_MON, "%s:%d Plan ID is not in range\n", __func__, __LINE__);
        return;
    }

    memset((char *)g_active_msmt.active_msmt.PlanId, '\0', PLAN_ID_LENGTH);
    strncpy((char *)g_active_msmt.active_msmt.PlanId, pPlanID,planid_len);
    g_active_msmt.active_msmt.PlanId[strlen((char *)g_active_msmt.active_msmt.PlanId)] = '\0';

    wifi_util_dbg_print(WIFI_MON, "%s:%d Plan id updated as %s\n", __func__, __LINE__, (char *)g_active_msmt.active_msmt.PlanId);
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtStepID                                           */
/*                                                                               */
/* DESCRIPTION   : This function set the Step Identifier configured for          */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : StepId - Step Identifier                                      */
/*                 StepIns - Step Instance                                       */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtStepID(unsigned int StepId, ULONG StepIns)
{
    wifi_util_dbg_print(WIFI_MON, "%s:%d: Active Measurement Step Id Changed to %d for ins : %d\n", __func__, __LINE__,StepId,StepIns);
    g_active_msmt.active_msmt.Step[StepIns].StepId = StepId;
}


/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetBlasterMqttTopic                                           */
/*                                                                               */
/* DESCRIPTION   : This function set the MQTT topic configured for               */
/*                 Blaster                                                       */
/*                                                                               */
/* INPUT         : BlasterMqttTopic - MQTT Topic for Blaster                     */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetBlasterMqttTopic(char *mqtt_topic)
{
    if (mqtt_topic == NULL) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d MQTT Topic is NULL\n", __func__, __LINE__);
        return;
    }
    unsigned int mqtt_len = 0;
    mqtt_len = strlen(mqtt_topic);
    if (mqtt_len > MAX_MQTT_TOPIC_LEN) {
        wifi_util_error_print(WIFI_MON, "%s:%d MQTT Topic length is not in range\n", __func__, __LINE__);
        return;
    }
    memset(g_active_msmt.active_msmt.blaster_mqtt_topic, '\0', MAX_MQTT_TOPIC_LEN);

    strncpy((char *)g_active_msmt.active_msmt.blaster_mqtt_topic, mqtt_topic, mqtt_len);
    g_active_msmt.active_msmt.blaster_mqtt_topic[strlen((char *)g_active_msmt.active_msmt.blaster_mqtt_topic)] = '\0';
    wifi_util_dbg_print(WIFI_MON, "%s:%d: Active Measurement topic changed %s\n", __func__, __LINE__, g_active_msmt.active_msmt.blaster_mqtt_topic);
}



/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtStepSrcMac                                       */
/*                                                                               */
/* DESCRIPTION   : This function set the Step Source Mac configured for          */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : SrcMac - Step Source Mac                                      */
/*                 StepIns - Step Instance                                       */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtStepSrcMac(char *SrcMac, ULONG StepIns)
{
    mac_address_t bmac;
    wifi_util_dbg_print(WIFI_MON, "%s:%d: Active Measurement Step Src Mac changed to %s for ins : %d\n", __func__, __LINE__,SrcMac,StepIns);
    str_to_mac_bytes(SrcMac, bmac);
    memset(g_active_msmt.active_msmt.Step[StepIns].SrcMac, 0, sizeof(mac_address_t));
    memcpy(g_active_msmt.active_msmt.Step[StepIns].SrcMac, bmac, sizeof(mac_address_t));
}

int ActiveMsmtConfValidation(active_msmt_t *cfg)
{
    int len;
    char msg[256] = {};

    if (!cfg->PlanId || ((len = strlen((char *)cfg->PlanId) < 1) || len > PLAN_ID_LENGTH - 2)) {
        snprintf(msg, sizeof(msg), "Invalid length of PlanID [%d]. Expected in range [1..%d]",
            len, PLAN_ID_LENGTH - 2);
        goto Error;
    }

    if ((cfg->ActiveMsmtNumberOfSamples < 1) || (cfg->ActiveMsmtNumberOfSamples > 100)) {
        snprintf(msg, sizeof(msg), "Invalid Sample count [%d]. Expected in range [1..100]",
            cfg->ActiveMsmtNumberOfSamples);
        goto Error;
    }

    if ((cfg->ActiveMsmtSampleDuration < 1000) || (cfg->ActiveMsmtSampleDuration > 10000)) {
        snprintf(msg, sizeof(msg), "Invalid Duration [%d]ms. Expected in range [1000..10000]",
            cfg->ActiveMsmtSampleDuration);
        goto Error;
    }

    if ((cfg->ActiveMsmtSampleDuration / cfg->ActiveMsmtNumberOfSamples) < 100) {
        snprintf(msg, sizeof(msg), "Invalid Duration/Sample_count ratio [%d/%d = %d]ms. Expected >= 100",
            cfg->ActiveMsmtSampleDuration, cfg->ActiveMsmtNumberOfSamples,
            cfg->ActiveMsmtSampleDuration / cfg->ActiveMsmtNumberOfSamples);
        goto Error;
    }

    if (cfg->ActiveMsmtPktSize < 64 || cfg->ActiveMsmtPktSize > 1470) {
        snprintf(msg, sizeof(msg), "Invalid Packet size [%d]bytes. Expected in range [64..1470]",
            cfg->ActiveMsmtPktSize);
        goto Error;
    }

    for (unsigned int i = 0; i < MAX_STEP_COUNT; i++) {
        len = strlen((char *) cfg->Step[i].DestMac);

        /* MAC could be 0 or XXXXXXXXXXXX */
        if (len != 0 && len != MAC_ADDRESS_LENGTH - 1) {
            snprintf(msg, sizeof(msg), "Invalid MAC address [%s]", cfg->Step[i].DestMac);
            active_msmt_report_error(__func__, cfg->PlanId, &cfg->Step[i], msg, ACTIVE_MSMT_STATUS_WRONG_ARG);
            active_msmt_set_step_status(__func__, i, ACTIVE_MSMT_STEP_INVALID);
            continue;
        }

        if (cfg->Step[i].StepId < 0 || cfg->Step[i].StepId > INT_MAX) {
            snprintf(msg, sizeof(msg), "Invalid StepID [%d]. Expected in range [0..INT_MAX]", cfg->Step[i].StepId);
            active_msmt_report_error(__func__, cfg->PlanId, &cfg->Step[i], msg, ACTIVE_MSMT_STATUS_WRONG_ARG);
            active_msmt_set_step_status(__func__, i, ACTIVE_MSMT_STEP_INVALID);
            continue;
        }
    }

    SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_SUCCEED);

    return RETURN_OK;

Error:
    active_msmt_report_all_steps(cfg, msg, ACTIVE_MSMT_STATUS_WRONG_ARG);

    return RETURN_ERR;
}

void SetActiveMsmtStatus(const char *func, active_msmt_status_t status)
{
    wifi_util_dbg_print(WIFI_MON, "%s: active msmt status changed %s -> %s\n", func,
        active_msmt_status_to_str(g_active_msmt.status), active_msmt_status_to_str(status));
    g_active_msmt.status = status;
}

void ResetActiveMsmtStepInstances(void)
{
    active_msmt_t *cfg = &g_active_msmt.active_msmt;

    wifi_util_dbg_print(WIFI_MON, "%s:%d Reseting active msmt step instances\n", __func__, __LINE__);

    for (int StepCount = 0; StepCount < MAX_STEP_COUNT; StepCount++) {
        cfg->StepInstance[StepCount] = ACTIVE_MSMT_STEP_DONE;
        memset(&cfg->Step[StepCount], 0, sizeof(active_msmt_step_t));
    }
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : SetActiveMsmtStepDstMac                                       */
/*                                                                               */
/* DESCRIPTION   : This function set the Step Destination Mac configured for     */
/*                 Active Measurement                                            */
/*                                                                               */
/* INPUT         : DstMac - Step Destination Mac                                 */
/*                 StepIns - Step Instance                                       */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void SetActiveMsmtStepDstMac(char *DstMac, ULONG StepIns)
{
    mac_address_t bmac;
    int i;
    wifi_mgr_t *mgr = get_wifimgr_obj();
    active_msmt_t *cfg = &g_active_msmt.active_msmt;
    wifi_ctrl_t *ctrl = &mgr->ctrl;

    if (cfg->StepInstance[StepIns] == ACTIVE_MSMT_STEP_INVALID) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d Active Measurement Step: %d is invalid. Skipping...\n",
            __func__, __LINE__, StepIns);
        return;
    }

    wifi_util_dbg_print(WIFI_MON, "%s:%d: Active Measurement Step Destination Mac changed to %s for step ins : %d\n", __func__, __LINE__,DstMac,StepIns);

    str_to_mac_bytes(DstMac, bmac);
    cfg->Step[StepIns].ApIndex = -1;

    memset(cfg->Step[StepIns].DestMac, 0, sizeof(mac_address_t));
    memcpy(cfg->Step[StepIns].DestMac, bmac, sizeof(mac_address_t));

    active_msmt_set_step_status(__func__, StepIns, ACTIVE_MSMT_STEP_PENDING);

    for (i = 0; i < (int)getTotalNumberVAPs(); i++) {
        UINT vap_index = VAP_INDEX(mgr->hal_cap, i);
        UINT radio = RADIO_INDEX(mgr->hal_cap, i);

        if (g_monitor_module.radio_presence[radio] == false) {
            continue;
        }

        if (is_device_associated(vap_index, DstMac)  == true) {
            wifi_util_dbg_print(WIFI_MON, "%s:%d: found client %s on ap %d\n", __func__, __LINE__, DstMac,vap_index);
            cfg->Step[StepIns].ApIndex = vap_index;
            return;
        }
    }

    wifi_util_dbg_print(WIFI_MON, "%s:%d: client %s not found \n", __func__, __LINE__, DstMac);

    if (ctrl->network_mode == rdk_dev_mode_type_ext) {
        char msg[256] = {};

        snprintf(msg, sizeof(msg), "Failed to find MAC in STA associated list");
        active_msmt_report_error(__func__, cfg->PlanId, &cfg->Step[StepIns], msg, ACTIVE_MSMT_STATUS_NO_CLIENT);

        /* Set status as succeed back to be able to procceed other Steps */
        SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_SUCCEED);
    } else {

        g_active_msmt.curStepData.ApIndex = -1;
        g_active_msmt.curStepData.StepId = cfg->Step[StepIns].StepId;
        memcpy(g_active_msmt.curStepData.DestMac, cfg->Step[StepIns].DestMac, sizeof(mac_address_t));

        process_active_msmt_diagnostics(cfg->Step[StepIns].ApIndex);
        stream_client_msmt_data(true);
    }

    active_msmt_log_message( "%s:%d no need to start pktgen for offline client: %s\n" ,__FUNCTION__, __LINE__, DstMac);
    active_msmt_set_step_status(__func__, StepIns, ACTIVE_MSMT_STEP_INVALID);
}

/**************************************************************************************************************************/
/*                                                                                                                        */
/* FUNCTION NAME : SetOffChanTscan                                                                                       */
/*                                                                                                                        */
/* DESCRIPTION   : This function sets Tscan param of Off Channel Scan                                                     */
/*                                                                                                                        */
/* INPUT         : R_Index - Radio Index                                                                                  */
/*                 Tscan - Time that a single channel is scanned (msec)                                                   */
/*                                                                                                                        */
/*                                                                                                                        */
/* OUTPUT        : NONE                                                                                                   */
/*                                                                                                                        */
/* RETURN VALUE  : Whether set is success                                                                                 */
/*                                                                                                                        */
/**************************************************************************************************************************/
int SetOffChanTscan(unsigned int R_Index, ULONG Tscan)
{
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
    if (R_Index  < getNumberRadios()) {
        if (g_monitor_module.off_channel_cfg[R_Index].TscanMsec == Tscan) {
            return RETURN_OK;
        }
        wifi_util_dbg_print(WIFI_MON,"%s:%d RADIO_INDEX:%u New value: %lu\n",__func__,__LINE__,R_Index,Tscan);
        memset(&g_monitor_module.off_channel_cfg[R_Index].TscanMsec, 0, sizeof(g_monitor_module.off_channel_cfg[R_Index].TscanMsec));
        pthread_mutex_lock(&g_monitor_module.queue_lock);
        g_monitor_module.off_channel_cfg[R_Index].TscanMsec = Tscan;
        wifi_util_dbg_print(WIFI_MON,"%s:%d Off_channel_scan radio:%u Changed value of Tscan:%lu\n",__func__,__LINE__,R_Index, g_monitor_module.off_channel_cfg[R_Index].TscanMsec);
        pthread_mutex_unlock(&g_monitor_module.queue_lock);
        return RETURN_OK;
    }
    wifi_util_error_print(WIFI_MON,"%s:%d: Invalid radio index\n", __func__, __LINE__);
    return RETURN_ERR;
#else //FEATURE_OFF_CHANNEL_SCAN_5G
    return RETURN_OK; //making stub call when distro not defined
#endif //FEATURE_OFF_CHANNEL_SCAN_5G
}

/**************************************************************************************************************************/
/*                                                                                                                        */
/* FUNCTION NAME : SetOffChanNscan                                                                                       */
/*                                                                                                                        */
/* DESCRIPTION   : This function sets Nscan param of Off Channel Scan                                                     */
/*                                                                                                                        */
/* INPUT         : R_Index - Radio Index                                                                                  */
/*                 Nscan - number of times a single channel must be scanned within a day, converted to seconds and stored */
/*                                                                                                                        */
/*                                                                                                                        */
/* OUTPUT        : NONE                                                                                                   */
/*                                                                                                                        */
/* RETURN VALUE  : Whether set is success                                                                                 */
/*                                                                                                                        */
/**************************************************************************************************************************/
int SetOffChanNscan(unsigned int R_Index, ULONG Nscan)
{
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
    if (R_Index  < getNumberRadios()) {
        if (g_monitor_module.off_channel_cfg[R_Index].NscanSec == Nscan) {
            return RETURN_OK;
        }
        wifi_util_dbg_print(WIFI_MON,"%s:%d RADIO_INDEX:%u New value: %lu\n",__func__,__LINE__,R_Index,Nscan);
        memset(&g_monitor_module.off_channel_cfg[R_Index].NscanSec, 0, sizeof(g_monitor_module.off_channel_cfg[R_Index].NscanSec));
        pthread_mutex_lock(&g_monitor_module.queue_lock);
        g_monitor_module.off_channel_cfg[R_Index].NscanSec = Nscan;
        wifi_util_dbg_print(WIFI_MON,"%s:%d Off_channel_scan radio:%u Changed value of Nscan:%lu\n",__func__,__LINE__,R_Index, g_monitor_module.off_channel_cfg[R_Index].NscanSec);
        pthread_mutex_unlock(&g_monitor_module.queue_lock);
        return RETURN_OK;
    }
    wifi_util_error_print(WIFI_MON,"%s:%d: Invalid radio index\n", __func__, __LINE__);
    return RETURN_ERR;
#else //FEATURE_OFF_CHANNEL_SCAN_5G
    return RETURN_OK; //making stub call when distro not defined
#endif //FEATURE_OFF_CHANNEL_SCAN_5G
}

/**************************************************************************************************************************/
/*                                                                                                                        */
/* FUNCTION NAME : SetOffChanTidle                                                                                        */
/*                                                                                                                        */
/* DESCRIPTION   : This function sets Tidle param of Off Channel Scan                                                     */
/*                                                                                                                        */
/* INPUT         : R_Index - Radio Index                                                                                  */
/*                 Tidle - time to account for network idleness (sec)                                                     */
/*                                                                                                                        */
/*                                                                                                                        */
/* OUTPUT        : NONE                                                                                                   */
/*                                                                                                                        */
/* RETURN VALUE  : Whether set is success                                                                                 */
/*                                                                                                                        */
/**************************************************************************************************************************/
int SetOffChanTidle(unsigned int R_Index, ULONG Tidle)
{
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
    if (R_Index  < getNumberRadios()) {
        if (g_monitor_module.off_channel_cfg[R_Index].TidleSec == Tidle) {
            return RETURN_OK;
        }
        wifi_util_dbg_print(WIFI_MON,"%s:%d RADIO_INDEX:%u New value: %lu\n",__func__,__LINE__,R_Index,Tidle);
        memset(&g_monitor_module.off_channel_cfg[R_Index].TidleSec, 0, sizeof(g_monitor_module.off_channel_cfg[R_Index].TidleSec));
        pthread_mutex_lock(&g_monitor_module.queue_lock);
        g_monitor_module.off_channel_cfg[R_Index].TidleSec = Tidle;
        wifi_util_dbg_print(WIFI_MON,"%s:%d Off_channel_scan radio:%u Changed value of Tidle:%lu\n",__func__,__LINE__,R_Index, g_monitor_module.off_channel_cfg[R_Index].TidleSec);
        pthread_mutex_unlock(&g_monitor_module.queue_lock);
        return RETURN_OK;
    }
    wifi_util_error_print(WIFI_MON,"%s:%d: Invalid radio index\n", __func__, __LINE__);
    return RETURN_ERR;
#else //FEATURE_OFF_CHANNEL_SCAN_5G
    return RETURN_OK; //making stub call when distro not defined
#endif //FEATURE_OFF_CHANNEL_SCAN_5G
}
/**************************************************************************************************************************/
/*                                                                                                                        */
/* FUNCTION NAME : SetOffChanParams                                                                                       */
/*                 Wrapper for setting params                                                                             */
/* DESCRIPTION   : This function sets Off Channel Scan Params                                                             */
/*                                                                                                                        */
/* INPUT         : R_Index - Radio Index                                                                                  */
/*                 Tscan - Time that a single channel is scanned (msec)                                                   */
/*                 Nscan - number of times a single channel must be scanned within a day, converted to seconds and stored */
/*                 Tidle - time to account for network idleness (sec)                                                     */
/* OUTPUT        : NONE                                                                                                   */
/*                                                                                                                        */
/* RETURN VALUE  : Whether set is success                                                                                 */
/*                                                                                                                        */
/**************************************************************************************************************************/
int SetOffChanParams(unsigned int R_Index, ULONG Tscan, ULONG Nscan, ULONG Tidle)
{
#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
    int ret = 0;
    ret |= SetOffChanTscan(R_Index, Tscan);
    ret |= SetOffChanNscan(R_Index, Nscan);
    ret |= SetOffChanTidle(R_Index, Tidle);
    if(ret != 0)
    {
        wifi_util_error_print(WIFI_MON,"%s:%d:Error in assignment for %u\n", __func__, __LINE__, R_Index);
        return RETURN_ERR;
    }
#endif //FEATURE_OFF_CHANNEL_SCAN_5G
    return RETURN_OK;
}

#if defined (FEATURE_OFF_CHANNEL_SCAN_5G)
/**************************************************************************************************************************/
/*                                                                                                                        */
/* FUNCTION NAME : off_chan_scan_init                                                                                     */
/*                                                                                                                        */
/* DESCRIPTION   : This function prints the required information for 5G off channel scan feature into WiFiLog.txt         */
/*                                                                                                                        */
/* INPUT         : radio index                                                                                                   */
/*                                                                                                                        */
/* OUTPUT        :  Status of 5G Off channel scan feature, DFS Feature, value of Parameters related to Off channel scan.  */
/*                  If scanned, No of BSS heard on each channel into WiFiLog.txt                                          */
/*                                                                                                                        */
/* RETURN VALUE  : INT                                                                                                    */
/*                                                                                                                        */
/**************************************************************************************************************************/
static int off_chan_scan_init (void *args)
{
    unsigned int radio_index;
    radio_index = *(unsigned int *) args;
    wifi_util_dbg_print(WIFI_MON,"%s:%d: Running Off_channel_scan for %u\n", __func__, __LINE__, radio_index);
    wifi_mgr_t *g_wifi_mgr = get_wifimgr_obj();
    ULONG Tscan = 0, Nscan = 0, Tidle = 0, NChannel = 0;
    wifi_neighborScanMode_t scanMode = WIFI_RADIO_SCAN_MODE_OFFCHAN;

    bool dfs_enable = g_wifi_mgr->rfc_dml_parameters.dfs_rfc;
    bool dfs_boot = g_wifi_mgr->rfc_dml_parameters.dfsatbootup_rfc;
    bool dfs = (dfs_enable | dfs_boot); /* checking if dfs is enabled in run time or boot up */
    bool off_scan_rfc = g_wifi_mgr->rfc_dml_parameters.wifioffchannelscan_rfc;
    Tscan = g_monitor_module.off_channel_cfg[radio_index].TscanMsec;
    Nscan = g_monitor_module.off_channel_cfg[radio_index].NscanSec;
    Tidle = g_monitor_module.off_channel_cfg[radio_index].TidleSec;
    CcspTraceDebug(("Off_channel_scan feature RFC = %d; TScan = %lu; NScan = %lu; Tidle = %lu; DFS:%d\n", off_scan_rfc, Tscan, Nscan, Tidle, dfs));

    if (!(is_radio_band_5G(g_wifi_mgr->radio_config[radio_index].oper.band))) {
        CcspTraceError(("Off_channel_scan Cannot run for radio index: %d as feature for the same is not developed yet\n",radio_index + 1));
        return TIMER_TASK_ERROR;
    }

    /*Checking if rfc is disabled or if any one of the params are 0; if yes, scan is aborted*/
    if (!off_scan_rfc || Tscan == 0 || Nscan == 0 || Tidle == 0) {
        CcspTraceInfo(("Off_channel_scan feature is disabled returning RFC = %d; TScan = %lu; NScan = %lu; Tidle = %lu\n", off_scan_rfc, Tscan, Nscan, Tidle));
        if ((g_monitor_module.off_channel_cfg[radio_index].curr_off_channel_scan_period != (int) Nscan) && (Nscan != 0)) {
            scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.off_channel_scan_id[radio_index], Nscan*1000);
            g_monitor_module.off_channel_cfg[radio_index].curr_off_channel_scan_period = Nscan;
        }
        return TIMER_TASK_COMPLETE;
    }

    //Getting primary channel and country code
    wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(radio_index);
    UINT prim_chan = radioOperation->channel;
    char countryStr[64] = {0};
    snprintf(countryStr, sizeof(wifiCountryMap[radioOperation->countryCode].countryStr),"%s", wifiCountryMap[radioOperation->countryCode].countryStr);
    wifi_util_dbg_print(WIFI_MON,"%s:%d Off_channel_scan Country Code:%s prim_chan:%u\n", __func__, __LINE__, countryStr, prim_chan);

    //If DFS enabled and country code is not US, CA or GB; the scan should not run for 5GHz radio. Possible updates might be required for GW using two 5G radios
    if (dfs && !(strncmp(countryStr, "US", 2) || strncmp(countryStr, "CA", 2) || strncmp(countryStr, "GB", 2))) {
        CcspTraceError(("Getting country code %s; skipping the scan!\n", countryStr));
        if ((g_monitor_module.off_channel_cfg[radio_index].curr_off_channel_scan_period != (int) Nscan) && (Nscan != 0)) {
            scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.off_channel_scan_id[radio_index], Nscan*1000);
            g_monitor_module.off_channel_cfg[radio_index].curr_off_channel_scan_period = Nscan;
        }
        return TIMER_TASK_COMPLETE;
    }
    CcspTraceInfo(("Off_channel_scan DFS:%d and country code is %s\n", dfs, countryStr));

    //Getting number of channels and channel numbers in a list
    wifi_radio_capabilities_t *wifiCapPtr = NULL;
    wifiCapPtr = getRadioCapability(radio_index);
    int num_chan = wifiCapPtr->channel_list[0].num_channels;
    wifi_util_dbg_print(WIFI_MON,"%s:%d off_channed_scan num of channels:%d\n", __func__, __LINE__, num_chan);
    UINT chan_list[MAX_5G_CHANNELS] = {'\0'};
    for (int num = 0; num < num_chan; num++) {
        chan_list[num] = wifiCapPtr->channel_list[0].channels_list[num];
        wifi_util_dbg_print(WIFI_MON,"%s:%d off_channel_scan chan number:%u\n", __func__, __LINE__, chan_list[num]);
    }

    //The Scan Kicks Off
    wifi_neighbor_ap2_t *neighbor_results = NULL;
    UINT array_size = 0;
    int ret = 0;
    for (int num = 0; num < num_chan; num++)
    {
        if (prim_chan == chan_list[num]) { //Skipping primary channel
            CcspTraceInfo(("Off_channel_scan  off channel number is same as current channel, skipping the off chan scan for channel:%d\n", chan_list[num]));
            continue;
        } else if (!dfs && (chan_list[num] >= DFS_START && chan_list[num] <= DFS_END)) { //Skip DFS channels if DFS disabled
            CcspTraceDebug(("Off_channel_scan Skipping DFS Channel\n"));
            continue;
        } else {
            wifi_startNeighborScan(radio_index, scanMode, Tscan, 1, &chan_list[num]);
            ret = wifi_getNeighboringWiFiStatus(radio_index, &neighbor_results, &array_size);
            if (ret == RETURN_OK) {
                CcspTraceInfo(("Off_channel_scan Total Scan Results:%d for channel %d \n", array_size, chan_list[num]));

                if (array_size > 0) {
                    off_chan_print_scan_data(radio_index, neighbor_results, array_size);
                }
                if (neighbor_results) {
                    free(neighbor_results);
                    neighbor_results = NULL;
                }
                NChannel++;
            } else {
                CcspTraceError(("Off_channel_scan Scan failed for channel %d\n", chan_list[num]));
            }
        }
    }

    //DCS metrics, getting channel utilization value
    wifi_channelMetrics_t * ptr, channelMetrics_array_1[MAX_5G_CHANNELS];
    int num_channels_dcs = 0;
    ptr = channelMetrics_array_1;
    memset(channelMetrics_array_1, 0, sizeof(channelMetrics_array_1));
    for (int num = 0; num < num_chan; num++)
    {
        if((dfs == 0) && (chan_list[num] >= DFS_START) && (chan_list[num] <= DFS_END)) {
            //Skipping DFS channels when DFS is disabled
            continue;
        }
        ptr[num_channels_dcs].channel_in_pool = TRUE;
        ptr[num_channels_dcs].channel_number = (chan_list[num]);
        ++num_channels_dcs;
    }

    ret = wifi_getRadioDcsChannelMetrics(radio_index, ptr, MAX_5G_CHANNELS);
    int num;
    for (num = 0; num < num_channels_dcs; num++, ptr++)
    {
        CcspTraceInfo(("Off_channel_scan Channel number:%d Channel Utilization:%d \n",ptr->channel_number, ptr->channel_utilization));
    }

    g_monitor_module.off_channel_cfg[radio_index].Nchannel = NChannel; //Update the number of channels scanned
    wifi_util_dbg_print(WIFI_MON,"Off_channel_scan Number of channels scanned: %lu\n", NChannel);

    if ((g_monitor_module.off_channel_cfg[radio_index].curr_off_channel_scan_period != (int) Nscan) && (Nscan != 0)) {
        scheduler_update_timer_task_interval(g_monitor_module.sched, g_monitor_module.off_channel_scan_id[radio_index], Nscan*1000);
        g_monitor_module.off_channel_cfg[radio_index].curr_off_channel_scan_period = Nscan;
    }
    return TIMER_TASK_COMPLETE;
}

/**************************************************************************************************************************/
/*                                                                                                                        */
/* FUNCTION NAME : off_chan_print_scan_data                                                                               */
/*                                                                                                                        */
/* DESCRIPTION   : This function prints the required information for 5G off channel scan feature into WiFiLog.txt         */
/*                                                                                                                        */
/* INPUT         : Neighbor report array and its size                                                                     */
/*                                                                                                                        */
/* OUTPUT        : Logs into WiFiLog.txt                                                                                 */
/*                                                                                                                        */
/* RETURN VALUE  : NONE                                                                                                   */
/*                                                                                                                        */
/***************************************************************************************************************************/
void off_chan_print_scan_data(unsigned int radio_index, wifi_neighbor_ap2_t *neighbor_result, int array_size)
{
    wifi_neighbor_ap2_t *ptr = NULL;
    int i = 0;
    for (i = 0, ptr = neighbor_result; i < array_size; i++, ptr++) {
        CcspTraceInfo(("Off_channel_scan Neighbor:%d ap_BSSID:%s ap_SignalStrength: %d\n", i + 1, ptr->ap_BSSID, ptr->ap_SignalStrength)); //Printing N/A if hidden SSID
        wifi_util_dbg_print(WIFI_MON,"Off channel scan ap_SSID:%s\n", (strlen(ptr->ap_SSID) != 0 ? ptr->ap_SSID : "N/A"));
    }
}
#endif //FEATURE_OFF_CHANNEL_SCAN_5G

#ifdef CCSP_COMMON
/* This function returns the system uptime at the time of init */
long get_sys_uptime()
{
    struct sysinfo s_info;
    int error = sysinfo(&s_info);
    if(error != 0) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d: Error reading sysinfo %d \n", __func__, __LINE__,error);
    }
    return s_info.uptime;
}

/*
  The get_upload_period takes two arguments iteration and oldInterval.
  Because, it will return old interval value if check is less than 5mins.
  */
 unsigned int get_upload_period  (int oldInterval)
 {
     FILE *fp;
     char buff[64];
     char *ptr;
     int logInterval=oldInterval;
     struct timeval polling_time = {0};
     time_t  time_gap = 0;
     gettimeofday(&polling_time, NULL);

     if ((fp = fopen("/tmp/upload", "r")) == NULL) {
     /* Minimum LOG Interval we can set is 300 sec, just verify every 5 mins any change in the LogInterval
        if any change in log_interval do the calculation and dump the VAP status */
          time_gap = polling_time.tv_sec - lastpolledtime;
          if ( time_gap >= 300 )
          {
               logInterval=readLogInterval();
               lastpolledtime = polling_time.tv_sec;
          }
          return logInterval;
     }

     fgets(buff, 64, fp);
     if ((ptr = strchr(buff, '\n')) != NULL) {
         *ptr = 0;
     }
     fclose(fp);

     return atoi(buff);
}
#endif // CCSP_COMMON

wifi_monitor_t *get_wifi_monitor()
{
    return &g_monitor_module;
}

wifi_actvie_msmt_t *get_active_msmt_data()
{
    return &g_active_msmt;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : getCurrentTimeInMicroSeconds                                  */
/*                                                                               */
/* DESCRIPTION   : This function returns the current time in micro seconds       */
/*                                                                               */
/* INPUT         : NONE                                                          */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : timestamp in micro seconds                                    */
/*                                                                               */
/*********************************************************************************/

unsigned long getCurrentTimeInMicroSeconds()
{
    struct timeval timer_usec;
    long long int timestamp_usec; /* timestamp in microsecond */

    if (!gettimeofday(&timer_usec, NULL)) {
        timestamp_usec = ((long long int) timer_usec.tv_sec) * 1000000ll +
          (long long int) timer_usec.tv_usec;
    } else {
        timestamp_usec = -1;
    }
    return timestamp_usec;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : isVapEnabled                                                  */
/*                                                                               */
/* DESCRIPTION   : This function checks whether AP is enabled or not             */
/*                                                                               */
/* INPUT         : wlanIndex - AP index                                          */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : TRUE / FALSE                                                  */
/*                                                                               */
/*********************************************************************************/

int isVapEnabled (int wlanIndex)
{

    DEBUG_PRINT (("Get_wifi_object_bss_parameter\n"));
    wifi_front_haul_bss_t *vap_bss_info = Get_wifi_object_bss_parameter(wlanIndex);
    if (vap_bss_info != NULL) {

        if (vap_bss_info->enabled == FALSE) {
            wifi_util_dbg_print(WIFI_MON, "ERROR> Wifi AP Not enabled for Index: %d\n", wlanIndex );
            return -1;
        }
    }

    return 0;
}
/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : WaitForDuration                                               */
/*                                                                               */
/* DESCRIPTION   : This function makes the calling thread to wait for particular */
/*                 time interval                                                 */
/*                                                                               */
/* INPUT         : timeInMs - time to wait                                       */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : TRUE / FALSE                                                  */
/*                                                                               */
/*********************************************************************************/

int WaitForDuration (int timeInMs)
{
    struct timespec   ts;
    struct timeval    tp;
    pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;
    int     ret;

    gettimeofday(&tp, NULL);

    /* Convert from timeval to timespec */
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;

    /* Add wait duration*/
    if ( timeInMs > 1000 ) {
        ts.tv_sec += (timeInMs/1000);
    } else 	{
        ts.tv_nsec = ts.tv_nsec + (timeInMs*CONVERT_MILLI_TO_NANO);
        ts.tv_sec = ts.tv_sec + ts.tv_nsec / 1000000000L;
        ts.tv_nsec = ts.tv_nsec % 1000000000L;
    }
    pthread_mutex_lock(&mutex);
    ret = pthread_cond_timedwait(&cond, &mutex, &ts);
    pthread_mutex_unlock(&mutex);

    return ret;
}

#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_)
static void convert_channel_width_to_str(wifi_channelBandwidth_t cw, char *str, size_t len)
{
    char arr_str[][8] = {"20", "40", "80", "160"};
    wifi_channelBandwidth_t arr_enum[] = {
        WIFI_CHANNELBANDWIDTH_20MHZ, 
        WIFI_CHANNELBANDWIDTH_40MHZ,
        WIFI_CHANNELBANDWIDTH_80MHZ,
        WIFI_CHANNELBANDWIDTH_160MHZ
    };

    for (size_t i = 0; i < ARRAY_SIZE(arr_enum); i++) {
        if (arr_enum[i] == cw) {
            snprintf(str, len, "%s", arr_str[i]);
            break;
        }
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s:%d %d converted to %s\n",__func__,__LINE__, cw, str);
}

static void convert_variant_to_str(wifi_ieee80211Variant_t variant, char *str, size_t len)
{
    char arr_str[][8] = {"a", "b", "g", "n", "ac", "ax"};
    wifi_ieee80211Variant_t arr_enum[] = {
        WIFI_80211_VARIANT_A,
        WIFI_80211_VARIANT_B,
        WIFI_80211_VARIANT_G,
        WIFI_80211_VARIANT_N,
        WIFI_80211_VARIANT_AC,
        WIFI_80211_VARIANT_AX
    };

    for (size_t i = 0; i < ARRAY_SIZE(arr_enum); i++) {
        if ((arr_enum[i] & variant) == arr_enum[i]) {
            snprintf(str, len, "%s,", arr_str[i]);
        }
    }

    str[strlen(str) - 1] = '\0';
    wifi_util_dbg_print(WIFI_CTRL, "%s:%d 0x%X converted to %s\n",__func__,__LINE__, variant, str);
}
#endif
/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : pktGen_BlastClient                                            */
/*                                                                               */
/* DESCRIPTION   : This function uses the pktgen utility and calculates the      */
/*                 throughput                                                    */
/*                                                                               */
/* INPUT         : vargp - ptr to variable arguments                             */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void pktGen_BlastClient (char *dst_mac, wifi_interface_name_t *ifname)
{
    unsigned int SampleCount = 0;
    wifi_associated_dev3_t dev_conn;
    unsigned long totalduration = 0;
    double  Sum = 0, AvgThroughput;
#ifdef CCSP_COMMON
    unsigned long DiffsamplesAck = 0, Diffsamples = 0, TotalAckSamples = 0, TotalSamples = 0;
    double  tp, AckRate, AckSum = 0, Rate, AvgAckThroughput;
#endif // CCSP_COMMON
    char    s_mac[MIN_MAC_LEN+1];
    wifi_ctrl_t *ctrl = get_wifictrl_obj();
    char msg[256] = {};
    active_msmt_step_t *step = &g_active_msmt.curStepData;
#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_)
    int index = g_active_msmt.curStepData.ApIndex;
    int radio_index = get_radio_index_for_vap_index(&(get_wifimgr_obj())->hal_cap.wifi_prop, index);
    wifi_radio_operationParam_t* radioOperation = NULL;
#endif
#ifndef CCSP_COMMON
    char *radio_type = NULL;
    int nf = 0;
    int sleep_mode = 0;
    int wakeup_cnt = 0;
    unsigned long prevSentAck = 0;
#endif // CCSP_COMMON

#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_)
    if (radio_index != RETURN_ERR) {
        radioOperation = getRadioOperationParam(radio_index);
#ifndef CCSP_COMMON
        radio_type = g_monitor_module.radio_data[radio_index].frequency_band;  
        nf = g_monitor_module.radio_data[radio_index].NoiseFloor;   
#endif // CCSP_COMMON
    }
#endif

    active_msmt_log_message("%s:%d Start pktGen utility and analyse received samples for active clients [" MAC_FMT_TRIMMED "]\n",
        __FUNCTION__, __LINE__,  MAC_ARG(g_active_msmt.curStepData.DestMac));

    snprintf(s_mac, MIN_MAC_LEN + 1, MAC_FMT_TRIMMED, MAC_ARG(g_active_msmt.curStepData.DestMac));

#if defined (DUAL_CORE_XB3)
    wifi_setClientDetailedStatisticsEnable(getRadioIndexFromAp(index), TRUE);
#endif

    if (onewifi_pktgen_start_wifi_blast((char *)ifname, dst_mac, GetActiveMsmtPktSize()) != ONEWIFI_PKTGEN_STATUS_SUCCEED) {

        snprintf(msg, sizeof(msg), "Failed to run Traffic generator");

        if (ctrl->network_mode == rdk_dev_mode_type_ext) {

            SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_FAILED);
            active_msmt_set_status_desc(__func__, g_active_msmt.active_msmt.PlanId, step->StepId, step->DestMac, msg);
        }

        active_msmt_log_message("%s:%d %s\n", __func__, __LINE__, msg);

        return;
    }

#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_)
    int waittime = GetActiveMsmtSampleDuration();
#endif

    /* allocate memory for the dynamic variables */
    g_active_msmt.active_msmt_data = (active_msmt_data_t *)calloc(GetActiveMsmtNumberOfSamples() + 1, sizeof(active_msmt_data_t));

    if (g_active_msmt.active_msmt_data == NULL) {
#if defined (DUAL_CORE_XB3)
        wifi_setClientDetailedStatisticsEnable(getRadioIndexFromAp(index), FALSE);
#endif
        active_msmt_log_message("%s:%d  ERROR> Memory allocation failed for active_msmt_data\n", __func__, __LINE__);
        return;
    }

    /* sampling */
    while ( SampleCount < (GetActiveMsmtNumberOfSamples() + 1)) {
        memset(&dev_conn, 0, sizeof(wifi_associated_dev3_t));

#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_)
#ifdef CCSP_COMMON
        active_msmt_log_message("%s : %d WIFI_HAL enabled, calling wifi_getApAssociatedClientDiagnosticResult with mac : %s\n",__func__,__LINE__,s_mac);
#endif // CCSP_COMMON

        unsigned long start = getCurrentTimeInMicroSeconds ();
        WaitForDuration ( waittime );

        if (radio_index != RETURN_ERR) {
#ifdef CCSP_COMMON
            if (wifi_getApAssociatedClientDiagnosticResult(index, s_mac, &dev_conn) == RETURN_OK) {
#else
            if (ow_mesh_ext_get_device_stats(index, radio_type, nf, g_active_msmt.curStepData.DestMac, &dev_conn, &sleep_mode) == RETURN_OK) {

                if (sleep_mode && (prevSentAck == dev_conn.cli_DataFramesSentAck) && wakeup_cnt < WIFI_BLASTER_WAKEUP_LIMIT) {
                    wakeup_cnt++;
                    continue;
                } else if (sleep_mode && wakeup_cnt >= WIFI_BLASTER_WAKEUP_LIMIT) {
                    snprintf(msg, sizeof(msg), "The client is in sleeping mode");

                    SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_SLEEP_CLIENT);
                    active_msmt_set_status_desc(__func__, g_active_msmt.active_msmt.PlanId, step->StepId, step->DestMac, msg);
                    active_msmt_log_message("%s:%d %s\n", __func__, __LINE__, msg);

                    free(g_active_msmt.active_msmt_data);
                    g_active_msmt.active_msmt_data = NULL;

                    return;
                }

                prevSentAck = dev_conn.cli_DataFramesSentAck;
#endif // CCSP_COMMON

                frameCountSample[SampleCount].WaitAndLatencyInMs = ((getCurrentTimeInMicroSeconds () - start) / 1000);
                active_msmt_log_message("PKTGEN_WAIT_IN_MS duration : %lu\n", ((getCurrentTimeInMicroSeconds () - start)/1000));

                g_active_msmt.active_msmt_data[SampleCount].rssi = dev_conn.cli_RSSI;
                g_active_msmt.active_msmt_data[SampleCount].TxPhyRate = dev_conn.cli_LastDataDownlinkRate;
                g_active_msmt.active_msmt_data[SampleCount].RxPhyRate = dev_conn.cli_LastDataUplinkRate;
                g_active_msmt.active_msmt_data[SampleCount].SNR = dev_conn.cli_SNR;
#ifdef CCSP_COMMON
                g_active_msmt.active_msmt_data[SampleCount].ReTransmission = dev_conn.cli_Retransmissions;
                g_active_msmt.active_msmt_data[SampleCount].MaxTxRate = dev_conn.cli_MaxDownlinkRate;
                g_active_msmt.active_msmt_data[SampleCount].MaxRxRate = dev_conn.cli_MaxUplinkRate;
#else
                g_active_msmt.active_msmt_data[SampleCount].RetransCount = dev_conn.cli_RetransCount;
#endif // CCSP_COMMON
                if (radioOperation != NULL) {
                    convert_channel_width_to_str(radioOperation->channelWidth, g_active_msmt.active_msmt_data[SampleCount].Operating_channelwidth, OPER_BUFFER_LEN);
                    convert_variant_to_str(radioOperation->variant, g_active_msmt.active_msmt_data[SampleCount].Operating_standard, OPER_BUFFER_LEN);
                }

                frameCountSample[SampleCount].PacketsSentAck = dev_conn.cli_DataFramesSentAck;
#ifdef CCSP_COMMON
                frameCountSample[SampleCount].PacketsSentTotal = dev_conn.cli_PacketsSent + dev_conn.cli_DataFramesSentNoAck;
#endif // CCSP_COMMON

                wifi_util_dbg_print(WIFI_MON,"samplecount[%d] : PacketsSentAck[%lu] PacketsSentTotal[%lu]"
                        " WaitAndLatencyInMs[%d ms] RSSI[%d] TxRate[%lu Mbps] RxRate[%lu Mbps] SNR[%d]"
                        "chanbw [%s] standard [%s] MaxTxRate[%d] MaxRxRate[%d]\n",
                        SampleCount, dev_conn.cli_DataFramesSentAck, (dev_conn.cli_PacketsSent + dev_conn.cli_DataFramesSentNoAck),
                        frameCountSample[SampleCount].WaitAndLatencyInMs, dev_conn.cli_RSSI, dev_conn.cli_LastDataDownlinkRate, dev_conn.cli_LastDataUplinkRate, dev_conn.cli_SNR,g_active_msmt.active_msmt_data[SampleCount].Operating_channelwidth ,g_active_msmt.active_msmt_data[SampleCount].Operating_standard,g_active_msmt.active_msmt_data[SampleCount].MaxTxRate, g_active_msmt.active_msmt_data[SampleCount].MaxRxRate);
            } else {

                if (ctrl->network_mode == rdk_dev_mode_type_ext) {
                    active_msmt_status_t status;

                    if (is_device_associated(index, s_mac) == false) {
                        snprintf(msg, sizeof(msg), "The MAC is disconnected");
                        status = ACTIVE_MSMT_STATUS_NO_CLIENT;
                    }
                    else {
                        snprintf(msg, sizeof(msg), "Failed to fill in client stats");
                        status = ACTIVE_MSMT_STATUS_FAILED;
                    }

                    SetActiveMsmtStatus(__func__, status);
                    active_msmt_set_status_desc(__func__, g_active_msmt.active_msmt.PlanId, step->StepId, step->DestMac, msg);
                }

#ifdef CCSP_COMMON
                active_msmt_log_message("%s : %d wifi_getApAssociatedClientDiagnosticResult failed for mac : %s\n",__func__,__LINE__,s_mac);
                frameCountSample[SampleCount].PacketsSentTotal = 0;
#else
                wifi_util_dbg_print(WIFI_MON,"%s:%d ow_mesh_ext_get_device_stats failed for mac:%s\n",__func__,__LINE__,s_mac);
#endif // CCSP_COMMON
                frameCountSample[SampleCount].PacketsSentAck = 0;
                frameCountSample[SampleCount].WaitAndLatencyInMs = 0;

                free(g_active_msmt.active_msmt_data);
                g_active_msmt.active_msmt_data = NULL;

                return;
            }
        } else {
            active_msmt_log_message("%s:%d radio_index is invalid. So, client is treated as offline\n",__func__, __LINE__);
#ifdef CCSP_COMMON
            frameCountSample[SampleCount].PacketsSentTotal = 0;
#endif // CCSP_COMMON
            frameCountSample[SampleCount].PacketsSentAck = 0;
            frameCountSample[SampleCount].WaitAndLatencyInMs = 0;
            strncpy(g_active_msmt.active_msmt_data[SampleCount].Operating_standard, "NULL",OPER_BUFFER_LEN);
            strncpy(g_active_msmt.active_msmt_data[SampleCount].Operating_channelwidth, "NULL",OPER_BUFFER_LEN);

            free(g_active_msmt.active_msmt_data);
            g_active_msmt.active_msmt_data = NULL;

            return;
        }
#endif
        SampleCount++;
    }

#if defined (DUAL_CORE_XB3)
        wifi_setClientDetailedStatisticsEnable(getRadioIndexFromAp(index), FALSE);
#endif

#ifdef CCSP_COMMON
    // Analyze samples and get Throughput
    for (SampleCount=0; SampleCount < GetActiveMsmtNumberOfSamples(); SampleCount++) {
        DiffsamplesAck = frameCountSample[SampleCount+1].PacketsSentAck - frameCountSample[SampleCount].PacketsSentAck;
        Diffsamples = frameCountSample[SampleCount+1].PacketsSentTotal - frameCountSample[SampleCount].PacketsSentTotal;

        tp = (double)(DiffsamplesAck*8*GetActiveMsmtPktSize());              //number of bits
        wifi_util_dbg_print(WIFI_MON,"tp = [%f bits]\n", tp );
        tp = tp/1000000;                //convert to Mbits
        wifi_util_dbg_print(WIFI_MON,"tp = [%f Mb]\n", tp );
        AckRate = (tp/frameCountSample[SampleCount+1].WaitAndLatencyInMs) * 1000;                        //calculate bitrate in the unit of Mbpms

        tp = (double)(Diffsamples*8*GetActiveMsmtPktSize());         //number of bits
        wifi_util_dbg_print(WIFI_MON,"tp = [%f bits]\n", tp );
        tp = tp/1000000;                //convert to Mbits
        wifi_util_dbg_print(WIFI_MON,"tp = [%f Mb]\n", tp );
        Rate = (tp/frameCountSample[SampleCount+1].WaitAndLatencyInMs) * 1000;                   //calculate bitrate in the unit of Mbpms

        /* updating the throughput in the global variable */
        g_active_msmt.active_msmt_data[SampleCount].throughput = AckRate;

        wifi_util_dbg_print(WIFI_MON,"Sample[%d]   DiffsamplesAck[%lu]   Diffsamples[%lu]   BitrateAckPackets[%.5f Mbps]   BitrateTotalPackets[%.5f Mbps]\n", SampleCount, DiffsamplesAck, Diffsamples, AckRate, Rate );
        AckSum += AckRate;
        Sum += Rate;
        TotalAckSamples += DiffsamplesAck;
        TotalSamples += Diffsamples;

        totalduration += frameCountSample[SampleCount+1].WaitAndLatencyInMs;
    }
    AvgAckThroughput = AckSum/GetActiveMsmtNumberOfSamples();
    AvgThroughput = Sum/GetActiveMsmtNumberOfSamples();
    active_msmt_log_message("\nTotal number of ACK Packets = %lu   Total number of Packets = %lu   Total Duration = %lu ms\n", TotalAckSamples, TotalSamples, totalduration );
    active_msmt_log_message("Calculated Average : ACK Packets Throughput[%.2lf Mbps]  Total Packets Throughput[%.2lf Mbps]\n\n", AvgAckThroughput, AvgThroughput );
#else

    /* Actual count of samples is N+1. MQTT report operates with [1, N+1] samples.
     * The 0 sample is involved to provide ability to get delta for PacketsSentAck & ReTransmission
     */

    for (SampleCount = 1; SampleCount < GetActiveMsmtNumberOfSamples() + 1; SampleCount++) {
        uint64_t bits_diff;

        /* PacketsSentAck could be 0 for few samples due to the sleep mode */
        if (!frameCountSample[SampleCount].PacketsSentAck) {
            continue;
        }

        bits_diff = (frameCountSample[SampleCount].PacketsSentAck - frameCountSample[SampleCount - 1].PacketsSentAck) * 8; 

        /* Analytics for MQTT report */
        g_active_msmt.active_msmt_data[SampleCount - 1].throughput = (bits_diff / 1000.0) / frameCountSample[SampleCount].WaitAndLatencyInMs;
        g_active_msmt.active_msmt_data[SampleCount - 1].ReTransmission = g_active_msmt.active_msmt_data[SampleCount].RetransCount -
            g_active_msmt.active_msmt_data[SampleCount - 1].RetransCount;

        /* Analytics for logs */
        totalduration += frameCountSample[SampleCount].WaitAndLatencyInMs;
        Sum += g_active_msmt.active_msmt_data[SampleCount - 1].throughput;
    }

    AvgThroughput = Sum / GetActiveMsmtNumberOfSamples();
    wifi_util_dbg_print(WIFI_MON, "%s:%d Total duration: %lu ms, Avg Throughput: %.2lf Mbps\n", __func__, __LINE__, totalduration, AvgThroughput);
#endif // CCSP_COMMON

    return;
}

/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : WiFiBlastClient                                               */
/*                                                                               */
/* DESCRIPTION   : This function starts the active measurement process to        */
/*                 start the pktgen and to calculate the throughput for a        */
/*                 particular client                                             */
/*                                                                               */
/* INPUT         : ClientMac - MAC address of the client                         */
/*                 apIndex - AP index                                            */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/

void WiFiBlastClient(void)
{
    char macStr[18] = {'\0'};
    unsigned int StepCount = 0;
    int apIndex = 0;
    unsigned int NoOfSamples = 0, oldNoOfSamples = 0;
    wifi_interface_name_t *interface_name = NULL;
    wifi_mgr_t *g_wifi_mgr = get_wifimgr_obj();
    wifi_ctrl_t *g_wifi_ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    active_msmt_t *cfg = &g_active_msmt.active_msmt;
    wifi_radio_operationParam_t* radioOperation;
    int radio_index;
    char msg[256] = {};

    for (StepCount = 0; StepCount < MAX_STEP_COUNT; StepCount++) {

        pthread_mutex_lock(&g_active_msmt.lock);

        if (cfg->ActiveMsmtEnable == false) {
            for (StepCount = StepCount; StepCount < MAX_STEP_COUNT; StepCount++) {
                cfg->StepInstance[StepCount] = ACTIVE_MSMT_STEP_DONE;
            }
            active_msmt_log_message("ActiveMsmtEnable changed from TRUE to FALSE"
                "Setting remaining [%d] step count to 0 and STOPPING further processing\n",
                (MAX_STEP_COUNT - StepCount));
            pthread_mutex_unlock(&g_active_msmt.lock);
            break;
        }

        NoOfSamples = GetActiveMsmtNumberOfSamples();

        if (oldNoOfSamples != NoOfSamples) {
            frameCountSample = (pktGenFrameCountSamples *)realloc(frameCountSample, (NoOfSamples + 1) * sizeof(pktGenFrameCountSamples));

            if (frameCountSample == NULL) {
                wifi_util_error_print(WIFI_MON, "Memory allocation failed for frameCountSample\n");
                pthread_mutex_unlock(&g_active_msmt.lock);
                break;
            }

            memset(frameCountSample, 0, (NoOfSamples + 1) * sizeof(pktGenFrameCountSamples));
            oldNoOfSamples = NoOfSamples;
            wifi_util_dbg_print(WIFI_MON, "%s:%d Size for frameCountSample changed to %d\n", __func__, __LINE__, NoOfSamples);
        }

        if (cfg->StepInstance[StepCount] == ACTIVE_MSMT_STEP_PENDING) {
            char s_mac[MIN_MAC_LEN + 1] = {};

            wifi_util_dbg_print(WIFI_MON,"%s : %d processing StepCount : %d \n",__func__,__LINE__,StepCount);
            apIndex = cfg->Step[StepCount].ApIndex;

            /*TODO RDKB-34680 CID: 154402,154401  Data race condition*/
            g_active_msmt.curStepData.ApIndex = apIndex;
            g_active_msmt.curStepData.StepId = cfg->Step[StepCount].StepId;

            memcpy(g_active_msmt.curStepData.DestMac, cfg->Step[StepCount].DestMac, sizeof(mac_address_t));
            snprintf(s_mac, MIN_MAC_LEN + 1, MAC_FMT_TRIMMED, MAC_ARG(g_active_msmt.curStepData.DestMac));

            wifi_util_dbg_print(WIFI_MON,"%s:%d copied mac address " MAC_FMT " to current step info\n", __func__, __LINE__,
                MAC_ARG(g_active_msmt.curStepData.DestMac));

            if (isVapEnabled(apIndex) != 0) {
                wifi_util_error_print(WIFI_MON, "ERROR running wifiblaster: Init Failed\n" );
                pthread_mutex_unlock(&g_active_msmt.lock);
                continue;
            }

            /* WiFiBlastClient is derefered task, so client could disconnect
             * before it starts
             */
            if (is_device_associated(apIndex, s_mac) == false) {

                if (g_wifi_ctrl->network_mode == rdk_dev_mode_type_ext) {

                    snprintf(msg, sizeof(msg), "The MAC is disconnected in traffic gen");
                    active_msmt_report_error(__func__, cfg->PlanId, &cfg->Step[StepCount], msg, ACTIVE_MSMT_STATUS_NO_CLIENT);

                    /* Set status as succeed back to be able to procceed other Steps */
                    SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_SUCCEED);
                } else {
                    process_active_msmt_diagnostics(apIndex);
                    stream_client_msmt_data(true);
                }

                active_msmt_log_message( "%s:%d no need to start pktgen for offline client: %s\n" ,__FUNCTION__, __LINE__, s_mac);
                active_msmt_set_step_status(__func__, StepCount, ACTIVE_MSMT_STEP_INVALID);
                pthread_mutex_unlock(&g_active_msmt.lock);
                continue;
            }

            if ((radio_index = get_radio_index_for_vap_index(&(get_wifimgr_obj())->hal_cap.wifi_prop, apIndex)) == RETURN_ERR ||
                (radioOperation = getRadioOperationParam(radio_index)) == NULL) {

                active_msmt_log_message("%s:%d Unable to get radio params for %d\n", __func__, __LINE__, radio_index);
                pthread_mutex_unlock(&g_active_msmt.lock);
                continue;
            }

            if (g_monitor_module.radio_data[radio_index].primary_radio_channel != radioOperation->channel) {
                if (g_wifi_ctrl->network_mode == rdk_dev_mode_type_ext) {

                    snprintf(msg, sizeof(msg), "Failed to fill in radio stats");
                    active_msmt_report_error(__func__, cfg->PlanId, &cfg->Step[StepCount], msg, ACTIVE_MSMT_STATUS_FAILED);

                    /* Set status as succeed back to be able to procceed other Steps */
                    SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_SUCCEED);
                } else {
                    process_active_msmt_diagnostics(apIndex);
                    stream_client_msmt_data(true);
                }

                active_msmt_log_message( "%s:%d No radio data for %d channel\n" ,__FUNCTION__, __LINE__, radioOperation->channel);
                active_msmt_set_step_status(__func__, StepCount, ACTIVE_MSMT_STEP_INVALID);
                pthread_mutex_unlock(&g_active_msmt.lock);
                continue;
            }

            if ((interface_name = get_interface_name_for_vap_index(apIndex, &g_wifi_mgr->hal_cap.wifi_prop)) == NULL) {
                wifi_util_error_print(WIFI_MON, "%s:%d Unable to get ifname for %d\n", __func__, __LINE__, apIndex);
                pthread_mutex_unlock(&g_active_msmt.lock);
                continue;
            }

            snprintf(macStr, sizeof(macStr), MAC_FMT, MAC_ARG(g_active_msmt.curStepData.DestMac));

            active_msmt_log_message("\n=========START THE TEST=========\n");
            active_msmt_log_message("Blaster test is initiated for Dest mac [%s]\n", macStr);
            active_msmt_log_message("Interface [%s], Send Duration: [%d msecs], Packet Size: [%d bytes], Sample count: [%d]\n",
                    interface_name, GetActiveMsmtSampleDuration(), GetActiveMsmtPktSize(), GetActiveMsmtNumberOfSamples());

            /* start blasting the packets to calculate the throughput */
            pktGen_BlastClient(macStr, interface_name);

            if (onewifi_pktgen_stop_wifi_blast() != RETURN_OK) {
                wifi_util_error_print(WIFI_MON, "%s:%d: Failed to stop pktgen\n", __FUNCTION__, __LINE__);
            }

            if (g_active_msmt.status == ACTIVE_MSMT_STATUS_SUCCEED) {
                active_msmt_set_status_desc(__func__, cfg->PlanId, cfg->Step[StepCount].StepId,
                    cfg->Step[StepCount].DestMac, NULL);

                /* calling process_active_msmt_diagnostics to update the station info */
                active_msmt_log_message("%s:%d calling process_active_msmt_diagnostics\n", __func__, __LINE__);
                process_active_msmt_diagnostics(apIndex);
            }

            /* calling stream_client_msmt_data to upload the data to AVRO schema */
            active_msmt_log_message("%s:%d calling stream_client_msmt_data\n", __func__, __LINE__);
            stream_client_msmt_data(true);

            cfg->StepInstance[StepCount] = ACTIVE_MSMT_STEP_DONE;

            if (g_active_msmt.status != ACTIVE_MSMT_STATUS_SUCCEED) {
                SetActiveMsmtStatus(__func__, ACTIVE_MSMT_STATUS_SUCCEED);
            }
        }
        pthread_mutex_unlock(&g_active_msmt.lock);
        /* Set CPU free for a while */
        WaitForDuration(WIFI_BLASTER_POST_STEP_TIMEOUT);
    }
    if (frameCountSample != NULL) {
        wifi_util_dbg_print(WIFI_MON, "%s : %d freeing memory for frameCountSample \n",__func__,__LINE__);
        free(frameCountSample);
        frameCountSample = NULL;
    }

    if (g_wifi_ctrl->network_mode == rdk_dev_mode_type_ext) {
        g_wifi_mgr->ctrl.webconfig_state |= ctrl_webconfig_state_blaster_cfg_complete_rsp_pending;
        wifi_util_dbg_print(WIFI_MON, "%s : %d  Extender Mode Activated. Updated the blaster state as complete\n", __func__, __LINE__);
    }
    else if (g_wifi_ctrl->network_mode == rdk_dev_mode_type_gw) {
        wifi_util_dbg_print(WIFI_MON, "%s : %d Device operating in GW mode. No need to update status\n", __func__, __LINE__);
    }

    wifi_util_dbg_print(WIFI_MON, "%s : %d exiting the function\n",__func__,__LINE__);
}
/*********************************************************************************/
/*                                                                               */
/* FUNCTION NAME : process_active_msmt_diagnostics                               */
/*                                                                               */
/* DESCRIPTION   : This function update the station info with the global monitor */
/*                 data info which gets uploaded to the AVRO schema              */
/*                                                                               */
/* INPUT         : ap_index - AP index                                           */
/*                                                                               */
/* OUTPUT        : NONE                                                          */
/*                                                                               */
/* RETURN VALUE  : NONE                                                          */
/*                                                                               */
/*********************************************************************************/
void process_active_msmt_diagnostics (int ap_index)
{
    hash_map_t     *sta_map;
    sta_data_t *sta;
    sta_key_t       sta_key;
    unsigned int count = 0;
    unsigned int vap_array_index;

    wifi_util_dbg_print(WIFI_MON, "%s : %d  apindex : %d \n",__func__,__LINE__,ap_index);

    /* changing the ApIndex to 0 since for offline client the ApIndex will be -1.
      with ApIndex as -1 the sta_map will fail to fetch the value which result in crash
      */
    if (ap_index == -1) {
        ap_index = 0;
        wifi_util_dbg_print(WIFI_MON, "%s : %d  changed ap_index to : %d \n",__func__,__LINE__,ap_index);
    }
    getVAPArrayIndexFromVAPIndex((unsigned int)ap_index, &vap_array_index);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;

    to_sta_key(g_active_msmt.curStepData.DestMac, sta_key);
    str_tolower(sta_key);

    sta = (sta_data_t *)hash_map_get(sta_map, sta_key);

    if (sta == NULL) {
        /* added the data in sta map for offline clients */
        wifi_util_dbg_print(WIFI_MON, "%s : %d station info is null \n",__func__,__LINE__);
        sta = (sta_data_t *) malloc (sizeof(sta_data_t));
        if (sta == NULL) {
            return;
        }
        memset(sta, 0, sizeof(sta_data_t));
        pthread_mutex_lock(&g_monitor_module.data_lock);
        memcpy(sta->sta_mac, g_active_msmt.curStepData.DestMac, sizeof(mac_addr_t));
        sta->updated = true;
        sta->dev_stats.cli_Active = false;
        hash_map_put(sta_map, strdup(sta_key), sta);
        memcpy(&sta->dev_stats.cli_MACAddress, g_active_msmt.curStepData.DestMac, sizeof(mac_addr_t));
        pthread_mutex_unlock(&g_monitor_module.data_lock);
    } else {
        wifi_util_dbg_print(WIFI_MON, "%s:%d copying mac : " MAC_FMT " to station info\n", __func__, __LINE__,
                MAC_ARG(g_active_msmt.curStepData.DestMac));
        memcpy(&sta->dev_stats.cli_MACAddress, g_active_msmt.curStepData.DestMac, sizeof(mac_addr_t));
    }
    wifi_util_dbg_print(WIFI_MON, "%s : %d allocating memory for sta_active_msmt_data \n",__func__,__LINE__);
    sta->sta_active_msmt_data = (active_msmt_data_t *) calloc (g_active_msmt.active_msmt.ActiveMsmtNumberOfSamples,sizeof(active_msmt_data_t));

    if (sta->sta_active_msmt_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s : %d allocating memory for sta_active_msmt_data failed\n",__func__,__LINE__);
        /*CID: 146766 Dereference after null check*/
        return;
    }

    active_msmt_log_message("%s:%d Number of sample %d for client [" MAC_FMT "]\n", __FUNCTION__, __LINE__,
         g_active_msmt.active_msmt.ActiveMsmtNumberOfSamples, MAC_ARG(g_active_msmt.curStepData.DestMac));

    if (g_active_msmt.active_msmt_data != NULL) {
        for (count = 0; count < g_active_msmt.active_msmt.ActiveMsmtNumberOfSamples; count++) {
            sta->sta_active_msmt_data[count].rssi = g_active_msmt.active_msmt_data[count].rssi;
            sta->sta_active_msmt_data[count].TxPhyRate = g_active_msmt.active_msmt_data[count].TxPhyRate;
            sta->sta_active_msmt_data[count].RxPhyRate = g_active_msmt.active_msmt_data[count].RxPhyRate;
            sta->sta_active_msmt_data[count].SNR = g_active_msmt.active_msmt_data[count].SNR;
            sta->sta_active_msmt_data[count].ReTransmission = g_active_msmt.active_msmt_data[count].ReTransmission;
            sta->sta_active_msmt_data[count].MaxRxRate = g_active_msmt.active_msmt_data[count].MaxRxRate;
            sta->sta_active_msmt_data[count].MaxTxRate = g_active_msmt.active_msmt_data[count].MaxTxRate;
            strncpy(sta->sta_active_msmt_data[count].Operating_standard, g_active_msmt.active_msmt_data[count].Operating_standard,OPER_BUFFER_LEN);
            strncpy(sta->sta_active_msmt_data[count].Operating_channelwidth, g_active_msmt.active_msmt_data[count].Operating_channelwidth,OPER_BUFFER_LEN);
            sta->sta_active_msmt_data[count].throughput = g_active_msmt.active_msmt_data[count].throughput;

            active_msmt_log_message("count[%d] : standard[%s] chan_width[%s] Retransmission [%d]"
                "RSSI[%d] TxRate[%lu Mbps] RxRate[%lu Mbps] SNR[%d] throughput[%.5lf Mbps]"
                "MaxTxRate[%d] MaxRxRate[%d]\n",
                count, sta->sta_active_msmt_data[count].Operating_standard,
                sta->sta_active_msmt_data[count].Operating_channelwidth,
                sta->sta_active_msmt_data[count].ReTransmission,
                sta->sta_active_msmt_data[count].rssi, sta->sta_active_msmt_data[count].TxPhyRate,
                sta->sta_active_msmt_data[count].RxPhyRate, sta->sta_active_msmt_data[count].SNR,
                sta->sta_active_msmt_data[count].throughput,
                sta->sta_active_msmt_data[count].MaxTxRate,
                sta->sta_active_msmt_data[count].MaxRxRate);
        }

        wifi_util_dbg_print(WIFI_MON, "%s : %d memory freed for g_active_msmt.active_msmt_data\n",__func__,__LINE__);
        free(g_active_msmt.active_msmt_data);
        g_active_msmt.active_msmt_data = NULL;
    }
    wifi_util_dbg_print(WIFI_MON, "%s : %d exiting the function\n",__func__,__LINE__);
}

#ifdef CCSP_COMMON
sta_data_t *get_stats_for_sta(unsigned int apIndex, mac_addr_t mac)
{
    sta_data_t  *sta;
    hash_map_t  *sta_map;
    unsigned int vap_array_index;

    getVAPArrayIndexFromVAPIndex(apIndex, &vap_array_index);

    pthread_mutex_lock(&g_monitor_module.data_lock);
    sta_map = g_monitor_module.bssid_data[vap_array_index].sta_map;

    sta = hash_map_get_first(sta_map);
    while (sta != NULL) {
        if (memcmp(mac, sta->sta_mac, sizeof(mac_addr_t)) == 0) {
            pthread_mutex_unlock(&g_monitor_module.data_lock);
            return sta;
        }
        sta = hash_map_get_next(sta_map, sta);
    }

    pthread_mutex_unlock(&g_monitor_module.data_lock);
    return NULL;
}

int get_dev_stats_for_radio(unsigned int radio_index, radio_data_t *radio_stats)
{
    if (radio_index < getNumberRadios()) {
        memcpy(radio_stats, &g_monitor_module.radio_data[radio_index], sizeof(radio_data_t));
        return RETURN_OK;
    } else {
        wifi_util_error_print(WIFI_MON, "%s : %d wrong radio index:%d\n", __func__, __LINE__, radio_index);
    }

    return RETURN_ERR;
}

int get_radio_channel_utilization(unsigned int radio_index, int *chan_util)
{
    int ret = RETURN_ERR;
    radio_data_t radio_stats;
    memset(&radio_stats, 0, sizeof(radio_stats));

    ret = get_dev_stats_for_radio(radio_index, &radio_stats);
    if (ret == RETURN_OK) {
        *chan_util = radio_stats.channelUtil;
    }

    return ret;
}

void adjust_app_control_counters(wifi_dca_element_t *dca_task_element, unsigned long new_task_interval, unsigned int new_task_rep)
{
    wifi_dca_app_element_t *app_element = NULL;

    if (dca_task_element->app_list == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: APP list is NULL\n", __func__,__LINE__);
        return;
    }

    app_element = hash_map_get_first(dca_task_element->app_list);
    while(app_element != NULL) {
        app_element->counter_limit = app_element->req_interval_ms/new_task_interval;
        app_element->execution_counter = (app_element->execution_counter * dca_task_element->curr_interval_ms)/new_task_interval;

        app_element = hash_map_get_next(dca_task_element->app_list, app_element);
    }

    return;
}

unsigned long find_interval_common_divisor(unsigned long *interval_array, int array_len)
{
    unsigned long result = 0, temp_time_interval = 0;
    int itr = 0;
    int found = 0;
    temp_time_interval = (interval_array[0]/MONITOR_RUNNING_INTERVAL_IN_MILLISEC)*MONITOR_RUNNING_INTERVAL_IN_MILLISEC;

    for (itr = 1; itr < array_len; itr++) {
        interval_array[itr] = (interval_array[itr]/MONITOR_RUNNING_INTERVAL_IN_MILLISEC)*MONITOR_RUNNING_INTERVAL_IN_MILLISEC;
        if (temp_time_interval > interval_array[itr]) {
            temp_time_interval = interval_array[itr];
        }
    }

    while (found == 0) {
        found = 1;
        for (itr = 0; itr < array_len; itr++) {
            if ((interval_array[itr] % temp_time_interval) != 0) {
                temp_time_interval = temp_time_interval - MONITOR_RUNNING_INTERVAL_IN_MILLISEC;
                if (temp_time_interval <= 0) {
                    wifi_util_error_print(WIFI_MON, "%s %d invalid time interval : %d\n",__func__,__LINE__, temp_time_interval);
                    return result;
                }
                found = 0;
                break;
            }
        }
    }
    result = temp_time_interval;

    return result;
}

void find_max_execution_time(unsigned long *max_execution_time, unsigned int repetitions, unsigned int interval_ms)
{
    unsigned long app_execution_time = 0;

    if (repetitions != 0) {
        app_execution_time = interval_ms * repetitions;

        //Find max execution time
        if (app_execution_time > *max_execution_time) {
            *max_execution_time = app_execution_time;
        }
    }

    return;
}

int dca_find_task_new_interval_rep_for_new_req(wifi_dca_element_t *dca_task_element, wifi_config_data_collection_t *dca, unsigned long *new_interval, unsigned int *new_rep)
{
    wifi_dca_app_element_t *dca_app_element = NULL;
    unsigned long temp_new_interval = 0;
    unsigned long *interval_array = NULL;
    unsigned int itr = 0;
    unsigned long max_app_execution_time = 0;
    unsigned int count =0;
    bool new_element = true;

    if (dca_task_element->app_list == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: APP list is NULL\n", __func__,__LINE__);
        return RETURN_ERR;
    }

    count =  hash_map_count(dca_task_element->app_list);

    interval_array = (unsigned long *)malloc((count+1) * sizeof(unsigned long));
    if (interval_array == NULL) {
        wifi_util_error_print(WIFI_MON, "%s %d malloc failed\n",__func__,__LINE__);
        return RETURN_ERR;
    }

    //Traverse through the dca_elements
    dca_app_element = hash_map_get_first(dca_task_element->app_list);
    while(dca_app_element != NULL) {
        if (dca_app_element->inst == dca->inst) {
            new_element = false;
            find_max_execution_time(&max_app_execution_time, dca->repetitions, dca->interval_ms);
            interval_array[itr] = dca->interval_ms;
        } else {
            find_max_execution_time(&max_app_execution_time, dca_app_element->req_repetitions, dca_app_element->req_interval_ms);

            interval_array[itr] = dca_app_element->req_interval_ms;
        }
        itr++;

        dca_app_element = hash_map_get_next(dca_task_element->app_list, dca_app_element);
    }


    if(new_element == true) {
        new_element = false;
        find_max_execution_time(&max_app_execution_time, dca->repetitions, dca->interval_ms);
        interval_array[itr] = dca->interval_ms;
        itr++;
    }

    temp_new_interval = find_interval_common_divisor(interval_array, itr);
    if (temp_new_interval == 0) {
        wifi_util_error_print(WIFI_MON, "%s %d invalid interval : %d\n",__func__,__LINE__, temp_new_interval);
        free(interval_array);
        return RETURN_ERR;
    }

    *new_interval = temp_new_interval;
    if (max_app_execution_time != 0) {
        *new_rep = max_app_execution_time/temp_new_interval;
    }

    free(interval_array);
    return RETURN_OK;
}

void dca_task_scheduler_update(wifi_dca_element_t * dca_task_element, unsigned long new_task_interval, unsigned int new_task_rep)
{
    wifi_monitor_t *monitor_param = (wifi_monitor_t *)get_wifi_monitor();
    BOOL is_interval_changed = FALSE;

    //compare the Intervals, update current task intervals
    if (new_task_interval != dca_task_element->curr_interval_ms) {
        is_interval_changed = TRUE;
    }

    if (is_interval_changed == TRUE) {
        adjust_app_control_counters(dca_task_element, new_task_interval, new_task_rep);

        dca_task_element->curr_interval_ms = new_task_interval;

        wifi_util_error_print(WIFI_MON, "%s %d dca_task_element->key : %s dca_task_element->curr_interval_ms : %d\n",__func__,__LINE__,  dca_task_element->key, dca_task_element->curr_interval_ms);
        scheduler_update_timer_task_interval(monitor_param->sched, dca_task_element->task_sched_id, new_task_interval);
    }

    //compare the Repetitions, update the current task repetitions
    if (new_task_rep != dca_task_element->curr_repetitions) {
        dca_task_element->curr_repetitions = new_task_rep;
        wifi_util_error_print(WIFI_MON, "%s %d dca_task_element->key : %s dca_task_element->curr_repetitions : %d\n",__func__,__LINE__,  dca_task_element->key, dca_task_element->curr_repetitions);
        scheduler_update_timer_task_repetitions(monitor_param->sched, dca_task_element->task_sched_id, new_task_rep);
    }
}


int dca_response_send_to_ctrl_queue(wifi_dca_response_t *response, wifi_dca_callback_arg_t *cb_args,  wifi_event_route_t *route)
{
    wifi_dca_element_t *dca_element = NULL;
    hash_map_t *app_list = NULL;

    dca_element = (wifi_dca_element_t *)cb_args->task_info;
    app_list = dca_element->app_list;
    if (app_list == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: APP list is NULL\n", __func__,__LINE__);
        return RETURN_ERR;
    }

    wifi_util_dbg_print(WIFI_MON,"%s:%d: Sending dca response for : %s with route : 0x%x to  ctrl queue\n", __func__,__LINE__, dca_element->key, route->u.inst_bit_map);
    push_monitor_event_to_ctrl_queue(response, sizeof(wifi_dca_response_t), wifi_event_type_monitor, wifi_event_monitor_data_collection_response, route);

    return RETURN_ERR;
}

int app_execution_event_check(wifi_dca_callback_arg_t *cb_arg, bool *send_event, wifi_event_route_t *route)
{
    wifi_dca_element_t *dca_element = NULL;
    dca_element = (wifi_dca_element_t *)cb_arg->task_info;
    wifi_dca_app_element_t *app_element = NULL;
    hash_map_t *app_list = NULL;
    app_list = dca_element->app_list;
    bool is_app_deleted = false;
    if (app_list == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: APP list is NULL\n", __func__,__LINE__);
        return RETURN_ERR;
    }
    memset(route, 0, sizeof(wifi_event_route_t));

    app_element = hash_map_get_first(app_list);
    while (app_element != NULL) {
        app_element->execution_counter++;
        if (app_element->execution_counter >= app_element->counter_limit) {
            *send_event = true;
            route->u.inst_bit_map |= app_element->inst;
            route->dst = wifi_sub_component_apps;
            app_element->execution_counter = 0;
        }
        if ( app_element->req_repetitions != 0) {
            if (app_element->app_execution_counter >= app_element->req_repetitions) {
                hash_map_remove(app_list, app_element->key);
                wifi_util_dbg_print(WIFI_MON, "%s:%d: Removing the app with key : %s\n", __func__,__LINE__, app_element->key);
                is_app_deleted = true;
            }
            app_element->app_execution_counter++;
        }
        app_element = hash_map_get_next(app_list, app_element);
    }

    if (is_app_deleted == true) {
        dca_task_scheduler_update_after_app_remove(&dca_element);
    }

    return RETURN_OK;
}


// scheduler callback function
int send_dca_radio_channel_statistics(void *arg)
{
    wifi_channelStats_t chan_stats[MAX_CHANNELS];
    wifi_dca_response_t *response;
    int ret = RETURN_OK;
    int chan_count = 0;
    bool rfc_status;
    memset(chan_stats, 0, sizeof(wifi_channelStats_t) * MAX_CHANNELS);
    wifi_dca_callback_arg_t *cb_arg = NULL;
    wifi_event_route_t route;
    bool send_event= false;

    cb_arg = (wifi_dca_callback_arg_t *)arg;
    if (cb_arg == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL wifi_dca_callback_arg_t pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    app_execution_event_check(cb_arg, &send_event, &route);

    if (send_event == false) {
        return RETURN_OK;
    }

    for (chan_count = 0; chan_count < cb_arg->args.channel_list.num_channels; chan_count++) {
        chan_stats[chan_count].ch_number = cb_arg->args.channel_list.channels_list[chan_count];
        chan_stats[chan_count].ch_in_pool= TRUE;
    }

    if (chan_count == 0) {
        wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(cb_arg->args.radio_index);
        if (radioOperation == NULL) {
            wifi_util_error_print(WIFI_MON,"%s:%d NULL radioOperation pointer\n", __func__, __LINE__);
            return RETURN_ERR;
        }
        chan_stats[chan_count].ch_number = radioOperation->channel;
        chan_stats[chan_count].ch_in_pool= TRUE;
        chan_count++;
    }

    get_wifi_rfc_parameters(RFC_WIFI_OW_CORE_THREAD, (bool *)&rfc_status);
    if (true == rfc_status) {
#ifdef HALW_IMPLEMENTATION
        struct wifi_halw_if_info if_info;
        memset(&if_info, 0, sizeof(struct wifi_halw_if_info));
        if_info.rix = cb_arg->args.radio_index;

        // call hal api to get data
        ret = wifi_halw_getRadioChannelStats(&if_info, cb_arg->args.scan_mode, chan_stats, chan_count);
#endif
    } else {
        wifi_util_dbg_print(WIFI_MON,"%s:%d WIFI OW CORE THREAD DISABLED\n", __func__, __LINE__);
        ret = wifi_getRadioChannelStats(cb_arg->args.radio_index, chan_stats, chan_count);
    }

    if (ret != 0) {
        wifi_util_error_print(WIFI_MON, "%s : %d  Failed to get radio channel statistics\n",__func__,__LINE__);
        return RETURN_ERR;
    }

    response = (wifi_dca_response_t *)calloc(1, sizeof(wifi_dca_response_t));
    if (response == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: Memory allocation failed for wifi_dca_response_t\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }

    // put all collected data to response structure
    memcpy(&response->args, &cb_arg->args, sizeof(wifi_dca_args_t));
    memcpy(&response->u.chan_stats, chan_stats, sizeof(wifi_channelStats_t) * chan_count);
    response->data_type = dca_radio_channel_stats;
    response->stat_array_size = chan_count;

    ret = dca_response_send_to_ctrl_queue(response, cb_arg, &route);
    free(response);

    return ret;
}

// scheduler callback function
int send_dca_neighbor_stats(void *arg)
{
    wifi_neighbor_ap2_t *neigh_stats = NULL;
    wifi_dca_response_t *response = NULL;
    int ret = RETURN_OK;
    bool rfc_status;
    unsigned int output_array_size = 0;
    wifi_dca_callback_arg_t *cb_arg = NULL;
    wifi_event_route_t route;
    bool send_event= false;

    cb_arg = (wifi_dca_callback_arg_t *)arg;
    if (cb_arg == NULL) {
        wifi_util_error_print(WIFI_MON,"%s : %d NULL wifi_dca_callback_arg_t pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    app_execution_event_check(cb_arg, &send_event, &route);

    if (send_event == false) {
        return RETURN_OK;
    }

    get_wifi_rfc_parameters(RFC_WIFI_OW_CORE_THREAD, (bool *)&rfc_status);
    if (true == rfc_status) {
#ifdef HALW_IMPLEMENTATION
        struct wifi_halw_if_info if_info;
        memset(&if_info, 0, sizeof(struct wifi_halw_if_info));
        if_info.rix = cb_arg->args.radio_index;
#endif
    }
    if (true == rfc_status) {
#ifdef HALW_IMPLEMENTATION
        wifi_halw_startScan(if_info, cb_arg->args.scan_mode, cb_arg->args.dwell_time, 1, &cb_arg->args.channel_list.channels_list[i]);
        // call hal api to get data
        ret = wifi_halw_getNeighborStats(if_info, cb_arg->args.scan_mode, &neigh_stats, &output_array_size);
#endif
    } else {
        wifi_util_dbg_print(WIFI_MON,"%s:%d WIFI OW CORE THREAD DISABLED\n", __func__, __LINE__);
        wifi_startNeighborScan(cb_arg->args.radio_index, cb_arg->args.scan_mode, cb_arg->args.dwell_time, cb_arg->args.channel_list.num_channels, (unsigned int *)cb_arg->args.channel_list.channels_list);
        ret = wifi_getNeighboringWiFiStatus(cb_arg->args.radio_index, &neigh_stats, &output_array_size);
    }

    if (ret != 0) {
        wifi_util_error_print(WIFI_MON, "%s : %d  Failed to get neighbor statistics\n",__func__,__LINE__);
        return RETURN_ERR;
    }
    response = (wifi_dca_response_t *)calloc(1, sizeof(wifi_dca_response_t));
    if (response == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: Memory allocation failed for wifi_dca_response_t\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }
    // put all collected data to response structure
    memcpy(&response->args, &cb_arg->args, sizeof(wifi_dca_args_t));
    memcpy(&response->u.neigh_ap, neigh_stats, sizeof(wifi_neighbor_ap2_t) * output_array_size);
    response->data_type = dca_neighbor_stats;
    response->stat_array_size = output_array_size;

    ret = dca_response_send_to_ctrl_queue(response, cb_arg, &route);
    free(neigh_stats);
    free(response);
    neigh_stats = NULL;
    response = NULL;

    return ret;
}

// scheduler callback function
int send_dca_associated_device_stats(void *arg)
{
    wifi_associated_dev3_t *dev_diagnostic = NULL;
    wifi_associated_dev_stats_t *dev_stats = NULL;
    wifi_dca_response_t *response = NULL;
    int ret = RETURN_OK;
    bool rfc_status;
    unsigned int num_devs = 0;
    uint64_t handle = 0;
    wifi_dca_callback_arg_t *cb_arg = NULL;
    wifi_event_route_t route;
    bool send_event= false;

    cb_arg = (wifi_dca_callback_arg_t *)arg;
    if (cb_arg == NULL) {
        wifi_util_error_print(WIFI_MON,"%s : %d NULL wifi_dca_callback_arg_t pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    app_execution_event_check(cb_arg, &send_event, &route);

    if (send_event == false) {
        return RETURN_OK;
    }

    get_wifi_rfc_parameters(RFC_WIFI_OW_CORE_THREAD, (bool *)&rfc_status);
    if (true == rfc_status) {
#ifdef HALW_IMPLEMENTATION
        struct wifi_halw_if_info if_info;
        memset(&if_info, 0, sizeof(struct wifi_halw_if_info));
        if_info.rix = cb_arg->args.radio_index;

        // call hal api to get data
        // TODO: to be implemented
#endif
    } else {
        wifi_util_dbg_print(WIFI_MON,"%s:%d WIFI OW CORE THREAD DISABLED\n", __func__, __LINE__);
        ret = wifi_getApAssociatedDeviceDiagnosticResult3(cb_arg->args.vap_index, &dev_diagnostic, &num_devs);
        if (ret != RETURN_OK) {
            wifi_util_error_print(WIFI_MON, "%s : %d  Failed to get associated device diagnostic\n",__func__,__LINE__);
            return RETURN_ERR;
        }

        dev_stats = (wifi_associated_dev_stats_t *)calloc(num_devs, sizeof(wifi_associated_dev_stats_t));
        if (dev_stats == NULL) {
            wifi_util_error_print(WIFI_MON, "%s:%d: Memory allocation failed for wifi_associated_dev_stats_t\n", __FUNCTION__, __LINE__);
            return RETURN_ERR;
        }

        for (unsigned int i = 0; i < num_devs; i++) {
            ret = wifi_getApAssociatedDeviceStats(cb_arg->args.vap_index, &dev_diagnostic[i].cli_MACAddress, &dev_stats[i], &handle);
            if (ret != RETURN_OK) {
                wifi_util_error_print(WIFI_MON, "%s : %d  Failed to get associated device statistics\n",__func__,__LINE__);
                free(dev_stats);
                free(dev_diagnostic);
                return RETURN_ERR;
            }
        }
    }

    response = (wifi_dca_response_t *)calloc(1, sizeof(wifi_dca_response_t));
    if (response == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: Memory allocation failed for wifi_dca_response_t\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }

    // put all collected data to response structure
    memcpy(&response->args, &cb_arg->args, sizeof(wifi_dca_args_t));
    memcpy(&response->u.assoc_dev_stats, dev_stats, sizeof(wifi_associated_dev3_t) * num_devs);
    response->data_type = dca_associated_device_stats;
    response->stat_array_size = num_devs;

    ret = dca_response_send_to_ctrl_queue(response, cb_arg, &route);
    free(response);
    free(dev_stats);
    free(dev_diagnostic);

    return ret;
}

// scheduler callback function
int send_dca_associated_device_diagnostic(void *arg)
{
    wifi_associated_dev3_t *dev_diagnostic = NULL;
    wifi_dca_response_t *response = NULL;
    int ret = RETURN_OK;
    bool rfc_status;
    unsigned int num_devs = 0;
    wifi_dca_callback_arg_t *cb_arg = NULL;
    wifi_event_route_t route;
    bool send_event= false;

    cb_arg = (wifi_dca_callback_arg_t *)arg;
    if (cb_arg == NULL) {
        wifi_util_error_print(WIFI_MON,"%s : %d NULL wifi_dca_callback_arg_t pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    app_execution_event_check(cb_arg, &send_event, &route);

    if (send_event == false) {
        return RETURN_OK;
    }

    get_wifi_rfc_parameters(RFC_WIFI_OW_CORE_THREAD, (bool *)&rfc_status);
    if (true == rfc_status) {
#ifdef HALW_IMPLEMENTATION
        struct wifi_halw_if_info if_info;
        memset(&if_info, 0, sizeof(struct wifi_halw_if_info));
        if_info.rix = cb_arg->args.radio_index;

        // call hal api to get data
        ret = wifi_halw_getAssociatedDeviceStats(&if_info, &dev_diagnostic, &num_devs);
#endif
    } else {
        wifi_util_dbg_print(WIFI_MON,"%s:%d WIFI OW CORE THREAD DISABLED\n", __func__, __LINE__);
        ret = wifi_getApAssociatedDeviceDiagnosticResult3(cb_arg->args.vap_index, &dev_diagnostic, &num_devs);
    }

    if (ret != 0) {
        wifi_util_error_print(WIFI_MON, "%s : %d  Failed to get associated device statistics\n",__func__,__LINE__);
        return RETURN_ERR;
    }

    response = (wifi_dca_response_t *)calloc(1, sizeof(wifi_dca_response_t));
    if (response == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: Memory allocation failed for wifi_dca_response_t\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }

    // put all collected data to response structure
    memcpy(&response->args, &cb_arg->args, sizeof(wifi_dca_args_t));
    memcpy(&response->u.assoc_dev_diag, dev_diagnostic, sizeof(wifi_associated_dev3_t) * num_devs);
    response->data_type = dca_associated_device_diag;
    response->stat_array_size = num_devs;

    ret = dca_response_send_to_ctrl_queue(response, cb_arg, &route);
    free(response);
    free(dev_diagnostic);

    return ret;
}

static void convert_dca_request_type_to_string(wifi_monitor_dca_t type, char *type_str, size_t len)
{
    memset(type_str, 0, len);
    snprintf(type_str, len - 1, "%d", type);
}

static void convert_channels_to_string(const wifi_channels_list_t *chan_list, char *buff, size_t len)
{
    char chan_buf[32];
    int i;

    if (chan_list->num_channels > 0) {
        strcat_s(buff, len - 1, "_");
        memset(chan_buf, 0, len);
        for (i = 0; i < chan_list->num_channels - 1; i++) {
            snprintf(chan_buf, sizeof(chan_buf) - 1, "%d,", chan_list->channels_list[i]);
            strcat_s(buff, len - 1, chan_buf);
        }
        snprintf(chan_buf, sizeof(chan_buf) - 1, "%d", chan_list->channels_list[i]);
        strcat_s(buff, len - 1, chan_buf);
    }
}

int generate_dca_element_key(wifi_monitor_dca_t data_type, wifi_dca_args_t *args, char *key_str, size_t len)
{
    char type_str[32];

    convert_dca_request_type_to_string(data_type, type_str, sizeof(type_str));
    memset(key_str, 0, len);

    switch (data_type) {
        case dca_radio_channel_stats:
            snprintf(key_str, len - 1, "%s_%02d_%d", type_str, args->radio_index, args->scan_mode);
            convert_channels_to_string(&args->channel_list, key_str, len);
        break;
        case dca_neighbor_stats:
            snprintf(key_str, len - 1, "%s_%02d_%d", type_str, args->radio_index, args->scan_mode);
            convert_channels_to_string(&args->channel_list, key_str, len);
        break;
        case dca_associated_device_diag:
            snprintf(key_str, len - 1, "%s_%02d", type_str, args->vap_index);
        break;
        case dca_associated_device_stats:
            snprintf(key_str, len - 1, "%s_%02d", type_str, args->vap_index);
        break;
        case dca_radio_traffic_stats:
            snprintf(key_str, len - 1, "%s_%02d", type_str, args->radio_index);
        break;
        default:
            wifi_util_error_print(WIFI_MON, "%s : %d Data type %d is not supported.\n",__func__,__LINE__, data_type);
            return RETURN_ERR;
    }
    return RETURN_OK;
}

static void generate_dca_app_element_key(wifi_app_inst_t inst, char *inst_str, size_t len)
{
    memset(inst_str, 0, len);
    snprintf(inst_str, len - 1, "ow_app_%d", inst);
}

int start_new_dca_task(wifi_config_data_collection_t *dca, int *task_sched_id, wifi_dca_callback_arg_t *task_args)
{
    switch (dca->data_type) {
        case dca_radio_channel_stats:
            scheduler_add_timer_task(g_monitor_module.sched, dca->task_priority, task_sched_id, send_dca_radio_channel_statistics,
                (void *)task_args, dca->interval_ms, dca->repetitions);
        break;
        case dca_neighbor_stats:
            scheduler_add_timer_task(g_monitor_module.sched, dca->task_priority, task_sched_id, send_dca_neighbor_stats,
                    (void *)task_args, dca->interval_ms, dca->repetitions);
        break;
        case dca_associated_device_diag:
            scheduler_add_timer_task(g_monitor_module.sched, dca->task_priority, task_sched_id, send_dca_associated_device_diagnostic,
                    (void *)task_args, dca->interval_ms, dca->repetitions);
        break;
        case dca_associated_device_stats:
            scheduler_add_timer_task(g_monitor_module.sched, dca->task_priority, task_sched_id, send_dca_associated_device_stats,
                (void *)task_args, dca->interval_ms, dca->repetitions);
        break;
        default:
            wifi_util_error_print(WIFI_MON, "%s : %d Data type %d is not supported.\n",__func__,__LINE__, dca->data_type);
            return RETURN_ERR;
    }
    return RETURN_OK;
}

wifi_dca_app_element_t *create_and_update_dca_app_element(wifi_config_data_collection_t *dca)
{
    wifi_dca_app_element_t *dca_app_element = NULL;

    dca_app_element = (wifi_dca_app_element_t *)calloc(1, sizeof(wifi_dca_app_element_t));
    if (dca_app_element == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: Memory allocation failed for wifi_dca_app_element_t\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    generate_dca_app_element_key(dca->inst, dca_app_element->key, DCA_APP_KEY_LEN);
    dca_app_element->inst = dca->inst;
    dca_app_element->req_interval_ms = dca->interval_ms;
    dca_app_element->req_repetitions = dca->repetitions;
    dca_app_element->execution_counter = 0;
    dca_app_element->app_execution_counter = 0;
    dca_app_element->counter_limit = (dca_app_element->req_interval_ms / dca->interval_ms);

    return dca_app_element;
}

int create_and_update_new_dca_task_element(wifi_dca_element_t **dca_task, wifi_config_data_collection_t *dca, char *dca_key, int key_len)
{
    wifi_dca_callback_arg_t *task_args = NULL;
    hash_map_t *dca_list = NULL;
    wifi_dca_app_element_t *dca_app_element = NULL;

    dca_list = g_monitor_module.dca_list;

    // create and update task element
    *dca_task = (wifi_dca_element_t *)calloc(1, sizeof(wifi_dca_element_t));
    if (*dca_task == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: Memory allocation failed for wifi_dca_element_t\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }
    memcpy(&(*dca_task)->key, dca_key, key_len);
    (*dca_task)->curr_interval_ms = dca->interval_ms;
    (*dca_task)->curr_repetitions = dca->repetitions;

    // create and update app element
    dca_app_element = create_and_update_dca_app_element(dca);
    if (dca_app_element == NULL) {
        free(*dca_task);
        return RETURN_ERR;
    }

    (*dca_task)->app_list = hash_map_create();
    hash_map_put((*dca_task)->app_list, strdup(dca_app_element->key), dca_app_element);

    // allocate and update task args
    task_args = (wifi_dca_callback_arg_t *)calloc(1, sizeof(wifi_dca_callback_arg_t));
    memcpy(&task_args->args, &dca->args, sizeof(wifi_dca_args_t));
    task_args->task_info = (void *)(*dca_task);
    (*dca_task)->task_args = task_args;

    // start new task scheduler
    start_new_dca_task(dca, &(*dca_task)->task_sched_id, task_args);
    // add new task element to dca_list
    hash_map_put(dca_list, strdup((*dca_task)->key), (*dca_task));

    return RETURN_OK;
}


int dca_task_scheduler_update_after_app_remove(wifi_dca_element_t **dca_task_element)
{
    hash_map_t *dca_app_list = NULL;
    hash_map_t *dca_list = NULL;
    unsigned int new_task_rep = 0;
    unsigned long new_task_interval = 0;
    wifi_monitor_t *monitor_param = (wifi_monitor_t *)get_wifi_monitor();
    wifi_dca_app_element_t *dca_app_element= NULL;
    unsigned int count = 0;
    unsigned int itr = 0;
    unsigned long max_app_execution_time = 0;
    unsigned long *interval_array = NULL;

    dca_list = monitor_param->dca_list;
    dca_app_list = (*dca_task_element)->app_list;

    //Now check the count
    if (hash_map_count(dca_app_list) == 0) {
        wifi_util_dbg_print(WIFI_MON, "%s:%d: App list is empty. Removing the task\n", __func__,__LINE__);
        scheduler_cancel_timer_task(monitor_param->sched, (*dca_task_element)->task_sched_id);
        free((*dca_task_element)->task_args);
        free(dca_app_list);
        hash_map_remove(dca_list, (*dca_task_element)->key);
        free(*dca_task_element);
    } else {
        //Calculate the interval for all the apps
        count =  hash_map_count((*dca_task_element)->app_list);
        interval_array = (unsigned long *)malloc((count) * sizeof(unsigned long));
        if (interval_array == NULL) {
            wifi_util_error_print(WIFI_MON, "%s %d malloc failed\n",__func__,__LINE__);
            return RETURN_ERR;
        }

        dca_app_element = hash_map_get_first((*dca_task_element)->app_list);
        while(dca_app_element != NULL) {
            find_max_execution_time(&max_app_execution_time, dca_app_element->req_repetitions, dca_app_element->req_interval_ms);
            interval_array[itr] = dca_app_element->req_interval_ms;
            itr++;
            dca_app_element = hash_map_get_next((*dca_task_element)->app_list, dca_app_element);
        }

        new_task_interval = find_interval_common_divisor(interval_array, itr);
        if (new_task_interval == 0) {
            wifi_util_error_print(WIFI_MON, "%s %d invalid interval : %d\n",__func__,__LINE__, new_task_interval);
            free(interval_array);
            return RETURN_ERR;
        }

        if (max_app_execution_time != 0) {
            new_task_rep = max_app_execution_time/new_task_interval;
        }
        dca_task_scheduler_update(*dca_task_element, new_task_interval, new_task_rep);
        free(interval_array);
    }

    return RETURN_OK;
}

int remove_dca_app_element_from_list(wifi_dca_element_t **dca_task_element, wifi_config_data_collection_t *dca)
{
    hash_map_t *dca_app_list = NULL;
    char dca_element_key[DCA_APP_KEY_LEN];
    wifi_dca_app_element_t *dca_app_element= NULL;

    dca_app_list = (*dca_task_element)->app_list;

    memset(dca_element_key, 0, sizeof(dca_element_key));
    generate_dca_app_element_key(dca->inst, dca_element_key, DCA_APP_KEY_LEN);

    //check if the app  is present
    dca_app_element = (wifi_dca_app_element_t *)hash_map_get(dca_app_list, dca_element_key);
    if (dca_app_element == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: App key : %s not present in app_list to remove\n", __func__,__LINE__, dca_element_key);
        return RETURN_ERR;
    }

    //Remove the app
    hash_map_remove(dca_app_list, dca_element_key);
    wifi_util_dbg_print(WIFI_MON, "%s:%d: Removing the app with key : %s\n", __func__,__LINE__, dca_element_key);

    dca_task_scheduler_update_after_app_remove(dca_task_element);

    return RETURN_OK;
}


void dca_update_to_app(wifi_dca_element_t *dca_task_element, wifi_config_data_collection_t *dca)
{
    char app_key[DCA_APP_KEY_LEN];
    wifi_dca_app_element_t *app_element = NULL;

    memset(app_key, 0, sizeof(app_key));
    generate_dca_app_element_key(dca->inst, app_key, DCA_APP_KEY_LEN);

    app_element = (wifi_dca_app_element_t *)hash_map_get(dca_task_element->app_list, app_key);
    if (app_element == NULL) {
        //Create and update the task element
        app_element = create_and_update_dca_app_element(dca);
        if (app_element == NULL) {
            wifi_util_error_print(WIFI_MON, "%s:%d: unable to update the app element\n", __func__,__LINE__);
            return;
        }
        //update app_element to the task_list
        hash_map_put(dca_task_element->app_list, strdup(app_element->key), app_element);

    } else { //app element is present
        app_element->req_repetitions = dca->repetitions;
        app_element->req_interval_ms = dca->interval_ms;
    }

    return;
}


int process_stats_data_collection_request(wifi_config_data_collection_t *dca)
{
    wifi_monitor_t *monitor_param = (wifi_monitor_t *)get_wifi_monitor();
    hash_map_t *dca_list = NULL;
    wifi_dca_element_t *dca_task_element = NULL;
    char dca_key[DCA_KEY_LEN];
    int ret = RETURN_ERR;
    unsigned int new_task_rep = 0;
    unsigned long new_task_interval = 0;

    if (dca == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: input dca is NULL\n", __func__,__LINE__);
        return RETURN_ERR;
    }

    if (dca->interval_ms == 0) {
        wifi_util_error_print(WIFI_MON, "%s:%d: interval_ms is %d\n", __func__,__LINE__, dca->interval_ms);
        return RETURN_ERR;
    }

    //Check for the incoming interval is valid
    if ((dca->interval_ms % MONITOR_RUNNING_INTERVAL_IN_MILLISEC) != 0) {
        wifi_util_error_print(WIFI_MON, "%s:%d: interval %d is not multiple of monitor interval : %d\n", __func__,__LINE__, dca->interval_ms, MONITOR_RUNNING_INTERVAL_IN_MILLISEC);
        return RETURN_ERR;
    }

    if (generate_dca_element_key(dca->data_type, &dca->args, dca_key, DCA_KEY_LEN) != 0) {
        return RETURN_ERR;
    }

    dca_list = monitor_param->dca_list;
    dca_task_element = (wifi_dca_element_t *)hash_map_get(dca_list, dca_key);
    if (dca_task_element == NULL) {
        if (dca->req_state == dca_request_state_start) {
            ret = create_and_update_new_dca_task_element(&dca_task_element, dca, dca_key, sizeof(dca_key));
            return ret;
        } else {
            wifi_util_error_print(WIFI_MON, "%s:%d: Task is not running. Request state %d is not expected\n", __func__,__LINE__, dca->req_state);
            return RETURN_ERR;
        }
    } else {
        if (dca->req_state == dca_request_state_start) {

            if (dca_find_task_new_interval_rep_for_new_req(dca_task_element, dca, &new_task_interval, &new_task_rep) == RETURN_ERR) {
                wifi_util_error_print(WIFI_MON, "%s:%d: dca_find_task_new_interval_rep_for_new_req failed\n", __func__,__LINE__);
                return RETURN_ERR;
            }

            //update the received dca request to app
            dca_update_to_app(dca_task_element, dca);

            //updates the new interval,rep to task and update counters for all the apps
            dca_task_scheduler_update(dca_task_element, new_task_interval, new_task_rep);

        } else {
            remove_dca_app_element_from_list(&dca_task_element, dca);
        }
    }
    return RETURN_OK;
}

#endif // CCSP_COMMON
