#include "uart_driver.h"
#include "esp_log.h"

static const char* TAG = "UART_DRV";

namespace drone {

UartDriver::UartDriver(uart_port_t uart_num) : _uart_num(uart_num), _initialized(false) {}

UartDriver::~UartDriver() {
    if (_initialized) {
        uart_driver_delete(_uart_num);
    }
}

esp_err_t UartDriver::init(int tx_pin, int rx_pin, uint32_t baud_rate, size_t rx_buf_size) {
    if (_initialized) return ESP_OK;

    uart_config_t uart_config = {
        .baud_rate = (int)baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 1. 설정 적용
    esp_err_t ret = uart_param_config(_uart_num, &uart_config);
    if (ret != ESP_OK) return ret;

    // 2. 핀 설정
    ret = uart_set_pin(_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;

    // 3. 드라이버 설치 (TX 버퍼는 보통 필요 없으므로 0)
    ret = uart_driver_install(_uart_num, (int)rx_buf_size, 0, 0, NULL, 0);
    if (ret == ESP_OK) {
        _initialized = true;
        ESP_LOGI(TAG, "UART %d 초기화 완료 (%d bps)", _uart_num, baud_rate);
    }

    return ret;
}

int UartDriver::write(const uint8_t* data, size_t len) {
    if (!_initialized) return -1;
    return uart_write_bytes(_uart_num, (const char*)data, len);
}

int UartDriver::read(uint8_t* buf, size_t len, uint32_t timeout_ms) {
    if (!_initialized) return -1;
    return uart_read_bytes(_uart_num, buf, len, pdMS_TO_TICKS(timeout_ms));
}

void UartDriver::flush() {
    if (_initialized) {
        uart_flush_input(_uart_num);
    }
}

} // namespace drone
