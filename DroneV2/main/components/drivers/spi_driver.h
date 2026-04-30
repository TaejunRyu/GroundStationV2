#pragma once

#include "driver/spi_master.h"
#include "esp_err.h"
#include <functional>

namespace drone {
namespace hal {

/**
 * @brief SPI 디바이스 설정
 */
struct SpiConfig {
    spi_host_device_t host = SPI2_HOST;
    int mosi = -1;
    int miso = -1;
    int clk = -1;
    int cs = -1;
    uint32_t clock_hz = 7 * 1000 * 1000; // 7MHz
    uint8_t mode = 3;
    uint8_t queue_size = 5;
};

/**
 * @brief SPI 전송 요청
 */
struct SpiTransaction {
    uint16_t cmd = 0;
    uint16_t addr = 0;
    const uint8_t* tx_data = nullptr;
    uint8_t* rx_data = nullptr;
    size_t length = 0;
    uint32_t timeout_ms = 100;
};

/**
 * @brief SPI 드라이버 클래스
 * - ESP32-S3 SPI 마스터 드라이버
 * - 콜백 기반 비동기 전송 지원
 */
class SpiDriver {
public:
    using TransmitCallback = std::function<void(const uint8_t*, size_t)>;

    SpiDriver();
    ~SpiDriver();

    esp_err_t init(const SpiConfig& config);
    esp_err_t deinit();

    // 동기 전송
    esp_err_t transmit(const SpiTransaction& trans);
    esp_err_t transmit(uint8_t addr, const uint8_t* tx_data, size_t tx_len,
                       uint8_t* rx_data, size_t rx_len);

    // 비동기 전송
    esp_err_t transmitAsync(const SpiTransaction& trans, TransmitCallback callback);

    // 단일 바이트读写
    esp_err_t writeRegister(uint8_t addr, uint8_t data);
    uint8_t readRegister(uint8_t addr);

    bool isInitialized() const { return _initialized; }

private:
    esp_err_t addDevice();

    SpiConfig _config;
    spi_device_handle_t _handle = nullptr;
    bool _initialized = false;
    SemaphoreHandle_t _mutex = nullptr;
};

/**
 * @brief SPI 버스 관리자
 */
class SpiBus {
public:
    static SpiBus& getInstance();

    esp_err_t init(spi_host_device_t host, int mosi, int miso, int clk, int wp = -1, int hd = -1);
    esp_err_t free();

    spi_host_device_t getHost() const { return _host; }
    bool isInitialized() const { return _initialized; }

private:
    SpiBus() : _host(SPI1_HOST), _initialized(false) {}
    ~SpiBus() = default;
    SpiBus(const SpiBus&) = delete;
    SpiBus& operator=(const SpiBus&) = delete;

    spi_host_device_t _host;
    bool _initialized;
};

} // namespace hal
} // namespace drone