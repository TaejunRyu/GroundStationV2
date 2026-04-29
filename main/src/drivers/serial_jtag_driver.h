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
#include <esp_timer.h>
#include "bridge_types.h"

// USB Serial JTAG select callback types
#include "driver/usb_serial_jtag_select.h"

namespace Core {
    class QueueManager;
}



namespace Drivers {

class SerialJtagDriver {

public:
    // 복사및 할당 금지. ( 싱글톤 )
    SerialJtagDriver(const SerialJtagDriver&) = delete;
    SerialJtagDriver& operator=(const SerialJtagDriver&) = delete;

    SerialJtagDriver();
    ~SerialJtagDriver();

    esp_err_t initialize();
    void deinitialize();

    // 데이터 송수신
    esp_err_t send_data(const uint8_t* data, size_t len);
    esp_err_t receive_data(uint8_t* buffer, size_t max_len, size_t& actual_len, uint32_t timeout_ms = 100);
    
    esp_err_t start(void);
    void stop();

    void set_buffer_size(size_t size) { buffer_size_ = size; }
    
    uint64_t get_last_rx_timestamp() const { return last_rx_timestamp_; }
    uint64_t get_time_since_last_rx() const {
        uint64_t now = esp_timer_get_time();
        return (last_rx_timestamp_ > 0) ? (now - last_rx_timestamp_) : -1;
    }
    void reset_last_rx_timestamp() { last_rx_timestamp_ = 0; }
    void update_last_rx_timestamp() { last_rx_timestamp_ = esp_timer_get_time(); }

    bool is_connected() const { return connected_ && initialized_; }
    void set_connected(bool status) { connected_ = status; }

    void register_select_callback(void);
    void unregister_select_callback(void);

    static void set_queue_manager(Core::QueueManager* qm) { queue_mgr_ = qm; }
    static void rx_task(void *pvParameters);
private:
    size_t buffer_size_;

    bool initialized_;
    bool running_;

    // USB Serial JTAG 연결 상태 추적
    bool connected_;
    uint64_t last_rx_timestamp_;

    // Static callback wrapper for USB Serial JTAG
    static void select_notif_callback(usj_select_notif_t event, int* task_woken);
    static SemaphoreHandle_t rx_sem_;
    static Core::QueueManager* queue_mgr_; // 드라이버에서 queue manager에 접근할 수 있도록 static 함수 제공
    //static SerialJtagDriver * serial_jtag_driver_;  //callback 함수에서 자신의 멤버에 접근할 수 있도록 static 멤버에 자기 자신 포인터 저장
    static const char* TAG;
};

} // namespace Drivers
