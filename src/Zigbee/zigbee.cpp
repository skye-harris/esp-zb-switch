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

#include <Arduino.h>
#include "Zigbee/zigbee.h"

// Callback functions to main application logic
void (*onCreateClustersCallback)(esp_zb_cluster_list_t *clusterList) = NULL;
esp_err_t (*onAttributeUpdatedCallback)(const esp_zb_zcl_set_attr_value_message_t *message);
esp_err_t (*onCustomClusterCommandCallback)(const esp_zb_zcl_custom_cluster_command_message_t *message);
void (*onIdentifyCallback)(bool isIdentifying) = NULL;

// Cluster Action callback
static esp_err_t onZigbeeAction(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    esp_err_t ret = ESP_OK;

    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            log_i("Receive Zigbee action ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID");

            if (onAttributeUpdatedCallback != NULL) {
                ret = onAttributeUpdatedCallback((esp_zb_zcl_set_attr_value_message_t *)message);
            }
            break;

        case ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID:
            log_i("Receive Zigbee action ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID");

            if (onCustomClusterCommandCallback != NULL) {
                ret = onCustomClusterCommandCallback((esp_zb_zcl_custom_cluster_command_message_t *)message);
            }
            break;

        case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
            log_i("Receive Zigbee action ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID");
            break;

        case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
            log_i("Receive Zigbee action ESP_ZB_CORE_REPORT_ATTR_CB_ID");
            break;

        case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
            log_i("Receive Zigbee action ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID");
            break;

        default:
            log_i("Receive Zigbee action(0x%x) callback", callback_id);
            break;
    }

    return ret;
}

// Handle identify functionality
void onZigbeeIdentify(uint8_t identify_on) {
    static bool isIdentifying = false;
    bool wasIdentifying = isIdentifying;
    isIdentifying = identify_on;

    if (onIdentifyCallback != NULL) {
        if (isIdentifying && !wasIdentifying) {
            onIdentifyCallback(true);
        } else if (!isIdentifying && wasIdentifying) {
            onIdentifyCallback(false);
        }
    }
}

// Zigbee signal handlers
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;

    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            log_i("Zigbee stack initialized");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                log_i("Start network steering");
                log_i("Device started up in %sfactory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non ");

                // Attach our identify handler
                esp_zb_identify_notify_handler_register(HA_ESP_SENSOR_ENDPOINT, &onZigbeeIdentify);

                if (esp_zb_bdb_is_factory_new()) {
                    log_i("Start network steering");
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    log_i("Device rebooted");
                }
            } else {
                /* commissioning failed */
                log_w("Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));

                // restart after 5 seconds
                delay(2000);
                esp_restart();
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                esp_zb_ieee_addr_t extended_pan_id;
                esp_zb_get_extended_pan_id(extended_pan_id);
                log_i(
                    "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                    extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3], extended_pan_id[2], extended_pan_id[1],
                    extended_pan_id[0], esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            } else {
                log_i("Network steering was not successful (status: %s)", esp_err_to_name(err_status));
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;

        default:
            log_i("ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
            break;
    }
}

// Create Basic & Identify Clusters
static esp_zb_cluster_list_t *createBasicClusters() {
    log_i("Creating Basic & Identify Clusters");
    esp_zb_cluster_list_t *clusterList = esp_zb_zcl_cluster_list_create();

    // Create Basic cluster
    esp_zb_basic_cluster_cfg_t clusterConfigBasic = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };

    esp_zb_attribute_list_t *basicCluster = esp_zb_basic_cluster_create(&clusterConfigBasic);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basicCluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basicCluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(clusterList, basicCluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Create Identify cluster
    esp_zb_identify_cluster_cfg_t clusterConfigIdentify = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
    };

    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(clusterList, esp_zb_identify_cluster_create(&clusterConfigIdentify), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(clusterList, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    return clusterList;
}

// Create Endpoint
static esp_zb_ep_list_t *createSensorEndpoint(uint8_t endpoint_id) {
    log_i("Creating Sensor Endpoint");
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = endpoint_id,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 1
    };

    esp_zb_cluster_list_t *clusterList = createBasicClusters();

    // If our callback is set, pass our clusterlist over for further updates
    if (onCreateClustersCallback != NULL) {
        log_i("Calling onCreateClustersCallback");
        onCreateClustersCallback(clusterList);
    } else {
        log_i("No cluster callback defined, skipping");
    }

    esp_zb_ep_list_add_ep(ep_list, clusterList, endpoint_config);

    return ep_list;
}

static void taskZigbeeMain(void *pvParameters) {
    esp_zb_set_trace_level_mask(ESP_ZB_TRACE_LEVEL_CRITICAL, ESP_ZB_TRACE_SUBSYSTEM_MAC | ESP_ZB_TRACE_SUBSYSTEM_APP);

    // Initialise our configuration
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Create customized sensor endpoint */
    esp_zb_ep_list_t *esp_zb_sensor_ep = createSensorEndpoint(HA_ESP_SENSOR_ENDPOINT);

    /* Register the device */
    esp_zb_device_register(esp_zb_sensor_ep);

    /* Register our action callback */
    esp_zb_core_action_handler_register(onZigbeeAction);

    /* Config the reporting info  */
    // configureReporting(ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,0x0,ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI);

    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    // Erase NVRAM before creating connection to new Coordinator
    // esp_zb_nvram_erase_at_start(true); //Comment out this line to erase NVRAM data if you are conneting to new Coordinator

    ESP_ERROR_CHECK(esp_zb_start(false));

    esp_zb_stack_main_loop();
}

// External interface functions
void ZB_StartMainTask() {
    // Init Zigbee
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    // Start task
    xTaskCreate(taskZigbeeMain, "Zigbee_main", 4096, NULL, 5, NULL);
}

void ZB_FactoryReset() {
    esp_zb_factory_reset();
}

void ZB_SetOnCreateClustersCallback(void (*callback)(esp_zb_cluster_list_t *clusterList)) {
    onCreateClustersCallback = callback;
}

void ZB_SetOnAttributeUpdatedCallback(esp_err_t (*callback)(const esp_zb_zcl_set_attr_value_message_t *message)) {
    onAttributeUpdatedCallback = callback;
}

void ZB_SetOnCustomClusterCommandCallback(esp_err_t (*callback)(const esp_zb_zcl_custom_cluster_command_message_t *message)) {
    onCustomClusterCommandCallback = callback;
}

void ZB_SetOnIdentifyCallback(void (*callback)(bool isIdentifying)) {
    onIdentifyCallback = callback;
}