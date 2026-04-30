#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "singleton_base.h"

/**
 * @brief PID 제어기 클래스
 * - 드론 비행 제어에 최적화
 * - Anti-windup 내장
 */
class PID {
public:
    float kp = 0, ki = 0, kd = 0;
    float target = 0;
    float integral = 0;
    float lastError = 0;
    float outMax = 1.0f;

    PID() = default;
    PID(float p, float i, float d, float max) 
        : kp(p), ki(i), kd(d), outMax(max) {}

    void configure(float p, float i, float d, float max) {
        kp = p; ki = i; kd = d; outMax = max;
    }

    float calculate(float current, float dt) {
        if (dt <= 0) return 0;
        float error = target - current;
        integral += error * dt;
        
        // Anti-windup
        if (integral > outMax) integral = outMax;
        else if (integral < -outMax) integral = -outMax;

        float derivative = (error - lastError) / dt;
        lastError = error;

        float output = (kp * error) + (ki * integral) + (kd * derivative);

        // 출력 제한
        if (output > outMax) output = outMax;
        else if (output < -outMax) output = -outMax;

        return output;
    }

    void reset() { integral = 0; lastError = 0; }
};

/**
 * @brief PID 관리자 클래스
 * - Roll, Pitch, Yaw 축별 PID 제어
 */
class PIDManager : public Singleton<PIDManager> {
    friend class Singleton<PIDManager>;

public:
    void configure(float roll_p, float roll_i, float roll_d,
                   float pitch_p, float pitch_i, float pitch_d,
                   float yaw_p, float yaw_i, float yaw_d);
    void update(float r_curr, float p_curr, float y_curr, float dt, 
                float* r_out, float* p_out, float* y_out);
    void reset();
    void deinit();

private:
    PIDManager();
    ~PIDManager();

    PID _rollPID;
    PID _pitchPID;
    PID _yawPID;
};

#endif
