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
                public: 
                    bool begin(
                        int8_t mosi,
                        int8_t miso, 
                        int8_t sck, 
                        int8_t cs, 
                        int8_t hs_pin);

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