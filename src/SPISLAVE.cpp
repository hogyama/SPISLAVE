#include "SPISLAVE.h"

SPISlave::SPISlave() {}

SPISlave::~SPISlave()
{
    end();
}

#if !(IS_S3)
esp_err_t SPISlave::begin(uint8_t spi_bus,
                          int8_t mosi,
                          int8_t miso,
                          int8_t sck,
                          int8_t cs,
                          int8_t hs,
                          int32_t size,
                          uint8_t mode)
{
    spi_host_device_t host_in = (spi_bus == HSPI) ? HSPI_HOST : VSPI_HOST;
    return begin(host_in, mosi, miso, sck, cs, hs, size, mode);
}
#endif

esp_err_t SPISlave::begin(spi_host_device_t host_in,
                          int8_t mosi,
                          int8_t miso,
                          int8_t sck,
                          int8_t cs,
                          int8_t hs,
                          int32_t size,
                          uint8_t mode)
{
    end();
    stats = Stats{};

    if (cs < 0) {
        stats.last_error = ESP_ERR_INVALID_ARG;
        return ESP_ERR_INVALID_ARG;
    }

    if ((mosi == -1) && (miso == -1) && (sck == -1)) {
#if (IS_S3)
        mosi = 11;
        miso = 13;
        sck  = 12;
#else
        sck  = (host_in == VSPI_HOST) ? VSPI_IOMUX_PIN_NUM_CLK  : HSPI_IOMUX_PIN_NUM_CLK;
        miso = (host_in == VSPI_HOST) ? VSPI_IOMUX_PIN_NUM_MISO : HSPI_IOMUX_PIN_NUM_MISO;
        mosi = (host_in == VSPI_HOST) ? VSPI_IOMUX_PIN_NUM_MOSI : HSPI_IOMUX_PIN_NUM_MOSI;
#endif
    }

    if (mosi < 0 || miso < 0 || sck < 0) {
        stats.last_error = ESP_ERR_INVALID_ARG;
        return ESP_ERR_INVALID_ARG;
    }

    if (size <= 0) {
        size = DEFAULT_BUFFER_SIZE;
    }

    host = host_in;
    hs_pin = hs;
    buffer_size = static_cast<uint32_t>(size);
    spi_mode = mode & 0x03;

    if (hs_pin >= 0) {
        gpio_reset_pin((gpio_num_t)hs_pin);
        gpio_set_direction((gpio_num_t)hs_pin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)hs_pin, 0);
    }

    rx_buf = static_cast<uint8_t*>(heap_caps_malloc(buffer_size, MALLOC_CAP_DMA));
    tx_buf = static_cast<uint8_t*>(heap_caps_malloc(buffer_size, MALLOC_CAP_DMA));
    if (rx_buf == nullptr || tx_buf == nullptr) {
        end();
        stats.last_error = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }

    memset(rx_buf, 0, buffer_size);
    memset(tx_buf, 0, buffer_size);

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = mosi;
    bus_cfg.miso_io_num = miso;
    bus_cfg.sclk_io_num = sck;
    bus_cfg.data2_io_num = -1;
    bus_cfg.data3_io_num = -1;
    bus_cfg.data4_io_num = -1;
    bus_cfg.data5_io_num = -1;
    bus_cfg.data6_io_num = -1;
    bus_cfg.data7_io_num = -1;
    bus_cfg.max_transfer_sz = static_cast<int>(buffer_size);

    spi_slave_interface_config_t slave_cfg = {};
    slave_cfg.spics_io_num = cs;
    slave_cfg.queue_size = DEFAULT_QUEUE_SIZE;
    slave_cfg.mode = spi_mode;
    slave_cfg.post_setup_cb = hssetup;
    slave_cfg.post_trans_cb = hsfree;

    esp_err_t ret = spi_slave_initialize(host, &bus_cfg, &slave_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        end();
        stats.last_error = ret;
        return ret;
    }

    is_initialized = true;
    transaction_queued = false;
    stats.last_error = ESP_OK;
    return ESP_OK;
}

esp_err_t SPISlave::queue(const uint8_t* tx, uint32_t size, uint32_t timeout)
{
    if (!is_initialized) {
        stats.queue_error++;
        stats.last_error = ESP_ERR_INVALID_STATE;
        return ESP_ERR_INVALID_STATE;
    }

    if (transaction_queued) {
        stats.queue_error++;
        stats.last_error = ESP_ERR_INVALID_STATE;
        return ESP_ERR_INVALID_STATE;
    }

    if (size == 0 || size > buffer_size) {
        stats.size_error++;
        stats.last_error = ESP_ERR_INVALID_SIZE;
        return ESP_ERR_INVALID_SIZE;
    }

    memset(tx_buf, 0, buffer_size);
    memset(rx_buf, 0, buffer_size);
    if (tx != nullptr) {
        memcpy(tx_buf, tx, size);
    }

    memset(&slave_trans, 0, sizeof(spi_slave_transaction_t));
    slave_trans.length = size * 8;
    slave_trans.tx_buffer = tx_buf;
    slave_trans.rx_buffer = rx_buf;
    slave_trans.user = static_cast<void*>(this);

    TickType_t timeout_ticks = (timeout == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    esp_err_t ret = spi_slave_queue_trans(host, &slave_trans, timeout_ticks);
    if (ret == ESP_OK) {
        transaction_queued = true;
        stats.queued++;
    } else {
        stats.queue_error++;
    }
    stats.last_error = ret;
    return ret;
}

int32_t SPISlave::wait(uint32_t timeout)
{
    if (!is_initialized || !transaction_queued) {
        stats.last_error = ESP_ERR_INVALID_STATE;
        return -1;
    }

    TickType_t timeout_ticks = (timeout == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    spi_slave_transaction_t* trans = nullptr;
    esp_err_t ret = spi_slave_get_trans_result(host, &trans, timeout_ticks);

    if (ret != ESP_OK) {
        if (ret == ESP_ERR_TIMEOUT) {
            stats.timeout++;
        }
        stats.last_error = ret;
        return -1;
    }

    transaction_queued = false;
    uint32_t bytes = trans->trans_len / 8;
    stats.last_rx_size = bytes;
    stats.completed++;
    stats.last_error = ESP_OK;
    return static_cast<int32_t>(bytes);
}

int32_t SPISlave::wait(uint8_t* rx, uint32_t rx_capacity, uint32_t timeout)
{
    int32_t size = wait(timeout);
    if (size <= 0) {
        return size;
    }

    if (rx != nullptr && rx_capacity > 0) {
        uint32_t copy_size = static_cast<uint32_t>(size);
        if (copy_size > rx_capacity) {
            copy_size = rx_capacity;
            stats.rx_overflow++;
            stats.last_error = ESP_ERR_INVALID_SIZE;
        }
        memcpy(rx, rx_buf, copy_size);
    }
    return size;
}

int32_t SPISlave::wait(uint8_t* rx, uint32_t timeout)
{
    return wait(rx, buffer_size, timeout);
}

int32_t SPISlave::transfer(const uint8_t* tx,
                           uint32_t tx_size,
                           uint8_t* rx,
                           uint32_t rx_capacity,
                           uint32_t timeout)
{
    esp_err_t ret = queue(tx, tx_size, 0);
    if (ret != ESP_OK) {
        return -1;
    }
    return wait(rx, rx_capacity, timeout);
}

const uint8_t* SPISlave::get_raw_rx() const
{
    return rx_buf;
}

const uint8_t* SPISlave::get_raw_tx() const
{
    return tx_buf;
}

bool SPISlave::initialized() const
{
    return is_initialized;
}

bool SPISlave::queued() const
{
    return transaction_queued;
}

uint32_t SPISlave::bufferSize() const
{
    return buffer_size;
}

uint32_t SPISlave::lastRxSize() const
{
    return stats.last_rx_size;
}

esp_err_t SPISlave::lastError() const
{
    return stats.last_error;
}

SPISlave::Stats SPISlave::getStats() const
{
    return stats;
}

void SPISlave::resetStats()
{
    esp_err_t err = stats.last_error;
    stats = Stats{};
    stats.last_error = err;
}

void SPISlave::end()
{
    if (is_initialized) {
        spi_slave_free(host);
        is_initialized = false;
        transaction_queued = false;
    }

    if (rx_buf != nullptr) {
        heap_caps_free(rx_buf);
        rx_buf = nullptr;
    }
    if (tx_buf != nullptr) {
        heap_caps_free(tx_buf);
        tx_buf = nullptr;
    }
}

void IRAM_ATTR SPISlave::hssetup(spi_slave_transaction_t *trans)
{
    SPISlave* me = static_cast<SPISlave*>(trans->user);
    if (me != nullptr && me->hs_pin >= 0) {
        gpio_set_level((gpio_num_t)me->hs_pin, 1);
    }
}

void IRAM_ATTR SPISlave::hsfree(spi_slave_transaction_t *trans)
{
    SPISlave* me = static_cast<SPISlave*>(trans->user);
    if (me != nullptr && me->hs_pin >= 0) {
        gpio_set_level((gpio_num_t)me->hs_pin, 0);
    }
}
