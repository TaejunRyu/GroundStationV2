#include "remote_control.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"

static const char* TAG = "REMOTE";

// Singleton 템플릿 특수화
RemoteControl* RemoteControl::_instance = nullptr;
void* RemoteControl::_mutex = nullptr;

RemoteControl::RemoteControl() 
    : _last_recv_time(0), _is_connected(false), _led_gpio(GPIO_NUM_NC) {
    memset(&_current_values, 0, sizeof(ControlData));
}

RemoteControl::~RemoteControl() {}

esp_err_t RemoteControl::init(gpio_num_t led_gpio) {
    if (_instance != nullptr) {
        return ESP_OK;
    }
    _instance = new RemoteControl();
    _instance->_led_gpio = led_gpio;
    return _instance->init(led_gpio);
}

esp_err_t RemoteControl::init(gpio_num_t led_gpio) {
    _led_gpio = led_gpio;

    // LED GPIO 설정
    if (led_gpio != GPIO_NUM_NC) {
        gpio_set_direction(led_gpio, GPIO_MODE_OUTPUT);
    }

    // Wi-Fi 초기화 (ESP-NOW는 Wi-Fi 기반)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // ESP-NOW 초기화
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW 초기화 실패");
        return ESP_FAIL;
    }

    // 수신 콜백 등록
    esp_now_register_recv_cb(onDataRecv);
    
    ESP_LOGI(TAG, "RemoteControl 초기화 완료");
    return ESP_OK;
}

void RemoteControl::onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (_instance && len == sizeof(ControlData)) {
        memcpy(&(_instance->_current_values), data, len);
        _instance->_last_recv_time = esp_timer_get_time() / 1000; // us -> ms
        _instance->_is_connected = true;
        
        // LED 깜빡임
        if (_instance->_led_gpio != GPIO_NUM_NC) {
            gpio_set_level(_instance->_led_gpio, 1);
        }
    }
}

ControlData RemoteControl::getControlData() {
    return _current_values;
}

bool RemoteControl::checkConnection() {
    uint64_t now = esp_timer_get_time() / 1000;
    if (now - _last_recv_time > FAILSAFE_TIMEOUT_MS) {
        _is_connected = false;
        memset(&_current_values, 0, sizeof(ControlData));
        _current_values.throttle = 0; // 페일세이프
    }
    return _is_connected;
}

void RemoteControl::deinit() {
    if (_instance != nullptr) {
        delete _instance;
        _instance = nullptr;
    }
}

bool RemoteControl::checkConnection() {
    // 마지막 수신 후 500ms 이상 지나면 연결 끊김으로 판단 (Failsafe)
    if (esp_timer_get_time() - lastRecvTime > 500000) {
        isConnected = false;
        currentValues.throttle = 0; // 연결 끊기면 스로틀 0
        currentValues.arming = false;
    }
    return isConnected;
}
