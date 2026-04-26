#include "flysky_sensor.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <cstring>

namespace Drivers {

const char* FlyskySensor::TAG = "FLYSKY";

FlyskySensor::FlyskySensor(uart_port_t port)
    : port_(port), uart_baud_(BAUD_RATE), tx_pin_(GPIO_NUM_17), rx_pin_(GPIO_NUM_18),
      initialized_(false), is_connected_(false), rx_task_handle_(nullptr), data_queue_(nullptr) {
    memset(rc_values_, 0, sizeof(rc_values_));
    ESP_LOGI(TAG, "FlyskySensor created for UART%d", port_);
}

FlyskySensor::~FlyskySensor() {
    deinitialize();
}

void FlyskySensor::set_uart_pins(gpio_num_t tx, gpio_num_t rx) {
    tx_pin_ = tx;
    rx_pin_ = rx;
}

esp_err_t FlyskySensor::initialize() {
    if (initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing FlyskySensor on UART%d...", port_);

    // UART 드라이버 설정
    uart_config_t uart_config = {};
    uart_config.baud_rate = uart_baud_;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    // 데이터 큐 생성
    data_queue_ = xQueueCreate(10, sizeof(uint8_t) * IBUS_PACKET_SIZE);
    if (!data_queue_) {
        ESP_LOGE(TAG, "Failed to create data queue");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret;

    // UART 드라이버 설치
    ret = uart_driver_install(port_, UART_BUF_SIZE * 2, 0, 30, &data_queue_, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver installation failed: %s", esp_err_to_name(ret));
        vQueueDelete(data_queue_);
        data_queue_ = nullptr;
        return ret;
    }

    // UART 설정 적용
    ret = uart_param_config(port_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART configuration failed: %s", esp_err_to_name(ret));
        uart_driver_delete(port_);
        return ret;
    }

    // 핀 설정
    ret = uart_set_pin(port_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART pin configuration failed: %s", esp_err_to_name(ret));
        uart_driver_delete(port_);
        return ret;
    }

    // 수신 태스크 생성
    ret = xTaskCreate(flysky_rx_task_static, "flysky_rx", 2048, this, 5, &rx_task_handle_);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        uart_driver_delete(port_);
        return ESP_ERR_NO_MEM;
    }

    initialized_ = true;
    is_connected_ = true;
    ESP_LOGI(TAG, "FlyskySensor initialized successfully");
    return ESP_OK;
}

void FlyskySensor::deinitialize() {
    if (!initialized_) return;

    if (rx_task_handle_ != nullptr) {
        vTaskDelete(rx_task_handle_);
        rx_task_handle_ = nullptr;
    }

    uart_driver_delete(port_);

    if (data_queue_ != nullptr) {
        vQueueDelete(data_queue_);
        data_queue_ = nullptr;
    }

    initialized_ = false;
    is_connected_ = false;
    ESP_LOGI(TAG, "FlyskySensor deinitialized");
}

uint16_t FlyskySensor::get_channel(uint8_t channel) const {
    if (channel >= IBUS_CHANNELS) return 0;
    return rc_values_[channel];
}

const uint16_t* FlyskySensor::get_all_channels() const {
    return rc_values_;
}

bool FlyskySensor::is_connected() const {
    return initialized_ && is_connected_;
}

esp_err_t FlyskySensor::parse_ibus_packet(const uint8_t* packet, size_t len) {
    if (!packet || len != IBUS_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    // iBUS 패킷 헤더 확인
    if (packet[0] != 0x20 || packet[1] != 0x40) {
        ESP_LOGD(TAG, "Invalid iBUS packet header");
        return ESP_ERR_INVALID_STATE;
    }

    // 체크섬 검증
    uint16_t calc_cksum = 0xFFFF;
    for (int j = 0; j < 30; j++) {
        calc_cksum -= packet[j];
    }
    uint16_t rx_cksum = packet[30] | (packet[31] << 8);

    if (calc_cksum != rx_cksum) {
        ESP_LOGD(TAG, "iBUS checksum mismatch");
        return ESP_ERR_INVALID_CRC;
    }

    // 채널 데이터 파싱 (14 채널)
    for (int ch = 0; ch < IBUS_CHANNELS; ch++) {
        rc_values_[ch] = packet[2 + ch * 2] | (packet[3 + ch * 2] << 8);
    }

    ESP_LOGD(TAG, "iBUS packet parsed successfully");
    return ESP_OK;
}

void FlyskySensor::flysky_rx_task_static(void* arg) {
    FlyskySensor* sensor = reinterpret_cast<FlyskySensor*>(arg);

    uint8_t temp_buf[128];
    uint8_t packet[IBUS_PACKET_SIZE];
    int p_idx = 0;
    uart_event_t event;

    while (true) {
        if (xQueueReceive(sensor->data_queue_, (void*)&event, portMAX_DELAY) == pdPASS) {
            if (event.type == UART_DATA && event.size > 0) {
                int len = uart_read_bytes(sensor->port_, temp_buf, sizeof(temp_buf), 0);

                for (int i = 0; i < len; i++) {
                    uint8_t b = temp_buf[i];

                    // 상태 머신 기반 동기화
                    if (p_idx == 0) {
                        if (b == 0x20) {
                            packet[p_idx++] = b;
                        }
                    } else if (p_idx == 1) {
                        if (b == 0x40) {
                            packet[p_idx++] = b;
                        } else {
                            p_idx = 0;  // 동기화 실패 시 리셋
                        }
                    } else {
                        packet[p_idx++] = b;

                        if (p_idx == IBUS_PACKET_SIZE) {
                            // 패킷 파싱 및 처리
                            if (sensor->parse_ibus_packet(packet, IBUS_PACKET_SIZE) == ESP_OK) {
                                ESP_LOGD(TAG, "iBUS channels: %u %u %u %u",
                                        sensor->rc_values_[0], sensor->rc_values_[1],
                                        sensor->rc_values_[2], sensor->rc_values_[3]);
                            }
                            p_idx = 0;
                        }
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART buffer overflow");
                uart_flush_input(sensor->port_);
                xQueueReset(sensor->data_queue_);
                p_idx = 0;
            }
        }
    }
}

} // namespace Drivers
