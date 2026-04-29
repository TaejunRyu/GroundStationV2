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
#include "led_strip_driver.h"
#include "bridge_types.h"

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
    strip_driver_ = std::make_unique<Drivers::LedStripDriver>();
    queue_manager_ = std::make_unique<Core::QueueManager>();


    ret = config_manager_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ConfigManager initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    config_manager_->set_drone_mac(config_manager_->get_drone_mac().data()); // ConfigManager에서 드론 MAC 주소 설정
    //config_manager_->set_bridge_mac(config_manager_->get_bridge_mac().data()); // ConfigManager에서 브리지 MAC 주소 설정

    ret = strip_driver_->initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LedStripDriver initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

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
    //wifi_driver_->set_data_callback([this](const uint8_t* data, size_t len, Types::DataSource source) {on_data_received(data, len, source);});

    // 타이머 콜백 설정 (Serial JTAG 폴링)  타이머는 100ms마다 on_timer_tick을 호출하도록 설정
    // 타이머는 단순히 callback함수를 실행시키는 서비스를 제공하므로, 타이머 서비스에서 on_timer_tick을 호출하도록 설정
   
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

    // UDP 초기화 및 수신 활성화
    ret = wifi_driver_->init_udp();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UDP");
        return ret;
    }
    
    ret = serial_jtag_driver_->start(); // Serial JTAG 드라이버는 별도의 start 함수가 없으므로 initialize에서 바로 사용 가능       
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Serial JTAG driver");
        return ret;        
    }  
    serial_jtag_driver_->set_queue_manager(queue_manager_.get()); // Serial JTAG 드라이버에 QueueManager 포인터 전달
  
    // 타이머 시작
    ret = timer_service_->start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer");
        return ret;
    }
   
    // 데이터 처리 태스크 생성 (우선순위 5, 코어 1 고정)
    xTaskCreatePinnedToCore(BridgeCore::process_task,           "process_task", 4096,this,                      5,&process_task_handle_,1);
    // Serial JTAG 수신 처리 태스크 생성 (우선순위 6, 코어 0 고정)
    xTaskCreatePinnedToCore(Drivers::SerialJtagDriver::rx_task, "rx_task",      4096,serial_jtag_driver_.get(), 6,&serial_jtag_rx_task_handle_,0);

    wifi_driver_->register_espnow_callbacks(); // ESP-NOW 콜백 등록
    wifi_driver_->enable_udp_recv();
    serial_jtag_driver_->register_select_callback(); // Serial JTAG select 콜백 등록
    timer_service_->set_timer_callback([this]() { on_timer_tick(); });
   
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
 * RSSI 값 조회 함수
 * @return int8_t 
 */
int8_t BridgeCore::get_rssi() const {
    return rssi_;
}

/**
 * @brief 
 * 노이즈 플로어 값 조회 함수
 * @return int8_t 
 */
int8_t BridgeCore::get_noise_floor() const {
    return noise_floor_;
}

/**
 * @brief 
 * 원격 RSSI 값 조회 함수
 * @return int8_t 
 */
int8_t BridgeCore::get_remote_rssi() const {
    return remote_rssi_;
}
/**
 * @brief 
 * 원격 노이즈 플로어 값 조회 함수
 * @return int8_t 
 */
int8_t BridgeCore::get_remote_noise_floor() const {
    return remote_noise_floor_;
}


/**
 * @brief 
 *  1. serial jtag 또는 wifi udp에서 데이터가 들어왔을 때 esp-now로 전달
 *  2. esp-now에서 데이터가 들어왔을 때 serial jtag 또는 wifi udp로 전달 
 *  3. 큐를 이용한 데이터 전달이므로, 데이터가 들어올 때마다 바로 처리하는 것이 아니라, 큐에 넣고 process_task에서 꺼내서 처리하도록 설계
 *  4. 이 함수는 사용하지 않음.   
 * @param data 
 * @param len  
 * @param source 
 */
void BridgeCore::on_data_received(const uint8_t* data, size_t len, Types::DataSource source) {
    // 데이터 수신 이벤트 처리
}


/**
 * @brief 
 *      1. Serial JTAG에서 데이터가 들어왔는지 주기적으로 체크하여 연결 상태 업데이트 (예: 2초 이상 데이터 수신 없으면 연결 끊김으로 간주)
 * @param pvParameters 
 */
void BridgeCore::process_task(void *pvParameters)
{

    BridgeCore* core = static_cast<BridgeCore*>(pvParameters);
    Types::QueueMessage* msg = nullptr;

    while (true) {

        // 1초 동안 데이터가 없으면 ESP_ERR_TIMEOUT 반환
        esp_err_t ret = core->queue_manager_->dequeue_packet(&msg, pdMS_TO_TICKS(1000));
        if (ret == ESP_OK) {
            // 1. 소스에 따른 분기 처리
            core->handle_incoming_data(msg);
            // 2. 처리가 끝난 메시지는 반드시 여기서 메모리 해제
            core->queue_manager_->free_message(msg);
        }
        else if (ret == ESP_ERR_TIMEOUT) {
        // 여기서 로그를 찍으면 1초마다 로그창이 도배되므로 
        // 로그 대신 상태 체크를 수행합니다.
        }

        // 데이터 유무와 상관없이 주기적으로 연결 상태 체크
       core->check_connection_timeout();

    }

}

void BridgeCore::handle_incoming_data(Types::QueueMessage *msg)
{
    const uint8_t* data = msg->data;
    size_t len = msg->length;
    Types::DataSource source = msg->source;

    // MAVLink 파싱 및 콜백 호출
    static mavlink_message_t mav_msg;
    static mavlink_status_t status;

    if (source == Types::DataSource::UART_SERIAL || source == Types::DataSource::WIFI_UDP) {
        if(wifi_driver_->is_connected(Types::DataSource::WIFI_ESPNOW)){
            esp_err_t ret = wifi_driver_->send_espnow(data, len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send data via ESP-NOW");
            } else {
                ESP_LOGD(TAG, "Forwarded %d bytes to ESP-NOW", len);
            }
        }
    }
    // ESP-NOW에서 들어온 데이터는 Serial JTAG로 전송
    else if (source == Types::DataSource::WIFI_ESPNOW || source == Types::DataSource::INTERNAL ) {

        esp_err_t ret;

        if(source == Types::DataSource::WIFI_ESPNOW){
            bool should_forward = true; // 중계 여부 결정 플래그

            for (size_t ii = 0; ii < len; ii++) {
                if (mavlink_parse_char(MAVLINK_COMM_2, data[ii], &mav_msg, &status)) {                    
                    if(source == Types::DataSource::WIFI_ESPNOW){
                        if(mav_msg.msgid == MAVLINK_MSG_ID_RADIO_STATUS){
                           
                            set_rssi(mavlink_msg_radio_status_get_rssi(&mav_msg));
                            set_noise_floor(mavlink_msg_radio_status_get_noise(&mav_msg));
                            // 가로챈 메시지는 원본을 중계하지 않음
                            should_forward = false;           
                        } else{
                             
                        }
                    }            
                }
            }
            // 2. 중계 로직 (분석 결과 중계가 필요할 때만 실행)
            if (!should_forward) return; // 가로챈 메시지라면 여기서 끝냄    
        }

        if (serial_jtag_driver_->is_connected()){
            ret = serial_jtag_driver_->send_data(data, len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send data via Serial JTAG");
            } else {
                ESP_LOGD(TAG, "Forwarded %d bytes to Serial JTAG", len);
            }
        }

        if(wifi_driver_->is_connected(Types::DataSource::WIFI_UDP)){
            ret = wifi_driver_->send_udp(data, len); // UDP로도 데이터 전달 
            if(ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send data via UDP");
            } else {
                ESP_LOGD(TAG, "Forwarded %d bytes to UDP", len);
            }
        }
    }
}

void BridgeCore::check_connection_timeout() {
    
    { // Serial JTAG 연결 상태 체크
        bool is_now_connected = (serial_jtag_driver_->get_time_since_last_rx() < 2'000'000); // 2초 이상 데이터 수신 없으면 연결 끊김으로 간주
        bool last_state = serial_jtag_driver_->is_connected();

        // 상태가 변했을 때만 처리 (Edge Detection)
        if (is_now_connected != last_state) {
            serial_jtag_driver_->set_connected(is_now_connected);

            if (!is_now_connected) {
                strip_driver_->set_status_warning();
                ESP_LOGW(TAG, "Serial JTAG: [DISCONNECTED]");
                // TODO: 나중에 여기에 '연결됨' 이벤트 발생 코드 삽입
            } 
        }
    }

    {// WIFI UDP 연결 상태 체크
        bool is_now_connected = wifi_driver_->get_time_since_last_udp_rx() < 2'000'000; // 2초 이상 데이터 수신 없으면 연결 끊김으로 간주
        bool last_state = wifi_driver_->is_connected(Types::DataSource::WIFI_UDP);

        // 상태가 변했을 때만 처리 (Edge Detection)
        if (is_now_connected != last_state) {
            wifi_driver_->set_udp_connected(is_now_connected);
            if (!is_now_connected) {
                strip_driver_->set_status_warning();
                ESP_LOGW(TAG, "Wi-Fi UDP: [DISCONNECTED]");
            } 
        }
    }

    {// WIFI ESPNOW 연결 상태 체크
        bool is_now_connected = wifi_driver_->get_time_since_last_espnow_rx() < 2'000'000; // 2초 이상 데이터 수신 없으면 연결 끊김으로 간주
        bool last_state = wifi_driver_->is_connected(Types::DataSource::WIFI_ESPNOW);

        // 상태가 변했을 때만 처리 (Edge Detection)
        if (is_now_connected != last_state) {
            wifi_driver_->set_espnow_connected(is_now_connected);
            if (!is_now_connected) {
                strip_driver_->set_status_warning();
                ESP_LOGW(TAG, "Wi-Fi ESPNOW: [DISCONNECTED]");
            }
        }        
    }
}

/**
 * @brief 
 *      1. 여기는 MAVLINK 메시지 처리, 타이머 이벤트 처리 등 주기적으로 해야 하는 작업들을 수행하는 곳입니다.
 *     2. 타이머 이벤트는 100ms마다 발생하도록 설정되어 있습니다. 따라서 타이머 이벤트가 발생할 때마다 on_timer_tick()이 호출됩니다.
 *     3. 현재는 타이머 이벤트가 발생할 때마다 tick_count를 증가시키고, tick_count에 따라 분기 처리를 하는 형태로 되어 있습니다.  
 */
void BridgeCore::on_timer_tick() {

    static uint8_t buf[1024]={};
    static uint8_t len = 0;  
    static mavlink_message_t msg;
    static uint32_t tick_count = 0;

    static Drivers::WiFiDriver& wifi_driver = get_wifi_driver(); 
    static Core::QueueManager& queue_manager = get_queue_manager();
    
    switch (tick_count) { 
        case 0:            
            break;
        case 1:
            break;
        case 2:{
                uint8_t rssi = get_rssi();
                uint8_t remote_rssi = get_remote_rssi();
                uint8_t noise_floor = get_noise_floor();
                uint8_t remote_noise_floor = get_remote_noise_floor();

                remote_rssi = wifi_driver.is_connected(Types::DataSource::WIFI_ESPNOW)? remote_rssi : -100;
                remote_noise_floor = wifi_driver.is_connected(Types::DataSource::WIFI_ESPNOW)? remote_noise_floor : -105;

                mavlink_msg_radio_status_pack_chan(
                                        Types::Config::SYSTEM_ID, Types::Config::COMPONENT_ID, MAVLINK_COMM_1, &msg,                                    
                                        rssi,  
                                        remote_rssi,
                                        queue_manager.get_queue_usage(),
                                        noise_floor, 
                                        remote_noise_floor,
                                        0, 
                                        0 
                                    );
                len = mavlink_msg_to_send_buffer(buf, &msg);
                queue_manager.enqueue_packet(buf,len,Types::DataSource::INTERNAL);
                break;
            }
        case 3:
            break;
        case 4:
            break;
        case 5:{ // 100ms * 10 중 5번째 슬롯 (1Hz 전송)
                // 1. 배터리 전압 읽기 (예: ADC를 통해 읽은 값, 없으면 고정값으로 테스트)
                // 2. 3.7V 배터리라면 실제 측정값(mV 단위)을 넣으세요.
                uint16_t battery_voltage_mv = 3700; 
                // uint16_t battery_mv = get_battery_voltage_mv();  --> ryu_adc.cpp     
                mavlink_msg_power_status_pack( Types::Config::SYSTEM_ID, Types::Config::COMPONENT_ID, &msg, 
                    battery_voltage_mv, 
                    0,          // Vservo
                    0           // flags
                    );
                len = mavlink_msg_to_send_buffer(buf, &msg);
                queue_manager.enqueue_packet(buf,len,Types::DataSource::INTERNAL);
            }
        break;
            break;
        case 6:
            break;
        case 7:{
                mavlink_msg_heartbeat_pack_chan(
                                        Types::Config::SYSTEM_ID, Types::Config::COMPONENT_ID, MAVLINK_COMM_1, &msg, 
                                        MAV_TYPE_ONBOARD_CONTROLLER,
                                        MAV_AUTOPILOT_INVALID,  // 실질적인 배행체는 아니다.MAV_STATE_CRITICAL
                                        0, 
                                        0, 
                                        MAV_STATE_ACTIVE); 
                len = mavlink_msg_to_send_buffer(buf, &msg);
                queue_manager.enqueue_packet(buf,len,Types::DataSource::INTERNAL);
                break;
            }
            
        case 8:
            break;
        case 9:
            break;
        default:
            break;
    }

    if (tick_count >= 9) 
        tick_count = 0;
    else 
        tick_count++; 
}


} // namespace Core