#include "queue_manager.h"
#include <cstring>
#include <esp_log.h>

namespace Core {

const char* QueueManager::TAG = "QUEUE_MANAGER";

QueueManager::QueueManager()
    : packet_queue_(nullptr), event_queue_(nullptr) {
    ESP_LOGI(TAG, "QueueManager created");
}

QueueManager::~QueueManager() {
    deinitialize();
    ESP_LOGI(TAG, "QueueManager destroyed");
}

esp_err_t QueueManager::initialize() {
    ESP_LOGI(TAG, "Initializing QueueManager...");

    // 패킷 큐 생성
    packet_queue_ = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(Types::QueueMessage*));
    if (!packet_queue_) {
        ESP_LOGE(TAG, "Packet queue creation failed");
        return ESP_ERR_NO_MEM;
    }

    // 이벤트 큐 생성
    event_queue_ = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(Types::EventData));
    if (!event_queue_) {
        ESP_LOGE(TAG, "Event queue creation failed");
        vQueueDelete(packet_queue_);
        packet_queue_ = nullptr;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "QueueManager initialized");
    return ESP_OK;
}

void QueueManager::deinitialize() {
    if (packet_queue_) {
        // 큐에 남아있는 메시지들 해제
        Types::QueueMessage* message;
        while (xQueueReceive(packet_queue_, &message, 0) == pdTRUE) {
            free_message(message);
        }
        vQueueDelete(packet_queue_);
        packet_queue_ = nullptr;
    }

    if (event_queue_) {
        vQueueDelete(event_queue_);
        event_queue_ = nullptr;
    }
}

esp_err_t QueueManager::enqueue_packet(const uint8_t* data, size_t len, Types::DataSource source) {
    if (!packet_queue_ || len > MAX_MESSAGE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    Types::QueueMessage* message = allocate_message(len);
    if (!message) {
        ESP_LOGE(TAG, "Message allocation failed");
        return ESP_ERR_NO_MEM;
    }

    message->source = source;
    message->length = len;
    memcpy(message->data, data, len);

    if (xQueueSend(packet_queue_, &message, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Packet queue full, dropping message");
        free_message(message);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGD(TAG, "Packet enqueued: source=%d, len=%d", static_cast<int>(source), len);
    return ESP_OK;
}

esp_err_t QueueManager::dequeue_packet(Types::QueueMessage** message, TickType_t timeout) {
    if (!packet_queue_ || !message) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xQueueReceive(packet_queue_, message, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGD(TAG, "Packet dequeued: source=%d, len=%d", static_cast<int>((*message)->source), (*message)->length);
    return ESP_OK;
}

esp_err_t QueueManager::enqueue_event(const Types::EventData& event) {
    if (!event_queue_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(event_queue_, &event, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping event");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGD(TAG, "Event enqueued: type=%d", static_cast<int>(event.event_type));
    return ESP_OK;
}

esp_err_t QueueManager::dequeue_event(Types::EventData* event, TickType_t timeout) {
    if (!event_queue_ || !event) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xQueueReceive(event_queue_, event, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGD(TAG, "Event dequeued: type=%d", static_cast<int>(event->event_type));
    return ESP_OK;
}

size_t QueueManager::get_packet_queue_size() const {
    return packet_queue_ ? uxQueueMessagesWaiting(packet_queue_) : 0;
}
 
size_t QueueManager::get_event_queue_size() const {
    return event_queue_ ? uxQueueMessagesWaiting(event_queue_) : 0;
}

Types::QueueMessage* QueueManager::allocate_message(size_t data_len) {
    size_t total_size = sizeof(Types::QueueMessage) + data_len;

    // 대용량 데이터는 PSRAM 사용
    if (total_size > 1024) {
        Types::QueueMessage* message = (Types::QueueMessage*)heap_caps_malloc(total_size, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if (message) {
            ESP_LOGD(TAG, "Allocated %d bytes in PSRAM for message", total_size);
            return message;
        } else {
            ESP_LOGW(TAG, "PSRAM allocation failed, falling back to SRAM");
        }
    }

    // PSRAM 실패 시 SRAM 사용
    Types::QueueMessage* message = (Types::QueueMessage*)malloc(total_size);
    if (message) {
        ESP_LOGD(TAG, "Allocated %d bytes in SRAM for message", total_size);
    }

    return message;
}

void QueueManager::free_message(Types::QueueMessage* message) {
    if (message) {
        heap_caps_free(message);
        ESP_LOGD(TAG, "Message freed");
    }
}

/**
 * 사용률 반환
 */
uint8_t QueueManager::get_queue_usage(){
    return (uint8_t)((uxQueueMessagesWaiting(packet_queue_) * 100) / PACKET_QUEUE_SIZE);
}

} // namespace Core