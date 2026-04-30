#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_heap_caps.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

// 새로운 아키텍처 헤더들
#include "core/bridge_core.h"
#include "core/queue_manager.h"

#include "services/mavlink_service.h"
#include "services/timer_service.h"
#include "services/ota_service.h"

#include "drivers/wifi_driver.h"
#include "drivers/led_driver.h"
#include "drivers/led_strip_driver.h"
#include "drivers/uart_driver.h"
#include "drivers/flysky_sensor.h"
#include "drivers/adc_driver.h"

#include "utils/config_manager.h"
#include "utils/memory_manager.h"

static const char* TAG = "MAIN";

// 시스템 상태 확인
void check_system_health_on_boot(void) {
    ESP_LOGI(TAG, "=== System Health Check ===");
    ESP_LOGI(TAG, "Free heap: %u bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Min free heap: %u bytes", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %u bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Total PSRAM: %u bytes", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
}

// LED 초기화 시퀀스
void init_led_sequence(Drivers::LedStripDriver& led_driver) {
    // 빨간색 - 에러 상태 표시
    led_driver.set_status_error();
    vTaskDelay(pdMS_TO_TICKS(300));
    
    // 녹색 - 정상 상태 표시
    led_driver.set_status_ok();
    vTaskDelay(pdMS_TO_TICKS(300));
    
    // 파란색 - 처리 중 표시
    led_driver.set_status_busy();
    vTaskDelay(pdMS_TO_TICKS(300));
    
    // 노란색 - 대기 상태 표시
    led_driver.set_status_warning();
    vTaskDelay(pdMS_TO_TICKS(300));
    
    // 꺼짐
    led_driver.clear();
}
extern "C" {
    void app_main(void);
}

void app_main(void) {
    ESP_LOGI(TAG, "======================================");
    ESP_LOGI(TAG, "Ground Station v2.0 starting...");
    ESP_LOGI(TAG, "ESP32-S3 MCU with 8MB PSRAM");
    ESP_LOGI(TAG, "======================================");

    // 1. 시스템 정보 출력
    check_system_health_on_boot();

    // 2. LED 스트립 초기화 및 시작 시퀀스
    Drivers::LedStripDriver led_strip(GPIO_NUM_48, 1);
    if (led_strip.initialize() == ESP_OK) {
        init_led_sequence(led_strip);
    }

    // 3. 로그 레벨 설정
    esp_log_level_set("*", ESP_LOG_NONE);
    
    esp_log_level_set("MAIN", ESP_LOG_INFO);
    esp_log_level_set("MAVLINK_SERVICE", ESP_LOG_INFO);
    esp_log_level_set("TIMER_SERVICE", ESP_LOG_INFO);
    esp_log_level_set("FLYSKY", ESP_LOG_INFO);
    esp_log_level_set("ADC", ESP_LOG_INFO);
    esp_log_level_set("LED_STRIP", ESP_LOG_INFO);
    esp_log_level_set("QUEUE_MANAGER", ESP_LOG_INFO);
    esp_log_level_set("WIFI_DRIVER", ESP_LOG_VERBOSE);
    esp_log_level_set("SERIAL_JTAG", ESP_LOG_VERBOSE);
    esp_log_level_set("BRIDGE_CORE", ESP_LOG_VERBOSE);

    // 4. NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS flash...");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 5. 이벤트 루프 생성
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 6. 설정 초기화
    Utils::ConfigManager::initialize();

    // 7. BridgeCore 생성 및 초기화
    // BridgeCore 인스턴스 가져오기
    Core::BridgeCore& bridge = Core::BridgeCore::get_instance();

    // 1. BridgeCore 초기화
    ret = bridge.initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BridgeCore initialization failed: %s", esp_err_to_name(ret));
        led_strip.set_status_error();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "Restarting system...");
        esp_restart();
    }

    ESP_LOGI(TAG, "System initialization complete");
    led_strip.set_status_ok();

    // 2. BridgeCore 시작 (메인 루프)
    ret = bridge.start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BridgeCore start failed: %s", esp_err_to_name(ret));
        led_strip.set_status_error();
        // 필요 시 여기서도 esp_restart()를 호출할 수 있습니다.
    }

    // 메인 루프: 시스템이 계속 실행되도록 무한 루프 추가
    ESP_LOGI(TAG, "Entering main loop...");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1초마다 딜레이
        // 추가적인 주기적 작업이 필요하면 여기서 수행
    }

    // 3. 정리 로직 (정상 종료 시 - 실제로는 도달하지 않음)
    bridge.deinitialize();
    ESP_LOGI(TAG, "System shutdown complete");
}