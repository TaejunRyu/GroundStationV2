#pragma once

#include <stdint.h>
#include <array>
#include "bridge_types.h"


namespace Utils {

class ConfigManager {
public:
    static esp_err_t initialize();
    static void deinitialize();

    // MAC 주소 설정/조회
    static void set_drone_mac(const uint8_t* mac);
    static void set_bridge_mac(const uint8_t* mac);
    static std::array<uint8_t, 6> get_drone_mac();
    static std::array<uint8_t, 6> get_bridge_mac();
    static std::array<uint8_t, 6> get_peer_mac(); // 드론과 통신할 때 사용할 MAC 주소 (예: ESP-NOW 피어)

    // Wi-Fi 설정
    static Types::WiFiConfig get_wifi_config();
    static void set_wifi_config(const Types::WiFiConfig& config);

    // ESP-NOW 설정
    static Types::EspNowConfig get_espnow_config();
    static void set_espnow_config(const Types::EspNowConfig& config);

private:
    static bool initialized_;
    static uint8_t drone_mac_[6];
    static uint8_t bridge_mac_[6];
    static Types::WiFiConfig wifi_config_;
    static Types::EspNowConfig espnow_config_;

    static const char* TAG;
};

} // namespace Utils