//
// Created by thecave3 on 19/08/20.
//
#include <string.h>

#include "esp_log.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#include "relay.h"
#include "ble_init.h"
#include "common.h"

#define TAG "RELAY"

uint16_t node_net_idx = ESP_BLE_MESH_KEY_UNUSED;
uint16_t node_app_idx = ESP_BLE_MESH_KEY_UNUSED;

esp_ble_mesh_cfg_srv_t config_server_relay = {
        .relay = ESP_BLE_MESH_RELAY_ENABLED,
        .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
        .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
        .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
        .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
        .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
        .default_ttl = 7, // default ttl value is 7 it decide the number of hops in the network
        /* 3 transmissions with 20ms interval */
        .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
        .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20)
};

// this is the root model or the model for the primary element
static esp_ble_mesh_model_t root_models[] = {
        ESP_BLE_MESH_MODEL_CFG_SRV(&config_server_relay)
};

// the number of element in a given node relation of with the model
static esp_ble_mesh_elem_t elements[] = {
        ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE) // primary element
};

static esp_ble_mesh_prov_t provision_relay = {
        .uuid = dev_uuid
};

static esp_ble_mesh_comp_t composition = {
        .cid = CID_ESP,
        .elements = elements,
        .element_count = ARRAY_SIZE(elements),
};

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index) {
    ESP_LOGI(TAG, "Provision has been completed");
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08x", flags, iv_index);
}

void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param) {
    switch (event) {
        case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                     param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                     param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
            prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                          param->node_prov_complete.flags, param->node_prov_complete.iv_index);
            break;
        case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
            break;
        case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d",
                     param->node_set_unprov_dev_name_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_OUTPUT_NUMBER_EVT:
            ESP_LOGI(TAG, " output number is %d ", param->node_prov_output_num.number);
            break;
        case ESP_BLE_MESH_NODE_PROV_OUTPUT_STRING_EVT:
            ESP_LOGI(TAG, " output string is %s", param->node_prov_output_str.string);
            break;

        case ESP_BLE_MESH_HEARTBEAT_MESSAGE_RECV_EVT:
            ESP_LOGI(TAG, " heartbeat hops %d feature 0x%04x", param->heartbeat_msg_recv.hops,
                     param->heartbeat_msg_recv.feature);
            break;

        case ESP_BLE_MESH_NODE_PROV_OOB_PUB_KEY_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_OOB_PUB_KEY_EVT");
            break;

        case ESP_BLE_MESH_NODE_PROV_SET_OOB_PUB_KEY_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_SET_OOB_PUB_KEY_COMP_EVT  error code is %d",
                     param->node_prov_set_oob_pub_key_comp.err_code);
            break;
        default:
            ESP_LOGW(TAG, "Event not handled, event code: %d", event);
            break;
    }
}


static void handle_level_service_msg(esp_ble_mesh_model_t *model, esp_ble_mesh_msg_ctx_t *ctx,
                                     esp_ble_mesh_server_recv_gen_level_set_t *set) {
    esp_ble_mesh_gen_level_srv_t *srv = model->user_data;

    switch (ctx->recv_op) {
        case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_GET:
            esp_ble_mesh_server_model_send_msg(model, ctx,
                                               ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS, sizeof(srv->state.level),
                                               &srv->state.level);
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK:
            srv->state.last_level = srv->state.level;
            srv->state.level = set->level;

            if (set->op_en) {
                srv->transition.trans_time = set->trans_time;
                srv->transition.delay = set->delay;
            }

            if (ctx->recv_op == ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_SET) {
                esp_ble_mesh_server_model_send_msg(model, ctx,
                                                   ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS,
                                                   sizeof(srv->state.level),
                                                   &srv->state.level);
            }

            esp_ble_mesh_model_publish(model, ESP_BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS,
                                       sizeof(srv->state.level), &srv->state.level, ROLE_NODE);

            break;
        default:
            break;
    }
}

static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                      esp_ble_mesh_cfg_server_cb_param_t *param) {

    log_ble_mesh_config_server_packet(TAG, param);

    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
            case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
                ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                         param->value.state_change.appkey_add.net_idx,
                         param->value.state_change.appkey_add.app_idx);
                ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
                ESP_LOGI(TAG, "opcode for the procedure is %x", param->ctx.recv_op);
                break;
            case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
                ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                         param->value.state_change.mod_app_bind.element_addr,
                         param->value.state_change.mod_app_bind.app_idx,
                         param->value.state_change.mod_app_bind.company_id,
                         param->value.state_change.mod_app_bind.model_id);
                if (param->value.state_change.mod_app_bind.company_id == 0xFFFF &&
                    param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_CLI) {
                    node_app_idx = param->value.state_change.mod_app_bind.app_idx;
                }
                break;
            case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD");
                ESP_LOGI(TAG, "elem_addr 0x%04x, sub_addr 0x%04x, cid 0x%04x, mod_id 0x%04x",
                         param->value.state_change.mod_sub_add.element_addr,
                         param->value.state_change.mod_sub_add.sub_addr,
                         param->value.state_change.mod_sub_add.company_id,
                         param->value.state_change.mod_sub_add.model_id);
                break;
            case ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET");
                ESP_LOGI(TAG, "publish address is 0x%04x", param->value.state_change.mod_pub_set.pub_addr);
                break;
            default:
                ESP_LOGI(TAG, "config server callback value is  0x%04x ", param->ctx.recv_op);
                break;
        }
    }
}


esp_err_t ble_mesh_init_relay(void) {
    esp_err_t err = ESP_OK;

    ble_mesh_get_dev_uuid(dev_uuid);

    esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(ble_mesh_config_server_cb);

    err = esp_ble_mesh_init(&provision_relay, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_set_unprovisioned_device_name(TAG);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set correct name (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return err;
}
