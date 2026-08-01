/*
  ===========================================
   ESP32-S3 WiFi Killer - Ported from
   ESP8266 Deauther by Stefan Kremser
   github.com/spacehuhn

   Target: ESP32-S3-N16R8 + 0.9" SSD1306 OLED
   Input:  KY-023 5-pin Joystick (VRx/VRy/SW)
  ===========================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#include "esp_wifi.h"

// ===== OLED Display (U8g2) =====
#define USE_DISPLAY

#ifdef USE_DISPLAY
  #include <U8g2lib.h>
  #include <Wire.h>

  // 0.9" SSD1306 128x64 I2C display (address 0x3C)
  // ESP32-S3: SDA=GPIO8, SCL=GPIO11
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

  // ===== Joystick + Button Pins =====
  // KY-023 joystick:
  //   GND → GND
  //   +5V → 3.3V (⚠️ 务必接3.3V! ESP32-S3不耐5V)
  //   VRx → GPIO 5  (X轴 模拟, 左右)
  //   VRy → GPIO 6  (Y轴 模拟, 上下)
  //   SW  → GPIO 7  (按下按键, 低电平触发, 内部上拉)
  #define joyX      5   // VRx - analog (左右)
  #define joyY      6   // VRy - analog (上下)
  #define joySW     7   // SW  - digital, 按下=LOW
  #define displayBtn 0  // BOOT按钮 - 开关显示屏

  // 摇杆模拟阈值 (12位ADC, 范围 0-4095)
  #define JOY_DEAD_LOW   1200   // 低于此值=上方向
  #define JOY_DEAD_HIGH  2800   // 高于此值=下方向
  // 中间区域(1200-2800) = 死区

  // Display settings
  #define fontSize    7
  #define rowsPerSite 8

  int rows = 4;
  int curRow = 0;
  int sites = 1;
  int curSite = 1;
  int lrow = 0;

  // 摇杆去抖 & 重复
  bool canBtnPress = true;
  int  buttonPressed = 0;     // 0=UP, 1=DOWN, 2=SELECT, 3=DISPLAY
  bool displayOn = true;

  // 上下方向长按重复
  unsigned long lastJoyMove = 0;
  #define JOY_REPEAT_DELAY  200   // 首次延迟(ms)
  #define JOY_REPEAT_RATE   80    // 连续重复间隔(ms)
  bool joyRepeatActive = false;
#endif

// ===== Optional Factory Reset Pin =====
// #define resetPin 4  // GPIO 4 - pull LOW on boot to reset settings

WebServer server(80);

#include <Preferences.h>
#include "data.h"
#include "NameList.h"
#include "APScan.h"
#include "ClientScan.h"
#include "Attack.h"
#include "Settings.h"
#include "SSIDList.h"

/* ========== DEBUG ========== */
const bool debug = true;
/* ========== DEBUG ========== */

// Run-Time Variables //
String wifiMode = "";
String attackMode_deauth = "";
String attackMode_beacon = "";
String scanMode = "SCAN";

bool warning = true;

NameList nameList;

APScan apScan;
ClientScan clientScan;
Attack attack;
Settings settings;
SSIDList ssidList;

void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  clientScan.packetSniffer(buf, type);
}

#ifdef USE_DISPLAY
void drawInterface() {
  if (!displayOn) {
    display.clearBuffer();
    display.sendBuffer();
    return;
  }

  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tf);  // 5x7 pixel font

  int _lrow = 0;
  for (int i = curSite * rowsPerSite - rowsPerSite; i < curSite * rowsPerSite; i++) {
    int yPos = _lrow * (fontSize + 1);

    if (i == 0) {
      String line = "> " + wifiMode;
      display.drawStr(0, yPos + fontSize, line.c_str());
    }
    else if (i == 1) {
      String line = "> " + scanMode;
      display.drawStr(0, yPos + fontSize, line.c_str());
    }
    else if (i == 2) {
      String line = "> " + attackMode_deauth + " deauth";
      display.drawStr(0, yPos + fontSize, line.c_str());
    }
    else if (i == 3) {
      String line = "> " + attackMode_beacon + " beacon";
      display.drawStr(0, yPos + fontSize, line.c_str());
    }
    else if (i - 4 < apScan.results) {
      display.drawStr(4, yPos + fontSize, apScan.getAPName(i - 4).c_str());
      if (apScan.isSelected(i - 4)) {
        display.drawVLine(1, yPos, fontSize);
        display.drawVLine(2, yPos, fontSize);
      }
    }

    // Draw cursor
    if (_lrow == lrow) {
      display.drawVLine(0, yPos, fontSize);
    }
    _lrow++;
  }

  display.sendBuffer();
}
#endif

void startWifi() {
  Serial.println("\nStarting WiFi AP:");
  WiFi.mode(WIFI_MODE_AP);

  // Set promiscuous callback for client scanning
  esp_wifi_set_promiscuous_rx_cb(sniffer);
  // Note: promiscuous mode is only enabled during client scan

  bool hidden = settings.ssidHidden;
  WiFi.softAP(settings.ssid.c_str(), settings.password.c_str(), settings.apChannel, hidden);

  Serial.println("SSID     : '" + settings.ssid + "'");
  Serial.println("Password : '" + settings.password + "'");
  Serial.println("-----------------------------------------------");
  if (settings.password.length() < 8)
    Serial.println("WARNING: password must have at least 8 characters!");
  if (settings.ssid.length() < 1 || settings.ssid.length() > 32)
    Serial.println("WARNING: SSID length must be between 1 and 32 characters!");
  wifiMode = "ON";
}

void stopWifi() {
  Serial.println("stopping WiFi AP");
  Serial.println("-----------------------------------------------");
  WiFi.disconnect();
  WiFi.mode(WIFI_MODE_STA); // Go to STA mode to allow scanning
  wifiMode = "OFF";
}

// ========== Web Server Routes ==========

void loadIndexHTML() {
  if (warning) {
    sendFile(200, "text/html", data_indexHTML, sizeof(data_indexHTML));
  } else {
    sendFile(200, "text/html", data_apscanHTML, sizeof(data_apscanHTML));
  }
}
void loadAPScanHTML() {
  warning = false;
  sendFile(200, "text/html", data_apscanHTML, sizeof(data_apscanHTML));
}
void loadStationsHTML() {
  sendFile(200, "text/html", data_stationsHTML, sizeof(data_stationsHTML));
}
void loadAttackHTML() {
  sendFile(200, "text/html", data_attackHTML, sizeof(data_attackHTML));
}
void loadSettingsHTML() {
  sendFile(200, "text/html", data_settingsHTML, sizeof(data_settingsHTML));
}
void load404() {
  sendFile(200, "text/html", data_errorHTML, sizeof(data_errorHTML));
}

void loadFunctionsJS() {
  sendFile(200, "text/javascript", data_js_functionsJS, sizeof(data_js_functionsJS));
}
void loadAPScanJS() {
  sendFile(200, "text/javascript", data_js_apscanJS, sizeof(data_js_apscanJS));
}
void loadStationsJS() {
  sendFile(200, "text/javascript", data_js_stationsJS, sizeof(data_js_stationsJS));
}
void loadAttackJS() {
  attack.ssidChange = true;
  sendFile(200, "text/javascript", data_js_attackJS, sizeof(data_js_attackJS));
}
void loadSettingsJS() {
  sendFile(200, "text/javascript", data_js_settingsJS, sizeof(data_js_settingsJS));
}

void loadStyle() {
  sendFile(200, "text/css;charset=UTF-8", data_styleCSS, sizeof(data_styleCSS));
}

void startWiFi(bool start) {
  if (start) startWifi();
  else stopWifi();
  clientScan.clearList();
}

//==========AP-Scan==========
void startAPScan() {
  scanMode = "scanning...";
#ifdef USE_DISPLAY
  drawInterface();
#endif
  if (apScan.start()) {

#ifdef USE_DISPLAY
    apScan.sort();
    rows = 4;
    rows += apScan.results;
    sites = rows / rowsPerSite;
    if (rows % rowsPerSite > 0) sites++;
#endif

    server.send(200, "text/json", "true");
    attack.stopAll();
    scanMode = "SCAN";
  }
}

void sendAPResults() {
  apScan.sendResults();
}

void selectAP() {
  if (server.hasArg("num")) {
    apScan.select(server.arg("num").toInt());
    server.send(200, "text/json", "true");
    attack.stopAll();
  }
}

//==========Client-Scan==========
void startClientScan() {
  if (server.hasArg("time") && apScan.getFirstTarget() > -1 && !clientScan.sniffing) {
    server.send(200, "text/json", "true");
    clientScan.start(server.arg("time").toInt());
    attack.stopAll();
  } else server.send(200, "text/json", "Error: no selected access point");
}

void sendClientResults() {
  clientScan.send();
}
void sendClientScanTime() {
  server.send(200, "text/json", (String)settings.clientScanTime);
}

void selectClient() {
  if (server.hasArg("num")) {
    clientScan.select(server.arg("num").toInt());
    attack.stop(0);
    server.send(200, "text/json", "true");
  }
}

void addClientFromList() {
  if (server.hasArg("num")) {
    int _num = server.arg("num").toInt();
    clientScan.add(nameList.getMac(_num));
    server.send(200, "text/json", "true");
  } else server.send(200, "text/json", "false");
}

void setClientName() {
  if (server.hasArg("id") && server.hasArg("name")) {
    if (server.arg("name").length() > 0) {
      nameList.add(clientScan.getClientMac(server.arg("id").toInt()), server.arg("name"));
      server.send(200, "text/json", "true");
    }
    else server.send(200, "text/json", "false");
  }
}

void deleteName() {
  if (server.hasArg("num")) {
    int _num = server.arg("num").toInt();
    nameList.remove(_num);
    server.send(200, "text/json", "true");
  } else server.send(200, "text/json", "false");
}

void clearNameList() {
  nameList.clear();
  server.send(200, "text/json", "true");
}

void editClientName() {
  if (server.hasArg("id") && server.hasArg("name")) {
    nameList.edit(server.arg("id").toInt(), server.arg("name"));
    server.send(200, "text/json", "true");
  } else server.send(200, "text/json", "false");
}

void addClient() {
  if (server.hasArg("mac") && server.hasArg("name")) {
    String macStr = server.arg("mac");
    macStr.replace(":", "");
    Serial.println("add " + macStr + " - " + server.arg("name"));
    if (macStr.length() < 12 || macStr.length() > 12) server.send(200, "text/json", "false");
    else {
      Mac _newClient;
      for (int i = 0; i < 6; i++) {
        const char* val = macStr.substring(i * 2, i * 2 + 2).c_str();
        uint8_t valByte = strtoul(val, NULL, 16);
        _newClient.setAt(valByte, i);
      }
      nameList.add(_newClient, server.arg("name"));
      server.send(200, "text/json", "true");
    }
  }
}

//==========Attack==========
void sendAttackInfo() {
  attack.sendResults();
}

void startAttack() {
  if (server.hasArg("num")) {
    int _attackNum = server.arg("num").toInt();
    if (apScan.getFirstTarget() > -1 || _attackNum == 1 || _attackNum == 2) {
      attack.start(server.arg("num").toInt());
      server.send(200, "text/json", "true");
    } else server.send(200, "text/json", "false");
  }
}

void addSSID() {
  if (server.hasArg("ssid") && server.hasArg("num") && server.hasArg("enc")) {
    int num = server.arg("num").toInt();
    if (num > 0) {
      ssidList.addClone(server.arg("ssid"), num, server.arg("enc") == "true");
    } else {
      ssidList.add(server.arg("ssid"), server.arg("enc") == "true" || server.arg("enc") == "1");
    }
    attack.ssidChange = true;
    server.send(200, "text/json", "true");
  } else server.send(200, "text/json", "false");
}

void cloneSelected() {
  if (apScan.selectedSum > 0) {
    int clonesPerSSID = 48 / apScan.selectedSum;
    ssidList.clear();
    for (int i = 0; i < apScan.results; i++) {
      if (apScan.isSelected(i)) {
        ssidList.addClone(apScan.getAPName(i), clonesPerSSID, apScan.getAPEncryption(i) != "none");
      }
    }
  }
  attack.ssidChange = true;
  server.send(200, "text/json", "true");
}

void deleteSSID() {
  ssidList.remove(server.arg("num").toInt());
  attack.ssidChange = true;
  server.send(200, "text/json", "true");
}

void randomSSID() {
  ssidList._random();
  attack.ssidChange = true;
  server.send(200, "text/json", "true");
}

void clearSSID() {
  ssidList.clear();
  attack.ssidChange = true;
  server.send(200, "text/json", "true");
}

void resetSSID() {
  ssidList.load();
  attack.ssidChange = true;
  server.send(200, "text/json", "true");
}

void saveSSID() {
  ssidList.save();
  server.send(200, "text/json", "true");
}

void restartESP() {
  server.send(200, "text/json", "true");
  ESP.restart();
}

void enableRandom() {
  attack.changeRandom(server.arg("interval").toInt());
  server.send(200, "text/json", "true");
}

//==========Settings==========
void getSettings() {
  settings.send();
}

void saveSettings() {
  if (server.hasArg("ssid")) settings.ssid = server.arg("ssid");
  if (server.hasArg("ssidHidden")) {
    if (server.arg("ssidHidden") == "false") settings.ssidHidden = false;
    else settings.ssidHidden = true;
  }
  if (server.hasArg("password")) settings.password = server.arg("password");
  if (server.hasArg("apChannel")) {
    if (server.arg("apChannel").toInt() >= 1 && server.arg("apChannel").toInt() <= 14) {
      settings.apChannel = server.arg("apChannel").toInt();
    }
  }
  if (server.hasArg("macAp")) {
    String macStr = server.arg("macAp");
    macStr.replace(":", "");
    Mac tempMac;
    if (macStr.length() == 12) {
      for (int i = 0; i < 6; i++) {
        const char* val = macStr.substring(i * 2, i * 2 + 2).c_str();
        uint8_t valByte = strtoul(val, NULL, 16);
        tempMac.setAt(valByte, i);
      }
      if (tempMac.valid()) settings.macAP.set(tempMac);
    } else if (macStr.length() == 0) {
      settings.macAP.set(settings.defaultMacAP);
    }
  }
  if (server.hasArg("randMacAp")) {
    if (server.arg("randMacAp") == "false") settings.isMacAPRand = false;
    else settings.isMacAPRand = true;
  }
  if (server.hasArg("scanTime")) settings.clientScanTime = server.arg("scanTime").toInt();
  if (server.hasArg("timeout")) settings.attackTimeout = server.arg("timeout").toInt();
  if (server.hasArg("deauthReason")) settings.deauthReason = server.arg("deauthReason").toInt();
  if (server.hasArg("packetRate")) settings.attackPacketRate = server.arg("packetRate").toInt();
  if (server.hasArg("apScanHidden")) {
    if (server.arg("apScanHidden") == "false") settings.apScanHidden = false;
    else settings.apScanHidden = true;
  }
  if (server.hasArg("beaconInterval")) {
    if (server.arg("beaconInterval") == "false") settings.beaconInterval = false;
    else settings.beaconInterval = true;
  }
  if (server.hasArg("useLed")) {
    if (server.arg("useLed") == "false") settings.useLed = false;
    else settings.useLed = true;
    attack.refreshLed();
  }
  if (server.hasArg("channelHop")) {
    if (server.arg("channelHop") == "false") settings.channelHop = false;
    else settings.channelHop = true;
  }
  if (server.hasArg("multiAPs")) {
    if (server.arg("multiAPs") == "false") settings.multiAPs = false;
    else settings.multiAPs = true;
  }
  if (server.hasArg("multiAttacks")) {
    if (server.arg("multiAttacks") == "false") settings.multiAttacks = false;
    else settings.multiAttacks = true;
  }

  if (server.hasArg("ledPin")) settings.setLedPin(server.arg("ledPin").toInt());
  if (server.hasArg("macInterval")) settings.macInterval = server.arg("macInterval").toInt();

  settings.save();
  server.send(200, "text/json", "true");
}

void resetSettings() {
  settings.reset();
  server.send(200, "text/json", "true");
}

void setup() {
  // ESP32-S3: Use esp_random() instead of os_random()
  randomSeed(esp_random());

  // Turn off built-in LED initially (active LOW on many boards)
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  Serial.begin(115200);
  // Wait for USB CDC serial on ESP32-S3
  delay(1000);

  Serial.println();
  Serial.println("================================================");
  Serial.println("  ESP32-S3 WiFi Killer v1.6 (Ported)");
  Serial.println("  Target: ESP32-S3-N16R8 + 0.9\" OLED");
  Serial.println("================================================");

  attackMode_deauth = "START";
  attackMode_beacon = "START";

  // Initialize NVS (required for Preferences on ESP32)
  // Preferences handles NVS init internally

  // Initialize LittleFS for logging
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }

  settings.load();
  if (debug) settings.info();
  settings.syncMacInterface();
  nameList.load();
  ssidList.load();

  attack.refreshLed();

  delay(500); // Prevent bssid leak

  startWifi();
  attack.stopAll();
  attack.generate();

  /* ========== Web Server ========== */

  /* HTML */
  server.onNotFound(load404);

  server.on("/", loadIndexHTML);
  server.on("/index.html", loadIndexHTML);
  server.on("/apscan.html", loadAPScanHTML);
  server.on("/stations.html", loadStationsHTML);
  server.on("/attack.html", loadAttackHTML);
  server.on("/settings.html", loadSettingsHTML);

  /* JS */
  server.on("/js/apscan.js", loadAPScanJS);
  server.on("/js/stations.js", loadStationsJS);
  server.on("/js/attack.js", loadAttackJS);
  server.on("/js/settings.js", loadSettingsJS);
  server.on("/js/functions.js", loadFunctionsJS);

  /* CSS */
  server.on("/style.css", loadStyle);

  /* JSON */
  server.on("/APScanResults.json", sendAPResults);
  server.on("/APScan.json", startAPScan);
  server.on("/APSelect.json", selectAP);
  server.on("/ClientScan.json", startClientScan);
  server.on("/ClientScanResults.json", sendClientResults);
  server.on("/ClientScanTime.json", sendClientScanTime);
  server.on("/clientSelect.json", selectClient);
  server.on("/setName.json", setClientName);
  server.on("/addClientFromList.json", addClientFromList);
  server.on("/attackInfo.json", sendAttackInfo);
  server.on("/attackStart.json", startAttack);
  server.on("/settings.json", getSettings);
  server.on("/settingsSave.json", saveSettings);
  server.on("/settingsReset.json", resetSettings);
  server.on("/deleteName.json", deleteName);
  server.on("/clearNameList.json", clearNameList);
  server.on("/editNameList.json", editClientName);
  server.on("/addSSID.json", addSSID);
  server.on("/cloneSelected.json", cloneSelected);
  server.on("/deleteSSID.json", deleteSSID);
  server.on("/randomSSID.json", randomSSID);
  server.on("/clearSSID.json", clearSSID);
  server.on("/resetSSID.json", resetSSID);
  server.on("/saveSSID.json", saveSSID);
  server.on("/restartESP.json", restartESP);
  server.on("/addClient.json", addClient);
  server.on("/enableRandom.json", enableRandom);

  server.begin();
  Serial.println("Web server started at http://192.168.4.1");

#ifdef USE_DISPLAY
  // Initialize I2C for OLED
  Wire.begin(8, 11);  // SDA=GPIO8, SCL=GPIO11

  display.begin();
  display.setContrast(255);

  // Setup joystick & button pins
  pinMode(joyX, INPUT);         // VRx 模拟输入
  pinMode(joyY, INPUT);         // VRy 模拟输入
  pinMode(joySW, INPUT_PULLUP); // SW  数字输入(按下=LOW)
  analogReadResolution(12);     // ESP32-S3 12位ADC (0-4095)

  if (displayBtn == 0) pinMode(displayBtn, INPUT);
  else pinMode(displayBtn, INPUT_PULLUP);

  // Show splash screen
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(0, 10, "ESP32-S3");
  display.setFont(u8g2_font_7x13_tf);
  display.drawStr(0, 28, "WiFi Killer");
  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(90, 28, "v1.6");
  display.drawStr(0, 45, "Joystick + OLED");
  display.drawStr(0, 55, "github.com/spacehuhn");
  display.sendBuffer();

  display.setFont(u8g2_font_5x7_tf);

  delay(1600);
#endif

#ifdef resetPin
  pinMode(resetPin, INPUT_PULLUP);
  if (digitalRead(resetPin) == LOW) settings.reset();
#endif

  if (debug) {
    Serial.println("\nStarting...\n");
#ifndef USE_DISPLAY
    delay(1600);
#endif
  }
}

void loop() {
  if (clientScan.sniffing) {
    if (clientScan.stop()) {
      // Client scan finished, restart WiFi AP
      startWifi();
    }
  } else {
    server.handleClient();
    attack.run();
  }

  // Serial console: type "reset" to reset settings
  if (Serial.available()) {
    String input = Serial.readString();
    if (input == "reset" || input == "reset\n" || input == "reset\r" || input == "reset\r\n") {
      settings.reset();
    }
  }

#ifdef USE_DISPLAY
  // ===== Joystick + Button Input =====
  int  joyYval = analogRead(joyY);        // 读Y轴 (上下)
  int  joyXval = analogRead(joyX);        // 读X轴 (左右, 备用)
  bool joySwPressed = (digitalRead(joySW) == LOW);  // 摇杆按下
  bool displayBtnPressed = (digitalRead(displayBtn) == LOW); // BOOT按钮

  // 检测是否有任何输入
  bool hasInput = (joyYval < JOY_DEAD_LOW) || (joyYval > JOY_DEAD_HIGH)
                  || joySwPressed || displayBtnPressed;

  if (hasInput) {
    if (canBtnPress) {
      // ---- 判断方向 ----
      if (joySwPressed) {
        buttonPressed = 2;   // SELECT
      } else if (displayBtnPressed) {
        buttonPressed = 3;   // DISPLAY toggle
      } else if (joyYval < JOY_DEAD_LOW) {
        buttonPressed = 0;   // UP
      } else if (joyYval > JOY_DEAD_HIGH) {
        buttonPressed = 1;   // DOWN
      }

      // ---- 执行动作 ----
      // ===== UP =====
      if (buttonPressed == 0 && curRow > 0) {
        curRow--;
        if (lrow - 1 < 0) {
          lrow = rowsPerSite - 1;
          curSite--;
        } else lrow--;

      // ===== DOWN =====
      } else if (buttonPressed == 1 && curRow < rows - 1) {
        curRow++;
        if (lrow + 1 >= rowsPerSite) {
          lrow = 0;
          curSite++;
        } else lrow++;

      // ===== SELECT =====
      } else if (buttonPressed == 2) {

        // ===== WIFI on/off =====
        if (curRow == 0) {
          if (wifiMode == "ON") stopWifi();
          else startWifi();

        // ===== scan for APs =====
        } else if (curRow == 1) {
          startAPScan();
          drawInterface();

        // ===== deauth attack =====
        } else if (curRow == 2) {
          if (attackMode_deauth == "START" && apScan.getFirstTarget() > -1) attack.start(0);
          else if (attackMode_deauth == "STOP") attack.stop(0);

        // ===== beacon attack =====
        } else if (curRow == 3) {
          if (attackMode_beacon == "START") {
            if (apScan.selectedSum > 0) {
              int clonesPerSSID = 48 / apScan.selectedSum;
              ssidList.clear();
              for (int i = 0; i < apScan.results; i++) {
                if (apScan.isSelected(i)) {
                  ssidList.addClone(apScan.getAPName(i), clonesPerSSID,
                                    apScan.getAPEncryption(i) != "none");
                }
              }
            }
            attack.ssidChange = true;
            attack.start(1);
          }
          else if (attackMode_beacon == "STOP") attack.stop(1);

        // ===== select APs =====
        } else if (curRow >= 4) {
          attack.stop(0);
          apScan.select(curRow - 4);
        }

      // ===== DISPLAY on/off =====
      } else if (buttonPressed == 3) {
        displayOn = !displayOn;
        display.clearBuffer();
        display.sendBuffer();
      }

      canBtnPress = false;
      lastJoyMove = millis();
      joyRepeatActive = false;
    }
    else {
      // 按键按住状态：摇杆方向可重复触发
      if (!joySwPressed && !displayBtnPressed) {
        if (millis() - lastJoyMove > (joyRepeatActive ? JOY_REPEAT_RATE : JOY_REPEAT_DELAY)) {
          joyRepeatActive = true;
          lastJoyMove = millis();

          if (joyYval < JOY_DEAD_LOW && curRow > 0) {
            curRow--;
            if (lrow - 1 < 0) { lrow = rowsPerSite - 1; curSite--; }
            else lrow--;
          }
          else if (joyYval > JOY_DEAD_HIGH && curRow < rows - 1) {
            curRow++;
            if (lrow + 1 >= rowsPerSite) { lrow = 0; curSite++; }
            else lrow++;
          }
        }
      }
    }
  } else {
    // 摇杆回中 + 所有按键松开 → 重置状态
    canBtnPress = true;
    joyRepeatActive = false;
  }

  drawInterface();
#endif
}
