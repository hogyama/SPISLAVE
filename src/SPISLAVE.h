#pragma once

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_slave.h>
#include <esp32-hal-spi.h>

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define IS_S3 1
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define IS_S3 0
#else
#error "No supported board specified!!!"
#endif

#define DEFAULT_QUEUE_SIZE 1
#define DEFAULT_BUFFER_SIZE 64

class SPISlave
{
public:
    struct Stats {
        uint32_t queued{0};
        uint32_t completed{0};
        uint32_t timeout{0};
        uint32_t queue_error{0};
        uint32_t size_error{0};
        uint32_t rx_overflow{0};
        esp_err_t last_error{ESP_OK};
        uint32_t last_rx_size{0};
    };

private:
    spi_host_device_t host{SPI2_HOST};
    int8_t hs_pin{-1};
    uint32_t buffer_size{DEFAULT_BUFFER_SIZE};
    uint8_t spi_mode{0};

    uint8_t* rx_buf{nullptr};
    uint8_t* tx_buf{nullptr};
    spi_slave_transaction_t slave_trans{};

    bool is_initialized{false};
    bool transaction_queued{false};
    Stats stats{};

public:
    SPISlave();
    ~SPISlave();

#if !(IS_S3)
    esp_err_t begin(
        uint8_t spi_bus = HSPI,
        int8_t mosi = -1,
        int8_t miso = -1,
        int8_t sck = -1,
        int8_t cs = -1,
        int8_t hs_pin = -1,
        int32_t size = DEFAULT_BUFFER_SIZE,
        uint8_t mode = 0);
#else
    esp_err_t begin(
        spi_host_device_t host_in = SPI2_HOST,
        int8_t mosi = -1,
        int8_t miso = -1,
        int8_t sck = -1,
        int8_t cs = -1,
        int8_t hs_pin = -1,
        int32_t size = DEFAULT_BUFFER_SIZE,
        uint8_t mode = 0);
#endif

    // Queue one SPI slave transaction. The master must clock exactly size bytes.
    esp_err_t queue(const uint8_t* tx, uint32_t size, uint32_t timeout = 0);

    // Wait for queued transaction completion. Returns received byte count, or -1 on error/timeout.
    int32_t wait(uint32_t timeout = portMAX_DELAY);

    // Safe receive copy. Copies at most rx_capacity bytes.
    int32_t wait(uint8_t* rx, uint32_t rx_capacity, uint32_t timeout = portMAX_DELAY);

    // Backward-compatible version. The caller must ensure rx buffer >= queued size.
    int32_t wait(uint8_t* rx, uint32_t timeout = portMAX_DELAY);

    // Convenience: queue then wait once.
    int32_t transfer(const uint8_t* tx,
                     uint32_t tx_size,
                     uint8_t* rx,
                     uint32_t rx_capacity,
                     uint32_t timeout = portMAX_DELAY);

    const uint8_t* get_raw_rx() const;
    const uint8_t* get_raw_tx() const;

    bool initialized() const;
    bool queued() const;
    uint32_t bufferSize() const;
    uint32_t lastRxSize() const;
    esp_err_t lastError() const;
    Stats getStats() const;
    void resetStats();

    void end();

    static void IRAM_ATTR hssetup(spi_slave_transaction_t *trans);
    static void IRAM_ATTR hsfree(spi_slave_transaction_t *trans);
};
