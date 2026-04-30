#pragma once

#include "esp_err.h"
#include "singleton_base.h"
#include "driver/i2c.h"

/**
 * @brief 기압 계측 데이터
 */
typedef struct {
    float pressure;    // hPa
    float temperature; // Celsius
    float altitude;   // 기준 고도 대비 m
    uint64_t timestamp;
} BaroData_t;

/**
 * @brief 기압 계管理器
 * - I2C 기반 BMP280/MS5611 지원
 * - 고도 계산 (표준 대기)
 */
class BaroManager : public Singleton<BaroManager> {
    friend class Singleton<BaroManager>;

public:
    esp_err_t init(i2c_port_t port, uint8_t addr);
    void update();
    BaroData_t getData();
    bool isHealthy() const { return _initialized; }
    void calibrate(float seaLevelPressure);
    void deinit();

private:
    BaroManager();
    ~BaroManager();

    esp_err_t readCalibration();
    esp_err_t readRawData(int32_t* pressure, int32_t* temperature);
    float calculateAltitude(float pressure);

    i2c_port_t _port = 0;
    uint8_t _addr = 0;
    bool _initialized = false;

    // BMP280 캘리브레이션 파라미터
    uint16_t _dig_T1 = 0;
    int16_t _dig_T2 = 0;
    int16_t _dig_T3 = 0;
    uint16_t _dig_P1 = 0;
    int16_t _dig_P2 = 0;
    int16_t _dig_P3 = 0;
    int16_t _dig_P4 = 0;
    int16_t _dig_P5 = 0;
    int16_t _dig_P6 = 0;
    int16_t _dig_P7 = 0;
    int16_t _dig_P8 = 0;
    int16_t _dig_P9 = 0;

    float _seaLevelPressure = 1013.25f;
    BaroData_t _data;
};

#endif