#pragma once

#include <memory>
#include <esp_log.h>
#include "bridge_types.h"

// 1. 클래스 내부가 아닌 여기서 먼저 선언하세요.
// 1. 서비스들이 속한 네임스페이스별로 전방 선언을 정확히 배치합니다.
namespace Services {
    class MavlinkService;
    class TimerService;
}

namespace Drivers {
    class WiFiDriver;
    //class UartDriver;
    class SerialJtagDriver;
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

    Drivers::SerialJtagDriver& get_serial_jtag_driver() { return *serial_jtag_driver_; }

private:

    // 서비스 객체들 (unique_ptr로 관리)
    std::unique_ptr<Utils::ConfigManager> config_manager_;
    std::unique_ptr<Utils::MemoryManager> memory_manager_;

    std::unique_ptr<Services::MavlinkService> mavlink_service_;
    std::unique_ptr<Services::TimerService> timer_service_;
    std::unique_ptr<Drivers::WiFiDriver> wifi_driver_;
    std::unique_ptr<Drivers::SerialJtagDriver> serial_jtag_driver_;
    std::unique_ptr<Core::QueueManager> queue_manager_;

    Types::SystemState system_state_;
    static const char* TAG;
};

} // namespace Core