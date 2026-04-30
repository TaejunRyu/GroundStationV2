#include "spi_driver.h"

#include "esp_log.h"
#include "esp_check.h"

static const char* TAG = "SPI_DRV";

namespace drone {
namespace hal {

SpiDriver::SpiDriver() : _handle(nullptr), _initialized(false) {
    _mutex = xSemaphoreCreateMutex();
}

SpiDriver::~SpiDriver() {
    deinit();
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

esp_err_t SpiDriver::init(const SpiConfig& config) {
    if (_initialized) {
        return ESP_OK;
    }

    _config = config;

    // 버스 초기화
    spi_bus_config_t buscfg = {
        .mosi_io_num = config.mosi,
        .miso_io_num = config.miso,
        .sclk_io_num = config.clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ESP_RETURN_ON_ERROR(spi_bus_initialize(config.host, &buscfg, SPI_DMA_CH_AUTO), TAG, "버스 초기화 실패");

    // 디바이스 추가
    ESP_RETURN_ON_ERROR(addDevice(), TAG, "디바이스 추가 실패");

    _initialized = true;
    ESP_LOGI(TAG, "SPI 초기화 완료 (MISO:%d, MOSI:%d, CLK:%d, CS:%d)", 
             config.mosi, config.miso, config.clk, config.cs);

    return ESP_OK;
}

esp_err_t SpiDriver::addDevice() {
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 8,
        .dummy_bits = 0,
        .mode = _config.mode,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = _config.clock_hz,
        .input_delay_ns = 0,
        .spics_io_num = _config.cs,
        .flags = 0,
        .queue_size = _config.queue_size,
    };

    return spi_bus_add_device(_config.host, &devcfg, &_handle);
}

esp_err_t SpiDriver::deinit() {
    if (_handle) {
        spi_bus_remove_device(_handle);
        _handle = nullptr;
    }
    if (_initialized) {
        spi_bus_free(_config.host);
        _initialized = false;
    }
    return ESP_OK;
}

esp_err_t SpiDriver::transmit(const SpiTransaction& trans) {
    if (!_initialized || !_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    spi_transaction_t t = {};
    t.addr = trans.addr;
    t.length = trans.length;

    if (trans.tx_data) {
        t.tx_buffer = trans.tx_data;
        t.flags = SPI_TRANS_USE_TXDATA;
    }
    if (trans.rx_data) {
        t.rx_buffer = trans.rx_data;
        t.flags = SPI_TRANS_USE_RXDATA;
    }

    return spi_device_polling_transmit(_handle, &t);
}

esp_err_t SpiDriver::transmit(uint8_t addr, const uint8_t* tx_data, size_t tx_len,
                              uint8_t* rx_data, size_t rx_len) {
    if (!_initialized || !_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    spi_transaction_t t = {};
    t.addr = addr;
    t.length = rx_len > 0 ? rx_len * 8 : tx_len * 8;
    t.tx_buffer = tx_data;
    t.rx_buffer = rx_data;

    return spi_device_polling_transmit(_handle, &t);
}

esp_err_t SpiDriver::transmitAsync(const SpiTransaction& trans, TransmitCallback callback) {
    // 비동기 전송 구현 (나중에 확장)
    return transmit(trans);
}

esp_err_t SpiDriver::writeRegister(uint8_t addr, uint8_t data) {
    spi_transaction_t t = {};
    t.addr = addr & 0x7F; // Write: MSB = 0
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = data;

    return spi_device_polling_transmit(_handle, &t);
}

uint8_t SpiDriver::readRegister(uint8_t addr) {
    spi_transaction_t t = {};
    t.addr = addr | 0x80; // Read: MSB = 1
    t.length = 8;
    t.flags = SPI_TRANS_USE_RXDATA;

    spi_device_polling_transmit(_handle, &t);
    return t.rx_data[0];
}

// SpiBus 구현
SpiBus& SpiBus::getInstance() {
    static SpiBus instance;
    return instance;
}

esp_err_t SpiBus::init(spi_host_device_t host, int mosi, int miso, int clk, int wp, int hd) {
    if (_initialized) {
        return ESP_OK;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = clk,
        .quadwp_io_num = wp,
        .quadhd_io_num = hd,
        .max_transfer_sz = 4096,
    };

    ESP_RETURN_ON_ERROR(spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI 버스 초기화 실패");

    _host = host;
    _initialized = true;
    ESP_LOGI(TAG, "SPI 버스 초기화 완료");

    return ESP_OK;
}

esp_err_t SpiBus::free() {
    if (_initialized) {
        spi_bus_free(_host);
        _initialized = false;
    }
    return ESP_OK;
}

} // namespace hal
} // namespace drone