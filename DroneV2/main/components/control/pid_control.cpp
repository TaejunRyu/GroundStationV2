#include "pid_controller.h"

#include "esp_log.h"

static const char* TAG = "PID_MGR";

// Singleton 템플릿 특수화
PIDManager* PIDManager::_instance = nullptr;
void* PIDManager::_mutex = nullptr;

PIDManager::PIDManager() 
    : _rollPID(1.5f, 0.05f, 0.1f, 400.0f),
      _pitchPID(1.5f, 0.05f, 0.1f, 400.0f),
      _yawPID(2.5f, 0.01f, 0.0f, 400.0f) {}

PIDManager::~PIDManager() {}

esp_err_t PIDManager::init() {
    if (_instance != nullptr) {
        return ESP_OK;
    }
    _instance = new PIDManager();
    return ESP_OK;
}

void PIDManager::configure(float roll_p, float roll_i, float roll_d,
                           float pitch_p, float pitch_i, float pitch_d,
                           float yaw_p, float yaw_i, float yaw_d) {
    _rollPID.configure(roll_p, roll_i, roll_d, 400.0f);
    _pitchPID.configure(pitch_p, pitch_i, pitch_d, 400.0f);
    _yawPID.configure(yaw_p, yaw_i, yaw_d, 400.0f);
}

void PIDManager::update(float r_curr, float p_curr, float y_curr, float dt, 
                        float* r_out, float* p_out, float* y_out) {
    *r_out = _rollPID.calculate(r_curr, dt);
    *p_out = _pitchPID.calculate(p_curr, dt);
    *y_out = _yawPID.calculate(y_curr, dt);
}

void PIDManager::reset() {
    _rollPID.reset();
    _pitchPID.reset();
    _yawPID.reset();
}

void PIDManager::deinit() {
    if (_instance != nullptr) {
        delete _instance;
        _instance = nullptr;
    }
}
