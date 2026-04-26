#include "serial_jtag_driver.h"

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_select.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <cstring>
#include "bridge_core.h"

/**
 * @brief 
 *    1. 가상통신방식으로 통신속도에 구애를 받지 않고 안정적인 통신이 가능하도록 설계
 *    2. UART 드라이버를 기반으로 하여, JTAG 인터페이스를 시뮬레이트하는 방식으로 구현
 *    3. UART 드라이버의 이벤트 큐를 활용하여, 수신된 데이터를 비동기적으로 처리할 수 있도록 설계
 *    4. 핀은 고정됨. esp32s3의 serial_jtag 포트는 19번과 20번 핀으로 고정되어 있음. (TX: GPIO19, RX: GPIO20)
 *    5. 저수준 함수 사용하여, JTAG 프로토콜에 맞게 데이터를 포맷팅하여 송수신할 수 있도록 구현
 *    6. 함수의 예로서 usb_serial_jtag_driver_install()와 usb_serial_jtag_driver_write() 등이 있음
 *    7. JTAG 프로토콜에 맞게 데이터를 포맷팅하여 송수신할 수 있도록 구현    
*/
namespace Drivers {

const char* SerialJtagDriver::TAG = "SERIAL_JTAG";
/**
 * @brief Construct a new Serial Jtag Driver:: Serial Jtag Driver object    
 * 
 */
SerialJtagDriver::SerialJtagDriver()
    :buffer_size_(1024), initialized_(false), event_queue_(nullptr), serial_jtag_rx_task_handle_(nullptr) {
    ESP_LOGI(TAG, "SerialJtagDriver created for UART Serial JTAG");
}

/**
 * @brief Destroy the Serial Jtag Driver:: Serial Jtag Driver object
 * 
 */
SerialJtagDriver::~SerialJtagDriver() {
    deinitialize();
}

/**
 * @brief Serial JTAG 드라이버 초기화
 * 
 * @return esp_err_t 
 */
esp_err_t SerialJtagDriver::initialize() {
    if (initialized_) return ESP_OK;

    usb_serial_jtag_driver_config_t cfg = {};
    cfg.rx_buffer_size = buffer_size_;
    cfg.tx_buffer_size = buffer_size_;

    auto ret = usb_serial_jtag_driver_install(&cfg);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install Serial JTAG driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Serial JTAG initialized successfully");
     return ESP_OK;
}

/**
 * @brief Serial JTAG 드라이버 비활성화
 * 
 */
void SerialJtagDriver::deinitialize() {
    if (!initialized_) return;

    usb_serial_jtag_driver_uninstall();
    event_queue_ = nullptr;
    initialized_ = false;
    serial_jtag_rx_task_handle_ = nullptr;
    ESP_LOGI(TAG, "Serial JTAG deinitialized");
}

/**
 * @brief Serial JTAG 데이터 전송
 * 
 * @param data 전송할 데이터
 * @param len 전송할 데이터 길이
 * @return esp_err_t 
 */
esp_err_t SerialJtagDriver::send_data(const uint8_t* data, size_t len) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Serial JTAG not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    int written = usb_serial_jtag_write_bytes(data, len, pdMS_TO_TICKS(100));
    if (written < 0) {
        ESP_LOGE(TAG, "Serial JTAG write error: %s", esp_err_to_name(ESP_FAIL));
        return ESP_FAIL;
    }

    if (written < (int)len) {
        ESP_LOGW(TAG, "Incomplete Serial JTAG transmission: %d / %u bytes", written, len);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

/**
 * @brief Serial JTAG 데이터 수신
 * 
 * @param buffer 수신할 버퍼
 * @param max_len 최대 수신 길이
 * @param actual_len 실제 수신 길이
 * @param timeout_ms 타임아웃 시간
 * @return esp_err_t 
 */
esp_err_t SerialJtagDriver::receive_data(uint8_t* buffer, size_t max_len, size_t& actual_len, uint32_t timeout_ms) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Serial JTAG not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!buffer || max_len == 0) return ESP_ERR_INVALID_ARG;

    int read_len = usb_serial_jtag_read_bytes(buffer, max_len, pdMS_TO_TICKS(timeout_ms));

    if (read_len > 0) {
        actual_len = read_len;
        ESP_LOGD(TAG, "Received %d bytes from Serial JTAG", read_len);
        return ESP_OK;
    } else if (read_len == 0) {
        actual_len = 0;
        return ESP_ERR_TIMEOUT;
    } else {
        return ESP_FAIL;
    }
}

/**
 * @brief Serial JTAG 드라이버 시작 (수신 태스크 생성)
 */
esp_err_t SerialJtagDriver::start(void){
    if (!initialized_ || running_) {
        ESP_LOGE(TAG, "Serial JTAG not initialized or already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting USB Serial/JTAG RX Task...");
    // FreeRTOS 태스크 생성
    // static으로 선언된 uart_rx_task_static을 실행합니다.
    BaseType_t res = xTaskCreate(
        serial_jtag_rx_task_static, // 실행할 함수
        "serial_jtag_rx_task_static",                    // 태스크 이름
        4096,                                  // 스택 크기 (필요에 따라 조절)
        this,                                  // 전달 인자 (this 포인터)
        10,                                     // 우선 순위
        &serial_jtag_rx_task_handle_                       // 태스크 핸들 (멤버 변수로 관리 권장)
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        return ESP_FAIL;
    }

    running_ = true;
    return ESP_OK;        
}

void SerialJtagDriver::stop() {
    if (running_ && serial_jtag_rx_task_handle_) {
        ESP_LOGI(TAG, "Stopping USB Serial/JTAG RX Task...");
        vTaskDelete(serial_jtag_rx_task_handle_);
        serial_jtag_rx_task_handle_ = nullptr;
        running_ = false;
    }
}


/**
 * @brief 
 *      1. Serial JTAG 드라이버 수신 태스크 (정적 멤버 함수)
 *      2. FreeRTOS 태스크로 실행되며, this 포인터를 전달받아 인스턴스 멤버에 접근
 *      3. 무한 루프에서 데이터를 수신하며, 수신된 데이터를 BridgeCore의 on_data_received() 메서드로 전달
 *      4. 수신된 데이터는 event_queue_를 통해 비동기적으로 처리할 수도 있도록 설계 (현재는 직접 on_data_received 호출)
 *      
 * @param arg this 포인터가 전달됨
 */
void SerialJtagDriver::serial_jtag_rx_task_static(void* arg) {
    SerialJtagDriver* driver = static_cast<SerialJtagDriver*>(arg);
    uint8_t* buffer = new uint8_t[driver->buffer_size_];

    Core::BridgeCore& bridge = Core::BridgeCore::get_instance();

    while (true) {
        size_t actual_len = 0;
        esp_err_t ret = driver->receive_data(buffer, driver->buffer_size_, actual_len, portMAX_DELAY);
        if (ret == ESP_OK && actual_len > 0) {
            //ESP_LOGI(TAG, "Received %d bytes in RX task", actual_len);
            
            bridge.on_data_received(buffer, actual_len, Types::DataSource::UART_SERIAL);
            
            // 수신된 데이터를 이벤트 큐에 전달
            //xQueueSend(driver->event_queue_, &actual_len, portMAX_DELAY);
        } else if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error receiving data in RX task: %s", esp_err_to_name(ret));
        }
    }
    delete[] buffer;
}


} // namespace Drivers