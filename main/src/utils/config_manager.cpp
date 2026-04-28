#include "config_manager.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace Utils {

const char* ConfigManager::TAG = "CONFIG_MANAGER";

bool ConfigManager::initialized_ = false;
uint8_t ConfigManager::drone_mac_[6]  = {0xB0, 0xCB, 0xD8, 0xD7, 0x2E, 0xB0}; // 드론의 MAC 주소 기본값 (환경변수나 하드코딩된 값 사용)
uint8_t ConfigManager::bridge_mac_[6] = {0x1C, 0xDB, 0xD4, 0xAE, 0x82, 0x04}; // 기본값 설정 (환경변수나 하드코딩된 값 사용)


Types::WiFiConfig ConfigManager::wifi_config_ = {
    .ssid       = CONFIG_GS_WIFI_SSID ? CONFIG_GS_WIFI_SSID : "GroundStationWiFi",
    .password   = CONFIG_GS_WIFI_PASSWORD ? CONFIG_GS_WIFI_PASSWORD : "groundstation2024",
    .channel    = CONFIG_GS_ESPNOW_CHANNEL,
    .auth_mode  = WIFI_AUTH_WPA2_PSK,
    .hidden     = false,
    .max_connections = 4
};

Types::EspNowConfig ConfigManager::espnow_config_ = {
    .peer_mac   = {},       // CONFIG_GS_DRONE_MAC에서 읽어올 예정
    .channel    = CONFIG_GS_ESPNOW_CHANNEL,
    .phy_mode   = WIFI_PHY_MODE_11B,
    .rate       = WIFI_PHY_RATE_2M_S
};

esp_err_t ConfigManager::initialize() {
    if (initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing ConfigManager...");

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // NVS에서 설정 읽기
    nvs_handle_t nvs_handle;
    ret = nvs_open("config", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // // 드론 MAC 주소 읽기
    size_t mac_len = sizeof(drone_mac_);
    ret = nvs_get_blob(nvs_handle, "drone_mac", drone_mac_, &mac_len);
    if (ret != ESP_OK) {
        // 기본값 설정 (환경변수나 하드코딩된 값 사용)
        std::stringstream ss(CONFIG_GS_DRONE_MAC ? CONFIG_GS_DRONE_MAC : "00:00:00:00:00:00");
        std::string token;
        int i = 0;
        while (std::getline(ss, token, ':') && i < 6) {
            unsigned int hex_val = 0;
            std::istringstream(token) >> std::hex >> hex_val;
            drone_mac_[i] = static_cast<uint8_t>(hex_val & 0xFF);
            i++;
        }
        ESP_LOGI(TAG, "Using default drone MAC");
    }

    // ESP-NOW 피어 MAC 주소는 드론 MAC 주소로 설정
    memcpy(espnow_config_.peer_mac, drone_mac_, sizeof(espnow_config_.peer_mac));


    // 브리지 MAC 주소 읽기
    mac_len = sizeof(bridge_mac_);
    ret = nvs_get_blob(nvs_handle, "bridge_mac", bridge_mac_, &mac_len);
    if (ret != ESP_OK) {
        // 기본값 설정
        std::stringstream ss(CONFIG_GS_BRIDGE_MAC ? CONFIG_GS_BRIDGE_MAC : "00:00:00:00:00:00");
        std::string token;
        int i = 0;
        while (std::getline(ss, token, ':') && i < 6) {
            unsigned int hex_val = 0;
            std::istringstream(token) >> std::hex >> hex_val;
            bridge_mac_[i] = static_cast<uint8_t>(hex_val & 0xFF);
            i++;
        }
        ESP_LOGI(TAG, "Using default bridge MAC");
    }

    nvs_close(nvs_handle);
    initialized_ = true;

    ESP_LOGI(TAG, "ConfigManager initialized");
    return ESP_OK;
}

void ConfigManager::deinitialize() {
    initialized_ = false;
}

void ConfigManager::set_drone_mac(const uint8_t* mac) {
    memcpy(drone_mac_, mac, sizeof(drone_mac_));
    memcpy(espnow_config_.peer_mac, mac, sizeof(espnow_config_.peer_mac));

    // NVS에 저장
    if (initialized_) {
        nvs_handle_t nvs_handle;
        if (nvs_open("config", NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_set_blob(nvs_handle, "drone_mac", drone_mac_, sizeof(drone_mac_));
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }
}

void ConfigManager::set_bridge_mac(const uint8_t* mac) {
    memcpy(bridge_mac_, mac, sizeof(bridge_mac_));

    // NVS에 저장
    if (initialized_) {
        nvs_handle_t nvs_handle;
        if (nvs_open("config", NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_set_blob(nvs_handle, "bridge_mac", bridge_mac_, sizeof(bridge_mac_));
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }
}

std::array<uint8_t, 6> ConfigManager::get_peer_mac() {
    return { espnow_config_.peer_mac[0], espnow_config_.peer_mac[1], espnow_config_.peer_mac[2], 
             espnow_config_.peer_mac[3], espnow_config_.peer_mac[4], espnow_config_.peer_mac[5] };
}

std::array<uint8_t, 6> ConfigManager::get_drone_mac() {
    return { drone_mac_[0], drone_mac_[1], drone_mac_[2], 
             drone_mac_[3], drone_mac_[4], drone_mac_[5] };
}

std::array<uint8_t, 6> ConfigManager::get_bridge_mac() {
    return { bridge_mac_[0], bridge_mac_[1], bridge_mac_[2], 
             bridge_mac_[3], bridge_mac_[4], bridge_mac_[5] };
}


Types::WiFiConfig ConfigManager::get_wifi_config() {
    return wifi_config_;
}

void ConfigManager::set_wifi_config(const Types::WiFiConfig& config) {
    wifi_config_ = config;
}

Types::EspNowConfig ConfigManager::get_espnow_config() {
    return espnow_config_;
}

void ConfigManager::set_espnow_config(const Types::EspNowConfig& config) {
    espnow_config_ = config;
}

} // namespace Utils