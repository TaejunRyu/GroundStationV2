#pragma once

#include <cstdint>
#include <esp_err.h>
#include <led_strip.h>

namespace Drivers {

class LedStripDriver {
public:
    LedStripDriver(uint8_t gpio_num = 48, uint8_t max_leds = 1);
    ~LedStripDriver();

    esp_err_t initialize();
    void deinitialize();

    // 색상 제어
    void set_pixel(uint32_t index, uint32_t red, uint32_t green, uint32_t blue);
    void update();  // 변경사항 적용
    void clear();   // 모든 LED 끄기

    // 상태 표시
    void set_status_ok();      // 녹색 - 정상
    void set_status_error();   // 빨간색 - 에러
    void set_status_busy();    // 파란색 - 처리 중
    void set_status_warning(); // 노란색 - 경고

private:
    led_strip_handle_t strip_handle_;
    uint8_t gpio_num_;
    uint8_t max_leds_;
    bool initialized_;
    static const char* TAG;
};

} // namespace Drivers
