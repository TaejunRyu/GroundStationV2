#pragma once

#include <cstdint>
#include <cstddef>
#include <esp_err.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "uart_driver.h"

namespace Drivers {

constexpr size_t IBUS_CHANNELS = 14;
constexpr size_t IBUS_PACKET_SIZE = 32;

class FlyskySensor {
public:
    FlyskySensor(uart_port_t port = UART_NUM_1);
    ~FlyskySensor();

    esp_err_t initialize();
    void deinitialize();

    // RC 채널 데이터 읽기
    uint16_t get_channel(uint8_t channel) const;
    const uint16_t* get_all_channels() const;

    // 상태 확인
    bool is_connected() const;

    // UART 설정
    void set_uart_baud(int baud) { uart_baud_ = baud; }
    void set_uart_pins(gpio_num_t tx, gpio_num_t rx);

private:
    uart_port_t port_;
    int uart_baud_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    bool initialized_;
    bool is_connected_;
    TaskHandle_t rx_task_handle_;
    QueueHandle_t data_queue_;
    uint16_t rc_values_[IBUS_CHANNELS];

    // 데이터 처리
    esp_err_t parse_ibus_packet(const uint8_t* packet, size_t len);

    // 수신 태스크
    static void flysky_rx_task_static(void* arg);

    static const char* TAG;
    static const int UART_BUF_SIZE = 256;
    static const int BAUD_RATE = 115200;
};
 
} // namespace Drivers
