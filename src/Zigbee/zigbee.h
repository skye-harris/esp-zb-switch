// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
// Modifications Copyright 2024 Skye Harris
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ZIGBEE_MODE_ZCZR
    #error "Zigbee device mode ZIGBEE_MODE_ZCZR is not set in platformio.ini build flags"
#endif

#include "esp_zigbee_attribute.h"
#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MANUFACTURER_NAME "\x0B" "Skye Harris"
#define MODEL_IDENTIFIER "\x0D" "Zigbee Switch"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE false                                     /* enable the install code policy for security */
#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 3000                                                  /* 3000 millisecond */
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */

/** Endpoint ID */
#define HA_ESP_SENSOR_ENDPOINT 1

/* Default End Device config */
#define ESP_ZB_ZCR_CONFIG()                               \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,         \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
        .nwk_cfg = {                                      \
            .zed_cfg =                                    \
                {                                         \
                    .ed_timeout = ED_AGING_TIMEOUT,       \
                    .keep_alive = ED_KEEP_ALIVE,          \
                },                                        \
        },                                                \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()       \
    {                                       \
        .radio_mode = ZB_RADIO_MODE_NATIVE, \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                          \
    {                                                         \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, \
    }


void ZB_StartMainTask();
void ZB_FactoryReset();
void ZB_SetOnCreateClustersCallback(void (*callback)(esp_zb_cluster_list_t *clusterList));
void ZB_SetOnAttributeUpdatedCallback(esp_err_t (*callback)(const esp_zb_zcl_set_attr_value_message_t *message));
void ZB_SetOnCustomClusterCommandCallback(esp_err_t (*callback)(const esp_zb_zcl_custom_cluster_command_message_t *message));
void ZB_SetOnIdentifyCallback(void (*callback)(bool isIdentifying));
void ZB_SetOnInitCallback(void (*callback)());