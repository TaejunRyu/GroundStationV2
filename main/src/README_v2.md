# Ground Station v2.0 - ESP32S3 기반 지상 관제 시스템

## 개요

ESP32S3 마이크로컨트롤러를 기반으로 한 고성능 지상 관제 시스템입니다. MAVLink 프로토콜을 통한 드론 통신, Wi-Fi AP 모드, ESP-NOW P2P 통신을 지원합니다.

## 새로운 아키텍처 특징

### 1. 모듈화된 설계
- **Core**: 시스템 핵심 로직 (BridgeCore, QueueManager)
- **Services**: 비즈니스 로직 (MAVLink, Timer, OTA)
- **Drivers**: 하드웨어 추상화 (Wi-Fi, UART, LED)
- **Utils**: 유틸리티 함수 (Config, Memory 관리)

### 2. ESP32S3 최적화
- 8MB PSRAM 적극 활용
- USB Serial/JTAG 디버깅 지원
- 향상된 Wi-Fi 성능

### 3. 메모리 관리
- PSRAM을 활용한 대용량 버퍼
- 스마트 메모리 할당 (PSRAM 우선, SRAM fallback)
- 메모리 누수 방지

### 4. 안정성 향상
- RAII 패턴 적용
- 예외 안전성
- Task 안전 중지/재시작

## 빌드 및 실행

```bash
# ESP-IDF 환경 설정
. ~/esp/esp-idf/export.sh

# 프로젝트 빌드
idf.py build

# 플래시 및 모니터
idf.py flash monitor
```

## 디렉토리 구조

```
src/
├── core/           # 시스템 코어
│   ├── bridge_core.h/cpp
│   └── queue_manager.h/cpp
├── services/       # 서비스 레이어
│   ├── mavlink_service.h/cpp
│   ├── timer_service.h/cpp
│   └── ota_service.h/cpp
├── drivers/        # 하드웨어 드라이버
│   ├── wifi_driver.h/cpp
│   ├── uart_driver.h/cpp
│   └── led_driver.h/cpp
└── utils/          # 유틸리티
    ├── config_manager.h/cpp
    └── memory_manager.h/cpp

include/            # 공통 헤더
test/              # 단위 테스트
```

## 설정

`menuconfig`를 통해 다음 설정을 구성할 수 있습니다:

- Wi-Fi SSID/Password
- ESP-NOW 채널
- MAVLink 시스템 ID
- 메모리 관리 옵션

## 모니터링

시스템은 5초마다 메모리 사용량을 모니터링하며, 저메모리 경고를 출력합니다.

## 개발자 노트

이 새로운 아키텍처는 다음과 같은 소프트웨어 공학 원칙을 따릅니다:

- **단일 책임 원칙**: 각 클래스는 하나의 책임만 가짐
- **의존성 역전**: 인터페이스를 통한 느슨한 결합
- **개방-폐쇄 원칙**: 확장을 위한 개방, 수정을 위한 폐쇄
- **RAII 패턴**: 리소스 관리 자동화

## 마이그레이션 가이드

기존 코드를 새로운 구조로 마이그레이션하려면:

1. Phase 1: 파일 구조 재배치
2. Phase 2: 인터페이스 분리
3. Phase 3: 의존성 주입 적용
4. Phase 4: 테스트 코드 작성
5. Phase 5: 최적화 및 검증