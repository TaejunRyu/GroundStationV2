#include "mavlink_service.h"

#include <cstring>
#include "bridge_core.h"
#include "queue_manager.h"

namespace Services {

const char* MavlinkService::TAG = "MAVLINK_SERVICE";

MavlinkService::MavlinkService() {
    ESP_LOGI(TAG, "MavlinkService created");
    memset(&mavlink_msg_, 0, sizeof(mavlink_msg_));
    memset(&mavlink_status_, 0, sizeof(mavlink_status_));
}

MavlinkService::~MavlinkService() {
    ESP_LOGI(TAG, "MavlinkService destroyed");
}

esp_err_t MavlinkService::initialize() {
    ESP_LOGI(TAG, "Initializing MavlinkService...");
    // 초기화 작업이 필요하면 여기에 추가
    return ESP_OK;
}

void MavlinkService:: process_packet(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_2, data[i], &mavlink_msg_, &mavlink_status_)) {
            // 메시지 처리
            switch (mavlink_msg_.msgid) {
                case MAVLINK_MSG_ID_HEARTBEAT:
                    ESP_LOGD(TAG, "Heartbeat received");
                    break;

                case MAVLINK_MSG_ID_MANUAL_CONTROL:
                    ESP_LOGD(TAG, "Manual control received");
                    break;

                case MAVLINK_MSG_ID_RADIO_STATUS:
                    ESP_LOGD(TAG, "Radio status received");
                    break;

                default:
                    ESP_LOGD(TAG, "MAVLink message received: msgid=%d", mavlink_msg_.msgid);
                    break;
            }
        }
    }
}

esp_err_t MavlinkService::send_status_text(const char* text, uint8_t severity) {
    Core::BridgeCore& bridge = Core::BridgeCore::get_instance();
    
    mavlink_msg_statustext_pack_chan(
        Types::Config::SYSTEM_ID, Types::Config::COMPONENT_ID, MAVLINK_COMM_1,
        &mavlink_msg_, severity, text, 0, 0
    );
    uint16_t len = mavlink_msg_to_send_buffer(temp_buffer_, &mavlink_msg_);
    bridge.get_queue_manager().enqueue_packet(temp_buffer_,len,Types::DataSource::INTERNAL);
    return ESP_OK;
}

esp_err_t MavlinkService::send_heartbeat() {
    Core::BridgeCore& bridge = Core::BridgeCore::get_instance();
    mavlink_msg_heartbeat_pack_chan(
        Types::Config::SYSTEM_ID, Types::Config::COMPONENT_ID, MAVLINK_COMM_1,
        &mavlink_msg_,
        MAV_TYPE_ONBOARD_CONTROLLER,
        MAV_AUTOPILOT_INVALID,
        0, 0, 0
    );

    uint16_t len = mavlink_msg_to_send_buffer(temp_buffer_, &mavlink_msg_);
    bridge.get_queue_manager().enqueue_packet(temp_buffer_,len,Types::DataSource::INTERNAL);
    return ESP_OK;
}

esp_err_t MavlinkService::send_radio_status(const Types::CommStats& drone_stats,
                                           const Types::CommStats& bridge_stats) {
    Core::BridgeCore& bridge = Core::BridgeCore::get_instance();

    mavlink_msg_radio_status_pack_chan(
        Types::Config::SYSTEM_ID, Types::Config::COMPONENT_ID, MAVLINK_COMM_1,
        &mavlink_msg_,
        bridge_stats.rssi,
        drone_stats.rssi,
        0,  // txbuf
        bridge_stats.noise_floor,
        drone_stats.noise_floor,
        0,  // rxerrors
        0   // fixed
    );

    uint16_t len = mavlink_msg_to_send_buffer(temp_buffer_, &mavlink_msg_);
    bridge.get_queue_manager().enqueue_packet(temp_buffer_,len,Types::DataSource::INTERNAL);
    return ESP_OK;
}

} // namespace Services