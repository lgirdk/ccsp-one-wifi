##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

ACLOCAL_AMFLAGS = -I m4

noinst_LTLIBRARIES = libCcspWifiAgent_ml.la

libCcspWifiAgent_ml_la_CPPFLAGS = -I$(top_srcdir)/source/dml/dml_webconfig \
                                  -I$(top_srcdir)/source/dml/tr_181/ml \
                                  -I$(top_srcdir)/source/dml/tr_181/sbapi \
                                  -I$(top_srcdir)/include/tr_181/ml \
                                  -I$(top_srcdir)/source/dml/wifi_ssp \
                                  -I$(top_srcdir)/include/wifi_ssp \
                                  -I$(top_srcdir)/include \
                                  -I$(top_srcdir)/source/core \
                                  -I$(top_srcdir)/source/stats \
                                  -I$(top_srcdir)/source/db \
                                  -I$(top_srcdir)/source/utils \
                                  -I$(top_srcdir)/lib/ovsdb \
                                  -I$(top_srcdir)/lib/inc \
                                  -I$(top_srcdir)/lib/json_util \
                                  -I$(top_srcdir)/lib/ds \
                                  -I$(top_srcdir)/lib/common \
                                  -I$(top_srcdir)/lib/pjs \
                                  -I$(top_srcdir)/lib/log \
                                  -I$(top_srcdir)/lib/const \
                                  -I$(top_srcdir)/lib/schema \
                                  -I$(top_srcdir)/lib/osp \
                                  -I$(top_srcdir)/lib/osa \
                                  -I$(top_srcdir)/lib/psfs \
                                  -I$(top_srcdir)/lib/qm \
                                  -I$(top_srcdir)/source/core/services \
                                  -I$(top_srcdir)/source/apps \
                                  -I$(top_srcdir)/source/apps/analytics \
                                  -I$(top_srcdir)/source/apps/levl \
                                  -I$(top_srcdir)/source/apps/csi \
                                  -I$(top_srcdir)/source/apps/motion \
                                  -I$(top_srcdir)/source/apps/cac \
                                  -I$(top_srcdir)/source/apps/sm \
                                  -I$(top_srcdir)/source/apps/whix \
                                  -I$(top_srcdir)/source/apps/harvester \
                                  -I$(top_srcdir)/source/apps/blaster \
                                  -I$(top_srcdir)/source/apps/ocs \
                                  -I$(top_srcdir)/source/platform/rdkb \
                                  -I$(top_srcdir)/source/platform/common \
                                  -I$(top_srcdir)/source/ccsp \
                                  -I$(top_srcdir)/source/dml/rdkb

libCcspWifiAgent_ml_la_SOURCES = plugin_main.c cosa_apis_util.c plugin_main_apis.c cosa_wifi_dml.c cosa_apis_busutil.c cosa_wifi_internal.c cosa_logging_internal.c cosa_logging_dml.c cosa_harvester_dml.c 
libCcspWifiAgent_ml_la_LDFLAGS = -lccsp_common -lsecure_wrapper 
if !HAL_IPC
libCcspWifiAgent_ml_la_LDFLAGS += -lhal_wifi
endif
