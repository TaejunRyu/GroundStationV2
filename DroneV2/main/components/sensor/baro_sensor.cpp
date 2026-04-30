#include "baro_manager.h"

#include <cmath>
#include "esp_log.h"
#include "esp_err.h"

static const char* TAG = "BARO";

// Singleton 템플릿 특수화
BaroManager* BaroManager::_instance = nullptr;
void* BaroManager::_mutex = nullptr;

BaroManager::BaroManager() : _port(0), _addr(0), _initialized(false) {}

BaroManager::~BaroManager() {}

esp_err_t BaroManager::init(i2c_port_t port, uint8_t addr) {
    if (_instance != nullptr) {
        return ESP_OK;
    }
    _instance = new BaroManager();
    _instance->_port = port;
    _instance->_addr = addr;
    return _instance->init(port, addr);
}

esp_err_t BaroManager::init(i2c_port_t port, uint8_t addr) {
    _port = port;
    _addr = addr;

    // BMP280 soft reset
    uint8_t cmd[2] = {0xE0, 0xB6};
    i2c_master_write_to_device(port, addr, cmd, 2, pdMS_TO_TICKS(100));

    // WHO_AM_I 확인
    uint8_t who = 0xD0;
    uint8_t resp = 0;
    i2c_master_read_from_device(port, addr, &who, 1, &resp, 1, pdMS_TO_TICKS(100));
    if (resp != 0x58) { // BMP280 ID
        ESP_LOGW(TAG, "BMP280 아님: 0x%02X", resp);
    }

    // 캘리브레이션 데이터 읽기
    readCalibration();

    // 정상 모드 설정 ( Oversampling x16, standby 62.5ms )
    uint8_t config[2] = {0xF2, 0x01}; // osrs_t = 1
    i2c_master_write_to_device(port, addr, config, 2, pdMS_TO_TICKS(10));
    config[0] = 0xF4;
    config[1] = 0x27; // osrs_p = 1, mode = normal
    i2c_master_write_to_device(port, addr, config, 2, pdMS_TO_TICKS(10));
    config[0] = 0xF5;
    config[1] = 0xA0; // t_sb = 5, filter = 16
    i2c_master_write_to_device(port, addr, config, 2, pdMS_TO_TICKS(10));

    _initialized = true;
    ESP_LOGI(TAG, "BaroManager 초기화 완료");
    return ESP_OK;
}

esp_err_t BaroManager::readCalibration() {
    uint8_t reg = 0x88;
    uint8_t data[24];
    i2c_master_read_from_device(_port, _addr, &reg, 1, data, 24, pdMS_TO_TICKS(100));

    _dig_T1 = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    _dig_T2 = (int16_t)data[2] | ((int16_t)data[3] << 8);
    _dig_T3 = (int16_t)data[4] | ((int16_t)data[5] << 8);
    _dig_P1 = (uint16_t)data[6] | ((uint16_t)data[7] << 8);
    _dig_P2 = (int16_t)data[8] | ((int16_t)data[9] << 8);
    _dig_P3 = (int16_t)data[10] | ((int16_t)data[11] << 8);
    _dig_P4 = (int16_t)data[12] | ((int16_t)data[13] << 8);
    _dig_P5 = (int16_t)data[14] | ((int16_t)data[15] << 8);
    _dig_P6 = (int16_t)data[16] | ((int16_t)data[17] << 8);
    _dig_P7 = (int16_t)data[18] | ((int16_t)data[19] << 8);
    _dig_P8 = (int16_t)data[20] | ((int16_t)data[21] << 8);
    _dig_P9 = (int16_t)data[22] | ((int16_t)data[23] << 8);

    return ESP_OK;
}

void BaroManager::update() {
    if (!_initialized) return;

    int32_t raw_press, raw_temp;
    readRawData(&raw_press, &raw_temp);

    // 보상된 온도 계산
    int32_t var1 = ((((raw_temp >> 3) - ((int32_t)_dig_T1 << 1))) * (int32_t)_dig_T2) >> 11;
    int32_t var2 = (((((raw_temp >> 4) - (int32_t)_dig_T1) * ((raw_temp >> 4) - (int32_t)_dig_T1)) >> 12) * (int32_t)_dig_T3) >> 14;
    int32_t t_fine = var1 + var2;
    _data.temperature = (float)((t_fine * 5 + 128) >> 8) / 5120.0f;

    // 보상된 압력 계산
    var1 = (int64_t)t_fine - 128000;
    var2 = var1 * var1 * (int64_t)_dig_P6;
    var2 = var2 + ((var1 * (int64_t)_dig_P5) << 17);
    var2 = var2 + ((int64_t)_dig_P4 << 35);
    var1 = ((var1 * var1 * (int64_t)_dig_P3) >> 8) + ((var1 * (int64_t)_dig_P2) << 12);
    var1 = ((((int64_t)1) << 47) + var1) * (int64_t)_dig_P1) >> 33;
    if (var1 == 0) return;

    int64_t p = ((((int64_t)raw_press << 35) - (int64_t)_dig_P7 << 20) - ((int64_t)_dig_P9 * ((raw_press >> 13) * (raw_press >> 13) >> 25))) / 16 + (var1 >> 4) + ((int64_t)_dig_P8 << 4);
    _data.pressure = (float)p / 256000.0f;
    _data.altitude = calculateAltitude(_data.pressure);
    _data.timestamp = esp_timer_get_time();
}

float BaroManager::calculateAltitude(float pressure) {
    // 바омет릭 고도 공식
    return 44330.0f * (1.0f - pow(pressure / _seaLevelPressure, 0.1903f));
}

BaroData_t BaroManager::getData() {
    return _data;
}

void BaroManager::calibrate(float seaLevelPressure) {
    _seaLevelPressure = seaLevelPressure;
    ESP_LOGI(TAG, "해면압력 보정: %.2f hPa", seaLevelPressure);
}

void BaroManager::deinit() {
    if (_instance != nullptr) {
        delete _instance;
        _instance = nullptr;
    }
}