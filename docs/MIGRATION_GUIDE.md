# Ground Station v2.0 마이그레이션 가이드

## Phase 1: 구조 재배치 (현재 진행 중)

### 완료된 작업
- [x] 새로운 디렉토리 구조 생성 (`src/core/`, `src/services/`, `src/drivers/`, `src/utils/`)
- [x] 공통 타입 정의 (`include/bridge_types.h`)
- [x] BridgeCore 클래스 구현
- [x] WiFiDriver 구현 (PSRAM 활용)
- [x] QueueManager 구현 (PSRAM 큐)
- [x] 새로운 main.cpp
- [x] CMakeLists.txt 업데이트
- [x] README_v2.md 작성
- [x] 기본 테스트 코드
- [x] SDK 설정 파일

### 다음 단계
1. **기존 파일들을 새로운 구조로 이동**
   ```
   mv main/ryu_wifi.cpp src/drivers/wifi_driver.cpp (기존 코드 통합)
   mv main/ryu_mavlink.cpp src/services/mavlink_service.cpp
   mv main/ryu_timer.cpp src/services/timer_service.cpp
   mv main/ryu_ota.cpp src/services/ota_service.cpp
   mv main/ryu_uart.cpp src/drivers/uart_driver.cpp
   mv main/ryu_led.cpp src/drivers/led_driver.cpp
   ```

2. **헤더 파일 정리**
   ```
   mv main/include/*.h include/
   ```

## Phase 2: 인터페이스 분리

### 목표
- 각 모듈의 인터페이스 정의
- 의존성 역전 원칙 적용
- 추상화 레이어 추가

### 작업 항목
- [ ] IService 인터페이스 정의
- [ ] IDriver 인터페이스 정의
- [ ] BridgeCore에 인터페이스 주입
- [ ] 팩토리 패턴 적용

## Phase 3: 의존성 주입 적용

### 목표
- 생성자 주입을 통한 느슨한 결합
- 테스트 용이성 향상
- 모듈 교체 가능성

### 작업 항목
- [ ] DI 컨테이너 구현
- [ ] 모의 객체(Mock) 지원
- [ ] 설정 기반 객체 생성

## Phase 4: 테스트 코드 작성

### 목표
- 단위 테스트 80% 이상 커버리지
- 통합 테스트 추가
- CI/CD 파이프라인 구축

### 작업 항목
- [ ] Unity 테스트 프레임워크 설정
- [ ] 각 모듈별 단위 테스트
- [ ] 메모리 누수 테스트
- [ ] 성능 테스트

## Phase 5: 최적화 및 검증

### 목표
- 메모리 사용량 최적화
- CPU 사용량 감소
- 안정성 검증

### 작업 항목
- [ ] 메모리 프로파일링
- [ ] CPU 사용량 분석
- [ ] 장시간 안정성 테스트
- [ ] 현장 테스트

## 마이그레이션 명령어 예시

```bash
# Phase 1: 구조 재배치
mkdir -p src/{core,services,drivers,utils} test
mv main/ryu_*.cpp src/services/  # 서비스 파일들 이동
mv main/ryu_*.cpp src/drivers/   # 드라이버 파일들 이동

# 기존 main.cpp 백업
cp main/ryu_main.cpp main/ryu_main.cpp.backup

# 새로운 main.cpp 적용
cp src/main.cpp main/ryu_main.cpp

# 빌드 테스트
idf.py build
```

## 주의사항

1. **ESP32S3 전용 코드**: PSRAM 관련 코드는 ESP32S3에서만 동작
2. **메모리 관리**: PSRAM 할당 실패 시 SRAM으로 fallback
3. **Task 스택**: 각 task의 스택 사이즈를 실제 사용량에 맞게 조정
4. **Wi-Fi 최적화**: ESP32S3의 향상된 Wi-Fi 성능 활용

## 검증 포인트

- [ ] 컴파일 성공
- [ ] Wi-Fi AP 모드 정상 동작
- [ ] ESP-NOW 통신 정상
- [ ] MAVLink 메시지 처리 정상
- [ ] 메모리 누수 없음
- [ ] CPU 사용량 적정 수준
- [ ] OTA 업데이트 정상

## 예상 이점

1. **유지보수성 향상**: 모듈화로 인한 코드 분리
2. **확장성 증가**: 새로운 기능 추가 용이
3. **테스트 용이성**: 단위 테스트 지원
4. **성능 최적화**: PSRAM 적극 활용
5. **안정성 향상**: RAII 패턴과 예외 처리