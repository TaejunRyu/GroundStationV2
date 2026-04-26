#pragma once

#include <functional>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "bridge_types.h"

namespace Services {

class TimerService {
public:
    TimerService();
    ~TimerService();

    esp_err_t initialize();
    esp_err_t start();
    void stop();

    // 콜백 설정
    void set_timer_callback(std::function<void()> callback);

    // 타이머 상태
    bool is_running() const;

private:
    static void timer_callback(TimerHandle_t xTimer);

    TimerHandle_t timer_handle_;
    std::function<void()> timer_callback_;
    bool initialized_;
    bool running_;

    static const char* TAG;
    static const uint32_t TIMER_PERIOD_MS = 100;  // 100ms
};

} // namespace Services