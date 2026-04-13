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

#define DEFAULT_QUEUE_SIZE 1
#define DEFAULT_BUFFER_SIZE 1024

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
                    int8_t hs_pin{-1}; 
                    uint32_t buffer_size{DEFAULT_BUFFER_SIZE}; 
                    uint8_t* rx_buf{nullptr}; 
                    uint8_t* tx_buf{nullptr}; 
                    spi_slave_transaction_t slave_trans{};
                    bool is_initialized{false};

                public: 
                    SPISlave();
                    ~SPISlave();
#if !(IS_S3)
                    // SPI通信の初期化を行う関数　ESP32無印向け
                    esp_err_t begin(
                        uint8_t spi_bus = HSPI,
                        int8_t mosi = -1, 
                        int8_t miso = -1, 
                        int8_t sck = -1, 
                        int8_t cs = -1, 
                        int8_t hs_pin = -1,
                        int32_t size = -1);
#else  
                    
                    // SPI通信の初期化を行う関数　ESP32-S3向け
                    esp_err_t begin(
                        spi_host_device_t host_in = SPI2_HOST,
                        int8_t mosi = -1,
                        int8_t miso = -1, 
                        int8_t sck = -1, 
                        int8_t cs = -1, 
                        int8_t hs_pin = -1,
                        int32_t size = -1);
#endif              
                    // 送信データtxとサイズsizeをキューに追加する関数
                    esp_err_t queue(
                        const uint8_t* tx,
                        uint32_t size);
                    
                    // 受信データをrxにコピーして受け取る関数
                    int32_t wait(uint8_t* rx, uint32_t timeout = portMAX_DELAY);
                    
                    // 受信データを待つ関数
                    int32_t wait(uint32_t timeout = portMAX_DELAY);
                    
                    // 受信データのポインタを取得する関数
                    const uint8_t* get_raw_rx() const;
                    
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