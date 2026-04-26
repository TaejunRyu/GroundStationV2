#pragma once

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <memory>
#include <vector>

namespace Utils {

class MemoryManager {
public:
    static esp_err_t initialize();
    static void deinitialize();

    // PSRAM 메모리 할당/해제
    static void* allocate_psram(size_t size);
    static void deallocate_psram(void* ptr);

    // 메모리 통계
    static void log_memory_stats();
    static size_t get_free_psram();
    static size_t get_total_psram();

    // 스마트 포인터 래퍼
    template<typename T>
    static std::unique_ptr<T, decltype(&deallocate_psram)> make_psram_unique(size_t count = 1) {
        return std::unique_ptr<T, decltype(&deallocate_psram)>(
            static_cast<T*>(allocate_psram(sizeof(T) * count)),
            &deallocate_psram
        );
    }

    template<typename T>
    static std::shared_ptr<T> make_psram_shared(size_t count = 1) {
        return std::shared_ptr<T>(
            static_cast<T*>(allocate_psram(sizeof(T) * count)),
            &deallocate_psram
        );
    }

private:
    static bool initialized_;
    static const char* TAG;
};

// 호환성을 위한 기존 함수들
extern void* init_psram_buffer(size_t size);
extern void free_psram_buffer(void* buffer);

} // namespace Utils