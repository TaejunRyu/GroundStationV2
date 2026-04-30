#include "i2c_driver.h"

#include "esp_log.h"
#include "esp_check.h"

static const char* TAG = "I2C_DRV";

namespace drone {
namespace hal {

I2cDriver::I2cDriver() : _initialized(false) {
    _mutex = xSemaphoreCreateMutex();
}

I2cDriver::~I2cDriver() {
    deinit();
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

esp_err_t I2cDriver::init(const I2cConfig& config) {
    if (_initialized) {
        return ESP_OK;
    }

    _config = config;

    // I2C 컨트롤러 설정
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = -1, // 버스에서 설정
        .scl_io_num = -1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = config.clock_hz,
        },
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(config.port, &conf), TAG, "I2C 파라미터 설정 실패");
    ESP_RETURN_ON_ERROR(i2c_driver_install(config.port, I2C_MODE_MASTER, 0, 0, 0), TAG, "I2C 드라이버 설치 실패");

    _initialized = true;
    ESP_LOGI(TAG, "I2C 초기화 완료 (포트:%d, 클럭:%dHz)", config.port, config.clock_hz);

    return ESP_OK;
}

esp_err_t I2cDriver::deinit() {
    if (_initialized) {
        i2c_driver_delete(_config.port);
        _initialized = false;
    }
    return ESP_OK;
}

esp_err_t I2cDriver::write(uint8_t addr, const uint8_t* data, size_t len) {
    if (!_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_RETURN_ON_ERROR(i2c_master_start(cmd), TAG, "I2C start 실패");
    ESP_RETURN_ON_ERROR(i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true), TAG, "주소 쓰기 실패");
    ESP_RETURN_ON_ERROR(i2c_master_write(cmd, data, len, true), TAG, "데이터 쓰기 실패");
    ESP_RETURN_ON_ERROR(i2c_master_stop(cmd), TAG, "I2C stop 실패");

    esp_err_t ret = i2c_master_cmd_begin(_config.port, cmd, pdMS_TO_TICKS(_config.timeout_ms));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C 쓰기 실패: 0x%02X (에러: %d)", addr, ret);
    }
    return ret;
}

esp_err_t I2cDriver::read(uint8_t addr, uint8_t* data, size_t len) {
    if (!_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_RETURN_ON_ERROR(i2c_master_start(cmd), TAG, "I2C start 실패");
    ESP_RETURN_ON_ERROR(i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true), TAG, "주소 읽기 실패");
    ESP_RETURN_ON_ERROR(i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK), TAG, "데이터 읽기 실패");
    ESP_RETURN_ON_ERROR(i2c_master_stop(cmd), TAG, "I2C stop 실패");

    esp_err_t ret = i2c_master_cmd_begin(_config.port, cmd, pdMS_TO_TICKS(_config.timeout_ms));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C 읽기 실패: 0x%02X (에러: %d)", addr, ret);
    }
    return ret;
}

esp_err_t I2cDriver::writeRegister(uint8_t dev_addr, uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    return write(dev_addr, buf, 2);
}

uint8_t I2cDriver::readRegister(uint8_t dev_addr, uint8_t reg) {
    uint8_t data = 0;
    write(dev_addr, &reg, 1);
    read(dev_addr, &data, 1);
    return data;
}

esp_err_t I2cDriver::writeRegister16(uint8_t dev_addr, uint8_t reg, uint16_t data) {
    uint8_t buf[3] = {reg, (uint8_t)(data >> 8), (uint8_t)(data & 0xFF)};
    return write(dev_addr, buf, 3);
}

esp_err_t I2cDriver::scan(uint8_t* found_addrs, size_t* count, size_t max_count) {
    *count = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(_config.port, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK && *count < max_count) {
            found_addrs[*count] = addr;
            (*count)++;
            ESP_LOGI(TAG, "I2C 디바이스 발견: 0x%02X", addr << 1);
        }
    }
    
    return ESP_OK;
}

// I2cBus 구현
I2cBus::I2cBus() {
    for (int i = 0; i < I2C_NUM_MAX; i++) {
        _buses[i].initialized = false;
    }
}

I2cBus& I2cBus::getInstance(i2c_port_t port) {
    static I2cBus instance;
    return instance;
}

esp_err_t I2cBus::init(i2c_port_t port, int sda, int scl, uint32_t clock_hz) {
    if (port >= I2C_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (_buses[port].initialized) {
        return ESP_OK;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = clock_hz,
        },
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(port, &conf), TAG, "I2C 파라미터 설정 실패");
    ESP_RETURN_ON_ERROR(i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0), TAG, "I2C 드라이버 설치 실패");

    _buses[port].initialized = true;
    ESP_LOGI(TAG, "I2C 버스 초기화 완료 (포트:%d, SDA:%d, SCL:%d)", port, sda, scl);

    return ESP_OK;
}

esp_err_t I2cBus::free(i2c_port_t port) {
    if (port < I2C_NUM_MAX && _buses[port].initialized) {
        i2c_driver_delete(port);
        _buses[port].initialized = false;
    }
    return ESP_OK;
}

bool I2cBus::isInitialized(i2c_port_t port) const {
    return port < I2C_NUM_MAX && _buses[port].initialized;
}

I2cDriver* I2cBus::getDriver(i2c_port_t port) {
    if (port < I2C_NUM_MAX && _buses[port].initialized) {
        return &_buses[port].driver;
    }
    return nullptr;
}

} // namespace hal
} // namespace drone