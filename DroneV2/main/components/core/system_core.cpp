#include "system_manager.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SYS_MGR";

// Singleton 템플릿 특수화
SystemManager* SystemManager::_instance = nullptr;
void* SystemManager::_mutex = nullptr;

SystemManager::SystemManager() 
    : _state(SYSTEM_STATE_INIT), _error_count(0), _uptime_ms(0),
      _low_battery(false), _sensor_error(false) {}

SystemManager::~SystemManager() {}

esp_err_t SystemManager::init() {
    if (_instance != nullptr) {
        return ESP_OK;
    }
    _instance = new SystemManager();
    ESP_LOGI(TAG, "SystemManager 초기화 완료");
    return ESP_OK;
}

void SystemManager::update() {
    _uptime_ms += 10; // 10ms 주기
}

void SystemManager::setState(DroneState_t state) {
    ESP_LOGI(TAG, "상태 전환: %d -> %d", _state, state);
    _state = state;
}

bool SystemManager::isSafeToFly() const {
    return (_state == SYSTEM_STATE_ARMED || _state == SYSTEM_STATE_STANDBY) &&
           !_low_battery && !_sensor_error && _error_count < 10;
}

void SystemManager::emergencyStop() {
    ESP_LOGW(TAG, "비상 정지!");
    _state = SYSTEM_STATE_ERROR;
}

void SystemManager::deinit() {
    if (_instance != nullptr) {
        delete _instance;
        _instance = nullptr;
    }
}