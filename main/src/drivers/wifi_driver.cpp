#include "wifi_driver.h"
#include <esp_event.h>
#include <esp_netif.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <esp_mac.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include <c_library_v2/common/mavlink.h>
#include "bridge_core.h"

namespace Drivers {

const char* WiFiDriver::TAG = "WIFI_DRIVER";
/**
 * @brief Construct a new Wi Fi Driver:: Wi Fi Driver object    
 * 
 */
WiFiDriver::WiFiDriver()
    : udp_rx_buffer_(nullptr), initialized_(false), ap_started_(false),
      espnow_initialized_(false), udp_initialized_(false), rssi_(0),
      udp_pcb_(nullptr), event_loop_(nullptr) {
    memset(drone_mac_, 0, sizeof(drone_mac_));
    ESP_LOGI(TAG, "WiFiDriver created");
}

/**
 * @brief 
 * WiFiDriver 소멸자
 */
WiFiDriver::~WiFiDriver() {
    stop();
    ESP_LOGI(TAG, "WiFiDriver destroyed");
}
/**
 * @brief 
 * WiFiDriver 초기화 함수
 * @return esp_err_t 
 */
esp_err_t WiFiDriver::initialize() {
    if (initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing WiFiDriver...");

    // 이벤트 루프 생성
    esp_event_loop_args_t loop_args = {
        .queue_size = 10,
        .task_name = "wifi_events",
        .task_priority = 5,
        .task_stack_size = 2048,
        .task_core_id = 0
    };

    esp_err_t ret = esp_event_loop_create(&loop_args, &event_loop_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Event loop creation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // NVS 초기화
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // PSRAM 버퍼 초기화
    if (!init_psram_buffer()) {
        ESP_LOGW(TAG, "PSRAM buffer initialization failed");
    }

    // 네트워크 스택 초기화
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Netif initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wi-Fi 초기화
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 이벤트 핸들러 등록
    ret = esp_event_handler_instance_register_with(event_loop_, WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   &wifi_event_handler, this, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi event handler registration failed: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "WiFiDriver initialized");
    return ESP_OK;
}

/**
 * @brief 
 * AP 시작 함수
 * @param config 
 * @return esp_err_t 
 */
esp_err_t WiFiDriver::start_ap(const Types::WiFiConfig& config) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Starting AP with SSID: %s", config.ssid);

    // AP 인터페이스 생성
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Network interface creation failed");
        return ESP_FAIL;
    }

    // Wi-Fi 모드 설정
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "WiFi mode set failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // AP 설정
    wifi_config_t wifi_config = {};
    strlcpy((char*)wifi_config.ap.ssid, config.ssid, sizeof(wifi_config.ap.ssid));
    strlcpy((char*)wifi_config.ap.password, config.password, sizeof(wifi_config.ap.password));
    wifi_config.ap.ssid_len = strlen(config.ssid);
    wifi_config.ap.channel = config.channel;
    wifi_config.ap.authmode = config.auth_mode;
    wifi_config.ap.ssid_hidden = config.hidden ? 1 : 0;
    wifi_config.ap.max_connection = config.max_connections;
    wifi_config.ap.beacon_interval = 100;

    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi config set failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret;
    }


    // 프로토콜 설정
    ret = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi protocol set failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 채널 설정
    ret = esp_wifi_set_channel(config.channel, WIFI_SECOND_CHAN_NONE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi channel set failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 전력 절약 모드 해제
    ret = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi power save set failed: %s", esp_err_to_name(ret));
        return ret;
    }


    ap_started_ = true;
    ESP_LOGI(TAG, "AP started successfully");
    return ESP_OK;
}
/**
 * @brief 
 * ESP-NOW 초기화 함수
 * @param config 
 * @return esp_err_t 
 */
esp_err_t WiFiDriver::init_espnow(const Types::EspNowConfig& config) {
    if (!initialized_) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Initializing ESP-NOW...");

    // 채널 고정
    esp_wifi_set_channel(config.channel, WIFI_SECOND_CHAN_NONE);

    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return ret;
    }
  
    
    // 드론 MAC 주소 저장 - Hardcoded 값으로 설정
    drone_mac_[0] = 0xB0; 
    drone_mac_[1] = 0xCB; 
    drone_mac_[2] = 0xD8; 
    drone_mac_[3] = 0xD7; 
    drone_mac_[4] = 0x2E; 
    drone_mac_[5] = 0xB0;
    
    // Peer 등록 - drone_mac_을 사용하여 send_espnow와 일관성 유지
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, drone_mac_, ESP_NOW_ETH_ALEN);
    peer_info.channel = config.channel;
    peer_info.encrypt = false;
    peer_info.ifidx = WIFI_IF_STA;

    if (esp_now_is_peer_exist(drone_mac_)) {
        esp_now_del_peer(drone_mac_);
    }

    ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW peer registration failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // STA 프로토콜 설정 (LR 모드 대신 일반 모드)
    ret = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    if (ret != ESP_OK) return ret;

    // Peer 속도 설정
    esp_now_rate_config_t rate_config = {};
    rate_config.phymode = config.phy_mode;
    rate_config.rate = config.rate;

    ret = esp_now_set_peer_rate_config(drone_mac_, &rate_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ESP-NOW rate config failed: %s", esp_err_to_name(ret));
    }else{
        ESP_LOGI(TAG, "Drone peer 등록 성공: %02x:%02x:%02x:%02x:%02x:%02x",
                 drone_mac_[0], drone_mac_[1], drone_mac_[2],
                 drone_mac_[3], drone_mac_[4], drone_mac_[5]);    
    }
    espnow_initialized_ = true;
    ESP_LOGI(TAG, "ESP-NOW initialized successfully");
    return ESP_OK;
}


/**
 * @brief 
 * UDP 초기화 함수
 * @return esp_err_t 
 */
esp_err_t WiFiDriver::init_udp() {
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    if (udp_initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing UDP...");

    // UDP PCB 생성
    udp_pcb_ = udp_new();
    if (udp_pcb_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate UDP PCB");
        return ESP_ERR_NO_MEM;
    }

    // 로컬 주소 설정 (모든 인터페이스, 포트 14550)
    ip_addr_t listen_addr;
    IP_ADDR4(&listen_addr, 0, 0, 0, 0);
    listen_addr.type = IPADDR_TYPE_V4;

    err_t err = udp_bind(udp_pcb_, &listen_addr, Types::Config::UDP_PORT);
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "UDP bind failed: %d", err);
        udp_remove(udp_pcb_);
        udp_pcb_ = nullptr;
        return ESP_FAIL;
    }

    udp_initialized_ = true;
    ESP_LOGI(TAG, "UDP initialized on port %d", Types::Config::UDP_PORT);
    return ESP_OK;
}
/**
 * @brief 
 * UDP 수신 콜백 활성화/비활성화 함수 추가
 * 
 */
 void WiFiDriver::enable_udp_recv() {
    if (udp_pcb_ != nullptr) {
        udp_recv(udp_pcb_, udp_recv_callback, this);
        ESP_LOGI(TAG, "UDP receive callback enabled");
    }
}
/**
 * @brief 
 * UDP 수신 콜백 비활성화 함수
 */
void WiFiDriver::disable_udp_recv() {
    if (udp_pcb_ != nullptr) {
        udp_recv(udp_pcb_, nullptr, nullptr);
        ESP_LOGI(TAG, "UDP receive callback disabled");
    }
}

/**
 * @brief 
 *  UDP 데이터 전송 함수
 * @param data 
 * @param len 
 * @return esp_err_t 
 */
esp_err_t WiFiDriver::send_udp(const uint8_t* data, size_t len) {
    if (udp_pcb_ == nullptr || !udp_initialized_) {
        ESP_LOGE(TAG, "UDP not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!p) {
        ESP_LOGE(TAG, "Failed to allocate pbuf");
        return ESP_ERR_NO_MEM;
    }

    memcpy(p->payload, data, len);

    // Broadcast address: 192.168.4.255 (AP의 broadcast 주소)
    ip_addr_t broadcast_addr;
    IP_ADDR4(&broadcast_addr, 192, 168, 4, 255);

    err_t err = udp_sendto(udp_pcb_, p, &broadcast_addr, Types::Config::UDP_PORT);
    pbuf_free(p);

    if (err != ERR_OK) {
        ESP_LOGE(TAG, "UDP sendto failed: %d", err);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "UDP sent %d bytes to broadcast address", len);
    return ESP_OK;
}

/**
 * @brief 
 * ESP-NOW 데이터 전송 함수
 * @param data 
 * @param len 
 * @return esp_err_t 
 */
esp_err_t WiFiDriver::send_espnow(const uint8_t* data, size_t len) {
    if (!espnow_initialized_) return ESP_ERR_INVALID_STATE;

    // ESP_LOGI(TAG, "Sending ESP-NOW data to %02x:%02x:%02x:%02x:%02x:%02x, len: %d",
    //          drone_mac_[0], drone_mac_[1], drone_mac_[2],
    //          drone_mac_[3], drone_mac_[4], drone_mac_[5], len);

    esp_err_t ret = esp_now_send(drone_mac_, data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW send failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief 
 * ESP-NOW 콜백 등록/해제 함수
 */
void WiFiDriver::register_espnow_callbacks() {
    if (espnow_initialized_) {
        esp_now_register_recv_cb(espnow_recv_cb);
        esp_now_register_send_cb(espnow_send_cb);
        ESP_LOGI(TAG, "ESP-NOW callbacks registered");
    }
}

/**
 * @brief 
 * ESP-NOW 콜백 해제 함수
 */
void WiFiDriver::unregister_espnow_callbacks() {
    if (espnow_initialized_) {
        esp_now_unregister_recv_cb();
        esp_now_unregister_send_cb();
        ESP_LOGI(TAG, "ESP-NOW callbacks unregistered");
    }
}
/**
 * @brief 
 * WiFiDriver 종료 함수
 */
void WiFiDriver::stop() {
    if (udp_initialized_) {
        if (udp_pcb_) {
            udp_remove(udp_pcb_);
            udp_pcb_ = nullptr;
        }
        udp_initialized_ = false;
    }

    if (espnow_initialized_) {
        unregister_espnow_callbacks();
        esp_now_deinit();
        espnow_initialized_ = false;
    }

    if (ap_started_) {
        esp_wifi_stop();
        ap_started_ = false;
    }

    if (initialized_) {
        if (event_loop_) {
            esp_event_loop_delete(event_loop_);
            event_loop_ = nullptr;
        }
        esp_wifi_deinit();
        initialized_ = false;
    }

    deinit_psram_buffer();
}

/**
 * @brief 
 * 연결 콜백 설정 함수
 * @param callback 
 */
void WiFiDriver::set_connect_callback(std::function<void()> callback) {
    connect_callback_ = callback;
}

/**
 * @brief 
 * 연결 해제 콜백 설정 함수
 * @param callback 
 */
void WiFiDriver::set_disconnect_callback(std::function<void()> callback) {
    disconnect_callback_ = callback;
}

/**
 * @brief 
 * 데이터 콜백 설정 함수
 * @param callback 
 */
void WiFiDriver::set_data_callback(std::function<void(const uint8_t*, size_t, Types::DataSource)> callback) {
    data_callback_ = callback;
}

/**
 * @brief 
 * 연결 상태 확인 함수
 * @param source 
 * @return bool 
 */
bool WiFiDriver::is_connected(Types::DataSource source) const {
    switch (source) {
        case Types::DataSource::WIFI_UDP:
            return udp_initialized_;
        case Types::DataSource::WIFI_ESPNOW:
            return espnow_initialized_;
        default:
            return false;
    }
}

/**
 * @brief 
 * RSSI 값 조회 함수
 * @return int8_t 
 */
int8_t WiFiDriver::get_rssi() const {
    return rssi_;
}

/**
 * @brief 
 * 내 MAC 주소 조회 함수
 * @return std::array<uint8_t, 6> 
 */
std::array<uint8_t, 6> WiFiDriver::get_my_mac_address() const {
    std::array<uint8_t, 6> mac;
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
    return mac;
}

/**
 * @brief 
 * PSRAM 버퍼 초기화 함수
 * @return bool 
 */
bool WiFiDriver::init_psram_buffer() {
    if (udp_rx_buffer_ == nullptr) {
        udp_rx_buffer_ = (uint8_t*)heap_caps_malloc(Types::Config::UDP_RX_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (udp_rx_buffer_ == nullptr) {
            ESP_LOGW(TAG, "PSRAM buffer allocation failed, using SRAM fallback");
            udp_rx_buffer_ = (uint8_t*)malloc(Types::Config::UDP_RX_BUFFER_SIZE);
            if (udp_rx_buffer_ == nullptr) {
                ESP_LOGE(TAG, "Buffer allocation failed");
                return false;
            }
        }
        ESP_LOGI(TAG, "PSRAM buffer allocated: %d bytes", Types::Config::UDP_RX_BUFFER_SIZE);
    }
    return true;
}

/**
 * @brief 
 * PSRAM 버퍼 해제 함수
 */
void WiFiDriver::deinit_psram_buffer() {
    if (udp_rx_buffer_ != nullptr) {
        free(udp_rx_buffer_);
        udp_rx_buffer_ = nullptr;
        ESP_LOGI(TAG, "PSRAM buffer deallocated");
    }
}


/**
 * @brief 
 * WiFi 이벤트 핸들러 함수
 * @param arg 
 * @param event_base 
 * @param event_id 
 * @param event_data 
 */
void WiFiDriver::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    WiFiDriver* driver = static_cast<WiFiDriver*>(arg);

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started");
                if (driver->connect_callback_) {
                    driver->connect_callback_();
                }
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP stopped");
                if (driver->disconnect_callback_) {
                    driver->disconnect_callback_();
                }
                break;
            default:
                break;
        }
    }
}

/**
 * @brief 
 * ESP-NOW 수신 콜백 함수
 * @param recv_info 
 * @param data 
 * @param len 
 */
void WiFiDriver::espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    // BridgeCore 인스턴스를 통해 콜백 호출
    //ESP_LOGI(TAG, "ESP-NOW received %d bytes", len);
    Core::BridgeCore& bridge = Core::BridgeCore::get_instance();
    bridge.on_data_received(data, len, Types::DataSource::WIFI_ESPNOW);
    ESP_LOGD(TAG, "ESP-NOW received %d bytes", len);
}

/**
 * @brief 
 * ESP-NOW 전송 콜백 함수
 * @param tx_info 
 * @param status 
 */
void WiFiDriver::espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    // TODO: WiFiDriver 인스턴스에 접근해서 콜백 호출
    ESP_LOGD(TAG, "ESP-NOW send status: %d", status);
}

/**
 * @brief 
 * UDP 수신 콜백 함수
 * @param arg 
 * @param pcb 
 * @param p 
 * @param addr 
 * @param port 
 */
void WiFiDriver::udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                   const ip_addr_t *addr, u16_t port) {
    WiFiDriver* driver = static_cast<WiFiDriver*>(arg);

    if (p == nullptr) return;

    // PSRAM 버퍼 활용
    uint8_t *process_buffer;
    bool use_psram = (p->tot_len > 1024) && (driver->udp_rx_buffer_ != nullptr);

    if (use_psram && p->tot_len <= Types::Config::UDP_RX_BUFFER_SIZE) {
        pbuf_copy_partial(p, driver->udp_rx_buffer_, p->tot_len, 0);
        process_buffer = driver->udp_rx_buffer_;
        ESP_LOGD(TAG, "Using PSRAM buffer for large packet: %d bytes", p->tot_len);
    } else {
        process_buffer = (uint8_t*)p->payload;
    }

    //ESP_LOGI(TAG, "UDP received %d bytes from %s:%d", p->tot_len, ipaddr_ntoa(addr), port);
    //Core::BridgeCore& bridge = Core::BridgeCore::get_instance();
    //bridge.on_data_received(process_buffer, p->tot_len, Types::DataSource::WIFI_UDP);

    
    // MAVLink 파싱 및 콜백 호출
    static mavlink_message_t msg;
    static mavlink_status_t status;
    static uint8_t temp_buffer[1024];

    for (size_t ii = 0; ii < p->tot_len; ii++) {
        if (mavlink_parse_char(MAVLINK_COMM_2, process_buffer[ii], &msg, &status)) {
            int packet_len = mavlink_msg_to_send_buffer(temp_buffer, &msg);

            if (driver->data_callback_) {
                driver->data_callback_(temp_buffer, packet_len, Types::DataSource::WIFI_UDP);
            }
        }
    }

    pbuf_free(p);
}

} // namespace Drivers