#include "memory_manager.h"
#include <esp_system.h>
#include <cstring>

namespace Utils {

const char* MemoryManager::TAG = "MEMORY_MANAGER";

bool MemoryManager::initialized_ = false;

esp_err_t MemoryManager::initialize() {
    if (initialized_) return ESP_OK;

    ESP_LOGI(TAG, "Initializing MemoryManager...");

    // PSRAM이 사용 가능한지 확인
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram_size == 0) {
        ESP_LOGE(TAG, "PSRAM not available");
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGI(TAG, "PSRAM size: %u bytes", psram_size);
    initialized_ = true;

    log_memory_stats();
    return ESP_OK;
}

void MemoryManager::deinitialize() {
    initialized_ = false;
}

void* MemoryManager::allocate_psram(size_t size) {
    if (!initialized_) {
        ESP_LOGE(TAG, "MemoryManager not initialized");
        return nullptr;
    }

    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr == nullptr) {
        ESP_LOGE(TAG, "PSRAM allocation failed for %u bytes", size);
        return nullptr;
    }

    ESP_LOGD(TAG, "Allocated %u bytes in PSRAM at %p", size, ptr);
    return ptr;
}

void MemoryManager::deallocate_psram(void* ptr) {
    if (ptr != nullptr) {
        ESP_LOGD(TAG, "Deallocating PSRAM at %p", ptr);
        heap_caps_free(ptr);
    }
}

void MemoryManager::log_memory_stats() {
    if (!initialized_) return;

    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "PSRAM Memory Stats:");
    ESP_LOGI(TAG, "  Total: %u bytes", info.total_allocated_bytes + info.total_free_bytes);
    ESP_LOGI(TAG, "  Used: %u bytes", info.total_allocated_bytes);
    ESP_LOGI(TAG, "  Free: %u bytes", info.total_free_bytes);
    ESP_LOGI(TAG, "  Largest free block: %u bytes", info.largest_free_block);
    ESP_LOGI(TAG, "  Minimum free: %u bytes", info.minimum_free_bytes);
    ESP_LOGI(TAG, "  Allocated blocks: %u", info.allocated_blocks);
    ESP_LOGI(TAG, "  Free blocks: %u", info.free_blocks);
}

size_t MemoryManager::get_free_psram() {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    return info.total_free_bytes;
}

size_t MemoryManager::get_total_psram() {
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    return info.total_allocated_bytes + info.total_free_bytes;
}


} // namespace Utils