#ifndef MAHONY_FILTER_H
#define MAHONY_FILTER_H

#include <cmath>
#include "imu_manager.h"
#include "singleton_base.h"

/**
 * @brief AHRS (Attitude and Heading Reference System) 관리자
 * - Mahony 필터 기반 자세 추정
 * - 쿼터니언 → 오일러 각 변환
 */
class AHRSManager : public Singleton<AHRSManager> {
    friend class Singleton<AHRSManager>;

public:
    void configure(float kp, float ki);
    void update(const ImuData_t& imu, float dt);
    void getEuler(float* roll, float* pitch, float* yaw);
    void getQuaternion(float* q0, float* q1, float* q2, float* q3);
    void reset();
    void deinit();

private:
    AHRSManager();
    ~AHRSManager();

    // 필터 계수
    float _twoKp = 1.0f;
    float _twoKi = 0.0f;
    
    // 쿼터니언 상태
    float _q0 = 1.0f, _q1 = 0.0f, _q2 = 0.0f, _q3 = 0.0f;
    float _integralFBx = 0.0f, _integralFBy = 0.0f, _integralFBz = 0.0f;

    // 출력
    float _roll = 0, _pitch = 0, _yaw = 0;
    uint64_t _last_update = 0;
};

#endif
