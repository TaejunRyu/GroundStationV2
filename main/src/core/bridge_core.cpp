#include "bridge_core.h"
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include "esp_log.h"

// 2. 불완전한 타입(Incomplete Types)의 실제 헤더들을 모두 포함
#include "mavlink_service.h"
#include "timer_service.h"
#include "wifi_driver.h"
#include "serial_jtag_driver.h"
#include "queue_manager.h"
#include "config_manager.h"
#include "memory_manager.h"

namespace Core {

const char* BridgeCore::TAG = "BRIDGE_CORE";

BridgeCore::BridgeCore()
    : system_state_(Types::SystemState::UNINITIALIZED) {
    ESP_LOGI(TAG, "BridgeCore created");
}

BridgeCore::~BridgeCore() {
    stop();
    ESP_LOGI(TAG, "BridgeCore destroyed");
}

void BridgeCore::deinitialize() {}

esp_err_t BridgeCore::initialize() {
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initializing BridgeCore...");

    // NVS 초기화
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 이벤트 루프는 main에서 이미 생성됨

    // 네트워크 인터페이스 초기화
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Netif initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 1. 서비스 객체 생성 (unique_ptr 할당)
    
    config_manager_ = std::make_unique<Utils::ConfigManager>();
    memory_manager_ = std::make_unique<Utils::MemoryManager>();
    mavlink_service_ = std::make_unique<Services::MavlinkService>();
    timer_service_ = std::make_unique<Services::TimerService>();
    wifi_driver_ = std::make_unique<Drivers::WiFiDriver>();
    serial_jtag_driver_ = std::make_unique<Drivers::SerialJtagDriver>();
    queue_manager_ = std::make_unique<Core::QueueManager>();


    ret = config_manager_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ConfigManager initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    config_manager_->set_drone_mac(config_manager_->get_drone_mac().data()); // ConfigManager에서 드론 MAC 주소 설정
    //config_manager_->set_bridge_mac(config_manager_->get_bridge_mac().data()); // ConfigManager에서 브리지 MAC 주소 설정

    ret = memory_manager_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MemoryManager initialization failed: %s", esp_err_to_name(ret));
        return ret; // 초기화 실패 시 즉시 반환
    }

    // 2. 각 서비스 초기화 및 결과 체크
    ret = mavlink_service_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MavlinkService init failed");
        return ret;
    }

    ret = timer_service_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TimerService init failed");
        return ret;
    }

    ret = wifi_driver_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFiDriver init failed");
        return ret;
    }

    ret = serial_jtag_driver_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SerialJtagDriver init failed");
        return ret;
    }

    ret = queue_manager_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QueueManager init failed");
        return ret;
    }

    // 3. 상태 변경 및 완료 로그
    system_state_ = Types::SystemState::READY;
    ESP_LOGI(TAG, "BridgeCore initialization completed");

    return ESP_OK;
}

esp_err_t BridgeCore::start() {
    if (system_state_ != Types::SystemState::READY) {
        ESP_LOGE(TAG, "System not ready for start");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting BridgeCore...");

    // 콜백 설정
    wifi_driver_->set_connect_callback([this]() { on_wifi_connected(); });
    wifi_driver_->set_disconnect_callback([this]() { on_wifi_disconnected(); });

    // 데이터 수신 콜백 설정 (Wi-Fi와 Serial JTAG 모두)
    // Wi-Fi와 Serial JTAG에서 데이터가 들어왔을 때 on_data_received를 호출하도록 설정
    wifi_driver_->set_data_callback([this](const uint8_t* data, size_t len, Types::DataSource source) {on_data_received(data, len, source);});

    // 타이머 콜백 설정 (Serial JTAG 폴링)  타이머는 100ms마다 on_timer_tick을 호출하도록 설정
    // 타이머는 단순히 callback함수를 실행시키는 서비스를 제공하므로, 타이머 서비스에서 on_timer_tick을 호출하도록 설정
    timer_service_->set_timer_callback([this]() { on_timer_tick(); });
   
    // Wi-Fi 설정 및 시작
    Types::WiFiConfig wifi_config = {
        .ssid = CONFIG_GS_WIFI_SSID,
        .password = CONFIG_GS_WIFI_PASSWORD,
        .channel = CONFIG_GS_ESPNOW_CHANNEL,
        .auth_mode = WIFI_AUTH_WPA2_PSK,
        .hidden = false,
        .max_connections = 4
    };

    esp_err_t ret = wifi_driver_->start_ap(wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi AP");
        return ret;
    }

    Types::EspNowConfig espnow_config = {};

    memcpy(espnow_config.peer_mac, config_manager_->get_drone_mac().data(), sizeof(espnow_config.peer_mac));
    espnow_config.channel = CONFIG_GS_ESPNOW_CHANNEL;
    espnow_config.phy_mode = WIFI_PHY_MODE_11B;
    espnow_config.rate = WIFI_PHY_RATE_6M;

    ret = wifi_driver_->init_espnow(espnow_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-NOW");
        return ret;
    }

    wifi_driver_->register_espnow_callbacks(); // ESP-NOW 콜백 등록


    // UDP 초기화 및 수신 활성화
    ret = wifi_driver_->init_udp();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UDP");
        return ret;
    }
    wifi_driver_->enable_udp_recv();

    ret = serial_jtag_driver_->start(); // Serial JTAG 드라이버는 별도의 start 함수가 없으므로 initialize에서 바로 사용 가능       
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Serial JTAG driver");
        return ret;        
    }
    
    // // Serial JTAG select callback 등록
    // serial_jtag_driver_->set_select_callback([](usj_select_notif_t event, int* task_woken) {
    //     ESP_LOGD(TAG, "Serial JTAG event: %d", event);
    //     if (task_woken) *task_woken = 0;
    // });


    // 타이머 시작
    ret = timer_service_->start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer");
        return ret;
    }
    
    system_state_ = Types::SystemState::RUNNING;
    ESP_LOGI(TAG, "BridgeCore started successfully");
    return ESP_OK;
}

void BridgeCore::stop() {
    if (system_state_ == Types::SystemState::RUNNING) {
        ESP_LOGI(TAG, "Stopping BridgeCore...");

        timer_service_->stop();
        wifi_driver_->stop();
        serial_jtag_driver_->stop();

        system_state_ = Types::SystemState::SHUTDOWN;
        ESP_LOGI(TAG, "BridgeCore stopped");
    }
}

void BridgeCore::on_wifi_connected() {
    ESP_LOGI(TAG, "Wi-Fi connected event received");
    // 연결 상태 업데이트 및 관련 서비스 알림
}

void BridgeCore::on_wifi_disconnected() {
    ESP_LOGI(TAG, "Wi-Fi disconnected event received");
    // 연결 해제 처리
}

/**
 * @brief 
 *  1. serial jtag 또는 wifi udp에서 데이터가 들어왔을 때 esp-now로 전달
 *  2. esp-now에서 데이터가 들어왔을 때 serial jtag 또는 wifi udp로 전달    
 * @param data 
 * @param len 
 * @param source 
 */
void BridgeCore::on_data_received(const uint8_t* data, size_t len, Types::DataSource source) {
    //ESP_LOGI(TAG, "Data received: %d bytes from source %d", len, static_cast<int>(source));

    // Serial JTAG 또는 UDP에서 들어온 데이터는 ESP-NOW로 전송
    if (source == Types::DataSource::UART_SERIAL || source == Types::DataSource::WIFI_UDP) {
//        if(wifi_driver_->is_connected(Types::DataSource::WIFI_ESPNOW)){
            esp_err_t ret = wifi_driver_->send_espnow(data, len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send data via ESP-NOW");
            } else {
                ESP_LOGD(TAG, "Forwarded %d bytes to ESP-NOW", len);
            }
//        }
    }
    // ESP-NOW에서 들어온 데이터는 Serial JTAG로 전송
    else if (source == Types::DataSource::WIFI_ESPNOW) {

        esp_err_t ret;

        if (serial_jtag_driver_->connected()){
            ret = serial_jtag_driver_->send_data(data, len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send data via Serial JTAG");
            } else {
                ESP_LOGD(TAG, "Forwarded %d bytes to Serial JTAG", len);
            }
        }

//        if(wifi_driver_->is_connected(Types::DataSource::WIFI_UDP)){
            ret = wifi_driver_->send_udp(data, len); // UDP로도 데이터 전달 (옵션)
            if(ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send data via UDP");
            } else {
                ESP_LOGD(TAG, "Forwarded %d bytes to UDP", len);
            }
//        }
    }

    // // 큐에 데이터 추가
    // esp_err_t ret = queue_manager_->enqueue_packet(data, len, source);
    // if (ret != ESP_OK) {
    //     ESP_LOGW(TAG, "Failed to enqueue packet");
    // }

    // MAVLink 처리
    // mavlink_service_->process_packet(data, len);
}


/**
 * @brief 타이머 틱 이벤트 처리 함수
 */
void BridgeCore::on_timer_tick() {
    //ESP_LOGI(TAG, "Timer tick event received");    
}

} // namespace Core