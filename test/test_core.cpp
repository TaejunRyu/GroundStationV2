#include <unity.h>
#include <esp_heap_caps.h>
#include "queue_manager.h"
#include "wifi_driver.h"

// 테스트 헬퍼 함수들
static void test_psram_allocation() {
    // PSRAM 메모리 할당 테스트
    size_t initial_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    uint8_t* buffer = (uint8_t*)heap_caps_malloc(2048, MALLOC_CAP_SPIRAM);
    TEST_ASSERT_NOT_NULL(buffer);

    size_t after_alloc = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    TEST_ASSERT_TRUE(initial_free > after_alloc);

    heap_caps_free(buffer);

    size_t after_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    TEST_ASSERT_EQUAL(initial_free, after_free);
}

static void test_queue_manager() {
    Core::QueueManager qm;

    esp_err_t ret = qm.initialize();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // 패킷 큐잉 테스트
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    ret = qm.enqueue_packet(test_data, sizeof(test_data), Types::DataSource::UART_SERIAL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL(1, qm.get_packet_queue_size());

    Types::QueueMessage* message;
    ret = qm.dequeue_packet(&message, pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL(sizeof(test_data), message->length);
    TEST_ASSERT_EQUAL_MEMORY(test_data, message->data, sizeof(test_data));

    // 메모리 해제
    free(message);

    qm.deinitialize();
}

static void test_event_queue() {
    Core::QueueManager qm;

    esp_err_t ret = qm.initialize();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // 이벤트 큐잉 테스트
    Types::EventData event = {
        .event_type = Types::BridgeEvent::WIFI_CONNECTED,
        .data = {}
    };

    ret = qm.enqueue_event(event);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL(1, qm.get_event_queue_size());

    Types::EventData dequeued_event;
    ret = qm.dequeue_event(&dequeued_event, pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(Types::BridgeEvent::WIFI_CONNECTED, dequeued_event.event_type);

    qm.deinitialize();
}

// Unity 테스트 러너
extern "C" void app_main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_psram_allocation);
    RUN_TEST(test_queue_manager);
    RUN_TEST(test_event_queue);

    UNITY_END();
}