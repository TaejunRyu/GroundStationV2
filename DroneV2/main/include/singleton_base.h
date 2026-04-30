#pragma once
#include <cstdint>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace drone {

template<typename T>
class Singleton {
protected:
    static T* _instance;
    static SemaphoreHandle_t _lock; // void* 대신 FreeRTOS용 세마포어 사용
    
    Singleton() = default;
    virtual ~Singleton() = default;

public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    /**
     * @brief 인스턴스 획득 (단순 반환)
     */
    static T* getInstance() {
        // init()이 먼저 호출되었다고 가정하고 인스턴스만 반환
        return _instance;
    }

    /**
     * @brief 명시적 초기화 (Double-Checked Locking 방식)
     */
    static esp_err_t init() {
        // 1차 체크: 이미 생성되었다면 즉시 반환 (성능 최적화)
        if (_instance == nullptr) {
            
            // 정적 뮤텍스가 없으면 생성 (최초 1회)
            if (_lock == nullptr) {
                _lock = xSemaphoreCreateMutex();
            }

            // 뮤텍스 획득 (자물쇠 잠금)
            if (xSemaphoreTake(_lock, portMAX_DELAY) == pdTRUE) {
                // 2차 체크: 락을 기다리는 동안 다른 태스크가 만들었는지 확인
                if (_instance == nullptr) {
                    _instance = new (std::nothrow) T();
                    if (_instance == nullptr) {
                        xSemaphoreGive(_lock);
                        return ESP_ERR_NO_MEM;
                    }
                }
                xSemaphoreGive(_lock); // 자물쇠 열기
            }
        }
        return ESP_OK;
    }

    /**
     * @brief 명시적 정리
     */
    static void deinit() {
        if (_lock != nullptr && xSemaphoreTake(_lock, portMAX_DELAY) == pdTRUE) {
            if (_instance != nullptr) {
                delete _instance;
                _instance = nullptr;
            }
            xSemaphoreGive(_lock);
        }
    }

    static bool isInitialized() {
        return _instance != nullptr;
    }
};

// 정적 변수 초기화
template<typename T> T* Singleton<T>::_instance = nullptr;
template<typename T> SemaphoreHandle_t Singleton<T>::_lock = nullptr;

} // namespace drone
