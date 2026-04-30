#pragma once

#include "singleton_base.h"
#include "drone_types.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "freertos/queue.h"

namespace drone {

class LoggerCore : public Singleton<LoggerCore> {
    friend class Singleton<LoggerCore>;

private:
    LoggerCore();
    
    sdmmc_card_t* _card;
    FILE* _log_file = nullptr;
    bool _is_logging = false;
    
    // 로그 데이터를 담을 큐 (실시간성 확보용)
    QueueHandle_t _log_queue;
    
    static void logTask(void* pvParameters);

public:
    // SPI 핀 설정을 포함한 초기화
    esp_err_t init(int miso, int mosi, int clk, int cs);
    
    // 비행 데이터 기록 요청 (큐에 삽입)
    void logFlightData(const Attitude_t& att, const MotorOutput_t& motors);
    
    void startLogging();
    void stopLogging();
};

} // namespace drone
