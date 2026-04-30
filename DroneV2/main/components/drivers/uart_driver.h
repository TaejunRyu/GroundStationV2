#pragma once

#include "driver/uart.h"
#include "esp_err.h"

namespace drone {

class UartDriver {
public:
    // 생성자에서 사용할 UART 포트 번호를 지정
    UartDriver(uart_port_t uart_num);
    ~UartDriver();

    // 초기화: 핀 번호, 통신 속도, 버퍼 크기 설정
    esp_err_t init(int tx_pin, int rx_pin, uint32_t baud_rate, size_t rx_buf_size = 1024);
    
    // 데이터 송신
    int write(const uint8_t* data, size_t len);
    
    // 데이터 수신 (timeout_ms 동안 대기)
    int read(uint8_t* buf, size_t len, uint32_t timeout_ms);
    
    // 수신 버퍼 비우기
    void flush();

private:
    uart_port_t _uart_num;
    bool _initialized = false;
};

} // namespace drone
