#ifndef Settings_h
#define Settings_h

#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include "Mac.h"
#include "MacList.h"
#include "NameList.h"

#include "esp_wifi.h"

extern void sendBuffer();
extern void sendToBuffer(String str);
extern void sendHeader(int code, String type, size_t _size);

extern const bool debug;
extern String data_getVendor(uint8_t first, uint8_t second, uint8_t third);
extern NameList nameList;

class Settings
{
  public:
    Settings();
    void load();
    void reset();
    void save();
    void send();
    void info();

    int ssidLen;
    String ssid = "";
    bool ssidHidden;
    int passwordLen;
    String password = "";
    int apChannel;
    bool apScanHidden;
    uint8_t deauthReason;
    unsigned int attackTimeout;
    int attackPacketRate;
    int clientScanTime;
    bool useLed;
    bool channelHop;
    bool multiAPs;
    bool multiAttacks;
    int macInterval;
    bool beaconInterval;
    int ledPin = 0;
    int prevLedPin = 0;
    Mac defaultMacAP;
    Mac macAP;
    bool isMacAPRand;
    bool isSettingsLoaded = false;
    void syncMacInterface();
    void setLedPin(int newLedPin);
    bool pinStateOff = true;  // When attack is off, pin state is HIGH

  private:
    size_t getSize();
};

#endif
