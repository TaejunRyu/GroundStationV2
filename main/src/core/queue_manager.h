#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_heap_caps.h>
#include "bridge_types.h"

namespace Core {

class QueueManager {
public:
    QueueManager();
    ~QueueManager();

    esp_err_t initialize();
    void deinitialize(); 

   
    // 패킷 큐잉
    esp_err_t enqueue_packet(const uint8_t* data, size_t len, Types::DataSource source);
    esp_err_t dequeue_packet(Types::QueueMessage** message, TickType_t timeout = portMAX_DELAY);

    // 이벤트 큐잉
    esp_err_t enqueue_event(const Types::EventData& event);
    esp_err_t dequeue_event(Types::EventData* event, TickType_t timeout = portMAX_DELAY);

    void free_message(Types::QueueMessage* message);
 
    // 큐 상태
    size_t get_packet_queue_size() const;
    size_t get_event_queue_size() const;
    size_t get_queue_usage();
private:
    // PSRAM을 활용한 큐 메시지 할당/해제
    Types::QueueMessage* allocate_message(size_t data_len);

    QueueHandle_t packet_queue_;
    QueueHandle_t event_queue_;

    static const size_t PACKET_QUEUE_SIZE = 250;
    static const size_t EVENT_QUEUE_SIZE = 10;
    static const size_t MAX_MESSAGE_SIZE = 2048;  // 2KB

    static const char* TAG;
};

} // namespace Core