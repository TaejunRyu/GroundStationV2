cmake_minimum_required(VERSION 3.16)

# 프로젝트 이름 설정 (보통 상위 CMakeLists.txt에 있음)
# project(ground_station) 

#set(CMAKE_CXX_STANDARD 17)

# 1. 컴포넌트 등록
idf_component_register(
    SRCS 
        "src/main.cpp"
        "src/drivers/led_strip_driver.cpp"
        "src/drivers/adc_driver.cpp"
        "src/drivers/flysky_sensor.cpp"
        "src/drivers/led_driver.cpp"
        "src/drivers/uart_driver.cpp"
        "src/drivers/serial_jtag_driver.cpp"
        "src/core/bridge_core.cpp"
        "src/core/queue_manager.cpp"
        "src/services/mavlink_service.cpp"
        "src/services/timer_service.cpp"
        "src/services/ota_service.cpp"
        "src/drivers/wifi_driver.cpp"
        "src/utils/config_manager.cpp"
        "src/utils/memory_manager.cpp"
    INCLUDE_DIRS 
        "include" 
        "src/core" 
        "src/services" 
        "src/drivers" 
        "src/utils"
    REQUIRES 
        esp_wifi esp_netif esp_event nvs_flash driver  espressif__cjson  
        esp_http_client app_update spi_flash esp_partition 
        esp_system freertos log heap lwip  esp_timer  esp_pm   esp_driver_gpio
        esp_driver_uart   driver esp_adc esp_https_ota esp_driver_usb_serial_jtag
    PRIV_REQUIRES 
        espressif__mavlink espressif__led_strip 
)

# 2. 컴포넌트 라이브러리 타겟에 옵션 적용
# idf_component_register는 내부적으로 __component_$(현재디렉토리명) 형태의 타겟을 생성하지만,
# 보통 컴포넌트 CMake 내에서는 'COMPONENT_LIB' 변수를 사용하면 됩니다.

if(TARGET ${COMPONENT_LIB})
    # 기본 컴파일 옵션
    target_compile_options(${COMPONENT_LIB} PRIVATE 
        -Wall -Wextra -Werror 
        -Wno-unused-parameter -Wno-sign-compare
        -Wno-deprecated-declarations
        -Wno-address-of-packed-member
        -O3
        -std=gnu++17
    )

    # ESP32S3 전용 최적화
    if(CONFIG_IDF_TARGET_ESP32S3)
        target_compile_options(${COMPONENT_LIB} PRIVATE  -DESP32S3=1 -mlongcalls)
    endif()

    # 빌드 타입별 설정
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${COMPONENT_LIB} PRIVATE -g3 -O0 -DDEBUG=1)
        # 메모리 래핑 (Linker Options는 target_link_options 사용)
        target_link_options(${COMPONENT_LIB} PRIVATE 
            "-Wl,--wrap=malloc" 
            "-Wl,--wrap=free" 
            "-Wl,--wrap=calloc" 
            "-Wl,--wrap=realloc"
        )
    else()
        target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -DNDEBUG=1)
    endif()

    # PSRAM 지원
    if(CONFIG_SPIRAM)
        target_compile_options(${COMPONENT_LIB} PRIVATE -DCONFIG_SPIRAM=1)
    endif()
endif()

# 링커 옵션 보강 (C++ 표준 라이브러리 링킹 보장)
#target_link_libraries(${COMPONENT_LIB} PUBLIC cxx) 