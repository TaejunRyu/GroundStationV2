#include "ahrs_manager.h"

#include <cmath>
#include "esp_log.h"

static const char* TAG = "AHRS";

// Singleton 템플릿 특수화
AHRSManager* AHRSManager::_instance = nullptr;
void* AHRSManager::_mutex = nullptr;

AHRSManager::AHRSManager() 
    : _twoKp(1.0f), _twoKi(0.0f),
      _q0(1.0f), _q1(0.0f), _q2(0.0f), _q3(0.0f),
      _integralFBx(0.0f), _integralFBy(0.0f), _integralFBz(0.0f),
      _roll(0), _pitch(0), _yaw(0), _last_update(0) {}

AHRSManager::~AHRSManager() {}

esp_err_t AHRSManager::init() {
    if (_instance != nullptr) {
        return ESP_OK;
    }
    _instance = new AHRSManager();
    return ESP_OK;
}

void AHRSManager::configure(float kp, float ki) {
    _twoKp = 2.0f * kp;
    _twoKi = 2.0f * ki;
}

void AHRSManager::update(const ImuData_t& imu, float dt) {
    float ax = imu.acc.x, ay = imu.acc.y, az = imu.acc.z;
    float gx = imu.gyro.x * (M_PI / 180.0f);
    float gy = imu.gyro.y * (M_PI / 180.0f);
    float gz = imu.gyro.z * (M_PI / 180.0f);

    // 가속도 정규화
    float recipNorm = 1.0f / sqrt(ax * ax + ay * ay + az * az);
    ax *= recipNorm; ay *= recipNorm; az *= recipNorm;

    // 중력 방향 예측
    float vx = 2.0f * (_q1 * _q3 - _q0 * _q2);
    float vy = 2.0f * (_q0 * _q1 + _q2 * _q3);
    float vz = _q0 * _q0 - _q1 * _q1 - _q2 * _q2 + _q3 * _q3;

    // 오차 계산
    float ex = (ay * vz - az * vy);
    float ey = (az * vx - ax * vz);
    float ez = (ax * vy - ay * vx);

    // 적분 (Ki)
    if (_twoKi > 0.0f) {
        _integralFBx += _twoKi * ex * dt;
        _integralFBy += _twoKi * ey * dt;
        _integralFBz += _twoKi * ez * dt;
        gx += _integralFBx; gy += _integralFBy; gz += _integralFBz;
    }
    
    // 비례 (Kp)
    gx += _twoKp * ex; gy += _twoKp * ey; gz += _twoKp * ez;

    // 쿼터니언 미분
    _q0 += (-_q1 * gx - _q2 * gy - _q3 * gz) * (0.5f * dt);
    _q1 += (_q0 * gx + _q2 * gz - _q3 * gy) * (0.5f * dt);
    _q2 += (_q0 * gy - _q1 * gz + _q3 * gx) * (0.5f * dt);
    _q3 += (_q0 * gz + _q1 * gy - _q2 * gx) * (0.5f * dt);

    // 정규화
    recipNorm = 1.0f / sqrt(_q0 * _q0 + _q1 * _q1 + _q2 * _q2 + _q3 * _q3);
    _q0 *= recipNorm; _q1 *= recipNorm; _q2 *= recipNorm; _q3 *= recipNorm;

    // 오일러 각도
    _roll = atan2(2.0f * (_q0 * _q1 + _q2 * _q3), 1.0f - 2.0f * (_q1 * _q1 + _q2 * _q2)) * (180.0f / M_PI);
    _pitch = asin(2.0f * (_q0 * _q2 - _q3 * _q1)) * (180.0f / M_PI);
    _yaw = atan2(2.0f * (_q0 * _q3 + _q1 * _q2), 1.0f - 2.0f * (_q2 * _q2 + _q3 * _q3)) * (180.0f / M_PI);
    
    _last_update = imu.timestamp;
}

void AHRSManager::getEuler(float* roll, float* pitch, float* yaw) {
    if (roll) *roll = _roll;
    if (pitch) *pitch = _pitch;
    if (yaw) *yaw = _yaw;
}

void AHRSManager::getQuaternion(float* q0, float* q1, float* q2, float* q3) {
    if (q0) *q0 = _q0;
    if (q1) *q1 = _q1;
    if (q2) *q2 = _q2;
    if (q3) *q3 = _q3;
}

void AHRSManager::reset() {
    _q0 = 1.0f; _q1 = 0.0f; _q2 = 0.0f; _q3 = 0.0f;
    _integralFBx = _integralFBy = _integralFBz = 0.0f;
    _roll = _pitch = _yaw = 0;
}

void AHRSManager::deinit() {
    if (_instance != nullptr) {
        delete _instance;
        _instance = nullptr;
    }
}
