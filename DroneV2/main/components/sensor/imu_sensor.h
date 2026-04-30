#pragma once

#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "core/singleton_base.h"

namespace drone {

/**
 * @brief ICM-20948 9축 센서 데이터 구조체
 */
struct ImuData {
    struct { float x, y, z; } acc;
    struct { float x, y, z; } gyro;
    struct { float x, y, z; } mag;
    float temp;
    uint64_t timestamp;
};

/**
 * @brief IMU 관리 클래스 (SPI 기반)
 * - 드론에 최적화된 효율적인 싱글톤
 * - 명시적 init()/deinit()으로 초기화 순서 제어
 * - 스레드 안전 데이터 접근
 */
class IMUManager : public Singleton<IMUManager> {
    friend class Singleton<IMUManager>;

public:
    esp_err_t init(spi_host_device_t host, int cs_pin);
    void update();
    ImuData getLatestData();
    bool isHealthy() const { return _is_initialized; }
    void deinit();

private:
    IMUManager();
    ~IMUManager();

    esp_err_t writeRegister(uint8_t bank, uint8_t reg, uint8_t data);
    uint8_t readRegister(uint8_t bank, uint8_t reg);
    esp_err_t readBurst(uint8_t bank, uint8_t reg, uint8_t* data, size_t len);
    void selectBank(uint8_t bank);

    static esp_err_t staticInit(spi_host_device_t host, int cs_pin);

    spi_device_handle_t _spi_handle = nullptr;
    ImuData _current_data;
    uint8_t _current_bank = 99;
    bool _is_initialized = false;

    const uint8_t REG_BANK_SEL = 0x7F;
    const float ACCEL_SCALE_2G = 16384.0f;
    const float GYRO_SCALE_250DPS = 131.0f;
};

} // namespace drone
