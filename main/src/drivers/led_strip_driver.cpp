#include "led_strip_driver.h"
#include <esp_log.h>

namespace Drivers {

const char* LedStripDriver::TAG = "LED_STRIP";

LedStripDriver::LedStripDriver(uint8_t gpio_num, uint8_t max_leds)
    : strip_handle_(nullptr), gpio_num_(gpio_num), max_leds_(max_leds), initialized_(false) {
    ESP_LOGI(TAG, "LedStripDriver created (GPIO: %u, Max LEDs: %u)", gpio_num_, max_leds_);
}

LedStripDriver::~LedStripDriver() {
    deinitialize();
}

esp_err_t LedStripDriver::initialize() {
    if (initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing LED strip on GPIO %u...", gpio_num_);

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio_num_;
    strip_config.max_leds = max_leds_;
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.flags.invert_out = false;

    // RMT 백엔드 설정
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000;  // 10MHz

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED strip initialization failed: %s", esp_err_to_name(ret));
        strip_handle_ = nullptr;
        return ret;
    }

    if (strip_handle_ == nullptr) {
        ESP_LOGE(TAG, "LED strip handle is null");
        return ESP_FAIL;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "LED strip initialized successfully");
    return ESP_OK;
}

void LedStripDriver::deinitialize() {
    if (!initialized_) return;

    if (strip_handle_ != nullptr) {
        led_strip_del(strip_handle_);
        strip_handle_ = nullptr;
    }

    initialized_ = false;
    ESP_LOGI(TAG, "LED strip deinitialized");
}

void LedStripDriver::set_pixel(uint32_t index, uint32_t red, uint32_t green, uint32_t blue) {
    if (!initialized_ || strip_handle_ == nullptr) return;

    if (index >= max_leds_) {
        ESP_LOGW(TAG, "Pixel index out of range: %u >= %u", index, max_leds_);
        return;
    }

    led_strip_set_pixel(strip_handle_, index, red, green, blue);
}

void LedStripDriver::update() {
    if (!initialized_ || strip_handle_ == nullptr) return;

    led_strip_refresh(strip_handle_);
}

void LedStripDriver::clear() {
    if (!initialized_ || strip_handle_ == nullptr) return;

    led_strip_clear(strip_handle_);
}

void LedStripDriver::set_status_ok() {
    if (!initialized_) return;

    set_pixel(0, 0, 20, 0);  // 녹색
    update();
}

void LedStripDriver::set_status_error() {
    if (!initialized_) return;

    set_pixel(0, 20, 0, 0);  // 빨간색
    update();
}

void LedStripDriver::set_status_busy() {
    if (!initialized_) return;

    set_pixel(0, 0, 0, 20);  // 파란색
    update();
}

void LedStripDriver::set_status_warning() {
    if (!initialized_) return;

    set_pixel(0, 20, 20, 0);  // 노란색
    update();
}

} // namespace Drivers
