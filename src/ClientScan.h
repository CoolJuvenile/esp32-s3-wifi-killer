#ifndef ClientScan_h
#define ClientScan_h

#define maxClientScanResults 40

#include <WiFi.h>
#include "esp_wifi.h"
#include "Mac.h"
#include "MacList.h"
#include "APScan.h"
#include "NameList.h"

extern APScan apScan;
extern NameList nameList;

extern String data_getVendor(uint8_t first, uint8_t second, uint8_t third);
extern bool intInArray(int num, int _array[], int size);
extern void PrintHex8(uint8_t *data, uint8_t length);
extern const bool debug;

class ClientScan {
  public:
    ClientScan();

    void start(int _time);
    void clearList();
    bool stop();
    void packetSniffer(void *buf, wifi_promiscuous_pkt_type_t type);
    int add(Mac adr);
    void send();
    void select(int num);

    String getClientName(int num);
    int getClientPackets(int num);
    String getClientVendor(int num);
    Mac getClientMac(int num);
    bool getClientSelected(int num);
    int getClientConnectedAp(int num);
    int getFirstClient();

    int results = 0;
    int selectedResults = 0;
    int timeout = 0;

    bool sniffing = false;
  private:
    size_t getSize();

    long startTime = 0;

    Mac from;
    Mac to;

    Mac broadcast;
    Mac zero;

    MacList clients;
    char vendors[maxClientScanResults][9];
    int packets[maxClientScanResults];
    bool selected[maxClientScanResults];
    int connectedToAp[maxClientScanResults];

    int channels[13];
    int channelsNum = 0;
    int curChannel = 0;
};

#endif
