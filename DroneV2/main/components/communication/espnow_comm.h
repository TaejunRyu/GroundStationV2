#pragma once

#include "esp_err.h"
#include "esp_now.h"
#include "singleton_base.h"
#include "driver/gpio.h"

/**
 * @brief ESP-NOW 데이터 타입
 */
typedef enum {
    ESPNOW_DATA_CONTROL = 0,  // 조종 데이터
    ESPNOW_DATA_TELEMETRY,    // 텔레메트리
    ESPNOW_DATA_CONFIG        // 설정
} EspNowDataType_t;

/**
 * @brief ESP-NOW 페이로드 구조
 */
typedef struct {
    uint8_t type;
    uint8_t payload[250];
    uint16_t crc;
} __attribute__((packed)) EspNowPacket_t;

/**
 * @brief ESP-NOW 콜백 타입
 */
typedef void (*EspNowRecvCallback)(const uint8_t* data, size_t len);

/**
 * @brief ESP-NOW 관리자
 * - 드론-지상국 무선 통신
 * - 쌍방향 통신 지원
 */
class EspNowManager : public Singleton<EspNowManager> {
    friend class Singleton<EspNowManager>;

public:
    esp_err_t init();
    esp_err_t addPeer(const uint8_t* mac);
    esp_err_t send(const uint8_t* mac, const uint8_t* data, size_t len);
    void setRecvCallback(EspNowRecvCallback cb);
    int8_t getRssi() const { return _rssi; }
    bool isConnected() const { return _connected; }
    void deinit();

private:
    EspNowManager();
    ~EspNowManager();

    static void onSendCb(const uint8_t* mac, esp_now_send_status_t status);
    static void onRecvCb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);

    bool _initialized = false;
    bool _connected = false;
    int8_t _rssi = -100;
    EspNowRecvCallback _recv_cb = nullptr;
    uint8_t _peer_mac[6] = {0};
};

#endif