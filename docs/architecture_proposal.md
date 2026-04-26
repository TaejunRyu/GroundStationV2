# ESP32S3 Ground Station - 개선된 아키텍처 제안
# ===============================================

## 현재 구조의 문제점
1. 모든 코드가 main/에 집중되어 관심사 분리가 부족
2. 컴포넌트 간 의존성이 복잡하게 얽혀 있음
3. 테스트하기 어려운 구조
4. 유지보수가 어려움
5. 확장성이 제한적

## 제안하는 개선된 구조

```
ground_station/
├── CMakeLists.txt
├── sdkconfig
├── partitions.csv
├── README.md
├── src/
│   ├── main.cpp                    # 앱 메인 엔트리 (단순화)
│   ├── core/
│   │   ├── bridge_core.cpp         # 브리지 핵심 로직
│   │   ├── bridge_core.h
│   │   ├── system_manager.cpp      # 시스템 관리
│   │   └── system_manager.h
│   ├── drivers/
│   │   ├── wifi_driver.cpp         # Wi-Fi/ESP-NOW 드라이버
│   │   ├── wifi_driver.h
│   │   ├── uart_driver.cpp         # UART 드라이버
│   │   ├── uart_driver.h
│   │   ├── usb_driver.cpp          # USB Serial/JTAG 드라이버
│   │   └── usb_driver.h
│   ├── services/
│   │   ├── mavlink_service.cpp     # MAVLink 처리 서비스
│   │   ├── mavlink_service.h
│   │   ├── ota_service.cpp         # OTA 서비스
│   │   ├── ota_service.h
│   │   ├── timer_service.cpp       # 타이머 서비스
│   │   └── timer_service.h
│   ├── protocols/
│   │   ├── espnow_protocol.cpp     # ESP-NOW 프로토콜
│   │   ├── espnow_protocol.h
│   │   ├── flysky_protocol.cpp     # FlySky 프로토콜
│   │   └── flysky_protocol.h
│   └── utils/
│       ├── config_manager.cpp      # 설정 관리
│       ├── config_manager.h
│       ├── led_controller.cpp      # LED 제어
│       ├── led_controller.h
│       ├── queue_manager.cpp       # 큐 관리
│       └── queue_manager.h
├── include/
│   ├── bridge_types.h              # 공통 타입 정의
│   ├── bridge_config.h             # 설정 헤더
│   └── bridge_events.h             # 이벤트 정의
├── test/
│   ├── CMakeLists.txt
│   ├── test_mavlink.cpp
│   ├── test_wifi.cpp
│   └── test_protocols.cpp
└── docs/
    ├── architecture.md
    ├── api_reference.md
    └── development_guide.md
```

## 주요 개선점

### 1. 관심사 분리 (Separation of Concerns)
- **core/**: 시스템 핵심 로직
- **drivers/**: 하드웨어 추상화 계층
- **services/**: 비즈니스 로직 서비스
- **protocols/**: 통신 프로토콜 처리
- **utils/**: 유틸리티 및 헬퍼 함수

### 2. 의존성 역전 (Dependency Inversion)
- 인터페이스를 통한 느슨한 결합
- 상위 모듈이 하위 모듈에 의존하지 않도록 설계

### 3. 단일 책임 원칙 (Single Responsibility)
- 각 모듈이 하나의 책임만 가지도록 분리

### 4. 테스트 용이성
- 각 컴포넌트를 독립적으로 테스트 가능
- 모의 객체(Mock)를 통한 단위 테스트 지원

## 구현 예시 코드

### 1. Core - Bridge Core (src/core/bridge_core.h)

```cpp
#pragma once

#include <memory>
#include "bridge_types.h"

namespace Core {

class BridgeCore {
public:
    BridgeCore();
    ~BridgeCore();

    esp_err_t initialize();
    esp_err_t start();
    void stop();

    // 이벤트 처리 인터페이스
    void on_wifi_connected();
    void on_wifi_disconnected();
    void on_data_received(const uint8_t* data, size_t len, DataSource source);

private:
    std::unique_ptr<Services::MavlinkService> mavlink_service_;
    std::unique_ptr<Services::TimerService> timer_service_;
    std::unique_ptr<Drivers::WiFiDriver> wifi_driver_;
    std::unique_ptr<Utils::QueueManager> queue_manager_;

    SystemState system_state_;
};

} // namespace Core
```

### 2. Services - MAVLink Service (src/services/mavlink_service.h)

```cpp
#pragma once

#include <memory>
#include <functional>
#include "bridge_types.h"

namespace Services {

class MavlinkService {
public:
    using DataCallback = std::function<void(const uint8_t*, size_t)>;

    MavlinkService();
    ~MavlinkService();

    esp_err_t initialize();
    esp_err_t process_packet(const uint8_t* data, size_t len);
    esp_err_t send_heartbeat();
    esp_err_t send_status_text(const char* text, uint8_t severity);

    void set_data_callback(DataCallback callback) {
        data_callback_ = callback;
    }

private:
    DataCallback data_callback_;
    mavlink_system_t mavlink_system_;
    mavlink_status_t mavlink_status_;
};

} // namespace Services
```

### 3. Drivers - WiFi Driver (src/drivers/wifi_driver.h)

```cpp
#pragma once

#include <memory>
#include <functional>
#include "bridge_types.h"

namespace Drivers {

class WiFiDriver {
public:
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void()>;
    using DataCallback = std::function<void(const uint8_t*, size_t)>;

    WiFiDriver();
    ~WiFiDriver();

    esp_err_t initialize();
    esp_err_t start_ap(const WiFiConfig& config);
    esp_err_t start_sta(const WiFiConfig& config);
    esp_err_t init_espnow();
    esp_err_t send_data(const uint8_t* data, size_t len);

    void set_connect_callback(ConnectCallback callback) {
        connect_callback_ = callback;
    }
    void set_disconnect_callback(DisconnectCallback callback) {
        disconnect_callback_ = callback;
    }
    void set_data_callback(DataCallback callback) {
        data_callback_ = callback;
    }

private:
    ConnectCallback connect_callback_;
    DisconnectCallback disconnect_callback_;
    DataCallback data_callback_;

    udp_pcb* udp_pcb_;
    esp_now_peer_info_t peer_info_;
};

} // namespace Drivers
```

### 4. Utils - Queue Manager (src/utils/queue_manager.h)

```cpp
#pragma once

#include <memory>
#include "bridge_types.h"

namespace Utils {

class QueueManager {
public:
    QueueManager();
    ~QueueManager();

    esp_err_t initialize();
    esp_err_t enqueue_packet(const uint8_t* data, size_t len, PacketType type);
    esp_err_t dequeue_packet(uint8_t* buffer, size_t* len, PacketType* type, TickType_t timeout = portMAX_DELAY);
    UBaseType_t get_queue_size() const;

private:
    QueueHandle_t packet_queue_;
    const size_t QUEUE_SIZE = 40;
};

} // namespace Utils
```

### 5. Main Entry Point (src/main.cpp)

```cpp
#include "core/bridge_core.h"

extern "C" void app_main(void) {
    // 시스템 초기화
    check_system_health_on_boot();

    // 브리지 코어 생성 및 초기화
    auto bridge_core = std::make_unique<Core::BridgeCore>();
    if (bridge_core->initialize() != ESP_OK) {
        ESP_LOGE("MAIN", "Bridge core initialization failed");
        return;
    }

    // 브리지 시작
    if (bridge_core->start() != ESP_OK) {
        ESP_LOGE("MAIN", "Bridge core start failed");
        return;
    }

    // 메인 루프 (감시용)
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        // 시스템 상태 모니터링
    }
}
```

## 장점

### 1. 유지보수성 향상
- 각 모듈이 독립적이므로 수정이 용이
- 인터페이스 변경 시 영향 범위가 제한적

### 2. 테스트 용이성
- 각 컴포넌트를 독립적으로 단위 테스트 가능
- 모의 객체를 통한 통합 테스트 지원

### 3. 확장성
- 새로운 프로토콜이나 드라이버 추가가 쉬움
- 플러그인 방식으로 기능 확장 가능

### 4. 코드 재사용성
- 모듈별로 다른 프로젝트에 재사용 가능

### 5. 디버깅 용이성
- 관심사별로 분리되어 문제 발생 시 원인 파악이 빠름

## 마이그레이션 전략

### Phase 1: 구조 재배치
1. 현재 파일들을 새로운 구조로 이동
2. 인터페이스 정의
3. 기본적인 컴파일 확인

### Phase 2: 리팩토링
1. 각 모듈의 책임 명확화
2. 의존성 주입 적용
3. 인터페이스 기반 설계 적용

### Phase 3: 최적화
1. 메모리 사용 최적화
2. 성능 튜닝
3. 테스트 코드 작성

## 결론

이러한 구조로 리팩토링하면 코드의 품질, 유지보수성, 확장성이 크게 향상됩니다. 특히 ESP32S3의 고성능을 최대한 활용하면서도 안정적인 시스템을 구축할 수 있습니다.