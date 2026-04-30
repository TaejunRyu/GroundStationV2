#pragma once

#include "singleton_base.h"
#include "esp_timer.h"
#include "esp_err.h"

namespace drone {

class TimerService : public Singleton<TimerService> {
    friend class Singleton<TimerService>;

private:
    TimerService();
    ~TimerService();

    esp_timer_handle_t _periodic_timer;
    uint64_t _start_time;

    // 하드웨어 타이머 콜백 (정적 메서드)
    static void periodic_timer_callback(void* arg);

public:
    // Singleton::init()에서 호출될 로직
    esp_err_t init();
    
    // 100ms 주기 타이머 시작/정지
    esp_err_t start_100ms_loop();
    void stop_100ms_loop();

    // 시간 측정 유틸리티
    uint64_t micros() const { return esp_timer_get_time() - _start_time; }
    float seconds() const { return (float)micros() / 1000000.0f; }

    /**
     * @brief 루프 간 dt 계산을 위한 헬퍼 클래스
     */
    class DeltaTimer {
    public:
        DeltaTimer() { reset(); }
        void reset() { last_time = esp_timer_get_time(); }
        float getDelta() {
            uint64_t now = esp_timer_get_time();
            float dt = (float)(now - last_time) / 1000000.0f;
            last_time = now;
            return (dt <= 0.0f || dt > 0.1f) ? 0.001f : dt; // 방어 로직
        }
    private:
        uint64_t last_time;
    };
};

} // namespace drone
