#pragma once

#include <memory>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>

#include "bridge_types.h"

// 1. 클래스 내부가 아닌 여기서 먼저 선언하세요.
// 2. 서비스들이 속한 네임스페이스별로 전방 선언을 정확히 배치합니다.
namespace Services {
    class MavlinkService;
    class TimerService;
}

namespace Drivers {
    class WiFiDriver;
    class SerialJtagDriver;
    class LedStripDriver;
    class AdcDriver;
}

namespace Core {
    class QueueManager;
}

namespace Utils {
    class ConfigManager;
    class MemoryManager;
}


namespace Core {

class BridgeCore {
private:
    BridgeCore();
public:
    // 🌟 복사 생성자와 대입 연산자 비활성화 (싱글톤 복사 방지)
    BridgeCore(const BridgeCore&) = delete;
    BridgeCore& operator=(const BridgeCore&) = delete;
    ~BridgeCore();

    esp_err_t initialize();
    void deinitialize();
    esp_err_t start();
    void stop();

    void start_task();

    // 이벤트 처리 인터페이스
    void on_wifi_connected();
    void on_wifi_disconnected();
    void on_data_received(const uint8_t* data, size_t len, Types::DataSource source);
    void on_timer_tick();
    
    // 싱글톤 인스턴스 접근 메서드
    // 🌟 get_instance() 메서드 구현
    static BridgeCore& get_instance() {
        static BridgeCore* instance = new BridgeCore(); // 힙에 할당하여 소멸 순서 꼬임 방지
        return *instance;
    }

    // 서비스 및 드라이버 접근자 (참조자로만 넘뎌준다)
    Drivers::WiFiDriver& get_wifi_driver() { return *wifi_driver_; }
    Drivers::SerialJtagDriver& get_serial_jtag_driver() { return *serial_jtag_driver_; }
    Drivers::LedStripDriver& get_led_strip_driver() { return *strip_driver_; }
    Drivers::AdcDriver& get_adc_driver(){return *adc_driver_;}
    Core::QueueManager& get_queue_manager() { return *queue_manager_; }
    Services::MavlinkService& get_mavlink_service(){return *mavlink_service_;}

    TaskHandle_t& get_task_handle() { return process_task_handle_; }
    static void process_task(void* pvParameters);
    void handle_incoming_data(Types::QueueMessage* msg);
    void check_connection_timeout();

    int8_t  get_rssi() const;
    void    set_rssi(int8_t rssi) { rssi_ = (uint8_t)((rssi + 121) * 2); }
    int8_t  get_noise_floor() const;
    void    set_noise_floor(int8_t noise_floor) { noise_floor_ = (uint8_t)((noise_floor + 121) * 2); }
    int8_t  get_remote_rssi() const;
    void    set_remote_rssi(int8_t rssi) { remote_rssi_ = (uint8_t)((rssi + 121) * 2); }
    int8_t  get_remote_noise_floor() const;
    void    set_remote_noise_floor(int8_t noise_floor) { remote_noise_floor_ = (uint8_t)((noise_floor + 121) * 2); }


private: 

    // 서비스 객체들 (unique_ptr로 관리)
    std::unique_ptr<Utils::ConfigManager> config_manager_;
    std::unique_ptr<Utils::MemoryManager> memory_manager_;

    std::unique_ptr<Services::MavlinkService> mavlink_service_;
    std::unique_ptr<Services::TimerService> timer_service_;

    std::unique_ptr<Drivers::AdcDriver> adc_driver_;
    std::unique_ptr<Drivers::LedStripDriver> strip_driver_;    
    std::unique_ptr<Drivers::WiFiDriver> wifi_driver_;
    std::unique_ptr<Drivers::SerialJtagDriver> serial_jtag_driver_;
    
    std::unique_ptr<Core::QueueManager> queue_manager_;

    Types::SystemState system_state_;

    TaskHandle_t process_task_handle_;
    
        // 드론과 브리지의 RSSI 및 노이즈 플로어 정보
    std::atomic<int8_t>  rssi_{0}, noise_floor_{0};
    std::atomic<int8_t>  remote_rssi_{0}, remote_noise_floor_{0};
    
    static const char* TAG;
};

} // namespace Core