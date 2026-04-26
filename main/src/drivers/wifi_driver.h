#pragma once

#include <functional>
#include <esp_wifi.h>
#include <esp_now.h>
#include <lwip/udp.h>
#include <array>
#include "bridge_types.h"

namespace Drivers {

class WiFiDriver {
public:
    WiFiDriver();
    ~WiFiDriver();

    esp_err_t initialize();
    esp_err_t start_ap(const Types::WiFiConfig& config);
    esp_err_t init_espnow(const Types::EspNowConfig& config);
    void stop();

    // 콜백 설정
    void set_connect_callback(std::function<void()> callback);
    void set_disconnect_callback(std::function<void()> callback);
    void set_data_callback(std::function<void(const uint8_t*, size_t, Types::DataSource)> callback);

    // UDP 통신
    esp_err_t init_udp();
    esp_err_t send_udp(const uint8_t* data, size_t len);
    void enable_udp_recv();
    void disable_udp_recv();

    // ESP-NOW 통신
    esp_err_t send_espnow(const uint8_t* data, size_t len);

    // ESP-NOW 콜백 관리
    void register_espnow_callbacks();
    void unregister_espnow_callbacks();

    // 상태 조회
    bool is_connected(Types::DataSource source) const;
    int8_t get_rssi() const;
    std::array<uint8_t, 6> get_my_mac_address() const;

    // 이벤트
    esp_event_loop_handle_t get_event_loop() const { return event_loop_; }

private:
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
    static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
    static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);
    static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                 const ip_addr_t *addr, u16_t port);

    // PSRAM 버퍼 관리
    uint8_t* udp_rx_buffer_;
    bool init_psram_buffer();
    void deinit_psram_buffer();

    std::function<void()> connect_callback_;
    std::function<void()> disconnect_callback_;
    std::function<void(const uint8_t*, size_t, Types::DataSource)> data_callback_;

    bool initialized_;
    bool ap_started_;
    bool espnow_initialized_;
    bool udp_initialized_;
    int8_t rssi_;

    // UDP 관련
    struct udp_pcb* udp_pcb_;
    ip_addr_t udp_remote_addr_;

    // ESP-NOW 관련
    uint8_t drone_mac_[6];

    // 이벤트 루프
    esp_event_loop_handle_t event_loop_;

    static const char* TAG;
};

} // namespace Drivers
