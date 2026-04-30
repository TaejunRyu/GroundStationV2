#pragma once

#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bridge_types.h"

namespace Services {

class OtaService {
public:
    OtaService();
    ~OtaService();

    esp_err_t initialize();
    esp_err_t start_ota(const char* url = nullptr);

    // 태스크 안전 제어
    void suspend_tasks();
    void resume_tasks();

    // 상태
    bool is_ota_running() const { return ota_running_; }

private:
    static void ota_task(void* pvParameters);

    TaskHandle_t ota_task_handle_;
    bool ota_running_;
    bool initialized_;

    // 일시 중지할 태스크 핸들들
    TaskHandle_t *serial_jtag_driver_handle_;
    TaskHandle_t *core_process_task_handle_;
    TaskHandle_t *rc_task_handle_;

    static const char* TAG;
    static const char* DEFAULT_OTA_URL;
};

} // namespace Services