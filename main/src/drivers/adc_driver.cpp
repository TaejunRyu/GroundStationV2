#include "adc_driver.h"
#include "esp_adc/adc_cali_scheme.h" // 보정 스킴 헤더 필수
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"


#include <esp_log.h>

namespace Drivers {

const char* AdcDriver::TAG = "ADC";

AdcDriver::AdcDriver()
    : adc1_handle_(nullptr), adc1_cali_handle_(nullptr),
      calibration_enabled_(false), initialized_(false) {
    ESP_LOGI(TAG, "AdcDriver created");
}

AdcDriver::~AdcDriver() {
    deinitialize();
}

esp_err_t AdcDriver::initialize() {
    if (initialized_) return ESP_OK;

    // 1. ADC 유닛 초기화 (동일)
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle_));

    // 2. 채널 설정 (동일)
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, ADC_CHANNEL_0, &config));

    // 3. Calibration 설정 (Line Fitting -> Curve Fitting으로 교체)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,              // GPIO 1
        //전압 범위: 코드에서 ADC_ATTEN_DB_12 설정을 사용 중이므로, 
        //해당 핀에는 최대 약 2.45V ~ 3.1V 사이의 전압까지만 입력 가능합니다 (정확한 권장 범위는 공식 문서 참고).
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    // 함수명 변경: create_scheme_curve_fitting
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle_);
    if (ret == ESP_OK) {
        calibration_enabled_ = true;
        ESP_LOGI(TAG, "ADC Curve Fitting calibration enabled");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed: %s", esp_err_to_name(ret));
        adc1_cali_handle_ = nullptr;
        calibration_enabled_ = false;
    }

    initialized_ = true;
    return ESP_OK;
}

void AdcDriver::deinitialize() {
    if (!initialized_) return;

    // 함수명 변경: delete_scheme_curve_fitting
    if (adc1_cali_handle_ != nullptr) {
        adc_cali_delete_scheme_curve_fitting(adc1_cali_handle_);
        adc1_cali_handle_ = nullptr;
    }

    if (adc1_handle_ != nullptr) {
        adc_oneshot_del_unit(adc1_handle_);
        adc1_handle_ = nullptr;
    }

    initialized_ = false;
}

esp_err_t AdcDriver::read_raw(adc_channel_t channel, int& raw_value) {
    if (!initialized_) {
        ESP_LOGE(TAG, "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = adc_oneshot_read(adc1_handle_, channel, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "ADC raw value: %d", raw_value);
    return ESP_OK;
}

esp_err_t AdcDriver::read_voltage(adc_channel_t channel, int& voltage_mv) {
    if (!initialized_) {
        ESP_LOGE(TAG, "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    int raw_value;
    esp_err_t ret = adc_oneshot_read(adc1_handle_, channel, &raw_value);
    if (ret != ESP_OK) {
        return ret;
    }

    if (calibration_enabled_) {
        ret = adc_cali_raw_to_voltage(adc1_cali_handle_, raw_value, &voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Voltage conversion failed: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        // 보정이 없을 경우 기본 수식 계산
        voltage_mv = (raw_value * 3300) / 4095;
    }

    ESP_LOGD(TAG, "ADC voltage: %d mV", voltage_mv);
    return ESP_OK;
}

uint16_t AdcDriver::get_battery_voltage_mv() {
    int voltage_mv = 0;
    esp_err_t ret = read_voltage(ADC_CHANNEL_0, voltage_mv);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read battery voltage");
        return 0;
    }

    // 2:1 분압이므로 실제 전압은 읽은 값의 2배
    uint16_t battery_voltage = (uint16_t)(voltage_mv * 2);
    ESP_LOGD(TAG, "Battery voltage: %u mV", battery_voltage);
    return battery_voltage;
}

} // namespace Drivers
