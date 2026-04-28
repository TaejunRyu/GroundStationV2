#include "serial_jtag_driver.h"

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_select.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <cstring>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "queue_manager.h"
#include "bridge_core.h"
#include "led_strip_driver.h"
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
// Global select callback for static callback function
//static usj_select_notif_callback_t g_select_callback = nullptr;
Core::QueueManager* SerialJtagDriver::queue_mgr_ = nullptr;
//SerialJtagDriver* SerialJtagDriver::serial_jtag_driver_ = nullptr;
SemaphoreHandle_t SerialJtagDriver::rx_sem_ = nullptr;
/**
 * @brief Construct a new Serial Jtag Driver:: Serial Jtag Driver object    
 * 
 */
SerialJtagDriver::SerialJtagDriver()
    :buffer_size_(2048), initialized_(false), running_(false),connected_(false), last_rx_timestamp_(0) {
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

    //callback 함수에서 자신의 멤버에 접근할 수 있도록 static 멤버에 자기 자신 포인터 저장
    //serial_jtag_driver_ = this; // static 멤버에 자기 자신 포인터 저장

    usb_serial_jtag_driver_config_t cfg = {};
    cfg.rx_buffer_size = buffer_size_;
    cfg.tx_buffer_size = buffer_size_;

    auto ret = usb_serial_jtag_driver_install(&cfg);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install Serial JTAG driver: %s", esp_err_to_name(ret));
        return ret;
    }

    if(rx_sem_ == nullptr) {
       rx_sem_ = xSemaphoreCreateBinary();
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
    if(rx_sem_) {
        vSemaphoreDelete(rx_sem_);
        rx_sem_ = nullptr;
    }
    usb_serial_jtag_driver_uninstall();
    //event_queue_ = nullptr;
    initialized_ = false;
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

    if(usb_serial_jtag_write_ready()){
        int written = usb_serial_jtag_write_bytes(data, len,0 /* pdMS_TO_TICKS(100)*/);
        if (written < 0) {
            ESP_LOGE(TAG, "Serial JTAG write error: %s", esp_err_to_name(ESP_FAIL));
            return ESP_FAIL;
        }

        if (written < (int)len) {
            ESP_LOGW(TAG, "Incomplete Serial JTAG transmission: %d / %u bytes", written, len);
            return ESP_ERR_TIMEOUT;
        }

        return ESP_OK;
    }else{
        ESP_LOGW(TAG, "Serial JTAG not ready for writing");
        return ESP_ERR_TIMEOUT;
    }
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
 * @brief Serial JTAG 드라이버 시작 (callback模式下에서는 태스크 생성 불필요)
 */
esp_err_t SerialJtagDriver::start(void){
    if (!initialized_ || running_) {
        ESP_LOGE(TAG, "Serial JTAG not initialized or already running");
        return ESP_OK;
    }
  


    running_ = true;
    ESP_LOGI(TAG, "Serial JTAG started in callback mode");
    return ESP_OK;        
}


void SerialJtagDriver::stop() {
    if (running_) {
        running_ = false;
        ESP_LOGI(TAG, "Serial JTAG stopped");
    }
}

void SerialJtagDriver::register_select_callback(void)
{
    // Register select notification callback
    usb_serial_jtag_set_select_notif_callback(select_notif_callback);
}

void SerialJtagDriver::unregister_select_callback(void)
{
    // Unregister select notification callback by setting it to nullptr
    usb_serial_jtag_set_select_notif_callback(nullptr); 
}

/**
 * @brief Select notification callback (정적 멤버 함수)
 *        USB Serial JTAG의 select() 이벤트에 대한 콜백
 *        callback模式下에서 데이터 수신 시 직접 데이터를 읽어 BridgeCore로 전달
 *
 * @param event 이벤트 타입 (USJ_SELECT_READ_NOTIF 또는 USJ_SELECT_WRITE_NOTIF)
 * @param task_woken FreeRTOS 태스크 각石 여부
 */
void SerialJtagDriver::select_notif_callback(usj_select_notif_t event, int* task_woken) {
    if (rx_sem_) {
        BaseType_t high_priority_task_woken = pdFALSE;
        xSemaphoreGiveFromISR(rx_sem_, &high_priority_task_woken);
        if (high_priority_task_woken) portYIELD_FROM_ISR();
    }
}

void SerialJtagDriver::rx_task(void* pvParameters) {
    Core::BridgeCore& bridge = Core::BridgeCore::get_instance();
    SerialJtagDriver* self = static_cast<SerialJtagDriver*>(pvParameters);
    uint8_t buffer[256];

    while (true) {
        // 콜백이 신호를 줄 때까지 무한 대기
        if (xSemaphoreTake(rx_sem_, portMAX_DELAY) == pdTRUE) {
            int len;
            self->update_last_rx_timestamp();
            if (!self->is_connected()) {
                self->set_connected(true);
                bridge.get_led_strip_driver().set_status_ok();
                ESP_LOGI(TAG, "Serial JTAG: [CONNECTED]");
            }
            // 읽을 데이터가 없을 때까지 계속 읽음 (버퍼 비우기)
            while ((len = usb_serial_jtag_read_bytes(buffer, sizeof(buffer), 0)) > 0) {
                self->queue_mgr_->enqueue_packet(buffer, len, Types::DataSource::UART_SERIAL);    
            }
        }
    }
}


} // namespace Drivers