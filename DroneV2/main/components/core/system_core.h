#pragma once

#include "esp_err.h"
#include "singleton_base.h"
#include "driver/gpio.h"

/**
 * @brief 드론 시스템 상태
 */
typedef enum {
    SYSTEM_STATE_INIT = 0,
    SYSTEM_STATE_CALIBRATING,
    SYSTEM_STATE_STANDBY,
    SYSTEM_STATE_ARMED,
    SYSTEM_STATE_FLIGHT,
    SYSTEM_STATE_ERROR
} DroneState_t;

/**
 * @brief 시스템 관리자 클래스
 * - 드론 전체 상태 관리
 * - 안전 장치 모니터링
 */
class SystemManager : public Singleton<SystemManager> {
    friend class Singleton<SystemManager>;

public:
    esp_err_t init();
    void update();
    DroneState_t getState() const { return _state; }
    void setState(DroneState_t state);
    bool isSafeToFly() const;
    void emergencyStop();
    void deinit();

private:
    SystemManager();
    ~SystemManager();

    DroneState_t _state = SYSTEM_STATE_INIT;
    uint32_t _error_count = 0;
    uint64_t _uptime_ms = 0;
    bool _low_battery = false;
    bool _sensor_error = false;
};

#endif