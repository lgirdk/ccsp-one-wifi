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
#include "he_bus_utils.h"
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static char *get_formatted_time(char *time)
{
    struct tm *tm_info;
    struct timeval tv_now;
    char tmp[128];

    gettimeofday(&tv_now, NULL);
    tm_info = (struct tm *)localtime(&tv_now.tv_sec);

    strftime(tmp, 128, "%y%m%d-%T", tm_info);

    snprintf(time, 256, "%s.%06lld", tmp, (long long)tv_now.tv_usec);
    return time;
}

void he_bus_print(he_bus_log_type_t level, he_bus_log_type_t module, char *format, ...)
{
    char buff[256] = { 0 };
    va_list list;
    FILE *fpg = NULL;
#if defined(__ENABLE_PID_INFO__) && (__ENABLE_PID_INFO__)
    pid_t pid;
#endif
    extern char *__progname;
    char filename_dbg_enable[64];
    char module_filename[32];
    char filename[100];

    switch (module) {
    case he_bus_core: {
        snprintf(filename_dbg_enable, sizeof(filename_dbg_enable),
            HE_BUS_LOG_PATH_PREFIX "bus_core_log");
        snprintf(module_filename, sizeof(module_filename), "bus_core_log.txt");
        break;
    }
    case he_bus_connector: {
        snprintf(filename_dbg_enable, sizeof(filename_dbg_enable),
            HE_BUS_LOG_PATH_PREFIX "bus_connector_log");
        snprintf(module_filename, sizeof(module_filename), "bus_connector_log.txt");
        break;
    }
    default:
        return;
    }

    if ((access(filename_dbg_enable, R_OK)) == 0) {
        snprintf(filename, sizeof(filename), "/tmp/%s", module_filename);
        fpg = fopen(filename, "a+");
        if (fpg == NULL) {
            return;
        }
    } else {
        switch (level) {
        case he_bus_log_lvl_info:
        case he_bus_log_lvl_error:
            snprintf(filename, sizeof(filename), "/rdklogs/logs/%s", module_filename);
            fpg = fopen(filename, "a+");
            if (fpg == NULL) {
                return;
            }
            break;
        case he_bus_log_lvl_debug:
        default:
            return;
        }
    }

#if defined(__ENABLE_PID_INFO__) && (__ENABLE_PID_INFO__)
    pid = syscall(__NR_gettid);
    sprintf(&buff[0], "%d - ", pid);
    get_formatted_time(&buff[strlen(buff)]);
#else
    snprintf(&buff[0], sizeof(buff), "[%s] ", __progname ? __progname : "");
    get_formatted_time(&buff[strlen(buff)]);
#endif

    static const char *level_marker[he_bus_log_lvl_max] = {
        [he_bus_log_lvl_debug] = "<D>",
        [he_bus_log_lvl_info] = "<I>",
        [he_bus_log_lvl_error] = "<E>",
    };
    if ((he_bus_log_level_t)level < he_bus_log_lvl_max) {
        snprintf(&buff[strlen(buff)], 256 - strlen(buff), "%s ", level_marker[level]);
    }

    fprintf(fpg, "%s ", buff);

    va_start(list, format);
    vfprintf(fpg, format, list);
    va_end(list);

    fflush(fpg);
    fclose(fpg);
}
