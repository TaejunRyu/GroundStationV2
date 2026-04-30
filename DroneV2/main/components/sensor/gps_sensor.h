#pragma once
#include "singleton_base.h"
#include "uart_driver.h"
#include "drone_types.h"

namespace drone {

class GpsSensor : public Singleton<GpsSensor> {
    friend class Singleton<GpsSensor>;

private:
    GpsSensor();
    UartDriver _uart;
    GpsData_t  _current_data;

    // UBX 파싱 상태 정의
    enum class ParserState { PREAMBLE1, PREAMBLE2, CLASS, ID, LEN1, LEN2, PAYLOAD, CK_A, CK_B };
    ParserState _state = ParserState::PREAMBLE1;

    // 패킷 수신용 버퍼 및 변수
    uint8_t  _msg_class, _msg_id;
    uint16_t _payload_len, _payload_counter;
    uint8_t  _payload_buf[100]; // NAV-PVT는 92바이트
    uint8_t  _ck_a, _ck_b;

    void processByte(uint8_t byte);
    void updateChecksum(uint8_t byte);
    void parsePVT();

    HomeLocation_t _home_pos = {0, 0, 0, false};

public:
    esp_err_t init();
    void update();
    GpsData_t getGpsData() { return _current_data; }

    void setHome() {
        if (_current_data.fix && _current_data.sats >= 6) {
            _home_pos.latitude = _current_data.latitude;
            _home_pos.longitude = _current_data.longitude;
            _home_pos.altitude = _current_data.altitude;
            _home_pos.is_set = true;
            ESP_LOGI("GPS", "Home Position Set!");
        }
    }
    HomeLocation_t getHome() { return _home_pos; }
};

} // namespace drone
