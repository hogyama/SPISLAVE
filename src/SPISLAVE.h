#ifndef SPISLAVE_H
#define SPISLAVE_H
#include <Arduino.h>
#include <driver/spi_slave.h>
#include <esp32-hal-spi.h>

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define IS_S3 1
#elif CONFIG_IDF_TARGET_ESP32
#define IS_S3 0
#else
#error "No supported board specified!!!"
#endif


namespace arduino
{
    namespace esp32
    {
        namespace spi
        {
            class SPISlave
            {   
                private:
                    spi_bus_config_t bus_cfg;
                    int max_size;
                public: 
                    // コンストラクタ
                    SPISlave();
#if !(IS_S3)
                    bool begin(
                        uint8_t spi_bus = HSPI,
                        int8_t mosi = -1, 
                        int8_t miso = -1, 
                        int8_t sck = -1, 
                        int8_t cs = -1, 
                        int8_t hs_pin = -1);
#else
                    bool begin(
                        spi_host_device_t host_in = SPI2_HOST,
                        int8_t mosi = -1,
                        int8_t miso = -1, 
                        int8_t sck = -1, 
                        int8_t cs = -1, 
                        int8_t hs_pin = -1);
#endif
                    bool queue(
                        const uint8_t* tx,
                        uint8_t* rx,
                        size_t size);
                    
                    uint32_t wait();

                    bool end();
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