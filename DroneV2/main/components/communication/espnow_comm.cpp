#include "EspNowManager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>

static const char* TAG = "ESPNOW_MGR";

EspNowManager::EspNowManager() : _initialized(false), _connected(false), _rssi(-100), _recv_cb(nullptr) {
    memset(_peer_mac, 0, 6);
}

EspNowManager::~EspNowManager() {
    deinit();
}

esp_err_t EspNowManager::init() {
    if (_initialized) return ESP_OK;

    // 1. Wi-Fi 초기화 (ESP-NOW는 Wi-Fi 모드가 켜져 있어야 함)
    ESP_ERROR_CHECK(esp_netif_init());
    if (esp_event_loop_create_default() != ESP_OK) { /* 이미 생성된 경우 무시 */ }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // 스테이션 모드
    ESP_ERROR_CHECK(esp_wifi_start());

    // 2. ESP-NOW 초기화
    ESP_ERROR_CHECK(esp_now_init());

    // 3. 콜백 등록
    ESP_ERROR_CHECK(esp_now_register_send_cb(onSendCb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onRecvCb));

    _initialized = true;
    ESP_LOGI(TAG, "ESP-NOW Manager 초기화 성공");
    return ESP_OK;
}

esp_err_t EspNowManager::addPeer(const uint8_t* mac) {
    if (!_initialized) return ESP_ERR_INVALID_STATE;

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, mac, 6);
    peer_info.channel = 0; // 현재 Wi-Fi 채널 사용
    peer_info.encrypt = false;
    
    // 이미 등록된 피어인지 확인 후 등록
    if (esp_now_is_peer_exist(mac)) {
        return ESP_OK;
    }
    
    return esp_now_add_peer(&peer_info);
}

esp_err_t EspNowManager::send(const uint8_t* mac, const uint8_t* data, size_t len) {
    if (!_initialized || len > 250) return ESP_ERR_INVALID_ARG;
    
    // 패킷 구성
    EspNowPacket_t packet;
    packet.type = ESPNOW_DATA_TELEMETRY; // 기본값
    memcpy(packet.payload, data, len);
    // CRC 계산 로직을 여기에 추가할 수 있습니다.

    return esp_now_send(mac, (uint8_t*)&packet, sizeof(uint8_t) + len + sizeof(uint16_t));
}

void EspNowManager::setRecvCallback(EspNowRecvCallback cb) {
    _recv_cb = cb;
}

void EspNowManager::deinit() {
    if (_initialized) {
        esp_now_unregister_recv_cb();
        esp_now_unregister_send_cb();
        esp_now_deinit();
        esp_wifi_stop();
        _initialized = false;
    }
}

// 송신 결과 콜백
void EspNowManager::onSendCb(const uint8_t* mac, esp_now_send_status_t status) {
    EspNowManager* mgr = EspNowManager::getInstance();
    mgr->_connected = (status == ESP_NOW_SEND_SUCCESS);
}

// 수신 콜백
void EspNowManager::onRecvCb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    EspNowManager* mgr = EspNowManager::getInstance();
    
    // RSSI 정보 업데이트 (v6.0 스타일)
    if (recv_info->rx_ctrl) {
        mgr->_rssi = recv_info->rx_ctrl->rssi;
    }

    if (mgr->_recv_cb && len >= sizeof(uint8_t)) {
        // 패킷 타입 확인 후 페이로드만 전달하거나 패킷 전체 전달
        // 여기서는 데이터의 유효성을 체크한 뒤 콜백 호출
        mgr->_recv_cb(data, len); 
    }
}
