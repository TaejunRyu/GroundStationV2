#pragma once

#include <cstdint>
#include <cstddef>
#include <esp_err.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <driver/gpio.h>

namespace Drivers {

class UartDriver {
public:
    UartDriver(uart_port_t port = UART_NUM_0);
    ~UartDriver();

    esp_err_t initialize(int baud_rate = 115200);
    void deinitialize();

    // 데이터 송수신
    esp_err_t send_data(const uint8_t* data, size_t len);
    esp_err_t receive_data(uint8_t* buffer, size_t max_len, size_t& actual_len, uint32_t timeout_ms = 100);

    // 설정
    void set_port(uart_port_t port) { port_ = port; }
    void set_pins(gpio_num_t tx, gpio_num_t rx) { tx_pin_ = tx; rx_pin_ = rx; }
    void set_buffer_size(size_t size) { buffer_size_ = size; }

    // 이벤트 큐 접근
    QueueHandle_t get_event_queue() const { return event_queue_; }

private:
    uart_port_t port_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    size_t buffer_size_;
    bool initialized_;
    QueueHandle_t event_queue_;
    TaskHandle_t rx_task_handle_;

    static const char* TAG;

    // 수신 태스크 (정적 멤버로 래핑)
    static void uart_rx_task_static(void* arg);
};

} // namespace Drivers
