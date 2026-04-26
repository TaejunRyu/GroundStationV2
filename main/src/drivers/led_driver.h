#pragma once
#include <driver/gpio.h>
#include <esp_log.h>

namespace Drivers {

class LedDriver {
public:
    LedDriver();
    ~LedDriver();

    esp_err_t initialize(gpio_num_t pin = GPIO_NUM_2);
    void set_color(uint8_t red, uint8_t green, uint8_t blue);
    void led_on();
    void led_off();
    void blink(uint32_t interval_ms = 500);

private:
    gpio_num_t pin_;
    bool initialized_;
    static const char* TAG;
};

// 호환성을 위한 기존 네임스페이스 함수들
namespace LED {
    inline constexpr gpio_num_t LED_GPIO_PIN = GPIO_NUM_2;
    extern void init_led(void);
    extern void led_blink_task(void *pvParameters);
    extern void set_color(uint8_t red, uint8_t green, uint8_t blue);
    extern void led_on();
    extern void led_off();
}

} // namespace Drivers