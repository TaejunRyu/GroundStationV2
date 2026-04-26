#include "timer_service.h"
#include <esp_log.h>
#include <esp_timer.h>

namespace Services {

const char* TimerService::TAG = "TIMER_SERVICE";

TimerService::TimerService()
    : timer_handle_(nullptr), initialized_(false), running_(false) {
    ESP_LOGI(TAG, "TimerService created");
}

TimerService::~TimerService() {
    stop();
    if (timer_handle_) {
        xTimerDelete(timer_handle_, 0);
        timer_handle_ = nullptr;
    }
    ESP_LOGI(TAG, "TimerService destroyed");
}

esp_err_t TimerService::initialize() {
    if (initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing TimerService...");

    timer_handle_ = xTimerCreate(
        "SystemTimer",
        pdMS_TO_TICKS(TIMER_PERIOD_MS),
        pdTRUE,  // Auto-reload
        this,
        timer_callback
    );

    if (timer_handle_ == nullptr) {
        ESP_LOGE(TAG, "Timer creation failed");
        return ESP_ERR_NO_MEM;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "TimerService initialized");
    return ESP_OK;
}

esp_err_t TimerService::start() {
    if (!initialized_ || running_) return ESP_OK;

    if (xTimerStart(timer_handle_, 0) != pdPASS) {
        ESP_LOGE(TAG, "Timer start failed");
        return ESP_FAIL;
    }

    running_ = true;
    ESP_LOGI(TAG, "TimerService started");
    return ESP_OK;
}

void TimerService::stop() {
    if (running_ && timer_handle_) {
        xTimerStop(timer_handle_, 0);
        running_ = false;
        ESP_LOGI(TAG, "TimerService stopped");
    }
}

void TimerService::set_timer_callback(std::function<void()> callback) {
    timer_callback_ = callback;
}

bool TimerService::is_running() const {
    return running_;
}


void TimerService::timer_callback(TimerHandle_t xTimer) {
    TimerService* service = static_cast<TimerService*>(pvTimerGetTimerID(xTimer));

    if (service && service->timer_callback_) {
        service->timer_callback_();
    }
}

} // namespace Services