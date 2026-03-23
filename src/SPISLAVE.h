#ifndef SPISLAVE_H
#define SPISLAVE_H
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_slave.h>
#include <esp32-hal-spi.h>

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define IS_S3 1
#elif CONFIG_IDF_TARGET_ESP32
#define IS_S3 0
#else
#error "No supported board specified!!!"
#endif

#define DEFALUT_QUEUE_SIZE 3
#define DEFALUT_BUFFER_SIZE 1024

namespace arduino
{
    namespace esp32
    {
        namespace spi
        {
            class SPISlave
            {   
                private:
                    spi_host_device_t host{SPI2_HOST};
                    uint8_t hs_pin{-1}; 
                    uint32_t buffer_size{DEFALUT_BUFFER_SIZE}; 
                    uint8_t* rx_buf{nullptr}; 
                    uint8_t* tx_buf{nullptr}; 
                    uint8_t* user_rx{nullptr}; // userが用意したrxバッファを保持 
                    bool is_initialized{false};

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
                        int32_t buffer_size = -1);
#else
                    esp_err_t begin(
                        spi_host_device_t host_in = SPI2_HOST,
                        int8_t mosi = -1,
                        int8_t miso = -1, 
                        int8_t sck = -1, 
                        int8_t cs = -1, 
                        int8_t hs_pin = -1,
                        int32_t size = -1);
#endif
                    esp_err_t queue(
                        const uint8_t* tx,
                        uint8_t* rx,
                        uint32_t size = 0);
                    
                    int32_t wait();

                    void end();

                    static void IRAM_ATTR hssetup(spi_slave_transaction_t *trans);
                    static void IRAM_ATTR hsfree(spi_slave_transaction_t *trans); 
            };
        }
    }
}

#ifndef SPISLAVE_BEGIN
#define SPISLAVE_BEGIN \
    namespace arduino  \
    { \
        namespace esp32  \
        { \
            namespace spi   \
            {
#endif
#ifndef SPISLAVE_END
#define SPISLAVE_END \
            } \
        } \
    }
#endif
namespace SPISLAVE = arduino::esp32::spi;

#endif