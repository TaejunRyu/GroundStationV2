#include "logger_core.h"
#include "esp_log.h"

static const char* TAG = "LOGGER";

namespace drone {

LoggerCore::LoggerCore() : _card(nullptr), _log_queue(nullptr) {
    _log_queue = xQueueCreate(100, sizeof(float) * 7); // 간단한 예시 데이터 크기
}

esp_err_t LoggerCore::init(int miso, int mosi, int clk, int cs) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi, .miso_io_num = miso, .sclk_io_num = clk,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 4000,
    };
    
    // SPI 버스 초기화
    esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) return ret;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)cs;
    slot_config.host_id = (spi_host_device_t)host.slot;

    // SD 카드 마운트
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &_card);
    if (ret != ESP_OK) return ret;

    // 로그 전용 저지연 태스크 생성 (Core 0 할당 추천)
    xTaskCreatePinnedToCore(logTask, "log_task", 4096, this, 2, nullptr, 0);

    return ESP_OK;
}

void LoggerCore::logFlightData(const Attitude_t& att, const MotorOutput_t& motors) {
    if (!_is_logging) return;

    float data[7] = {att.roll, att.pitch, att.yaw, motors.m1, motors.m2, motors.m3, motors.m4};
    xQueueSend(_log_queue, &data, 0); // Non-blocking
}

void LoggerCore::logTask(void* pvParameters) {
    LoggerCore* core = (LoggerCore*)pvParameters;
    float data[7];

    while (1) {
        if (xQueueReceive(core->_log_queue, &data, portMAX_DELAY)) {
            if (core->_log_file) {
                fprintf(core->_log_file, "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                        data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
            }
        }
    }
}

void LoggerCore::startLogging() {
    _log_file = fopen("/sdcard/flight.csv", "a");
    if (_log_file) {
        _is_logging = true;
        fprintf(_log_file, "Roll,Pitch,Yaw,M1,M2,M3,M4\n"); // 헤더 기록
    }
}

void LoggerCore::stopLogging() {
    _is_logging = false;
    if (_log_file) {
        fclose(_log_file);
        _log_file = nullptr;
    }
}

} // namespace drone
