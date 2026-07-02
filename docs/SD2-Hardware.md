# SD2 小电视 — 硬件技术参考

> 本文档基于 [IKuaiMonitor](https://github.com/if8336/IKuaiMonitor) 项目实际运行配置整理，供在同一硬件平台上开发其他固件时复用。  
> 开源硬件参考：[立创开源 · ESP 小电视](https://oshwhub.com/Q21182889/esp-xiao-dian-shi)

---

## 1. 硬件概述

| 项目 | 说明 |
|------|------|
| 产品名称 | SD2 小电视 / ESP 小电视 |
| 主控 | ESP8266（模组 ESP-12E/ESP-12F 类） |
| 开发板定义 | PlatformIO `board = nodemcuv2`（NodeMCU 1.0 引脚命名） |
| Flash | 通常 4 MB（以实际模组为准） |
| 通信接口 | Micro USB + **CH340** USB 转串口（刷机前请确认板载 CH340） |
| 显示 | 1.3" 方屏，**ST7789**，分辨率 **240×240**，SPI，无 CS 片选线 |
| 状态灯 | 板载 **WS2812** × 1 |
| 触摸/按键 | 本硬件无触摸屏；原项目未接物理旋钮/按键（LVGL 输入为占位实现） |

---

## 2. 引脚定义总表

NodeMCU `PIN_Dx` 与 ESP8266 `GPIO` 对应关系如下（TFT_eSPI Setup24 配置）：

| 功能 | NodeMCU 丝印 | GPIO | 方向 | 备注 |
|------|-------------|------|------|------|
| TFT DC（数据/命令） | D3 | **GPIO0** | 输出 | 启动 Strap：上电需为高才能从 Flash 启动 |
| TFT RST（复位） | D4 | **GPIO2** | 输出 | 启动 Strap：上电需为高 |
| TFT SCLK（SPI 时钟） | D5 | **GPIO14** | 输出 | HSPI CLK |
| TFT MOSI（SPI 数据） | D7 | **GPIO13** | 输出 | HSPI MOSI |
| TFT 背光 PWM | D1 | **GPIO5** | 输出 | `analogWrite` 调光 |
| WS2812 数据 | D6 | **GPIO12** | 输出 | 同时是 HSPI MISO；启动 Strap：上电需为低 |
| TFT CS | — | **未接** | — | Setup24 中无 CS，屏常选中 |
| TFT MISO | — | **未用** | — | 仅 SPI 写屏，不读屏 |

### 2.1 当前已占用 GPIO

```
GPIO0  — TFT_DC
GPIO2  — TFT_RST
GPIO5  — TFT_BL（背光）
GPIO12 — WS2812
GPIO13 — TFT_MOSI
GPIO14 — TFT_SCLK
```

### 2.2 理论可用但未在板上验证的 GPIO

| NodeMCU | GPIO | 注意 |
|---------|------|------|
| D0 | GPIO16 | 无内部上拉；Deep-sleep 唤醒脚；ADC 不可用 |
| D2 | GPIO4 | 通用 IO，相对安全 |
| D8 | GPIO15 | 启动 Strap：上电需为低；带内部下拉 |
| RX/TX | GPIO3/GPIO1 | 串口调试，一般保留给 USB |

> 扩展外设前请对照立创开源原理图确认走线，勿仅凭 GPIO 空闲假设焊盘已引出。

---

## 3. 显示屏（ST7789 240×240）

### 3.1 驱动与库配置

- **驱动库**：TFT_eSPI `^2.5.0`
- **User Setup**：`Setup24_ST7789.h`（`USER_SETUP_ID = 24`）
- **选择方式**：`.pio/libdeps/.../TFT_eSPI/User_Setup_Select.h` 中启用：

```cpp
#include <User_Setups/Setup24_ST7789.h>
```

- **驱动宏**：`ST7789_DRIVER`
- **分辨率**：`TFT_WIDTH 240` / `TFT_HEIGHT 240`
- **SPI 频率**：40 MHz（`SPI_FREQUENCY 40000000`）
- **色彩**：RGB565（LVGL `LV_COLOR_DEPTH 16`）

### 3.2 显示方向与坐标

当前 IKuaiMonitor 固件：

```cpp
tft.setRotation(0);
disp_drv.hor_res = 240;
disp_drv.ver_res = 240;
```

### 3.3 背光控制

背光脚为 `TFT_BL`（`PIN_D1` / GPIO5）。

```cpp
void setBrightness(int value) {
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, value);  // 0~1023（ESP8266 PWM 分辨率）
}
```

- 当前默认亮度：`180`（约 70%）
- 注释说明：数值越小越暗 — 实际取决于硬件背光电路（低电平有效/高电平有效），以实测为准

### 3.4 LVGL 显示缓冲

```cpp
static lv_color_t buf[LV_HOR_RES_MAX * 10];  // 240×10 行双缓冲之一
lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);
```

- LVGL 版本：lv_arduino `^3.0.1`（LVGL 7.x）
- 水平/垂直最大分辨率：`LV_HOR_RES_MAX` / `LV_VER_RES_MAX` = 240

---

## 4. WS2812 状态灯

| 参数 | 值 |
|------|-----|
| 数据脚 | **GPIO12**（NodeMCU D6） |
| 灯珠数量 | 1 |
| 颜色顺序 | `NEO_GRB` |
| 协议 | `NEO_KHZ800`（800 kHz） |
| 全局亮度 | 64（`setBrightness(64)`，范围 0~255） |

### 4.1 时序与中断

WS2812 对时序极敏感。发送数据时建议屏蔽中断：

```cpp
noInterrupts();
ws2812.show();
interrupts();
```

否则在 WiFi 活动频繁时可能出现偶发闪烁或颜色错误。

### 4.2 GPIO12 启动约束

GPIO12 为 ESP8266 启动 Strap 脚：**上电时须为低电平** 才能正常从 Flash 启动。WS2812 数据线上若有上拉或灯珠保持高电平，可能导致无法启动 — 硬件设计通常已处理，自定义接线时需注意。

---

## 5. 串口与刷机

| 参数 | 值 |
|------|-----|
| 上传波特率 | 115200 |
| 监视波特率 | 921600 |
| USB 芯片 | CH340（需安装 [WCH 驱动](https://www.wch.cn/downloads/category/67.html)） |
| 典型串口（macOS） | `/dev/cu.wchusbserial*` |
| 典型串口（Windows） | `COMx` |

### PlatformIO 命令示例

```bash
# 编译
pio run

# 上传（替换为实际端口）
pio run -t upload --upload-port /dev/cu.wchusbserial2120

# 串口监视
pio device monitor -b 921600
```

---

## 6. PlatformIO 工程模板

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
lib_deps =
    bodmer/TFT_eSPI@^2.5.0
    lvgl/lv_arduino@^3.0.1
monitor_speed = 921600
upload_speed = 115200
monitor_filters = esp8266_exception_decoder
```

### 推荐依赖版本（已验证可编译）

| 库 | 版本 |
|----|------|
| TFT_eSPI | ^2.5.0 |
| lv_arduino | ^3.0.1 |
| ArduinoJson | ^6.20.0 |
| Adafruit NeoPixel | ^1.12.0 |

---

## 7. 内存与性能参考

以 IKuaiMonitor 当前固件编译结果为准（`pio run`）：

| 资源 | 占用 | 总量 | 比例 |
|------|------|------|------|
| RAM | 72,348 B | 81,920 B | **88.3%** |
| Flash | 582,993 B | 1,044,464 B | **55.8%** |

### 开发建议

- LVGL 7 + 240×240 全屏 UI + WiFi + HTTP 已接近 RAM 上限，新项目应：
  - 控制 LVGL 控件数量与样式对象
  - 显示缓冲行数可从 `×10` 降至 `×5` 以省 RAM（牺牲刷新流畅度）
  - 中文字体仅嵌入所需汉字子集，避免全字库
  - 避免同时使用 SPIFFS/LittleFS 大文件与复杂 JSON
- WiFi + WS2812 同机运行时注意 `show()` 与网络栈的时序冲突

---

## 8. 最小显示初始化示例

```cpp
#include <lvgl.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

void setup() {
    Serial.begin(921600);
    lv_init();
    tft.begin();
    tft.setRotation(0);

    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 180);

    lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);
}
```

---

## 9. WS2812 最小初始化示例

```cpp
#include <Adafruit_NeoPixel.h>

#define WS2812_PIN   12
#define WS2812_COUNT 1

Adafruit_NeoPixel ws2812(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

void ws2812ShowSafe(Adafruit_NeoPixel &strip) {
    noInterrupts();
    strip.show();
    interrupts();
}

void setup() {
    pinMode(WS2812_PIN, OUTPUT);
    ws2812.begin();
    ws2812.setBrightness(64);
    ws2812.setPixelColor(0, ws2812.Color(0, 255, 0));
    ws2812ShowSafe(ws2812);
}
```

---

## 10. ESP8266 启动 Strap 速查

| GPIO | 上电电平要求 | 本板用途 |
|------|-------------|----------|
| GPIO0 | 高 = 正常运行 | TFT_DC |
| GPIO2 | 高 | TFT_RST |
| GPIO15 | 低 | （未用于 TFT） |
| GPIO12 | 低 | WS2812 数据 |

刷机/启动异常时，优先检查上述引脚外接电路是否拉错电平。

---

## 11. 相关源文件索引（IKuaiMonitor）

| 文件 | 内容 |
|------|------|
| `platformio.ini` | 板型、库依赖、串口速率 |
| `.pio/libdeps/nodemcuv2/TFT_eSPI/User_Setups/Setup24_ST7789.h` | 屏幕引脚与 SPI 参数 |
| `.pio/libdeps/nodemcuv2/lv_arduino/lv_conf.h` | LVGL 分辨率与色彩深度 |
| `src/main.ino` | TFT/LVGL 初始化、背光、显示刷新 |
| `src/Ws2812Status.h` | WS2812 引脚与状态灯逻辑 |
| `src/guide_font_16.c` | 16px 中文子集字库（引导页用） |

---

## 12. 采购与硬件注意

- 淘宝/闲鱼/拼多多搜 **「SD2 小电视」**，均价约 50 元
- 购买时确认：**带 CH340**、可自行烧录
- 外壳与屏幕为一体设计，无额外扩展接口时需飞线才能使用空闲 GPIO

---

*文档版本：2026-07-02 · 对应 IKuaiMonitor 当前 `nodemcuv2` 构建配置*
