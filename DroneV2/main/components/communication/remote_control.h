#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include "esp_now.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "singleton_base.h"
#include "driver/gpio.h"

// 조종기에서 오는 데이터 구조체
struct ControlData {
    float throttle; // 0.0 ~ 1.0
    float roll;     // -30.0 ~ 30.0 (목표 각도)
    float pitch;    // -30.0 ~ 30.0
    float yaw;      // -180.0 ~ 180.0
    bool arming;    // 시동 여부
};

/**
 * @brief 원격 조종 관리자
 * - ESP-NOW 기반 수신
 * - 페일세이프 기능
 */
class RemoteControl : public Singleton<RemoteControl> {
    friend class Singleton<RemoteControl>;

public:
    esp_err_t init(gpio_num_t led_gpio);
    ControlData getControlData();
    bool checkConnection();
    bool isConnected() const { return _is_connected; }
    void deinit();

private:
    RemoteControl();
    ~RemoteControl();

    static void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

    ControlData _current_values = {};
    uint64_t _last_recv_time = 0;
    bool _is_connected = false;
    gpio_num_t _led_gpio = GPIO_NUM_NC;

    // 페일세이프
    const uint32_t FAILSAFE_TIMEOUT_MS = 500;
};

#endif
