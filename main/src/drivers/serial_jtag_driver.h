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
    void set_buffer_size(size_t size) { buffer_size_ = size; }

    // 이벤트 큐 접근
    QueueHandle_t get_event_queue() const { return event_queue_; }

private:

    size_t buffer_size_;
    bool initialized_;
    bool running_;
    bool connected_;
    QueueHandle_t event_queue_;
    TaskHandle_t serial_jtag_rx_task_handle_;

    
    static const char* TAG;

    // 수신 태스크 (정적 멤버로 래핑)
    static void serial_jtag_rx_task_static(void* arg);
};

} // namespace Drivers
