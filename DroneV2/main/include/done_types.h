#pragma once

#include <cstdint>

namespace drone {

/**
 * @brief 3축 데이터를 위한 공통 구조체 (가속도, 자이로, 오일러 각 등)
 */
typedef struct {
    float x;
    float y;
    float z;
} Vector3f_t;

/**
 * @brief 드론의 비행 상태 모드
 */
typedef enum {
    FLIGHT_MODE_DISARMED = 0, // 시동 꺼짐 (안전)
    FLIGHT_MODE_MANUAL,       // 수동 (Acro)
    FLIGHT_MODE_ANGLE,        // 자동 수평 유지 (Self-level)
    FLIGHT_MODE_HOLD,         // 고도/위치 유지
    FLIGHT_MODE_FAILSAFE      // 비상 상황 (자동 착륙 등)
} FlightMode_t;

/**
 * @brief 조종기로부터 수신된 원시 제어 데이터
 */
typedef struct {
    float throttle; // 0.0 ~ 1.0
    float roll;     // 목표 각도 또는 속도
    float pitch;    // 목표 각도 또는 속도
    float yaw;      // 목표 각도 또는 속도
    bool  aux1;     // 스위치 (주로 Arming 용도)
    bool  aux2;     // 스위치 (모드 변경 용도)
} ControlInput_t;

/**
 * @brief 센서 통합 데이터 (AHRS 서비스에서 사용)
 */
typedef struct {
    Vector3f_t acc;     // 가속도 (g)
    Vector3f_t gyro;    // 자이로 (deg/s)
    Vector3f_t mag;     // 지자계 (uT)
    float      temp;    // 센서 온도
    uint64_t   timestamp; // 데이터 갱신 시점 (us)
} SensorData_t;

/**
 * @brief 최종 계산된 자세 데이터
 */
typedef struct {
    float roll;
    float pitch;
    float yaw;
    float heading;
    float altitude;     // 기압계 기반 고도
} Attitude_t;

/**
 * @brief 모터 출력 값 (4개 모터)
 */
typedef struct {
    float m1, m2, m3, m4; // 0.0 ~ 1.0
} MotorOutput_t;

/**
 * @brief GPS 위치 및 상태 데이터
 */
typedef struct {
    double    latitude;   // 위도 (Decimal Degrees)
    double    longitude;  // 경도 (Decimal Degrees)
    float     altitude;   // 해발 고도 (m)
    
    float     speed;      // 지면 속도 (m/s)
    float     course;     // 진행 방향 (Degrees)
    
    uint8_t   sats;       // 연결된 위성 개수
    float     hdop;       // 수평 정밀도 저하율 (낮을수록 정밀)
    bool      fix;        // 위치 고정 여부 (3D Fix 등)
    
    uint64_t  timestamp;  // 데이터 갱신 시점 (us)
} GpsData_t;




typedef enum {
    FAILSAFE_IDLE = 0,
    FAILSAFE_LANDING,    // 천천히 하강하여 착륙
    FAILSAFE_RTH,        // 출발지로 자동 귀환 (GPS 있을 때)
    FAILSAFE_EMERGENCY   // 모터 즉시 정지 (추락 감수)
} FailsafeLevel_t;

//RTH 관련 데이터
typedef struct {
    double latitude;
    double longitude;
    float  altitude;
    bool   is_set;
} HomeLocation_t;


} // namespace drone
