本ドキュメントは、ESP-IDFにおけるSPI通信の低レイヤー仕様、およびハードウェアの制約についてまとめた技術メモです。
## 1. SPIコントローラーの指定（spi_host_device_t）
```
typedef enum {
    SPI1_HOST=0,    
    SPI2_HOST=1,    
#if SOC_SPI_PERIPH_NUM > 2
    SPI3_HOST=2,    
#endif
} spi_host_device_t;
(hal/spi_types.h)
```
ESP32-S3では
* **SPI1_HOST**: フラッシュメモリおよびPSRAMとの通信専用。ユーザープログラムからは使用不可（操作するとシステムがクラッシュする）。
* **SPI2_HOST**: ユーザーが自由に利用可能なコントローラー。
* **SPI3_HOST**: ユーザーが自由に利用可能なもう一つのコントローラー。
デフォルトピンはsckが12,misoが13,mosiが11で、SPI2_HOSTとSPI3_HOSTで共通している。

ESP32無印では
* **SPI0_HOST**, **SPI1_HOST**: フラッシュメモリやPSRAMとの通信専用。
* **HSPI_HOST**:ユーザが自由に利用可能なSPIコントローラー。
デフォルトピンは、HSPI_IOMUX_PIN_NUM_CLK,HSPI_IOMUX_PIN_NUM_MISO,HSPI_IOMUX_PIN_NUM_MOSIで参照できる。
* **VSPI_HOST**:ユーザが自由に利用可能なもう一つのSPIコントローラー。
デフォルトピンは、VSPI_IOMUX_PIN_NUM_CLK,VSPI_IOMUX_PIN_NUM_MISO,VSPI_IOMUX_PIN_NUM_MOSIで参照できる。
そのため、SPISLAVEでは#ifで分岐してから処理することにする。

## 2.バスの指定（spi_bus_config_t）
SPIバスの設定は、`spi_bus_config_t`構造体で行なう
```
typedef struct {
    union {
      int mosi_io_num; // Standard SPIのMOSIピン
      int data0_io_num; // Quad SPIのData0ピン、Octal SPIのData0ピン
    };
    union {
      int miso_io_num; // Standard SPIのMISOピン
      int data1_io_num; // Quad SPIのData1ピン、Octal SPIのData1ピン
    };
    int sclk_io_num; // 全てのSPIで共通のSCLKピン
    union {
      int quadwp_io_num; // Quad SPIのWPピン
      int data2_io_num; // Octal SPIのData2ピン
    };
    union {
      int quadhd_io_num;　// Quad SPIのHDピン  
      int data3_io_num;  　// Octal SPIのData3ピン
    };
    int data4_io_num; // Octal SPIのData4ピン
    int data5_io_num; // Octal SPIのData5ピン
    int data6_io_num; // Octal SPIのData6ピン
    int data7_io_num; // Octal SPIのData7ピン
    int max_transfer_sz; // 1回のDMA転送で送れる最大バイト数
    uint32_t flags; // バスのフラグ
    int intr_flags; // 割り込みのフラグ
} spi_bus_config_t;
(hal/spi_common.h)
```
SPI通信には、いくつか種類がある。
* **Standard SPI**: MOSI、MISO、SCLKの3本の線を使用。
mosi_io_num、miso_io_num、sclk_io_numを設定し、他の線は-1に設定する。
* **Quad SPI**: MOSI、MISO、SCLKに加えて、WP（Write Protect）とHD（Hold）を使用。
mosi_io_num、miso_io_num、sclk_io_num、quadwp_io_num、quadhd_io_numを設定し、他の線は-1に設定する。
* **Octal SPI**: さらに4本のデータ線を使用。
data0_io_numからdata7_io_numまでを設定し、sclk_io_numも設定する。

「MOSI」と「Data0」は全く同じ役割の通信線であるため、unionで共用されている。他の線も同様に共用されている。

SPISLAVEではStandard SPIを使用するため、MOSI、MISO、SCLKの3本の線を設定し、他の線は-1に設定する。

## 3. デバイスのハンドル（spi_device_handle_t）
SPIデバイスを操作するためのハンドルは、`spi_device_handle_t`型で表されます。
```
typedef struct spi_device_t *spi_device_handle_t; 
(driver/spi_master.h)
```
`spi_device_handle_t`は、SPI通信を行う際に必要な情報を保持するための構造体であり、SPIバスにデバイスを追加する際に取得される。これを使用して、特定のデバイスとの通信を行うことができる。

この `spi_device_handle_t` の中には、以下の情報が記憶されている。
1. **CSピンの番号**: 通信する瞬間に、どのピンをLOWにするか。
2. **通信速度**: そのデバイス専用の通信速度
3. **SPIモード**: Mode 0 ～ Mode 3 の設定

spi_device_handle_tは、SPIバスにデバイスを追加する際に、`spi_bus_add_device()`関数を呼び出すことで取得される。この関数は、SPIバスの設定とデバイスの設定を引数として受け取り、通信に必要な情報を構築して返す。
spi_device_transmit()関数は、`spi_device_handle_t`を引数として受け取り、そのデバイスとの通信を行うために使用される。これにより、特定のデバイスに対してデータの送受信が可能になる。

## 4. デバイスのインターフェース設定（spi_device_interface_config_t）
ハンドル（`spi_device_handle_t`）を取得するためには、デバイスごとの通信ルールを定義する必要がある。そのための構造体が `spi_device_interface_config_t`である。

マスター用インターフェース設定
```
typedef struct {
    uint8_t command_bits;     // コマンドフェーズのビット数（0ならスキップ）
    uint8_t address_bits;     // アドレスフェーズのビット数（0ならスキップ）
    uint8_t dummy_bits;       // ダミーフェーズのビット数
    uint8_t mode;             // SPIモード (0〜3)
    uint16_t duty_cycle_pos;  // クロックのデューティ比（通常は128 = 50%）
    uint16_t cs_ena_pretrans; // 送信前のCSセットアップ時間
    uint8_t cs_ena_posttrans; // 送信後のCSホールド時間
    int clock_speed_hz;       // クロック周波数 (Hz)
    int input_delay_ns;       // MISOピンの入力遅延 (ns)
    int spics_io_num;         // CSピンのGPIO番号
    uint32_t flags;           // デバイス固有の動作フラグ (SPI_DEVICE_*)
    int queue_size;           // キューに積めるトランザクションの最大数
    transaction_cb_t pre_cb;  // トランザクション開始前に呼ばれるコールバック
    transaction_cb_t post_cb; // トランザクション完了後に呼ばれるコールバック
} spi_device_interface_config_t;
(driver/spi_master.h)
```
この構造体で指定したclock_speed_hzやmode、spics_io_numなどの情報が、先述の `spi_device_handle_t` 内部にカプセル化される。1つのバスに複数のデバイスを接続する場合、デバイスの数だけこの構造体を定義し、それぞれ `spi_bus_add_device()`を呼び出して個別のハンドルを取得する。

SPIスレーブ側の通信のルールを決める構造体が`spi_slave_interface_config_t`である。`
spi_slave_initialize()`に渡し、SPIの初期化ができる。
スレーブ用インターフェース設定
```
typedef struct {
    int spics_io_num;               // CSピンのGPIO番号
    uint32_t flags;                 // スレーブ固有の動作フラグ (SPI_SLAVE_*)
    int queue_size;                 // キューに積めるトランザクションの最大数
    uint8_t mode;                   // SPIモード (0〜3)
    slave_transaction_cb_t post_setup_cb; // レジスタのセットアップ完了時に呼ばれるコールバック
    slave_transaction_cb_t post_trans_cb; // トランザクション完了後に呼ばれるコールバック
} spi_slave_interface_config_t;
// (driver/spi_slave.h)
```

### 4.1 `spi_device_interface_config_t`で指定できるフラグ
|フラグ名|役割と効果|使うシチュエーション|
|:---|:---|:---|
|`SPI_DEVICE_TXBIT_LSBFIRST`|送信データを「LSB（最下位ビット）」から順に送る（※デフォルトはMSB First）|相手のデバイスがLSB First規格を要求している時。|
|`SPI_DEVICE_RXBIT_LSBFIRST`|受信データを「LSB（最下位ビット）」から順に受け取る|同上。|
|`SPI_DEVICE_HALFDUPLEX`|全二重（同時送受信）ではなく、半二重（送信フェーズが終わってから受信フェーズに移行）で通信する|1本線を送受信で使い回す「3ワイヤSPI」のデバイスなどと通信する時。|
|`SPI_DEVICE_NO_DUMMY`|読み取り通信時でも、ダミーフェーズを完全に省略する|クロックの空打ち（ウェイト）を一切許容しないシビアなデバイスの時。|
|`SPI_DEVICE_POSITIVE_CS`|CSピンの論理を逆転させ、普段はLOW、通信中をHIGHとする|特殊な論理回路を持つデバイスと通信する時。（※非常に稀）|
## 5. データ通信のトランザクション（`spi_transaction_t`/ `spi_slave_transaction_t `）
実際にSPI通信でデータを送受信する1回分の操作をトランザクションと呼び、マスター用とスレーブ用でそれぞれ専用の構造体が用意されている。
マスター用トランザクション
```
struct spi_transaction_t {
    uint32_t flags;         // トランザクション動作のフラグ
    uint16_t cmd;           // 送信するコマンドデータ
    uint64_t addr;          // 送信するアドレスデータ
    size_t length;          // 【重要】送信するデータの総ビット長
    size_t rxlength;        // 受信するデータの総ビット長（0の場合はlengthと同じ）
    void *user;             // ユーザー定義の変数（コールバック用など）
    union {
        const void *tx_buffer; // 送信用バッファのポインタ
        uint8_t tx_data[4];    // 32ビット以下のデータならバッファなしで直接送信可能
    };
    union {
        void *rx_buffer;       // 受信用バッファのポインタ
        uint8_t rx_data[4];    // 32ビット以下のデータならバッファなしで直接受信可能
    };
};(driver/spi_master.h)
```
スレーブ用トランザクション
```
struct spi_slave_transaction_t {
    size_t length;          // 【重要】通信する総ビット長
    size_t trans_len;       // 実際に通信されたビット長（結果）
    const void *tx_buffer;  // 送信用バッファのポインタ
    void *rx_buffer;        // 受信用バッファのポインタ
    void *user;             // ユーザー定義の変数
};
// (driver/spi_slave.h)
```
### 5.1 拡張トランザクション
特定のデバイスでは、基本は8bitのアドレスを使用するが、特定のコマンドの時だけ24bitのアドレスを送信しなければならないといいった変則的な通信を要求されることがある。このような一回のトランザクションだけ、通常の 変更したい通信を行いたい場合は、`spi_transaction_ext_t`構造体を使用することができる。
```
typedef struct {
    struct spi_transaction_t base;  // 通常のトランザクション構造体（先頭に配置される）
    uint8_t command_bits;           // 今回だけ適用するコマンドのビット長
    uint8_t address_bits;           // 今回だけ適用するアドレスのビット長
    uint8_t dummy_bits;             // 今回だけ適用するダミーのビット長
} spi_transaction_ext_t;
```
構造体の先頭に`base`として通常のトランザクション構造体が配置されているため、関数に渡す際は通常のポインタにキャストして渡すことができる。
```
spi_transaction_ext_t ext_t = {};
// 1. baseのflagsに「今回だけ長さを変える」というフラグを立てる
ext_t.base.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
ext_t.base.cmd = 0x13;      // コマンド
ext_t.base.addr = 0x123456; // 24ビットのアドレス
ext_t.base.length = 8;

// 2. 今回だけ適用したいビット長を拡張領域に指定する
ext_t.command_bits = 8;
ext_t.address_bits = 24; 

// 3. 通常の構造体にキャストして送信関数に渡す
spi_device_transmit(handle, (spi_transaction_t *)&ext_t);
```
## 5.2 `spi_transaction_t`で指定できるフラグ
|フラグ名|役割と効果|使うシチュエーション|
|:---|:---|:---|
|`SPI_TRANS_USE_TXDATA`|送信ポインタ(`tx_buffer`)を無視し、構造体内部の配列(`tx_data`)から送信する|4バイト以下の短いデータを最速で送る時。DMAの制約も無視できる。|
|`SPI_TRANS_USE_RXDATA`|受信ポインタ(`rx_buffer`)を無視し、構造体内部の配列(`rx_data`)に受信する|4バイト以下の短いデータを最速で受け取る時。|
|`SPI_TRANS_CS_KEEP_ACTIVE`|通信が完了しても、CSピンをHIGHに戻さずLOW（通信中）のまま維持する|1回のDMAで送りきれない大容量データ（画像など）を分割して連続送信する時。|
|`SPI_TRANS_VARIABLE_CMD`|今回の通信に限り、コマンドフェーズのビット長を上書きする|spi_transaction_ext_t（拡張構造体）を使って、特殊なコマンドを送る時。|
|`SPI_TRANS_VARIABLE_ADDR`|今回の通信に限り、アドレスフェーズのビット長を上書きする|同上。大容量メモリの深層アドレス（24bit等）にアクセスする時など。|
|`SPI_TRANS_VARIABLE_DUMMY`|今回の通信に限り、ダミーフェーズの長さを上書きする。|同上。|


## 6. SPIのフェーズ
SPIのフェーズには、Command Phase,Addres Phase, Dummy Phase, Data Phase がある。
### 6.1 Command Phase
データを読み込む命令（READ）、書きこむ命令（WRITE）といった命令をSPIデバイスに送信するフェーズである。
`spi_device_interface_config_t`の`command_bits`メンバでコマンドのビット数を指定する。
`spi_transaction_t`の`cmd`メンバで実際のコマンドを設定する。
### 6.2 Address Phase
SPIデバイスにアクセスするためのアドレスを送信するフェーズである。
`spi_device_interface_config_t`の`address_bits`メンバでアドレスのビット数を指定する。
`spi_transaction_t`の`addr`メンバで実際のアドレスを設定する。
### 6.3 Dummy Phase
データの読み書きを行う前に、データを送受信せずにクロックだけを送るフェーズである。SPIデバイスによっては、コマンドやアドレスを送信した後に、一定のクロックを送る必要があるものがある。
`spi_device_interface_config_t`の`dummy_bits`メンバでダミーのビット数（クロック数）を指定する。
### 6.4 Data Phase
実際のデータを送受信するフェーズである。
`spi_transaction_t`の`tx_buffer`と`rx_buffer`メンバで送受信するデータのアドレスを指定する。
`spi_transaction_t`の`length`メンバで送信するデータのビット数を指定する。
`spi_transaction_t`の`rxlength`メンバで受信するデータのビット数を指定する（通常は0にして、送信と同じビット数を受信するようにする）。
`spi_transaction_t`の`flags`メンバで、SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATAを指定すると`spi_transaction_t`の`rx_data`と`tx_data`メンバを使用して、ESP32のアドレスを指定せずに（`tx_buffer`, `rx_buffer`を用いずに）、最大4バイトのデータを送受信することができる。
## 7. DMAとメモリの制約
CPUを介さずにメモリとSPIペリフェラル間で直接データを転送するDMA機能を使用する場合、ESP32のハードウェア使用上、厳格なメモリ制約が発生する。

### 配置メモリの制約
DMAが直接アクセスできるのは、ESP32の内部RAMのみである。標準のRAMや外部PSRAMはDMAの対象外であるため、SPI通信でDMAを使用する場合、送受信するデータは内部RAMに配置する必要がある。
### 4バイトアラインメントの制約
DMA転送の効率を最大化するために、送受信するデータは4バイトアラインメントで配置する必要がある。つまり、データの開始アドレスは4の倍数でなければならない。特に受信用バッファ（`rx_buffer`）は、メモリ上のアドレスは必ず4の倍数から始まっている必要がある。
### 送受信データのサイズ制約
バッファの長さも4バイト制約であることが推奨される。
### 解決策
DMAを使用する通信を行う場合、`heap_caps_malloc`関数を使用して、MALLOC_CAP_DMAフラグを指定して内部RAMにバッファを確保しなければならない。
```
// DMA転送可能な内部RAMを、4バイト境界に揃えて確保する
uint8_t* tx_buf = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);
uint8_t* rx_buf = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);
```
## 8. エンディアンとバイトスワップの注意
ESP32のプロセッサとSPIの間には、データの並び順の一致しないところがある。
ESP32のメモリ配置：リトルエンディアン（下位バイトからメモリに格納）
SPI通信の標準規格：MSB First（最上位ビットから送信）
そのため、16bit（uint16_t）や32bit（uint32_t）の整数変数をそのままSPIで送信すると、バイトの順序が反転して相手に届いてしまう。これを防ぐため、バイトスワップマクロが用意されている。
```
// 送信前：ESP32のリトルエンディアンを、SPI用の順番に変換
// 第一引数：データ本体　第二引数：データのビット長
uint32_t tx_data = SPI_SWAP_DATA_TX(0x12345678, 32); 

// 受信後：SPIの順番から、ESP32のリトルエンディアンに変換
uint32_t rx_data = SPI_SWAP_DATA_RX(raw_rx_data, 32);
```
## 9. spi_master/ spi_slaveの関数一覧
## SPIマスター用 関数一覧 (`driver/spi_master.h`)

マスター側は「バスを構築」→「デバイスを登録」→「通信する」という3段階のステップを踏む。

### 9.1. 準備・設定フェーズ

| 関数名 | 引数の例 | 役割と解説 |
| :--- | :--- | :--- |
| **`spi_bus_initialize`** | `(host, &bus_config, dma_chan)` | **バスの初期化。** MOSI/MISO/SCLKのピン番号やDMAチャンネルを設定します。最初に1回だけ呼び出します。 |
| **`spi_bus_add_device`** | `(host, &dev_config, &handle)` | **デバイスの登録。** 通信相手のルール（速度、CSピン、モードなど）を教えます。成功すると通信用の `handle` が発行されます。 |

### 9.2. 通信フェーズ

通信方法には「同期（完了まで待つ）」と「非同期（裏で処理させる）」の2系統があります。

| 関数名 | 引数の例 | 役割と解説 | 使い分けの目安 |
| :--- | :--- | :--- | :--- |
| **`spi_device_transmit`** | `(handle, &trans)` | **標準的な同期通信。** 通信が終わるまで、実行中のタスクを一時停止（Sleep）させて待ちます。 | 数十バイト以上のDMA通信など、時間がかかる時に。 |
| **`spi_device_polling_transmit`** | `(handle, &trans)` | **最速のポーリング通信。** CPUを休ませず、レジスタを監視し続けて最速で処理を終えます。 | センサーから数バイト読むなど、一瞬で終わる通信に。 |
| **`spi_device_queue_trans`** | `(handle, &trans, ticks)` | **非同期通信の予約。** トランザクションを送信予約キューに入れ、関数はすぐに終了します。 | 画面描画の裏で別の計算をしたい時などに。 |
| **`spi_device_get_trans_result`** | `(handle, &trans_ptr, ticks)` | **非同期通信の結果受け取り。** 予約した通信が終わったか確認し、データを受け取ります。 | `queue_trans` と必ずセットで使います。 |

### 9.3. リセットフェーズ

| 関数名 | 引数の例 | 役割と解説 |
| :--- | :--- | :--- |
| **`spi_bus_remove_device`** | `(handle)` | 発行された `handle` を無効にし、デバイスの登録を解除します。 |
| **`spi_bus_free`** | `(host)` | バスを解体し、ピンやメモリを解放します（※全てのデバイスをremoveしてから呼びます）。 |

---

## SPIスレーブ用 関数一覧 (`driver/spi_slave.h`)

スレーブ側は「自分がデバイスそのもの」になるため、デバイス登録関数が存在しない。バスを初期化したら、マスターから通信が来るのを待ち構える。

### 9.4. 準備・設定フェーズ

| 関数名 | 引数の例 | 役割と解説 |
| :--- | :--- | :--- |
| **`spi_slave_initialize`** | `(host, &bus_cfg, &slave_cfg, dma)` | **スレーブとして初期化。** バスの設定とスレーブ独自の設定を両方渡し、一発で初期化を完了させます。 |

### 9.5. 通信フェーズ

スレーブ側は「いつ通信が始まるか主導権を握っていない」ため、ポーリング通信は存在しない。事前にバッファをセットして待ち構えるのが基本。

| 関数名 | 引数の例 | 役割と解説 |
| :--- | :--- | :--- |
| **`spi_slave_transmit`** | `(host, &trans, ticks)` | **同期通信（待機）。** マスターから通信が来て完了するまで、タスクを停止して永遠に待ち続けます。 |
| **`spi_slave_queue_trans`** | `(host, &trans, ticks)` | **非同期通信の準備。** 「次に呼ばれたらこれを返す」とバッファをセットし、すぐに別の処理に戻ります。 |
| **`spi_slave_get_trans_result`** | `(host, &trans_ptr, ticks)` | **非同期通信の結果受け取り。** マスターが実際にデータを読み書きしてくれたかを確認します。 |

### 9.6. リセットフェーズ

| 関数名 | 引数の例 | 役割と解説 |
| :--- | :--- | :--- |
| **`spi_slave_free`** | `(host)` | スレーブとしての設定を解除し、ピンや割り込み、メモリを解放します。 |