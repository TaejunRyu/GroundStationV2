#include "timer_service.h"
#include "esp_log.h"

static const char* TAG = "TIMER_SVC";

namespace drone {

// 정적 인스턴스 및 뮤텍스 초기화 (singleton_base.h 규격에 따름)
template<> TimerService* Singleton<TimerService>::_instance = nullptr;
template<> SemaphoreHandle_t Singleton<TimerService>::_lock = nullptr;

TimerService::TimerService() : _periodic_timer(nullptr), _start_time(0) {}

TimerService::~TimerService() {
    stop_100ms_loop();
    if (_periodic_timer) {
        esp_timer_delete(_periodic_timer);
    }
}

esp_err_t TimerService::init() {
    _start_time = esp_timer_get_time();

    // 1. 타이머 인자 설정
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &TimerService::periodic_timer_callback,
        .arg = this, // 필요한 경우 인스턴스 포인터 전달
        .name = "100ms_event_loop"
    };

    // 2. 타이머 생성
    esp_err_t ret = esp_timer_create(&periodic_timer_args, &_periodic_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create periodic timer");
        return ret;
    }

    ESP_LOGI(TAG, "Timer Service Initialized");
    return ESP_OK;
}

esp_err_t TimerService::start_100ms_loop() {
    if (!_periodic_timer) return ESP_ERR_INVALID_STATE;
    
    // 100,000us = 100ms 주기 시작
    return esp_timer_start_periodic(_periodic_timer, 100000);
}

void TimerService::stop_100ms_loop() {
    if (_periodic_timer) {
        esp_timer_stop(_periodic_timer);
    }
}

// 100ms마다 실행되는 핵심 콜백
void TimerService::periodic_timer_callback(void* arg) {
    // 이 함수는 ISR(Interrupt Service Routine)과 유사한 문맥에서 실행됨
    // 따라서 매우 가볍고 비차단(Non-blocking) 로직만 수행해야 함
    
    // 예: 시스템 상태 모니터링 서비스 호출
    // SystemManager::getInstance()->checkHealth();
    
    // 예: 배터리 전압 체크 요청 플래그 설정
    // BatteryService::getInstance()->requestUpdate();
}

} // namespace drone
