#include "SPISLAVE.h"
SPISLAVE_BEGIN

SPISlave::SPISlave(){
    bus_cfg = {};
    max_size = SPI_MAX_DMA_LEN;
}

/**
 * @brief SPISlaveを利用するときに最初に実行する関数
 *  ESP32無印のみ対応の関数としている
 */
#if !(IS_S3)
bool SPISlave::begin(uint8_t spi_bus, int8_t mosi, int8_t miso, int8_t sck int8_t cs, int8_t hs_pin){
    spi_host_device_t host_in;
    host_in = (spi_bus == HSPI) ? HSPI_HOST : VSPI_HOST
    begin(host_in,mosi,miso,sck,cs,hs_pin);
}
#endif

bool SPISlave::begin(spi_host_device_t host_in, int8_t mosi, int8_t miso, int8_t sck, int8_t cs, int8_t hs_pin){
    if((mosi ==  -1) && (miso == -1) && (sck == -1)){
#if (IS_S3)
        mosi = 12;
        miso = 13;
        sck = 11;
#else 
        sck = (host_in == VSPI_HOST) ? VSPI_IOMUX_PIN_NUM_CLK : HSPI_IOMUX_PIN_NUM_CLK;
        miso = (host_in == VSPI_HOST) ? VSPI_IOMUX_PIN_NUM_MISO : HSPI_IOMUX_PIN_NUM_MISO;
        mosi = (host_in == VSPI_HOST) ? VSPI_IOMUX_PIN_NUM_MOSI : HSPI_IOMUX_PIN_NUM_MOSI;
#endif
    } 
    bus_cfg.mosi_io_num = mosi;
    bus_cfg.miso_io_num = miso;
    bus_cfg.sclk_io_num = sck;
    bus_cfg.data2_io_num = -1;
    bus_cfg.data3_io_num = -1;
    bus_cfg.data4_io_num = -1;
    bus_cfg.data5_io_num = -1;
    bus_cfg.data6_io_num = -1;
    bus_cfg.data7_io_num = -1;
    
}
SPISLAVE_END