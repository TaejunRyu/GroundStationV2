#pragma once

#include "singleton_base.h"
#include "uart_driver.h"
#include "drone_types.h"
#include "mavlink/common/mavlink.h" // Mavlink 라이브러리 포함 필요

namespace drone {

class MavlinkService : public Singleton<MavlinkService> {
    friend class Singleton<MavlinkService>;

private:
    MavlinkService();
    
    UartDriver _uart;
    uint8_t _system_id = 1;
    uint8_t _component_id = 1;

    void sendPacket(mavlink_message_t* msg);

public:
    esp_err_t init();
    
    // 비행 상태 전송 (1Hz ~ 10Hz)
    void sendHeartbeat(FlightMode_t mode);
    
    // 자세 데이터 시각화 전송 (50Hz 이상 권장)
    void sendAttitude(const Attitude_t& att, const Vector3f_t& gyro);
    
    // 센서 원시 데이터 전송 (디버깅용)
    void sendRawImu(const SensorData_t& sensor);

    // 수신 데이터 처리 (GCS 명령 수신)
    void update();
};

} // namespace drone
