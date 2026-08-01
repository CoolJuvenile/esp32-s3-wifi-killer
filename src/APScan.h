#ifndef APScan_h
#define APScan_h

#define maxAPScanResults 80

#include <WiFi.h>
#include "Mac.h"
#include "MacList.h"
#include "Settings.h"
#include <WebServer.h>

extern String data_getVendor(uint8_t first, uint8_t second, uint8_t third);
extern WebServer server;
extern void sendBuffer();
extern void sendToBuffer(String str);
extern void sendHeader(int code, String type, size_t _size);
extern const bool debug;

extern Settings settings;

class APScan {
  public:
    APScan();

    bool start();
    void sort();
    String getResultsJSON();
    void select(int num);
    void sendResults();

    String getAPName(int num);
    String getAPEncryption(int num);
    String getAPMac(int num);
    bool isHidden(int num);
    int getAPRSSI(int num);
    int getAPChannel(int num);

    int getFirstTarget();
    bool isSelected(int num);

    int results = 0;
    int selectedSum;
    MacList aps;
  private:

    int channels[maxAPScanResults];
    int rssi[maxAPScanResults];
    char names[maxAPScanResults][33];
    int encryption[maxAPScanResults];
    bool hidden[maxAPScanResults];

    String getEncryption(int code);

    bool selected[maxAPScanResults];
};

#endif
