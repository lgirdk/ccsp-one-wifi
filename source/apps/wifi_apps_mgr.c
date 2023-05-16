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

wifi_app_t *get_app_by_inst(wifi_apps_mgr_t *apps_mgr, wifi_app_inst_t inst)
{
    char key_str[32];
    wifi_app_t *node = NULL;

    snprintf(key_str, sizeof(key_str), "onewifi_app_%d", inst);
    node = (wifi_app_t *)hash_map_get(apps_mgr->apps_map, key_str);

    return node;
}

void *app_detached_event_func(void *data)
{
    int rc;
    wifi_event_t *clone;
    wifi_app_t *app = (wifi_app_t *)data;
    struct timespec time_to_wait;
    struct timeval tv_now;
    time_t  time_diff;

    pthread_mutex_lock(&app->lock);
    while (app->exit_app == false) {
        gettimeofday(&tv_now, NULL);
        time_to_wait.tv_nsec = 0;
        time_to_wait.tv_sec = tv_now.tv_sec + app->poll_period;

        if (app->last_signalled_time.tv_sec > app->last_polled_time.tv_sec) {
            time_diff = app->last_signalled_time.tv_sec - app->last_polled_time.tv_sec;
            if ((UINT)time_diff < app->poll_period) {
                time_to_wait.tv_sec = tv_now.tv_sec + (app->poll_period - time_diff);
            }
        }

        rc = pthread_cond_timedwait(&app->cond, &app->lock, &time_to_wait);
        if ((rc == 0) || (queue_count(app->queue) != 0)) {
            // dequeue data
            while (queue_count(app->queue)) {
                clone = queue_pop(app->queue);
                if (clone == NULL) {
                    continue;
                }

                app->desc.event_fn(app, clone);

                free_cloned_event(clone);
            }
        }
    }
    pthread_mutex_unlock(&app->lock);

    return NULL;
}

int push_event_to_app_queue(wifi_app_t *app, wifi_event_t *event)
{
    wifi_event_t *clone;

    clone_wifi_event(event, &clone);
    if(clone == NULL) {
        wifi_util_error_print(WIFI_CTRL,"RDK_LOG_WARN, WIFI %s: failed to clone event\n",__FUNCTION__);
        return RETURN_ERR;
    }

    pthread_mutex_lock(&app->lock);
    queue_push(app->queue, clone);
    pthread_cond_signal(&app->cond);
    pthread_mutex_unlock(&app->lock);

    return RETURN_OK;
}

int apps_mgr_event(wifi_apps_mgr_t *apps_mgr, wifi_event_t *event)
{
    wifi_app_t	*app = NULL;
    unsigned int i,  mask = wifi_app_inst_base;

    // check if the event is unicast to any app
    if (unicast_event_to_apps(event)) {
        for (i = 0; i < sizeof(mask); i++) {
            app = get_app_by_inst(apps_mgr, (event->route.u.inst_bit_map & (mask << i)));
            if (app != NULL) {
                (app->desc.create_flag & APP_DETACHED) ? push_event_to_app_queue(app, event):app->desc.event_fn(app, event);
            }
        }
        return RETURN_OK;
    }

    // forward event to all registered apps
    app = hash_map_get_first(apps_mgr->apps_map);
    while ((app != NULL) && (app->desc.rfc == true)) {
        if (app->desc.reg_events_types & event->event_type) {
            if ( app->desc.inst != wifi_app_inst_analytics ) {
                (app->desc.create_flag & APP_DETACHED) ? push_event_to_app_queue(app, event):app->desc.event_fn(app, event);
            }
        }
        app = hash_map_get_next(apps_mgr->apps_map, app);

    }
    return RETURN_OK;
}


int apps_mgr_analytics_event(wifi_apps_mgr_t *apps_mgr, wifi_event_type_t type, wifi_event_subtype_t sub_type, void *arg)
{
    wifi_event_t event;
    wifi_app_t  *app = NULL;

    event.u.analytics_data = arg;
    event.event_type = type;
    event.sub_type = sub_type;

    app = get_app_by_inst(apps_mgr, wifi_app_inst_analytics);

    app->desc.event_fn(app, &event);
    return RETURN_OK;
}

int app_deinit(wifi_app_t *app, unsigned int create_flag)
{
    if (create_flag & APP_DETACHED) {
        if ((app->tid != 0)) {
            pthread_cancel(app->tid);
        }
    }

    return RETURN_OK;
}

int app_init(wifi_app_t *app, unsigned int create_flag)
{
    if (create_flag & APP_DETACHED) {

        pthread_cond_init(&app->cond, NULL);
        pthread_mutex_init(&app->lock, NULL);
        app->poll_period = 1;
        gettimeofday(&app->last_signalled_time, NULL);
        gettimeofday(&app->last_polled_time, NULL);

        app->queue = queue_create();

        if (pthread_create(&app->tid, NULL, app_detached_event_func, app) != 0) {
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

int update_rfc_params(wifi_app_descriptor_t *descriptor)
{
    if (descriptor == NULL) {
        wifi_util_error_print(WIFI_CTRL,"%s:%d NULL Pointer\n", __func__, __LINE__);
        return -1;
    }

    wifi_rfc_dml_parameters_t *rfc_param = (wifi_rfc_dml_parameters_t *) get_wifi_db_rfc_parameters();
    if (rfc_param == NULL) {
         wifi_util_error_print(WIFI_CTRL,"%s:%d NULL rfc pointer\n", __func__, __LINE__);
         return -1;
    }

    if (descriptor->inst == wifi_app_inst_levl) {
        descriptor->rfc = rfc_param->levl_enabled_rfc;
        descriptor->enable = descriptor->rfc;
    }
    return 0;
}

int app_register(wifi_apps_mgr_t *apps_mgr, wifi_app_descriptor_t *descriptor)
{
    wifi_app_t *app;
    char key_str[32];

    if ((app = get_app_by_inst(apps_mgr, descriptor->inst)) != NULL) {
        return RETURN_OK;
    }
    update_rfc_params(descriptor);

    app = (wifi_app_t *)malloc(sizeof(wifi_app_t));
    memset(app, 0, sizeof(wifi_app_t));
    memcpy(&app->desc, descriptor, sizeof(wifi_app_descriptor_t));
    snprintf(key_str, sizeof(key_str), "onewifi_app_%d", descriptor->inst);
    hash_map_put(apps_mgr->apps_map, strdup(key_str), app);
    if (descriptor->rfc == true) {
        app->desc.init_fn(app, app->desc.create_flag);
    }

    return RETURN_OK;

}

int apps_mgr_update(wifi_app_t *app)
{
    app->desc.update_fn(app);
    return RETURN_OK;
}

int apps_mgr_init(wifi_ctrl_t *ctrl, wifi_app_descriptor_t *descriptor, unsigned int num_apps)
{
    wifi_apps_mgr_t *apps_mgr;
    int ret = RETURN_OK;
    unsigned int i = 0;

    apps_mgr = &ctrl->apps_mgr;
    apps_mgr->ctrl = ctrl;
    apps_mgr->apps_map = hash_map_create();

    for (i = 0; i < num_apps; i++) {
        if (descriptor->rfc == true) {
            app_register(apps_mgr, descriptor);
        }
        descriptor++;
    }
    return ret;
}
