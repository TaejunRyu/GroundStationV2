#pragma once

#include <cstdint>
#include <cstddef>
#include <esp_err.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <functional>
#include "bridge_types.h"

// USB Serial JTAG select callback types
#include "driver/usb_serial_jtag_select.h"

namespace Drivers {

class SerialJtagDriver {
public:
    SerialJtagDriver();
    ~SerialJtagDriver();

    esp_err_t initialize();
    void deinitialize();

    // 데이터 송수신
    esp_err_t send_data(const uint8_t* data, size_t len);
    esp_err_t receive_data(uint8_t* buffer, size_t max_len, size_t& actual_len, uint32_t timeout_ms = 100);
    esp_err_t start(void);
    bool  connected();
    void stop();
    void set_buffer_size(size_t size) { buffer_size_ = size; }
    
    // 이벤트 큐 접근
    QueueHandle_t get_event_queue() const { return event_queue_; }

    // Select notification callback 설정
    void set_select_callback(usj_select_notif_callback_t callback);


    uint64_t get_last_rx_timestamp() const { return last_rx_timestamp_; }
    uint64_t get_time_since_last_rx() const {
        uint64_t now = esp_timer_get_time();
        return (last_rx_timestamp_ > 0) ? (now - last_rx_timestamp_) : -1;
    }
    void reset_last_rx_timestamp() { last_rx_timestamp_ = 0; }
    void update_last_rx_timestamp() { last_rx_timestamp_ = esp_timer_get_time(); }

    bool is_connected() const { return connected_; }
    void set_connected(bool status) { connected_ = status; }
    
private:
    // Static callback wrapper for USB Serial JTAG
    static void select_notif_callback(usj_select_notif_t event, int* task_woken);

    size_t buffer_size_;
    bool initialized_;
    bool running_;

    // USB Serial JTAG 연결 상태 추적
    bool connected_;
    uint64_t last_rx_timestamp_;

    QueueHandle_t event_queue_;
    //static usj_select_notif_callback_t g_select_callback;// = nullptr;
    static const char* TAG;
};

} // namespace Drivers
