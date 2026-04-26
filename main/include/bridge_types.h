#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include "esp_wifi_types.h"

// 공통 타입 정의 (기존 ryu_types.h와 ryu_config.h 통합)
namespace Types {

// 시스템 상태
enum class SystemState {
    UNINITIALIZED,
    INITIALIZING,
    READY,
    RUNNING,
    ERROR,
    SHUTDOWN
};

// 데이터 소스
enum class DataSource {
    WIFI_UDP,
    WIFI_ESPNOW,
    UART_SERIAL,
    UART_FLYSKY,
    INTERNAL
};

// 패킷 타입 (기존 packet_type_t 확장)
enum class PacketType {
    ON_ESP_NOW_RECV = 0x00,
    ON_UART_RECV    = 0x01,
    ON_UDP_RECV     = 0x02,
    ON_BRIDGE_MAKE  = 0x03,
    ON_RC_OVERRIDE  = 0x04,
    MAVLINK_HEARTBEAT,
    MAVLINK_COMMAND,
    MAVLINK_STATUS,
    RC_CHANNELS,
    TELEMETRY,
    RAW_DATA
};

// 이벤트 타입 (기존 bridge_event_type 확장)
enum class BridgeEvent {
    // UDP 전송 관련
    BRIDGE_UDP_TX_SUCCESS,
    BRIDGE_UDP_TX_ERR_MEM,
    BRIDGE_UDP_TX_ERR_RTE,
    BRIDGE_UDP_TX_ERR_VAL,
    BRIDGE_UDP_TX_ERR_USE,
    BRIDGE_UDP_TX_ERR_ETC,
    // QGC 상태 관련
    BRIDGE_QGC_CONNECTED,
    BRIDGE_QGC_DISCONNECTED,
    BRIDGE_QGC_RECV_SUCCESS,
    BRIDGE_QGC_HEARTBEAT,
    // ESPNOW 상태 관련
    BRIDGE_ESPNOW_CONNECTED,
    BRIDGE_ESPNOW_DISCONNECTED,
    BRIDGE_ESPNOW_SEND_SUCCESS,
    BRIDGE_ESPNOW_SEND_FAIL,
    BRIDGE_ESPNOW_RECV_SUCCESS,
    // UART 상태 관련
    BRIDGE_SJTAG_CONNECT,
    BRIDGE_SJTAG_DISCONNECT,
    // 시스템 이벤트
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    DATA_RECEIVED,
    OTA_STARTED,
    OTA_COMPLETED,
    OTA_FAILED,
    SYSTEM_ERROR
};

// ESP-NOW 이벤트 데이터
struct EspNowEventData {
    uint8_t mac[6];
    int8_t  rssi;
    uint8_t noise_floor;
} __attribute__((packed));

// RC 전용 정적 구조체
struct RcStaticPacket {
    size_t  length;
    PacketType type;
    uint8_t data[64];
} __attribute__((packed));

// 큐 메시지 구조체 (가변 길이 데이터 지원)
struct QueueMessage {
    size_t  length;
    PacketType type;
    DataSource source;
    uint8_t data[];  // 가변 길이 배열
} __attribute__((packed));

// 이벤트 데이터
struct EventData {
    BridgeEvent event_type;
    union {
        struct {
            uint8_t mac[6];
            int8_t rssi;
        } wifi_data;
        struct {
            size_t length;
            const uint8_t* data;
        } packet_data;
        EspNowEventData espnow_data;
        esp_err_t error_code;
    } data;
};

// Wi-Fi 설정
struct WiFiConfig {
    const char* ssid;
    const char* password;
    uint8_t channel;
    wifi_auth_mode_t auth_mode;
    bool hidden;
    uint8_t max_connections;
};

// ESP-NOW 설정
struct EspNowConfig {
    uint8_t peer_mac[6];
    uint8_t channel;
    wifi_phy_mode_t phy_mode;
    wifi_phy_rate_t rate;
};

// MAVLink 시스템 정보
struct MavlinkSystem {
    uint8_t system_id;
    uint8_t component_id;
    uint8_t target_system;
    uint8_t target_component;
};

// 통신 통계
struct CommStats {

    uint32_t tx_count;
    uint32_t rx_count;
    int64_t last_send_time;
    int64_t last_receive_time;
    int64_t connected_time;
    bool is_connected;
    int8_t rssi;
    int8_t noise_floor;
};

// 시스템 설정 상수
namespace Config {
    inline constexpr uint8_t SYSTEM_ID = 1;
    inline constexpr uint8_t COMPONENT_ID = 240;  // MAV_COMPONENT_ID_UDP_BRIDGE
    inline constexpr uint8_t DRONE_COMP_ID = 1;

    inline constexpr uint8_t ESPNOW_CHANNEL = 6;
    inline constexpr uint16_t ESP_NOW_MAX_LEN = 290;
    inline constexpr uint16_t UPDOWNLINK_QUEUE_SIZE = 40;

    inline constexpr uint16_t UDP_PORT = 14550;
    inline constexpr size_t UDP_RX_BUFFER_SIZE = 4096;
    inline constexpr size_t MAX_MESSAGE_SIZE = 2048;
}

} // namespace Types