#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include "driver/mcpwm_prelude.h"
#include "esp_err.h"
#include "singleton_base.h"

/**
 * @brief 모터 드라이버 클래스
 * - 드론에 최적화된 효율적인 싱글톤
 * - MCPWM 기반 4채널 PWM 출력
 * - OneShot100 프로토콜 지원
 */
class MotorDriver : public Singleton<MotorDriver> {
    friend class Singleton<MotorDriver>;

public:
    esp_err_t init(int m1_pin, int m2_pin, int m3_pin, int m4_pin);
    void setOutputs(float m1, float m2, float m3, float m4);
    void stopAll();
    void deinit();

private:
    MotorDriver();
    ~MotorDriver();

    mcpwm_timer_handle_t _timer = nullptr;
    mcpwm_oper_handle_t _operators[4] = {nullptr};
    mcpwm_cmpr_handle_t _comparators[4] = {nullptr};
    mcpwm_gen_handle_t _generators[4] = {nullptr};

    const uint32_t MIN_PULSE = 1000;
    const uint32_t MAX_PULSE = 2000;
};

#endif
