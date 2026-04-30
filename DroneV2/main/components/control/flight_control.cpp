#include "flight_controller.h"

#include "esp_log.h"
#include "esp_err.h"

static const char* TAG = "FLIGHT_CTL";

// Singleton 템플릿 특수화
FlightController* FlightController::_instance = nullptr;
void* FlightController::_mutex = nullptr;

FlightController::FlightController() 
    : _mode(FLIGHT_MODE_STABILIZE), _armed(false), _is_flying(false),
      _input_roll(0), _input_pitch(0), _input_yaw(0), _input_throttle(0) {}

FlightController::~FlightController() {
    if (_armed) {
        MotorDriver::getInstance()->stopAll();
    }
}

esp_err_t FlightController::init() {
    if (_instance != nullptr) {
        return ESP_OK;
    }
    _instance = new FlightController();
    ESP_LOGI(TAG, "FlightController 초기화 완료");
    return ESP_OK;
}

void FlightController::update(float dt) {
    if (!_armed) return;

    // AHRS에서 자세 데이터 가져오기 (여기서는 IMU에서 직접)
    ImuData_t imu_data = IMUManager::getInstance()->getLatestData();
    
    // 간단한 자이로 기반 자세 계산
    _flight_data.roll += imu_data.gyro.x * dt;
    _flight_data.pitch += imu_data.gyro.y * dt;
    _flight_data.yaw += imu_data.gyro.z * dt;

    // 고도 업데이트
    if (BaroManager::getInstance()->isHealthy()) {
        _flight_data.altitude = BaroManager::getInstance()->getData().altitude;
    }

    // 안전 검사
    applySafetyChecks();

    // PID 제어
    float roll_out, pitch_out, yaw_out;
    PIDManager::getInstance()->update(
        _flight_data.roll - _input_roll,
        _flight_data.pitch - _input_pitch,
        _flight_data.yaw - _input_yaw,
        dt, &roll_out, &pitch_out, &yaw_out
    );

    // 모터 믹싱
    mixMotors(roll_out, pitch_out, yaw_out, _input_throttle);
}

void FlightController::mixMotors(float roll, float pitch, float yaw, float throttle) {
    // X형 프레임 모터 배치
    //   M1      M2
    //      ↑    
    //   M4      M3
    
    float m1 = throttle - roll + pitch + yaw;
    float m2 = throttle + roll + pitch - yaw;
    float m3 = throttle + roll - pitch + yaw;
    float m4 = throttle - roll - pitch - yaw;

    // 클리핑
    m1 = fmaxf(0, fminf(1, m1));
    m2 = fmaxf(0, fminf(1, m2));
    m3 = fmaxf(0, fminf(1, m3));
    m4 = fmaxf(0, fminf(1, m4));

    MotorDriver::getInstance()->setOutputs(m1, m2, m3, m4);
}

void FlightController::setControlInput(float roll, float pitch, float yaw, float throttle) {
    _input_roll = roll;
    _input_pitch = pitch;
    _input_yaw = yaw;
    _input_throttle = throttle;
}

void FlightController::applySafetyChecks() {
    // 각도 제한
    if (fabs(_flight_data.roll) > MAX_ANGLE || fabs(_flight_data.pitch) > MAX_ANGLE) {
        ESP_LOGW(TAG, "각도 제한 초과!");
        _input_throttle = 0;
    }

    // 배터리 부족
    if (_flight_data.battery < MIN_BATTERY) {
        ESP_LOGW(TAG, "배터리 부족!");
        emergencyStop();
    }
}

float FlightController::calculateThrottle(float target_alt, float current_alt, float dt) {
    float error = target_alt - current_alt;
    static float integral = 0;
    integral += error * dt;
    
    // Anti-windup
    if (integral > 100) integral = 100;
    if (integral < -100) integral = -100;

    return 0.5f + (error * 0.01f) + (integral * 0.001f);
}

bool FlightController::arm() {
    if (!_armed) {
        // 자이로 캘리브레이션 확인
        if (!IMUManager::getInstance()->isHealthy()) {
            ESP_LOGE(TAG, "IMU 오류 - 시동 불가");
            return false;
        }
        _armed = true;
        _is_flying = false;
        ESP_LOGI(TAG, "시동 ON");
    }
    return true;
}

void FlightController::disarm() {
    if (_armed) {
        MotorDriver::getInstance()->stopAll();
        _armed = false;
        _is_flying = false;
        PIDManager::getInstance()->reset();
        ESP_LOGI(TAG, "시동 OFF");
    }
}

void FlightController::emergencyStop() {
    MotorDriver::getInstance()->stopAll();
    _armed = false;
    _is_flying = false;
    SystemManager::getInstance()->emergencyStop();
    ESP_LOGW(TAG, "비상 정지!");
}

void FlightController::deinit() {
    if (_instance != nullptr) {
        disarm();
        delete _instance;
        _instance = nullptr;
    }
}