#include "uart_driver.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <cstring>

namespace Drivers {

const char* UartDriver::TAG = "UART";

UartDriver::UartDriver(uart_port_t port)
    : port_(port), tx_pin_((gpio_num_t)UART_PIN_NO_CHANGE), rx_pin_((gpio_num_t)UART_PIN_NO_CHANGE),
      buffer_size_(1024), initialized_(false), event_queue_(nullptr), rx_task_handle_(nullptr) {
    ESP_LOGI(TAG, "UartDriver created for UART%d", port_);
}

UartDriver::~UartDriver() {
    deinitialize();
}

esp_err_t UartDriver::initialize(int baud_rate) {
    if (initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing UART%d at %d baud", port_, baud_rate);

    uart_config_t uart_config = {};
    uart_config.baud_rate = baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t ret;

    // UART 드라이버 설치
    ret = uart_driver_install(port_, buffer_size_, buffer_size_, 20, &event_queue_, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver installation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // UART 설정 적용
    ret = uart_param_config(port_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART configuration failed: %s", esp_err_to_name(ret));
        uart_driver_delete(port_);
        event_queue_ = nullptr;
        return ret;
    }

    // 핀 설정
    ret = uart_set_pin(port_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART pin configuration failed: %s", esp_err_to_name(ret));
        uart_driver_delete(port_);
        event_queue_ = nullptr;
        return ret;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "UART%d initialized successfully", port_);

    // 수신 태스크 시작
    xTaskCreate(uart_rx_task_static, "uart_rx_task", 2048, this, 5, &rx_task_handle_);

    return ESP_OK;
}

void UartDriver::deinitialize() {
    if (!initialized_) return;

    // 수신 태스크 삭제
    if (rx_task_handle_ != nullptr) {
        vTaskDelete(rx_task_handle_);
        rx_task_handle_ = nullptr;
    }

    // UART 드라이버 제거
    uart_driver_delete(port_);
    event_queue_ = nullptr;
    initialized_ = false;

    ESP_LOGI(TAG, "UART%d deinitialized", port_);
}

esp_err_t UartDriver::send_data(const uint8_t* data, size_t len) {
    if (!initialized_) {
        ESP_LOGE(TAG, "UART not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    // UART로 데이터 전송
    int written = uart_write_bytes(port_, data, len);
    if (written < 0) {
        ESP_LOGE(TAG, "UART write error: %s", esp_err_to_name(ESP_FAIL));
        return ESP_FAIL;
    }

    if (written < (int)len) {
        ESP_LOGW(TAG, "Incomplete UART transmission: %d / %u bytes", written, len);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t UartDriver::receive_data(uint8_t* buffer, size_t max_len, size_t& actual_len, uint32_t timeout_ms) {
    if (!initialized_) {
        ESP_LOGE(TAG, "UART not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!buffer || max_len == 0) return ESP_ERR_INVALID_ARG;

    // UART에서 데이터 읽기
    int read_len = uart_read_bytes(port_, buffer, max_len, pdMS_TO_TICKS(timeout_ms));

    if (read_len > 0) {
        actual_len = read_len;
        ESP_LOGD(TAG, "Received %d bytes", read_len);
        return ESP_OK;
    } else if (read_len == 0) {
        actual_len = 0;
        return ESP_ERR_TIMEOUT;
    } else {
        return ESP_FAIL;
    }
}

void UartDriver::uart_rx_task_static(void* arg) {
    // UartDriver 인스턴스 포인터 받기
    UartDriver* driver = reinterpret_cast<UartDriver*>(arg);
    
    uint8_t buf[128];
    uart_event_t event;

    while (true) {
        if (xQueueReceive(driver->event_queue_, (void*)&event, portMAX_DELAY) == pdPASS) {
            bzero(buf, sizeof(buf));

            switch (event.type) {
                case UART_DATA:
                    if (event.size > 0) {
                        int u_len = uart_read_bytes(driver->port_, buf, event.size, pdMS_TO_TICKS(100));
                        if (u_len > 0) {
                            ESP_LOGD(TAG, "UART data received: %d bytes", u_len);
                            // 데이터 처리는 외부에서 이루어짐
                        }
                    }
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "HW FIFO Overflow");
                    uart_flush_input(driver->port_);
                    xQueueReset(driver->event_queue_);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "Ring Buffer Full");
                    uart_flush_input(driver->port_);
                    xQueueReset(driver->event_queue_);
                    break;

                case UART_BREAK:
                case UART_PARITY_ERR:
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "UART Communication Error (type: %d)", event.type);
                    break;

                default:
                    ESP_LOGI(TAG, "Unhandled UART event: %d", event.type);
                    break;
            }
        }
    }
}

} // namespace Drivers
