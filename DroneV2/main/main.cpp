#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

// [Core]
#include "singleton_base.h"
#include "drone_types.h"

// [Drivers & Comm]
#include "motor_driver.h"
#include "uart_driver.h"
#include "espnow_comm.h"

// [Sensors]
#include "imu_sensor.h"
#include "gps_sensor.h"

// [Services]
#include "timer_service.h"
#include "ahrs_service.h"
#include "mavlink_service.h"

// [Control]
#include "pid_control.h"

static const char* TAG = "DRONE_MAIN";

using namespace drone;

/**
 * @brief 비행 제어 루프 (Core 1 전담)
 * 주기: 1ms (1000Hz)
 */
void flight_control_task(void* pvParameters) {
    auto imu    = ImuSensor::getInstance();
    auto ahrs   = AhrsService::getInstance();
    auto pid    = PidControl::getInstance();
    auto motor  = MotorDriver::getInstance();
    auto timers = TimerService::getInstance();

    TimerService::DeltaTimer flight_dt;
    ESP_LOGI(TAG, "비행 제어 루프 시작 (Core 1)");

    while (true) {
        // 1. 시간 간격 계산
        float dt = flight_dt.getDelta();

        // 2. 센서 데이터 취득 및 자세 추정
        imu->update();
        SensorData_t raw_data = imu->getData();
        ahrs->update(raw_data, dt);
        Attitude_t attitude = ahrs->getAttitude();

        // 3. 조종기 입력 및 PID 계산 (조종기 데이터는 Core 0에서 업데이트됨)
        // ControlInput_t input = EspNowComm::getInstance()->getControlInput();
        // pid->setTarget(input.roll, input.pitch, input.yaw);
        
        float r_out, p_out, y_out;
        pid->update(attitude, dt, &r_out, &p_out, &y_out);

        // 4. 모터 출력 반영 (X-Quad Mixing)
        // float throttle = input.throttle;
        // motor->setOutputs(throttle + r_out + p_out - y_out, ...);

        // 1ms 주기 유지
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief 시스템 초기화 및 메인 진입점
 */
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "--- Drone Firmware Booting ---");

    // [STEP 1] 기초 서비스 초기화
    TimerService::init();
    TimerService::getInstance()->start_100ms_loop(); // 100ms 이벤트 시작

    // [STEP 2] 드라이버 및 통신 초기화
    MotorDriver::init();
    EspNowComm::init(); // 내부적으로 Wi-Fi 및 ESP-NOW 설정
    MavlinkService::init(); // UART2 기반 텔레메트리

    // [STEP 3] 센서 초기화 (SPI/I2C 버스 포함)
    ImuSensor::init();
    GpsSensor::init(); // UART1 기반 UBX M10

    // [STEP 4] 연산 서비스 초기화
    AhrsService::init();
    PidControl::init();

    ESP_LOGI(TAG, "--- All Systems Initialized ---");

    // [STEP 5] 비행 태스크 생성 (Core 1)
    xTaskCreatePinnedToCore(
        flight_control_task, "flight_task", 8192, NULL, 10, NULL, 1
    );

    // [STEP 6] 시스템 감시 및 Mavlink 루프 (Core 0)
    while (true) {
        // 텔레메트리 데이터 전송 (20Hz)
        MavlinkService::getInstance()->sendAttitude(
            AhrsService::getInstance()->getAttitude(),
            ImuSensor::getInstance()->getData().gyro
        );
        
        MavlinkService::getInstance()->update(); // GCS 명령 수신 처리
        
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}
