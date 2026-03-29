#include "SPISLAVE.h"
SPISLAVE_BEGIN

SPISlave::SPISlave(){};

SPISlave::~SPISlave(){
    this->end();
}

/**
 * @brief SPISlaveを利用するときに最初に実行する関数
 *  ESP32無印のみ対応の関数としている
 */
#if !(IS_S3)
int8_t SPISlave::begin(uint8_t spi_bus, int8_t mosi, int8_t miso, int8_t sck, int8_t cs, int8_t hs, int32_t size){
    spi_host_device_t host_in;
    host_in = (spi_bus == HSPI) ? HSPI_HOST : VSPI_HOST;
    return begin(host_in, mosi, miso, sck, cs, hs, size);
}
#endif

/**
 * @brief SPISlaveを利用するときに最初に実行する関数
 *  ESP32-S3対応の関数
 *  hsピンを引数で指定すると高速に通信できる
 *  1024byte以上のデータを送りたいときのみ引数でsizeを指定する
 */
esp_err_t SPISlave::begin(spi_host_device_t host_in, int8_t mosi, int8_t miso, int8_t sck, int8_t cs, int8_t hs, int32_t size){
    // 二重呼び出し対策
    this->end();
    // デフォルトピン割り当て
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
    this->host = host_in;
    
    // ハンドシェイクピンのセットアップ
    this->hs_pin = hs;
    if(hs_pin >= 0){
        gpio_reset_pin((gpio_num_t)hs_pin);
        gpio_set_direction((gpio_num_t)hs_pin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)hs_pin, 0);
    }

    // サイズが指定されていれば上書き
    if(size >= 0){
        this->buffer_size = size; 
    }

    // DMA用メモリ確保
    this->rx_buf = (uint8_t*)heap_caps_malloc((size_t)this->buffer_size,MALLOC_CAP_DMA);
    this->tx_buf = (uint8_t*)heap_caps_malloc((size_t)this->buffer_size,MALLOC_CAP_DMA);
    if(this->rx_buf == nullptr || this->tx_buf == nullptr){
        this->end();
        return ESP_ERR_NO_MEM;
    }

    // DMA用メモリ初期化
    memset(this->rx_buf, 0, this->buffer_size);
    memset(this->tx_buf, 0, this->buffer_size);
    
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

    spi_slave_interface_config_t slave_cfg = {};
    slave_cfg.spics_io_num = cs;
    slave_cfg.queue_size = DEFAULT_QUEUE_SIZE;
    slave_cfg.post_setup_cb = hssetup;
    slave_cfg.post_trans_cb = hsfree;

    esp_err_t ret = spi_slave_initialize(host, &bus_cfg, &slave_cfg, SPI_DMA_CH_AUTO);
    if(ret == ESP_OK){
        this->is_initialized = true;
    }else{
        this->end();
    }
    return ret;
}
/**
 * @brief SPI通信をキューに追加する関数
 *  tx: 送信データのポインタ
 *  size: 送信データのサイズ
 */

esp_err_t SPISlave::queue(const uint8_t* tx, uint32_t size){
    if(!this->is_initialized){
        return ESP_ERR_INVALID_STATE;
    }
    // サイズチェック
    if(size > this->buffer_size){
        return ESP_ERR_INVALID_SIZE;
    }
    // DMAバッファにコピー
    if(tx != nullptr){
        memcpy(this->tx_buf, tx, size);
    }
    memset(&this->slave_trans, 0, sizeof(spi_slave_transaction_t));
    this->slave_trans.length = size * 8; 
    this->slave_trans.tx_buffer = this->tx_buf;
    this->slave_trans.rx_buffer = this->rx_buf;
    this->slave_trans.user = (void*)this;
    esp_err_t ret = spi_slave_queue_trans(this->host, &this->slave_trans, portMAX_DELAY);
    return ret;
}

/**
 * @brief SPI通信の完了を待つ関数
 *  受信データはユーザーバッファにコピーされないが、受信データのバイトサイズを返す
 *  timeoutを指定しない場合は、無限に待ち続ける
 */
int32_t SPISlave::wait(uint32_t timeout){
    if(!this->is_initialized){
        return -1;
    }
    TickType_t timeout_ticks = (timeout == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    spi_slave_transaction_t* trans;
    esp_err_t ret = spi_slave_get_trans_result(this->host, &trans, timeout_ticks);
    if(ret == ESP_OK){
        return (int32_t)(trans->trans_len / 8);
    }
    return -1;
}
/**
 * @brief SPI通信の完了を待ち、受信データをユーザーバッファにコピーする関数
 *  受信データのバイトサイズを返す
 *  timeoutを指定しない場合は、無限に待ち続ける
 */
int32_t SPISlave::wait(uint8_t* rx, uint32_t timeout){
    if(!this->is_initialized){
        return -1;
    }
    int32_t size = wait(timeout);
    if(size > 0 && rx != nullptr){
        memcpy(rx, this->rx_buf, size);
    }
    return size;
}
/**
 * @brief 受信データのポインタを取得する関数
 */
const uint8_t* SPISlave::get_raw_rx() const{
    return this->rx_buf;
}

/**
 * @brief SPI通信を終了する関数
 */
void SPISlave::end(){
    if(this->is_initialized){
        spi_slave_free(this->host);
        this->is_initialized = false;   
    }                                                                                                                                                                                
    if(this->rx_buf != nullptr){
        heap_caps_free(this->rx_buf);
        this->rx_buf = nullptr;
    }
    if(this->tx_buf != nullptr){
        heap_caps_free(this->tx_buf);
        this->tx_buf = nullptr;
    }
    return;
}

/**
 * @brief HS信号をセットアップする関数
 */
void IRAM_ATTR SPISlave::hssetup(spi_slave_transaction_t *trans){
    SPISlave* me = static_cast<SPISlave*>(trans->user);
    if(me->hs_pin >= 0){
        gpio_set_level((gpio_num_t)me->hs_pin, 1);
    }
}
/**
 * @brief HS信号を解放する関数
 */
void IRAM_ATTR SPISlave::hsfree(spi_slave_transaction_t *trans) {
    SPISlave* me = static_cast<SPISlave*>(trans->user);
    if (me->hs_pin >= 0) gpio_set_level((gpio_num_t)me->hs_pin, 0);

}

SPISLAVE_END