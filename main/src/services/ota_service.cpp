#include "ota_service.h"
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_https_ota.h>
#include <cstring>
#include "bridge_core.h"
#include "serial_jtag_driver.h"

namespace Services {

const char* OtaService::TAG = "OTA_SERVICE";
const char* OtaService::DEFAULT_OTA_URL = "http://192.168.4.2:8000/firmware.bin";

OtaService::OtaService()
    : ota_task_handle_(nullptr), ota_running_(false), initialized_(false),
      serial_jtag_driver_handle_(nullptr), core_process_task_handle_(nullptr), rc_task_handle_(nullptr) {
    ESP_LOGI(TAG, "OtaService created");
}



OtaService::~OtaService() {
    if (ota_task_handle_) {
        vTaskDelete(ota_task_handle_);
        ota_task_handle_ = nullptr;
    }
    ESP_LOGI(TAG, "OtaService destroyed");
}

esp_err_t OtaService::initialize() {
    if (initialized_) return ESP_OK;

    Core::BridgeCore& core = Core::BridgeCore::get_instance();
    serial_jtag_driver_handle_ =  &core.get_serial_jtag_driver().get_task_handle();
    core_process_task_handle_  =  &core.get_task_handle();

    ESP_LOGI(TAG, "Initializing OtaService...");
    initialized_ = true;
    return ESP_OK;
}

esp_err_t OtaService::start_ota(const char* url) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    if (ota_running_) return ESP_ERR_INVALID_STATE;

    const char* ota_url = url ? url : DEFAULT_OTA_URL;

    ESP_LOGI(TAG, "Starting OTA from: %s", ota_url);

    // OTA 태스크 생성
    BaseType_t ret = xTaskCreatePinnedToCore(
        ota_task,
        "ota_task",
        12288,  // 기존 크기 유지
        (void*)ota_url,
        5,
        &ota_task_handle_,
        0
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "OTA task creation failed");
        return ESP_FAIL;
    }

    ota_running_ = true;
    ESP_LOGI(TAG, "OTA task started");
    return ESP_OK;
}

void OtaService::suspend_tasks() {
    // UART 태스크 중지
    if (*serial_jtag_driver_handle_ && eTaskGetState(*serial_jtag_driver_handle_) != eDeleted) {
        vTaskSuspend(*serial_jtag_driver_handle_);
    }

    // ESP-NOW 태스크 중지
    if (*core_process_task_handle_ && eTaskGetState(*core_process_task_handle_) != eDeleted) {
        vTaskSuspend(*core_process_task_handle_);
    }

    // RC 태스크 중지
    if (*rc_task_handle_ && eTaskGetState(*rc_task_handle_) != eDeleted) {
        vTaskSuspend(*rc_task_handle_);
    }

    ESP_LOGI(TAG, "Tasks suspended for OTA");
}
 


void OtaService::resume_tasks() {
    // 태스크 재개
    if (*serial_jtag_driver_handle_ && eTaskGetState(*serial_jtag_driver_handle_) == eSuspended) {
        vTaskResume(*serial_jtag_driver_handle_);
    }

    if (*core_process_task_handle_ && eTaskGetState(*core_process_task_handle_) == eSuspended) {
        vTaskResume(*core_process_task_handle_);
    }

    if (*rc_task_handle_ && eTaskGetState(*rc_task_handle_) == eSuspended) {
        vTaskResume(*rc_task_handle_);
    }

    ESP_LOGI(TAG, "Tasks resumed after OTA");
}

void OtaService::ota_task(void* pvParameters) {
    const char* url = static_cast<const char*>(pvParameters);

    ESP_LOGI(TAG, "OTA task started, URL: %s", url);

    // 네트워크 안정화 대기
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 다른 태스크들 중지 (이 구현에서는 외부에서 호출해야 함)
    ESP_LOGI(TAG, "OTA starting... (tasks should be suspended)");

    esp_http_client_config_t config = {};
    config.url = url;
    config.transport_type = HTTP_TRANSPORT_OVER_TCP;
    config.timeout_ms = 30000;
    config.keep_alive_enable = true;
    config.buffer_size = 2048;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;

    ESP_LOGI(TAG, "Performing OTA update...");
    vTaskDelay(pdMS_TO_TICKS(2000));  // 네트워크 안정화

    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA completed successfully! Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
        // 실패 시 태스크 재개는 외부에서 처리
    }

    // 태스크 종료
    OtaService* service = nullptr;  // 싱글톤이나 다른 방식으로 접근 필요
    if (service) {
        service->ota_running_ = false;
    }

    vTaskDelete(nullptr);
}

} // namespace Services