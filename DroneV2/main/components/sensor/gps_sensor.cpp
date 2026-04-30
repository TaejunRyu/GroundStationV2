#include "gps_sensor.h"

namespace drone {

GpsSensor::GpsSensor() : _uart(UART_NUM_1) {
    memset(&_current_data, 0, sizeof(GpsData_t));
}

esp_err_t GpsSensor::init() {
    // M10은 기본 9600일 수 있으나, 보통 드론용은 115200으로 설정함
    return _uart.init(GPIO_NUM_18, GPIO_NUM_17, 115200); 
}

void GpsSensor::update() {
    uint8_t buf[64];
    int len = _uart.read(buf, sizeof(buf), 0); // Non-blocking read
    for (int i = 0; i < len; i++) {
        processByte(buf[i]); // 한 바이트씩 상태 머신으로 파싱
    }
}

// UBX 상태 머신 로직 (간략화)
void GpsSensor::processByte(uint8_t byte) {
    // 여기에 0xB5, 0x62 헤더를 찾고 Checksum을 검사하는 로직이 들어갑니다.
    // 검사가 통과되면 parsePayload()를 호출합니다.
}

void GpsSensor::parsePayload(uint8_t* payload, uint16_t len) {
    auto pvt = reinterpret_cast<ubx_nav_pvt_t*>(payload);
    
    // UBX는 정수형으로 데이터를 주므로 drone_types의 double/float로 변환
    _current_data.latitude  = pvt->lat / 10000000.0;
    _current_data.longitude = pvt->lon / 10000000.0;
    _current_data.altitude  = pvt->hMSL / 1000.0f;
    _current_data.speed     = pvt->gSpeed / 1000.0f;
    _current_data.sats      = pvt->numSV;
    _current_data.hdop      = pvt->pDOP / 100.0f;
    _current_data.fix       = (pvt->fixType >= 3); // 3D Fix 이상
    _current_data.timestamp = esp_timer_get_time();
}

} // namespace drone
