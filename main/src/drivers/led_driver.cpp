#include "led_driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace Drivers {

const char* LedDriver::TAG = "LED_DRIVER";

LedDriver::LedDriver() : pin_(GPIO_NUM_2), initialized_(false) {
    ESP_LOGI(TAG, "LedDriver created");
}

LedDriver::~LedDriver() {
    led_off();
    ESP_LOGI(TAG, "LedDriver destroyed");
}

esp_err_t LedDriver::initialize(gpio_num_t pin) {
    if (initialized_) return ESP_OK;

    pin_ = pin;

    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << pin_),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&gpio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(pin_, 0);
    initialized_ = true;

    ESP_LOGI(TAG, "LedDriver initialized on GPIO %d", pin_);
    return ESP_OK;
}

void LedDriver::set_color(uint8_t red, uint8_t green, uint8_t blue) {
    // ESP32-S3의 내장 LED는 단색이므로, 색상 값에 따라 밝기 조절
    if (!initialized_) return;

    // 간단한 색상 매핑: Red > Green > Blue 우선순위
    if (red > 0) {
        gpio_set_level(pin_, 1);
    } else if (green > 0) {
        gpio_set_level(pin_, 1);
    } else if (blue > 0) {
        gpio_set_level(pin_, 1);
    } else {
        gpio_set_level(pin_, 0);
    }
}

void LedDriver::led_on() {
    if (initialized_) {
        gpio_set_level(pin_, 1);
    }
}

void LedDriver::led_off() {
    if (initialized_) {
        gpio_set_level(pin_, 0);
    }
}

void LedDriver::blink(uint32_t interval_ms) {
    if (!initialized_) return;

    static bool state = false;
    state = !state;
    gpio_set_level(pin_, state ? 1 : 0);
}

// 호환성을 위한 기존 함수들
namespace LED {

void init_led(void) {
    static LedDriver led_driver;
    led_driver.initialize(LED_GPIO_PIN);
}

void led_blink_task(void *pvParameters) {
    static LedDriver led_driver;
    led_driver.initialize(LED_GPIO_PIN);

    while (1) {
        led_driver.blink(500);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void set_color(uint8_t red, uint8_t green, uint8_t blue) {
    static LedDriver led_driver;
    if (!led_driver.initialize(LED_GPIO_PIN)) {
        led_driver.set_color(red, green, blue);
    }
}

void led_on() {
    static LedDriver led_driver;
    if (!led_driver.initialize(LED_GPIO_PIN)) {
        led_driver.led_on();
    }
}

void led_off() {
    static LedDriver led_driver;
    if (!led_driver.initialize(LED_GPIO_PIN)) {
        led_driver.led_off();
    }
}

} // namespace LED

} // namespace Drivers