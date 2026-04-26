#pragma once

#include <cstdint>
#include <esp_err.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>

namespace Drivers {

class AdcDriver {
public:
    AdcDriver();
    ~AdcDriver();

    esp_err_t initialize();
    void deinitialize();

    // ADC 읽기
    esp_err_t read_raw(adc_channel_t channel, int& raw_value);
    esp_err_t read_voltage(adc_channel_t channel, int& voltage_mv);

    // 배터리 전압 읽기 (2:1 분압)
    uint16_t get_battery_voltage_mv();

private:
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    bool calibration_enabled_;
    bool initialized_;
    static const char* TAG;
};

} // namespace Drivers
