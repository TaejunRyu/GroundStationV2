#pragma once

#include "driver/i2c.h"
#include "esp_err.h"
#include <functional>

namespace drone {
namespace hal {

/**
 * @brief I2C 디바이스 설정
 */
struct I2cConfig {
    i2c_port_t port = I2C_NUM_0;
    uint8_t addr = 0x68;
    uint32_t clock_hz = 400000; // 400kHz
    bool addr_10bit = false;
    uint32_t timeout_ms = 100;
};

/**
 * @brief I2C 전송 요청
 */
struct I2cTransaction {
    uint8_t addr;
    i2c_cmd_handle_t cmd;
};

/**
 * @brief I2C 드라이버 클래스
 * - ESP32-S3 I2C 마스터 드라이버
 * -阻塞/비동기 전송 지원
 */
class I2cDriver {
public:
    using ReadCallback = std::function<void(const uint8_t*, size_t)>;
    using WriteCallback = std::function<void(esp_err_t)>;

    I2cDriver();
    ~I2cDriver();

    esp_err_t init(const I2cConfig& config);
    esp_err_t deinit();

    // 동기读写
    esp_err_t write(uint8_t addr, const uint8_t* data, size_t len);
    esp_err_t read(uint8_t addr, uint8_t* data, size_t len);
    esp_err_t writeRegister(uint8_t dev_addr, uint8_t reg, uint8_t data);
    uint8_t readRegister(uint8_t dev_addr, uint8_t reg);
    esp_err_t writeRegister16(uint8_t dev_addr, uint8_t reg, uint16_t data);

    // 버스 스캔
    esp_err_t scan(uint8_t* found_addrs, size_t* count, size_t max_count);

    bool isInitialized() const { return _initialized; }
    i2c_port_t getPort() const { return _config.port; }

private:
    I2cConfig _config;
    bool _initialized = false;
    SemaphoreHandle_t _mutex = nullptr;
};

/**
 * @brief I2C 버스 관리자
 */
class I2cBus {
public:
    static I2cBus& getInstance(i2c_port_t port = I2cPort::I2C_NUM_0);

    esp_err_t init(i2c_port_t port, int sda, int scl, uint32_t clock_hz = 400000);
    esp_err_t free(i2c_port_t port);

    bool isInitialized(i2c_port_t port) const;
    I2cDriver* getDriver(i2c_port_t port);

private:
    I2cBus();
    ~I2cBus() = default;
    I2cBus(const I2cBus&) = delete;
    I2cBus& operator=(const I2cBus&) = delete;

    struct BusInfo {
        bool initialized = false;
        I2cDriver driver;
    };
    BusInfo _buses[I2C_NUM_MAX];
};

} // namespace hal
} // namespace drone