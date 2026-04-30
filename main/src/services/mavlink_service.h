#pragma once

#include <esp_log.h>
#include <c_library_v2/common/mavlink.h>
#include "bridge_types.h"

namespace Services {

class MavlinkService {
public:
    MavlinkService();
    ~MavlinkService();

    esp_err_t initialize();
    void process_packet(const uint8_t* data, size_t len);

    // MAVLink 메시지 생성
    esp_err_t send_status_text(const char* text, uint8_t severity);
    esp_err_t send_heartbeat();
    esp_err_t send_power_status();
    esp_err_t send_radio_status();

private:
    static const char* TAG;

    // MAVLink 메시지 파싱
    mavlink_message_t mavlink_msg_;
    mavlink_status_t mavlink_status_;
    uint8_t temp_buffer_[1024];
};

} // namespace Services