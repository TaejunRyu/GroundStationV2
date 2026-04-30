#include "mavlink_service.h"

namespace drone {

MavlinkService::MavlinkService() : _uart(UART_NUM_2) {} // Mavlink용 UART 2번 사용

esp_err_t MavlinkService::init() {
    // 텔레메트리 무선 모듈 속도에 맞춰 설정 (보통 57600 또는 115200)
    return _uart.init(GPIO_NUM_17, GPIO_NUM_16, 115200); 
}

void MavlinkService::sendPacket(mavlink_message_t* msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, msg);
    _uart.write(buf, len);
}

void MavlinkService::sendHeartbeat(FlightMode_t mode) {
    mavlink_message_t msg;
    uint8_t base_mode = (mode == FLIGHT_MODE_DISARMED) ? 0 : MAV_MODE_FLAG_SAFETY_ARMED;
    
    mavlink_msg_heartbeat_pack(_system_id, _component_id, &msg,
                               MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC,
                               base_mode, 0, MAV_STATE_ACTIVE);
    sendPacket(&msg);
}

void MavlinkService::sendAttitude(const Attitude_t& att, const Vector3f_t& gyro) {
    mavlink_message_t msg;
    // Mavlink는 Radian 단위를 사용함에 주의
    mavlink_msg_attitude_pack(_system_id, _component_id, &msg,
                              esp_timer_get_time() / 1000,
                              att.roll * (M_PI / 180.0f),
                              att.pitch * (M_PI / 180.0f),
                              att.yaw * (M_PI / 180.0f),
                              gyro.x * (M_PI / 180.0f),
                              gyro.y * (M_PI / 180.0f),
                              gyro.z * (M_PI / 180.0f));
    sendPacket(&msg);
}

void MavlinkService::update() {
    uint8_t byte;
    mavlink_message_t msg;
    mavlink_status_t status;

    // UART에서 한 바이트씩 읽어 패킷 조립
    while (_uart.read(&byte, 1, 0) > 0) {
        if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &status)) {
            // 패킷 조립 완료 시 메시지 ID에 따라 처리
            switch (msg.msgid) {
                case MAVLINK_MSG_ID_SET_MODE:
                    // 모드 변경 명령 처리 로직
                    break;
                // 추가적인 명령(ARM/DISARM 등) 처리
            }
        }
    }
}

} // namespace drone
