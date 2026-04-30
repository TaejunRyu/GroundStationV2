#include "gps_sensor.h"
#include <cstring>

namespace drone {

GpsSensor::GpsSensor() : _uart(UART_NUM_1) {
    memset(&_current_data, 0, sizeof(GpsData_t));
}

esp_err_t GpsSensor::init() {
    return _uart.init(GPIO_NUM_18, GPIO_NUM_17, 115200); 
}

void GpsSensor::update() {
    uint8_t buf[64];
    int len = _uart.read(buf, sizeof(buf), 0);
    for (int i = 0; i < len; i++) {
        processByte(buf[i]);
    }
}

void GpsSensor::updateChecksum(uint8_t byte) {
    _ck_a += byte;
    _ck_b += _ck_a;
}

void GpsSensor::processByte(uint8_t b) {
    switch (_state) {
        case ParserState::PREAMBLE1:
            if (b == 0xB5) _state = ParserState::PREAMBLE2;
            break;

        case ParserState::PREAMBLE2:
            if (b == 0x62) {
                _ck_a = _ck_b = 0; // 체크섬 초기화
                _state = ParserState::CLASS;
            } else _state = ParserState::PREAMBLE1;
            break;

        case ParserState::CLASS:
            _msg_class = b;
            updateChecksum(b);
            _state = ParserState::ID;
            break;

        case ParserState::ID:
            _msg_id = b;
            updateChecksum(b);
            _state = ParserState::LEN1;
            break;

        case ParserState::LEN1:
            _payload_len = b;
            updateChecksum(b);
            _state = ParserState::LEN2;
            break;

        case ParserState::LEN2:
            _payload_len |= (b << 8);
            updateChecksum(b);
            _payload_counter = 0;
            _state = (_payload_len > 0 && _payload_len <= sizeof(_payload_buf)) ? 
                      ParserState::PAYLOAD : ParserState::PREAMBLE1;
            break;

        case ParserState::PAYLOAD:
            _payload_buf[_payload_counter++] = b;
            updateChecksum(b);
            if (_payload_counter >= _payload_len) _state = ParserState::CK_A;
            break;

        case ParserState::CK_A:
            if (b == _ck_a) _state = ParserState::CK_B;
            else _state = ParserState::PREAMBLE1;
            break;

        case ParserState::CK_B:
            if (b == _ck_b) {
                // 체크섬 통과! NAV-PVT 메시지인지 확인
                if (_msg_class == 0x01 && _msg_id == 0x07) parsePVT();
            }
            _state = ParserState::PREAMBLE1;
            break;
    }
}

void GpsSensor::parsePVT() {
    // 92바이트 PVT 구조체 매핑 (앞서 정의한 ubx_nav_pvt_t 사용)
    struct __attribute__((packed)) ubx_nav_pvt_t {
        uint32_t iTOW; uint16_t year; uint8_t month, day, hour, min, sec;
        uint8_t valid; uint32_t tAcc; int32_t nano; uint8_t fixType;
        uint8_t flags; uint8_t flags2; uint8_t numSV;
        int32_t lon; int32_t lat; int32_t height; int32_t hMSL;
        uint32_t hAcc; uint32_t vAcc; int32_t velN; int32_t velE; int32_t velD;
        int32_t gSpeed; int32_t headMot; uint32_t sAcc; uint32_t headAcc;
        uint16_t pDOP; uint16_t flags3; uint8_t reserved1;
    };

    auto pvt = reinterpret_cast<ubx_nav_pvt_t*>(_payload_buf);

    _current_data.latitude  = pvt->lat / 10000000.0;
    _current_data.longitude = pvt->lon / 10000000.0;
    _current_data.altitude  = pvt->hMSL / 1000.0f;
    _current_data.speed     = pvt->gSpeed / 1000.0f;
    _current_data.sats      = pvt->numSV;
    _current_data.hdop      = pvt->pDOP / 100.0f;
    _current_data.fix       = (pvt->fixType >= 3);
    _current_data.timestamp = esp_timer_get_time();
}

} // namespace drone
