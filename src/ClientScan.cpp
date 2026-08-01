#include "ClientScan.h"

// Forward declaration for the promiscuous callback defined in main.cpp
extern void sniffer(void *buf, wifi_promiscuous_pkt_type_t type);

ClientScan::ClientScan() {
  broadcast.set(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
  zero.set(0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
}

void ClientScan::clearList() {
  clients._clear();
}

void ClientScan::start(int _time) {
  Serial.println();
  Serial.println("starting client scan");

  clearList();
  for (int i = 0; i < maxClientScanResults; i++) {
    selected[i] = false;
    packets[i] = 0;
  }

  for (int i = 0; i < 13; i++) channels[i] = 0;

  results = 0;
  timeout = _time;
  sniffing = true;

  channelsNum = 0;
  curChannel = 0;

  for (int i = 0; i < apScan.results; i++) {
    if (apScan.isSelected(i)) {
      if (!intInArray(apScan.getAPChannel(i), channels, channelsNum)) {
        channels[channelsNum] = apScan.getAPChannel(i);
        channelsNum++;
      }
    }
  }

  // ESP32-S3 promiscuous mode setup
  // First disable, switch to STA mode, set channel, re-register callback, then re-enable
  esp_wifi_set_promiscuous(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous_rx_cb(sniffer);  // Re-register after mode change
  esp_wifi_set_channel(channels[curChannel], WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);

  if (debug) Serial.println("set channel to " + (String)channels[curChannel]);
  curChannel++;

  startTime = millis();
  sniffing = true;
}

bool ClientScan::stop() {
  long curTime = millis();
  if (channelsNum > 0 &&
      curTime - startTime >= (timeout * 1000) / channelsNum &&
      curChannel < channelsNum) {
    if (debug) Serial.println("changing to channel " + (String)channels[curChannel]);
    esp_wifi_set_channel(channels[curChannel], WIFI_SECOND_CHAN_NONE);
    curChannel++;
  }
  else if (curTime - startTime >= timeout * 1000 || channelsNum == 0) {
    sniffing = false;
    esp_wifi_set_promiscuous(false);

    Serial.println("stopping client scan after " + (String)(curTime - startTime) + "ms");
    if (debug) {
      for (int i = 0; i < results && i < maxClientScanResults; i++) {
        Serial.print(i);
        Serial.print(": ");
        Serial.print(getClientPackets(i));
        Serial.print(" ");
        Serial.print(getClientVendor(i));
        Serial.print(" ");
        Serial.print(getClientMac(i).toString());
        Serial.println();
      }
    }
    return true;
  }
  return false;
}

int ClientScan::add(Mac adr) {
  int clientNum = clients.getNum(adr);
  if (clientNum == -1 && results < maxClientScanResults) {
    data_getVendor(adr._get(0), adr._get(1), adr._get(2)).toCharArray(vendors[results], 9);
    connectedToAp[results] = -1;
    results++;
    clientNum = clients.add(adr);
  }
  return clientNum;
}

void ClientScan::packetSniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  // ESP32-S3: buf is wifi_promiscuous_pkt_t* with rx_ctrl + payload
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  uint8_t *frame = pkt->payload;
  uint16_t len = pkt->rx_ctrl.sig_len;

  if (sniffing && len > 27) {
    // Parse MAC addresses from 802.11 frame
    // Bytes 16-21: Address 3 (BSSID in most frames)
    // Bytes 22-27: Sequence Control + first bytes of frame body
    from.set(frame[16], frame[17], frame[18], frame[19], frame[20], frame[21]);
    to.set(frame[22], frame[23], frame[24], frame[25], frame[26], frame[27]);

    for (int i = 0; i < apScan.results; i++) {
      if (apScan.isSelected(i)) {
        if (apScan.aps._get(i).compare(from) && !broadcast.compare(to)) {
          int clientNum = add(to);
          if (clientNum >= 0) {
            connectedToAp[clientNum] = i;
            packets[clientNum]++;
          }
        }
      }
    }
  }
}

String ClientScan::getClientName(int num) {
  return nameList.getByMac(getClientMac(num));
}
int ClientScan::getClientPackets(int num) {
  return packets[clients.getNum(clients._get(num))];
}
String ClientScan::getClientVendor(int num) {
  return vendors[num];
}
Mac ClientScan::getClientMac(int num) {
  return clients._get(num);
}
int ClientScan::getClientConnectedAp(int num) {
  return connectedToAp[num];
}
bool ClientScan::getClientSelected(int num) {
  return selected[num];
}
int ClientScan::getFirstClient() {
  for (int i = 0; i < maxClientScanResults; i++) {
    if (getClientSelected(i)) return i;
  }
  return -1;
}

void ClientScan::select(int num) {
  if (num < 0) {
    if (num == -1) {
      for (int i = 0; i < results; i++) { selected[i] = true; }
      selectedResults = results;
    } else {
      for (int i = 0; i < results; i++) { selected[i] = false; }
      selectedResults = 0;
    }
  } else {
    selected[num] = !selected[num];
    if (selected[num]) selectedResults++;
    else selectedResults--;
  }
}

size_t ClientScan::getSize() {
  size_t jsonSize = 0;
  String json = "{\"clients\":[";
  jsonSize += json.length();
  for (int i = 0; i < results && i < maxClientScanResults; i++) {
    json = "{";
    json += "\"i\":" + (String)i + ",";
    json += "\"p\":" + (String)getClientPackets(i) + ",";
    json += "\"m\":\"" + getClientMac(i).toString() + "\",";
    json += "\"n\":\"" + (String)nameList.getByMac(getClientMac(i)) + "\",";
    json += "\"l\":" + (String)nameList.getNumByMac(getClientMac(i)) + ",";
    json += "\"v\":\"" + (String)getClientVendor(i) + "\",";
    json += "\"s\":" + (String)getClientSelected(i) + ",";
    if (getClientConnectedAp(i) >= 0) json += "\"a\":\"" + (String)apScan.getAPName(getClientConnectedAp(i)) + "\"";
    else json += "\"a\":\"?\"";
    json += "}";
    if ((i != results - 1) && (i != maxClientScanResults - 1)) json += ",";
    jsonSize += json.length();
  }
  json = "],\"nameList\":[";
  jsonSize += json.length();

  for (int i = 0; i < nameList.len; i++) {
    json = "{";
    json += "\"n\":\"" + nameList.get(i) + "\",";
    json += "\"m\":\"" + nameList.getMac(i).toString() + "\"";
    json += "}";
    if (i != nameList.len - 1) json += ",";
    jsonSize += json.length();
  }
  jsonSize += 2;

  return jsonSize;
}

void ClientScan::send() {
  size_t _size = getSize();
  sendHeader(200, "text/json", _size);
  if (debug) Serial.println("getting client scan JSON (" + (String)_size + ")");

  String json;
  sendToBuffer("{\"clients\":[");
  for (int i = 0; i < results && i < maxClientScanResults; i++) {
    json = "{";
    json += "\"i\":" + (String)i + ",";
    json += "\"p\":" + (String)getClientPackets(i) + ",";
    json += "\"m\":\"" + getClientMac(i).toString() + "\",";
    json += "\"n\":\"" + (String)nameList.getByMac(getClientMac(i)) + "\",";
    json += "\"l\":" + (String)nameList.getNumByMac(getClientMac(i)) + ",";
    json += "\"v\":\"" + (String)getClientVendor(i) + "\",";
    json += "\"s\":" + (String)getClientSelected(i) + ",";
    if (getClientConnectedAp(i) >= 0) json += "\"a\":\"" + (String)apScan.getAPName(getClientConnectedAp(i)) + "\"";
    else json += "\"a\":\"?\"";
    json += "}";
    if ((i != results - 1) && (i != maxClientScanResults - 1)) json += ",";
    sendToBuffer(json);
  }
  sendToBuffer("],\"nameList\":[");

  for (int i = 0; i < nameList.len; i++) {
    json = "{";
    json += "\"n\":\"" + nameList.get(i) + "\",";
    json += "\"m\":\"" + nameList.getMac(i).toString() + "\"";
    json += "}";
    if (i != nameList.len - 1) json += ",";
    sendToBuffer(json);
  }
  sendToBuffer("]}");

  sendBuffer();

  if (debug) Serial.println("\ndone");
}
