# ESP32-S3 WiFi Killer

Ported from [ESP8266 Deauther](https://github.com/spacehuhn/esp8266_deauther) v1.6 by Stefan Kremser.

## 硬件清单 (仅需3个模块)

| 模块 | 型号 |
|------|------|
| 主控 | ESP32-S3-N16R8 |
| 屏幕 | 0.9" SSD1306 128x64 OLED (I2C) |
| 输入 | KY-023 四轴摇杆 (5脚) |

## 接线图

### OLED 0.9寸 (4脚 I2C)

| OLED | ESP32-S3 |
|------|----------|
| VCC  | 3.3V     |
| GND  | GND      |
| SDA  | GPIO 8   |
| SCL  | GPIO 11  |

### KY-023 摇杆 (5脚)

| 摇杆 | ESP32-S3 | 说明 |
|------|----------|------|
| GND  | GND      | 地 |
| **+5V** → | **3.3V** ⚠️ | **必须接3.3V! ESP32-S3引脚不耐5V!** |
| VRx  | GPIO 5   | X轴模拟量 (左右, 预留) |
| VRy  | GPIO 6   | Y轴模拟量 (上下翻页) |
| SW   | GPIO 7   | 按下确认键 |

### BOOT 按钮 (板载)

| BOOT | ESP32-S3 | 用途 |
|------|----------|------|
| GPIO 0 | - (板载) | 短按=开关OLED屏幕 |

## 操作说明

| 操作 | 效果 |
|------|------|
| 摇杆 ↑ | 光标上移 |
| 摇杆 ↓ | 光标下移 |
| 摇杆按下 (SW) | 确认/选择 |
| BOOT按钮 | 开关屏幕 |
| 摇杆 ↑/↓ 长按 | 连续滚动 |

## Features
- **Deauth Attack**: Disconnect devices from WiFi networks
- **Beacon Flood**: Broadcast fake access points (up to 48 SSIDs)
- **Probe Request Flood**: Send fake probe requests
- **Web Interface**: Control via browser at `http://192.168.4.1`
- **OLED Display**: On-device control with 4 buttons
- **Client Scanning**: Find devices connected to target networks

## Default Settings
- SSID: `HACKER`
- Password: `woshiheike`
- Channel: 9
- Web UI: `http://192.168.4.1`

## Building

### Requirements
- [PlatformIO IDE](https://platformio.org/) (VS Code extension recommended)
- Or [PlatformIO Core CLI](https://docs.platformio.org/en/latest/core/installation.html)

### Build & Upload
```bash
cd esp32-s3-wifi-killer
platformio run          # Compile
platformio run -t upload # Compile & upload
platformio device monitor # Serial monitor (115200 baud)
```

### Changing Pins
Edit `src/main.cpp` to adjust pin assignments for your specific hardware.

## Key Changes from ESP8266 Original

| ESP8266 API | ESP32-S3 API |
|-------------|--------------|
| `ESP8266WiFi.h` | `WiFi.h` + `esp_wifi.h` |
| `ESP8266WebServer.h` | `WebServer.h` |
| `wifi_send_pkt_freedom()` | `esp_wifi_80211_tx()` |
| `wifi_set_channel()` | `esp_wifi_set_channel()` |
| `wifi_promiscuous_enable()` | `esp_wifi_set_promiscuous()` |
| `wifi_set_opmode()` | `WiFi.mode()` |
| `EEPROM.h` | `Preferences.h` (NVS) |
| `SPIFFS` | `LittleFS` |
| `os_random()` | `esp_random()` |
| `SSD1306.h` (squix78) | `U8g2` (u8g2) |

## Known Limitations
- Some ESP32-S3 firmware versions may filter deauth management frames; test with your specific SDK version
- Promiscuous mode disables normal WiFi operations during client scanning
- The 1.7MB data.h causes slower initial compilation
- Serial console: type `reset` to factory reset settings

## License
MIT License (same as original project)
