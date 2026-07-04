# SPISlave ライブラリ README

ESP32 / ESP32-S3 を **SPI Slave** として使うための簡易ライブラリです。  
想定構成は以下です。

```text
Raspberry Pi : SPI Master
ESP32/ESP32-S3 : SPI Slave
```

CanSatでは、Raspberry Piが画像処理を行い、ESP32へ処理結果だけを送る用途に向いています。画像そのものをSPIで送る用途ではなく、`CAM_RESULT`や`STATUS`のような小さい固定長フレームを送る使い方を推奨します。

---

## 重要な前提

このライブラリは、ESP-IDFの `spi_slave` ドライバをArduino環境から使いやすくする薄いラッパーです。

主な特徴は以下です。

```text
・ESP側はSPI Slaveとして待つ
・Raspberry Pi側がSPI Masterとしてクロックを出す
・1回分のSPI transactionを queue() で登録する
・Raspberry Piが通信すると wait() が戻る
・hs_pinを使うと、ESPが受信準備できたことをRaspberry Piへ通知できる
・デフォルトフレームサイズは64 byte
・デフォルトqueue数は1
```

特に重要なのは、**ESP側で `queue()` 済みでないと、Raspberry PiがSPIを叩いても正常に受けられない**という点です。

そのため、Raspberry Pi側は以下のように動かします。

```text
hspin HIGHを待つ
↓
64 byte固定でSPI転送
↓
次のhspin HIGHを待つ
```

---

## ファイル構成

```text
SPISLAVE.h
SPISLAVE.cpp
```

---

## 対応環境

ヘッダ内で以下のターゲットを判定しています。

```cpp
#ifdef CONFIG_IDF_TARGET_ESP32S3
#define IS_S3 1
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define IS_S3 0
#else
#error "No supported board specified!!!"
#endif
```

対応する主なターゲットは以下です。

```text
ESP32-S3
ESP32 無印
```

それ以外のターゲットではコンパイルエラーになります。

---

## デフォルト設定

```cpp
#define DEFAULT_QUEUE_SIZE 1
#define DEFAULT_BUFFER_SIZE 64
```

意味は以下です。

| 定数 | 意味 |
|---|---|
| `DEFAULT_QUEUE_SIZE` | SPI slave transactionの待ち行列数。現在は1。 |
| `DEFAULT_BUFFER_SIZE` | デフォルトの送受信バッファサイズ。現在は64 byte。 |

`DEFAULT_QUEUE_SIZE` が1なので、**同時に複数のtransactionは積めません**。  
つまり、必ず以下の順で使います。

```text
queue()
↓
wait()
↓
次のqueue()
```

---

## クラス概要

```cpp
class SPISlave
```

このクラスは、以下を内部で管理します。

```text
SPI host番号
hspin番号
送受信バッファサイズ
SPI mode
DMA対応rxバッファ
DMA対応txバッファ
SPI transaction構造体
初期化済みフラグ
transaction queue済みフラグ
統計情報
```

---

# 各関数の解説

## `SPISlave::SPISlave()`

```cpp
SPISlave::SPISlave() {}
```

コンストラクタです。  
特別な初期化は行いません。実際のSPI初期化は `begin()` で行います。

---

## `SPISlave::~SPISlave()`

```cpp
SPISlave::~SPISlave()
{
    end();
}
```

デストラクタです。  
オブジェクト破棄時に `end()` を呼び、SPI slaveを解放し、DMAバッファも解放します。

---

## `begin()`

SPI Slave機能を初期化します。

### ESP32-S3用

```cpp
esp_err_t begin(
    spi_host_device_t host_in = SPI2_HOST,
    int8_t mosi = -1,
    int8_t miso = -1,
    int8_t sck = -1,
    int8_t cs = -1,
    int8_t hs_pin = -1,
    int32_t size = DEFAULT_BUFFER_SIZE,
    uint8_t mode = 0
);
```

### ESP32無印用

```cpp
esp_err_t begin(
    uint8_t spi_bus = HSPI,
    int8_t mosi = -1,
    int8_t miso = -1,
    int8_t sck = -1,
    int8_t cs = -1,
    int8_t hs_pin = -1,
    int32_t size = DEFAULT_BUFFER_SIZE,
    uint8_t mode = 0
);
```

### 引数

| 引数 | 意味 |
|---|---|
| `host_in` | 使用するSPI host。ESP32-S3では通常 `SPI2_HOST`。 |
| `spi_bus` | ESP32無印用。`HSPI` または `VSPI`。 |
| `mosi` | Master Out Slave In。RasPi → ESP。 |
| `miso` | Master In Slave Out。ESP → RasPi。 |
| `sck` | SPIクロック。RasPi → ESP。 |
| `cs` | チップセレクト。RasPi → ESP。必ず指定する。 |
| `hs_pin` | ハンドシェイクピン。ESP → RasPi。不要なら `-1`。 |
| `size` | 内部DMAバッファサイズ。推奨は64。 |
| `mode` | SPI mode。まずは0を推奨。 |

### 処理内容

`begin()` は内部で以下を行います。

```text
1. 既存のSPI設定を end() で解放
2. 統計情報を初期化
3. csピンが有効か確認
4. mosi/miso/sckが未指定ならデフォルト値を入れる
5. hspinを出力LOWに初期化
6. DMA対応のrx_buf / tx_bufを確保
7. SPI bus設定を作る
8. SPI slave設定を作る
9. spi_slave_initialize() で初期化
10. 成功したら is_initialized = true
```

### 戻り値

| 戻り値 | 意味 |
|---|---|
| `ESP_OK` | 初期化成功 |
| `ESP_ERR_INVALID_ARG` | ピン指定などが不正 |
| `ESP_ERR_NO_MEM` | DMAバッファ確保失敗 |
| その他 | `spi_slave_initialize()` のエラー |

### 注意

`cs` は必ず指定してください。`cs < 0` の場合は `ESP_ERR_INVALID_ARG` を返します。

---

## `queue()`

```cpp
esp_err_t queue(const uint8_t* tx, uint32_t size, uint32_t timeout = 0);
```

次のSPI通信でRaspberry Piへ返すデータをセットし、1回分のSPI slave transactionを登録します。

### 引数

| 引数 | 意味 |
|---|---|
| `tx` | Raspberry Piへ返す送信データ。`nullptr`なら全0送信。 |
| `size` | 今回のSPI transactionサイズ。byte単位。 |
| `timeout` | `spi_slave_queue_trans()` の待ち時間。ms単位。通常は0でよい。 |

### 処理内容

```text
1. 初期化済みか確認
2. すでにtransactionがqueue済みでないか確認
3. sizeが0またはbuffer_size超過でないか確認
4. tx_buf/rx_bufを0クリア
5. txをtx_bufへコピー
6. spi_slave_transaction_tを設定
7. spi_slave_queue_trans()で登録
8. 成功したら transaction_queued = true
```

### 重要

`queue()` を呼んだだけでは通信は発生しません。  
実際の通信は、Raspberry Pi masterがCSを下げてSCLKを出したときに行われます。

```text
ESP queue()
↓
ESPはRasPiの通信を待つ
↓
RasPiがxfer2()する
↓
通信完了
↓
ESP wait()が戻る
```

### 二重queueは禁止

このライブラリでは `transaction_queued` で二重queueを防いでいます。  
`wait()` で前回のtransactionが完了する前に再度 `queue()` すると、`ESP_ERR_INVALID_STATE` を返します。

---

## `wait(uint32_t timeout)`

```cpp
int32_t wait(uint32_t timeout = portMAX_DELAY);
```

queue済みのSPI transactionが完了するのを待ちます。

### 引数

| 引数 | 意味 |
|---|---|
| `timeout` | 待ち時間。ms単位。`portMAX_DELAY`なら無限待ち。 |

### 戻り値

| 戻り値 | 意味 |
|---|---|
| `0以上` | 受信したbyte数 |
| `-1` | エラーまたはタイムアウト |

### 処理内容

```text
1. 初期化済みか確認
2. transaction queue済みか確認
3. spi_slave_get_trans_result()で完了待ち
4. タイムアウトならstats.timeoutを加算
5. 成功したらtransaction_queued = false
6. 受信byte数を返す
```

### 注意

この関数だけでは、受信データはユーザー側バッファにコピーされません。  
受信データを取り出したい場合は、次の `wait(rx, rx_capacity, timeout)` を使います。

---

## `wait(uint8_t* rx, uint32_t rx_capacity, uint32_t timeout)`

```cpp
int32_t wait(uint8_t* rx, uint32_t rx_capacity, uint32_t timeout = portMAX_DELAY);
```

SPI transaction完了を待ち、受信データをユーザーバッファへコピーします。

### 引数

| 引数 | 意味 |
|---|---|
| `rx` | 受信データのコピー先 |
| `rx_capacity` | `rx`の容量。byte単位。 |
| `timeout` | 待ち時間。ms単位。 |

### 安全性

この関数は `rx_capacity` を見て、コピーしすぎを防ぎます。  
内部の受信サイズが `rx_capacity` を超えた場合、コピー量を `rx_capacity` に制限し、`stats.rx_overflow` を加算します。

推奨はこちらです。

```cpp
uint8_t rx[64];
int32_t n = spiSlave.wait(rx, sizeof(rx), 1000);
```

---

## `wait(uint8_t* rx, uint32_t timeout)`

```cpp
int32_t wait(uint8_t* rx, uint32_t timeout = portMAX_DELAY);
```

後方互換用の関数です。  
内部的には以下を呼びます。

```cpp
return wait(rx, buffer_size, timeout);
```

### 注意

この関数では、`rx`側の実際の配列サイズをライブラリが知ることができません。  
そのため、`rx`が`buffer_size`より小さいとメモリ破壊の危険があります。

新規コードでは、できるだけこちらを使ってください。

```cpp
wait(rx, sizeof(rx), timeout);
```

---

## `transfer()`

```cpp
int32_t transfer(
    const uint8_t* tx,
    uint32_t tx_size,
    uint8_t* rx,
    uint32_t rx_capacity,
    uint32_t timeout = portMAX_DELAY
);
```

`queue()` と `wait()` を1回で行う便利関数です。

内部処理は以下です。

```text
queue(tx, tx_size, 0)
↓
wait(rx, rx_capacity, timeout)
```

簡単なテストには便利ですが、実機では `queue()` と `wait()` を分けた方が状態を追いやすいです。

---

## `get_raw_rx()`

```cpp
const uint8_t* get_raw_rx() const;
```

内部の受信バッファポインタを返します。

通常は `wait(rx, sizeof(rx), timeout)` を使えばよいです。  
コピーを避けたい場合やデバッグ時に使います。

---

## `get_raw_tx()`

```cpp
const uint8_t* get_raw_tx() const;
```

内部の送信バッファポインタを返します。  
直前にqueueした送信データを確認したい場合に使います。

---

## `initialized()`

```cpp
bool initialized() const;
```

SPI slaveが初期化済みなら `true` を返します。

---

## `queued()`

```cpp
bool queued() const;
```

現在、SPI transactionがqueue済みなら `true` を返します。

```text
true  : ESPはRasPiからのSPI通信待ち
false : まだqueueされていない、または通信完了済み
```

---

## `bufferSize()`

```cpp
uint32_t bufferSize() const;
```

内部バッファサイズを返します。  
通常は64です。

---

## `lastRxSize()`

```cpp
uint32_t lastRxSize() const;
```

直近のSPI通信で受信したbyte数を返します。

64 byte固定通信なら、常に64になるのが理想です。

---

## `lastError()`

```cpp
esp_err_t lastError() const;
```

直近のエラーコードを返します。

---

## `getStats()`

```cpp
SPISlave::Stats getStats() const;
```

通信統計を取得します。

`Stats`の中身は以下です。

```cpp
struct Stats {
    uint32_t queued;
    uint32_t completed;
    uint32_t timeout;
    uint32_t queue_error;
    uint32_t size_error;
    uint32_t rx_overflow;
    esp_err_t last_error;
    uint32_t last_rx_size;
};
```

### 各項目の意味

| 項目 | 意味 |
|---|---|
| `queued` | `queue()` に成功した回数 |
| `completed` | SPI通信が完了した回数 |
| `timeout` | `wait()` がタイムアウトした回数 |
| `queue_error` | queue失敗回数 |
| `size_error` | サイズ不正回数 |
| `rx_overflow` | ユーザー受信バッファが小さかった回数 |
| `last_error` | 最後のエラーコード |
| `last_rx_size` | 最後に受信したbyte数 |

デバッグ時は以下を定期的に表示すると原因を追いやすいです。

```cpp
auto st = spiSlave.getStats();
Serial.printf(
    "queued=%lu completed=%lu timeout=%lu qerr=%lu sizeerr=%lu rxovf=%lu last_err=%d last_rx=%lu\n",
    st.queued,
    st.completed,
    st.timeout,
    st.queue_error,
    st.size_error,
    st.rx_overflow,
    st.last_error,
    st.last_rx_size
);
```

---

## `resetStats()`

```cpp
void resetStats();
```

統計情報をリセットします。  
ただし、`last_error`だけはリセット前の値を保持します。

---

## `end()`

```cpp
void end();
```

SPI slaveを終了します。

内部で以下を行います。

```text
1. spi_slave_free()でSPI slaveを解放
2. is_initialized = false
3. transaction_queued = false
4. rx_bufを解放
5. tx_bufを解放
```

RasPiの電源を切る前や、GPIOの役割を切り替える前に使います。

---

## `hssetup()` / `hsfree()`

```cpp
static void IRAM_ATTR hssetup(spi_slave_transaction_t *trans);
static void IRAM_ATTR hsfree(spi_slave_transaction_t *trans);
```

ESP-IDFのSPI slave callbackです。通常ユーザーが直接呼ぶ必要はありません。

### `hssetup()`

SPI transactionがセットアップされたタイミングで呼ばれます。  
`hs_pin >= 0` の場合、hspinをHIGHにします。

```text
hspin HIGH = ESPがSPI通信を受けられる状態
```

### `hsfree()`

SPI transaction完了後に呼ばれます。  
hspinをLOWに戻します。

```text
hspin LOW = ESPが次のtransactionを準備中、または通信完了直後
```

---

# 推奨通信方式

このライブラリでは、以下を推奨します。

```text
SPI mode 0
100 kHzから開始
64 byte固定長
RasPi master
ESP32/ESP32-S3 slave
hspin使用
1フレーム遅れ応答
CRC付き
seq番号付き
```

---

## 1フレーム遅れ応答とは

SPIは同時送受信です。  
RasPiが送ったコマンドに対して、ESPが同じSPI転送中に返答を作ることはできません。

そのため、以下のように考えます。

```text
n回目:
  RasPi -> ESP : command_n
  ESP   -> RasPi : response_n-1

n+1回目:
  RasPi -> ESP : command_n+1
  ESP   -> RasPi : response_n
```

これを仕様として受け入れると安定します。

---

# 推奨フレーム形式

64 byte固定の例です。

```text
byte 0      magic       0xCA
byte 1      version     0x01
byte 2      type
byte 3      seq
byte 4      flags
byte 5      len
byte 6-61   payload
byte 62-63  crc16 little endian
```

payloadは最大56 byteです。

---

## フレームtype例

### RasPi → ESP

```cpp
enum SpiMsgType : uint8_t {
    SPI_NOP          = 0x00,
    SPI_PING         = 0x01,
    SPI_STATUS_POLL  = 0x02,
    SPI_CAM_RESULT   = 0x10,
    SPI_IMAGE_SAVED  = 0x11,
    SPI_SHUTDOWN_READY = 0x12,
    SPI_ERROR        = 0x7F,
};
```

### ESP → RasPi

```cpp
enum SpiRespType : uint8_t {
    SPI_RESP_EMPTY   = 0x80,
    SPI_RESP_STATUS  = 0x81,
    SPI_RESP_ACK     = 0x82,
    SPI_RESP_NACK    = 0x83,
    SPI_RESP_ERROR   = 0xFF,
};
```

---

# ESP32-S3側サンプルコード

以下は、ESP32-S3をSPI Slaveとして動かす最小例です。

```cpp
#include <Arduino.h>
#include "SPISLAVE.h"

SPISlave spiSlave;

// 実際の基板に合わせて変更してください
constexpr int PIN_SPI_MOSI = 11;   // RasPi MOSI -> ESP MOSI
constexpr int PIN_SPI_MISO = 13;   // ESP MISO  -> RasPi MISO
constexpr int PIN_SPI_SCLK = 12;   // RasPi SCLK -> ESP SCLK
constexpr int PIN_SPI_CS   = 10;   // RasPi CS   -> ESP CS
constexpr int PIN_SPI_HS   = 4;    // 旧shutdown_ack線などをhspin化する場合

constexpr uint8_t SPI_MAGIC = 0xCA;
constexpr uint8_t SPI_VER   = 0x01;
constexpr size_t FRAME_SIZE = 64;

uint8_t tx_frame[FRAME_SIZE];
uint8_t rx_frame[FRAME_SIZE];
uint8_t seq_tx = 0;

uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

void buildStatusFrame(uint8_t* f)
{
    memset(f, 0, FRAME_SIZE);
    f[0] = SPI_MAGIC;
    f[1] = SPI_VER;
    f[2] = 0x81;       // SPI_RESP_STATUS
    f[3] = seq_tx++;
    f[4] = 0x00;       // flags
    f[5] = 4;          // payload length

    // payload例
    f[6] = 0x03;       // CanSat state例: GPSNAV
    f[7] = 0x01;       // RasPi通信OKなど
    f[8] = 0x00;
    f[9] = 0x00;

    uint16_t crc = crc16_ccitt(f, FRAME_SIZE - 2);
    f[62] = crc & 0xFF;
    f[63] = crc >> 8;
}

bool checkFrame(const uint8_t* f)
{
    if (f[0] != SPI_MAGIC) return false;
    if (f[1] != SPI_VER) return false;

    uint16_t recv_crc = static_cast<uint16_t>(f[62]) |
                        (static_cast<uint16_t>(f[63]) << 8);
    uint16_t calc_crc = crc16_ccitt(f, FRAME_SIZE - 2);
    return recv_crc == calc_crc;
}

void parseRpiFrame(const uint8_t* f)
{
    if (!checkFrame(f)) {
        Serial.println("SPI frame CRC error");
        return;
    }

    uint8_t type = f[2];
    uint8_t seq  = f[3];

    Serial.printf("RX type=0x%02X seq=%u\n", type, seq);

    if (type == 0x10) { // SPI_CAM_RESULT
        uint8_t found = f[6];
        int16_t angle_x10 = static_cast<int16_t>(f[7] | (f[8] << 8));
        uint16_t distance_mm = static_cast<uint16_t>(f[9] | (f[10] << 8));
        uint8_t confidence = f[11];

        Serial.printf(
            "CAM found=%u angle_x10=%d distance=%u conf=%u\n",
            found,
            angle_x10,
            distance_mm,
            confidence
        );
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    esp_err_t ret = spiSlave.begin(
        SPI2_HOST,
        PIN_SPI_MOSI,
        PIN_SPI_MISO,
        PIN_SPI_SCLK,
        PIN_SPI_CS,
        PIN_SPI_HS,    // hspinを使わないなら -1
        FRAME_SIZE,
        0              // SPI mode 0
    );

    if (ret != ESP_OK) {
        Serial.printf("SPISlave begin failed: %d\n", ret);
        return;
    }

    Serial.println("SPISlave ready");
}

void loop()
{
    buildStatusFrame(tx_frame);

    esp_err_t qret = spiSlave.queue(tx_frame, FRAME_SIZE);
    if (qret != ESP_OK) {
        Serial.printf("queue failed: %d\n", qret);
        delay(10);
        return;
    }

    int32_t n = spiSlave.wait(rx_frame, sizeof(rx_frame), 1000);
    if (n == FRAME_SIZE) {
        parseRpiFrame(rx_frame);
    } else {
        auto st = spiSlave.getStats();
        Serial.printf(
            "wait failed n=%ld timeout=%lu qerr=%lu sizeerr=%lu last_err=%d\n",
            n,
            st.timeout,
            st.queue_error,
            st.size_error,
            st.last_error
        );
    }

    // SPI専用タスクにする場合は不要。
    delay(1);
}
```

---

# Raspberry Pi側サンプルコード

Python + `spidev` + `RPi.GPIO` の例です。

## インストール例

```bash
sudo apt update
sudo apt install -y python3-spidev python3-rpi.gpio
```

## `/boot/firmware/config.txt` または `/boot/config.txt`

SPIを有効にします。

```text
dtparam=spi=on
```

GPIO3をshutdown_reqとして使う場合は、以下のようにします。

```text
dtoverlay=gpio-shutdown,gpio_pin=3,active_low=1,gpio_pull=up
```

GPIO4をhspinとして使うなら、GPIO4はRasPi側で入力専用にしてください。

---

## RasPi SPI master サンプル

```python
import time
import spidev
import RPi.GPIO as GPIO

SPI_MAGIC = 0xCA
SPI_VER = 0x01
FRAME_SIZE = 64

HS_PIN = 4  # ESP -> RasPi hspin。旧shutdown_ack線を使う場合など。


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for x in data:
        crc ^= x << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_ping(seq: int) -> list[int]:
    f = bytearray(FRAME_SIZE)
    f[0] = SPI_MAGIC
    f[1] = SPI_VER
    f[2] = 0x01  # SPI_PING
    f[3] = seq & 0xFF
    f[4] = 0
    f[5] = 0
    crc = crc16_ccitt(f[:-2])
    f[62] = crc & 0xFF
    f[63] = (crc >> 8) & 0xFF
    return list(f)


def build_cam_result(seq: int, found: int, angle_x10: int, distance_mm: int, confidence: int) -> list[int]:
    f = bytearray(FRAME_SIZE)
    f[0] = SPI_MAGIC
    f[1] = SPI_VER
    f[2] = 0x10  # SPI_CAM_RESULT
    f[3] = seq & 0xFF
    f[4] = 0
    f[5] = 6

    f[6] = found & 0xFF
    f[7] = angle_x10 & 0xFF
    f[8] = (angle_x10 >> 8) & 0xFF
    f[9] = distance_mm & 0xFF
    f[10] = (distance_mm >> 8) & 0xFF
    f[11] = confidence & 0xFF

    crc = crc16_ccitt(f[:-2])
    f[62] = crc & 0xFF
    f[63] = (crc >> 8) & 0xFF
    return list(f)


def check_frame(rx: list[int]) -> bool:
    if len(rx) != FRAME_SIZE:
        return False
    if rx[0] != SPI_MAGIC:
        return False
    if rx[1] != SPI_VER:
        return False

    recv_crc = rx[62] | (rx[63] << 8)
    calc_crc = crc16_ccitt(bytes(rx[:-2]))
    return recv_crc == calc_crc


def wait_hs_high(timeout_s: float = 1.0) -> bool:
    start = time.monotonic()
    while time.monotonic() - start < timeout_s:
        if GPIO.input(HS_PIN) == GPIO.HIGH:
            return True
        time.sleep(0.0002)
    return False


def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(HS_PIN, GPIO.IN, pull_up_down=GPIO.PUD_OFF)

    spi = spidev.SpiDev()
    spi.open(0, 0)              # SPI0 CE0
    spi.max_speed_hz = 100000   # 最初は100kHz推奨
    spi.mode = 0
    spi.bits_per_word = 8

    seq = 0

    try:
        while True:
            if not wait_hs_high(timeout_s=1.0):
                print("HS timeout")
                continue

            # HSが上がった直後に少し待つと余裕が出る
            time.sleep(0.0001)

            tx = build_ping(seq)
            rx = spi.xfer2(tx)

            if check_frame(rx):
                print(f"RX OK type=0x{rx[2]:02X} seq={rx[3]}")
            else:
                print("RX invalid")

            seq = (seq + 1) & 0xFF

            # 必要なら少し間隔を空ける
            time.sleep(0.01)

    finally:
        spi.close()
        GPIO.cleanup()


if __name__ == "__main__":
    main()
```

---

# CanSatでの推奨配線

現在の設計では、以下が扱いやすいです。

```text
SPI_MOSI : RasPi -> ESP
SPI_MISO : ESP   -> RasPi
SPI_SCLK : RasPi -> ESP
SPI_CS   : RasPi -> ESP
GPIO3    : shutdown_req。ESPがNMOSでLowに落とす
GPIO4    : hspin。ESP -> RasPi。RasPiは入力専用
```

旧 `shutdown_ack` 線をhspinに変更する場合、RasPi側でGPIO4を出力にする処理は消してください。

---

# RasPi shutdownの推奨手順

`gpio-shutdown` を使う場合、ESPはGPIO3をLowに落とすだけでRasPiにshutdownを要求できます。

```text
ESPがSPIで PREPARE_SHUTDOWN を送る
↓
RasPiが画像保存・close・sync
↓
RasPiがSPIで SHUTDOWN_READY を返す
↓
ESPがGPIO3をLowにする
↓
RasPi OS shutdown開始
↓
ESPが5〜10秒待つ
↓
ESPがRasPi電源MOSFETをOFF
```

`SHUTDOWN_READY` が来ない場合は、30〜45秒程度待ってから強制OFFします。

---

# 通信テスト手順

最初は本番の画像処理結果を送らず、PING/STATUSだけで確認してください。

## 条件

```text
SPI mode 0
100 kHz
64 byte固定
hspin使用
1000回送信
```

## 合格条件

```text
CRCエラー 0
seq飛び 0
last_rx_size = 64
timeout = 0
```

---

# よくある失敗と原因

| 症状 | 原因候補 | 対策 |
|---|---|---|
| たまに成功するが多く失敗する | RasPiがESPのqueue前にSPIを叩いている | hspinを使う |
| 1回目だけ成功する | ESPが次のqueueを準備する前にRasPiが連続転送している | RasPi側でhspin待ち、または送信間隔を空ける |
| `wait()` がtimeoutする | RasPiからクロックが来ていない、CS不良、ピン指定違い | CS/MOSI/MISO/SCLK確認 |
| `last_rx_size` が64でない | xfer長が不一致、CSが途中で切れている | 必ず1回のxferで64 byte送る |
| CRCエラーが多い | SPI mode不一致、SCLKが速すぎる、配線品質が悪い | mode 0、100kHz、SCLK直列抵抗 |
| queue_errorが増える | 二重queueしている | queue→wait→queueの順にする |
| rx_overflowが増える | 受信バッファが小さい | `wait(rx, sizeof(rx), timeout)`を使う |

---

# 実機での注意

## 1. SPI線は短くする

CanSat内部はモーター、サーボ、DCDCのノイズが多いです。  
SPI線はできるだけ短くし、モーター線やサーボ線と並走させないでください。

## 2. SCLKに直列抵抗を入れる

可能なら以下を入れると波形が安定しやすいです。

```text
SCLK : 33Ω〜100Ω
MOSI : 33Ω〜100Ω
MISO : 33Ω〜100Ω
CS   : 33Ω〜100Ω
```

特にSCLKが重要です。

## 3. 最初から高速にしない

最初は100kHzで確認します。

```text
100 kHz
↓
250 kHz
↓
500 kHz
↓
1 MHz
```

画像処理結果だけなら、100kHzでも十分です。

## 4. 構造体をそのまま送らない

C/C++の構造体はpaddingやエンディアンの問題があります。  
SPIフレームは `uint8_t frame[64]` に手で詰めるのが安全です。

## 5. SPIコールバック内で重い処理をしない

`hssetup()` / `hsfree()` はGPIOを変えるだけにしてください。  
Flash書き込み、Serial大量出力、状態遷移の重い処理は別タスクで行います。

---

# まとめ

このライブラリの基本形は以下です。

```cpp
buildTxFrame(tx);
spiSlave.queue(tx, 64);
int32_t n = spiSlave.wait(rx, sizeof(rx), 1000);
parseRxFrame(rx);
```

Raspberry Pi側は以下です。

```python
hspinがHIGHになるまで待つ
rx = spi.xfer2(tx64)
```

CanSatで安定させるための要点は以下です。

```text
・64 byte固定長
・SPI mode 0
・100kHzから開始
・hspinを使う
・CRCとseqを入れる
・1フレーム遅れ応答にする
・RasPi側はGPIO4を入力専用にする
・shutdown_ackはSPIのSHUTDOWN_READYメッセージに置き換える
```
