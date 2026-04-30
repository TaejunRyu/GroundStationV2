#pragma once

#include "esp_err.h"
#include "singleton_base.h"
#include "pid_controller.h"
#include "imu_manager.h"
#include "motor_controller.h"
#include "baro_manager.h"

/**
 * @brief 비행 모드
 */
typedef enum {
    FLIGHT_MODE_STABILIZE = 0,  // 안정화 모드
    FLIGHT_MODE_ALTITUDE_HOLD,   // 고도 유지
    FLIGHT_MODE_POSITION_HOLD,   // 위치 유지
    FLIGHT_MODE_RETURN_HOME,     // 귀환
    FLIGHT_MODE_LANDING          // 착륙
} FlightMode_t;

/**
 * @brief 비행 데이터
 */
typedef struct {
    float roll;      // 현재 Roll 각도
    float pitch;     // 현재 Pitch 각도
    float yaw;       // 현재 Yaw 각도
    float altitude;  // 현재 고도
    float vx, vy, vz; // 속도
    float battery;   // 배터리 전압
} FlightData_t;

/**
 * @brief 비행 제어기
 * - PID 제어 기반 안정화
 * - 모터 믹싱
 * - 안전 장치
 */
class FlightController : public Singleton<FlightController> {
    friend class Singleton<FlightController>;

public:
    esp_err_t init();
    esp_err_t init();
    void update(float dt);
    
    // 모드 설정
    void setMode(FlightMode_t mode) { _mode = mode; }
    FlightMode_t getMode() const { return _mode; }
    
    // 조종 입력
    void setControlInput(float roll, float pitch, float yaw, float throttle);
    
    // 상태
    FlightData_t getFlightData() const { return _flight_data; }
    bool isFlying() const { return _is_flying; }
    bool isArmed() const { return _armed; }
    
    // 시동/정지
    bool arm();
    void disarm();
    void emergencyStop();
    
    void deinit();

private:
    FlightController();
    ~FlightController();

    void mixMotors(float roll, float pitch, float yaw, float throttle);
    void applySafetyChecks();
    float calculateThrottle(float target_alt, float current_alt, float dt);

    FlightMode_t _mode = FLIGHT_MODE_STABILIZE;
    bool _armed = false;
    bool _is_flying = false;

    // 제어 입력
    float _input_roll = 0;
    float _input_pitch = 0;
    float _input_yaw = 0;
    float _input_throttle = 0;

    // 비행 데이터
    FlightData_t _flight_data = {};

    // 안전 임계값
    const float MAX_ANGLE = 45.0f;
    const float MAX_THROTTLE = 1.0f;
    const float MIN_BATTERY = 3.3f * 3; // 3S LiPo
};

#endif