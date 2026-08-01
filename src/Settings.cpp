#include "Settings.h"

static Preferences settingsPrefs;

Settings::Settings() {
  uint8_t tempMAC[6];
  // ESP32-S3: Use WiFi.softAPmacAddress()
  String macStr = WiFi.softAPmacAddress();
  // Parse MAC string to bytes
  for (int i = 0; i < 6; i++) {
    tempMAC[i] = strtoul(macStr.substring(i * 3, i * 3 + 2).c_str(), NULL, 16);
  }
  defaultMacAP.set(tempMAC);
  if (!defaultMacAP.valid()) defaultMacAP.randomize();
}

void Settings::syncMacInterface() {
  if (debug) Serial.println("Trying to sync the MAC addr with settings");
  if (isSettingsLoaded) {
    Mac macToSync;
    if (isMacAPRand) {
      macToSync.randomize();
      esp_wifi_set_mac((wifi_interface_t)WIFI_IF_AP, macToSync._get());
      if (debug) Serial.println("Synced with a random mac addr : " + macToSync.toString());
    } else if (macAP.valid()) {
      macToSync = macAP;
      esp_wifi_set_mac((wifi_interface_t)WIFI_IF_AP, macToSync._get());
      if (debug) Serial.println("Synced with saved mac addr : " + macToSync.toString());
    } else {
      if (debug) Serial.println("Could not sync because of invalid settings!");
    }
  } else {
    if (debug) Serial.println("Could not sync because settings are not loaded!");
  }
}

void Settings::setLedPin(int newLedPin) {
  prevLedPin = ledPin;
  if (newLedPin > 0 && newLedPin != prevLedPin) {
    ledPin = newLedPin;
    pinMode(ledPin, OUTPUT);
    if (prevLedPin != 0) {
      digitalWrite(ledPin, digitalRead(prevLedPin));
      digitalWrite(prevLedPin, pinStateOff);
      pinMode(prevLedPin, INPUT);
    } else {
      digitalWrite(ledPin, pinStateOff);
    }
  }
}

void Settings::load() {
  settingsPrefs.begin("settings", false);

  if (!settingsPrefs.getBool("valid", false)) {
    settingsPrefs.end();
    reset();
    return;
  }

  ssid = settingsPrefs.getString("ssid", "HACKER");
  ssidLen = ssid.length();
  password = settingsPrefs.getString("pass", "woshiheike");
  passwordLen = password.length();

  if (ssidLen < 1 || ssidLen > 32 || (passwordLen < 8 && passwordLen != 0) || passwordLen > 32) {
    settingsPrefs.end();
    reset();
    return;
  }

  ssidHidden = settingsPrefs.getBool("ssidHidden", false);
  apChannel = settingsPrefs.getUChar("apChannel", 9);
  if (apChannel < 1 || apChannel > 14) apChannel = 1;

  apScanHidden = settingsPrefs.getBool("apScanHidden", true);
  deauthReason = settingsPrefs.getUChar("deauthReas", 0x01);
  attackTimeout = settingsPrefs.getUShort("attkTimeout", 300);
  attackPacketRate = settingsPrefs.getUChar("pktRate", 10);
  clientScanTime = settingsPrefs.getUChar("scanTime", 15);
  useLed = settingsPrefs.getBool("useLed", true);
  channelHop = settingsPrefs.getBool("channelHop", false);
  multiAPs = settingsPrefs.getBool("multiAPs", false);
  multiAttacks = settingsPrefs.getBool("multiAtt", false);
  macInterval = settingsPrefs.getUShort("macIval", 4);
  beaconInterval = settingsPrefs.getBool("bcnIval", false);

  // Load MAC AP
  uint8_t macBytes[6];
  size_t macLen = settingsPrefs.getBytesLength("macAP");
  if (macLen == 6) {
    settingsPrefs.getBytes("macAP", macBytes, 6);
    for (int i = 0; i < 6; i++) macAP.setAt(macBytes[i], i);
  }
  if (!macAP.valid()) macAP.set(defaultMacAP);
  isMacAPRand = settingsPrefs.getBool("isMacAPRand", false);

  setLedPin(settingsPrefs.getUChar("ledPin", 2));

  settingsPrefs.end();
  isSettingsLoaded = true;
}

void Settings::reset() {
  if (debug) Serial.print("reset settings...");

  ssid = "HACKER";
  password = "woshiheike";
  ssidHidden = false;
  apChannel = 9;

  ssidLen = ssid.length();
  passwordLen = password.length();
  macAP = defaultMacAP;
  isMacAPRand = false;

  apScanHidden = true;

  deauthReason = 0x01;
  attackTimeout = 5 * 60;
  attackPacketRate = 10;
  clientScanTime = 15;
  useLed = true;
  channelHop = false;
  multiAPs = false;
  multiAttacks = false;
  macInterval = 4;
  beaconInterval = false;
  ledPin = 2;

  if (debug) Serial.println(" done");

  save();
}

void Settings::save() {
  ssidLen = ssid.length();
  passwordLen = password.length();

  settingsPrefs.begin("settings", false);
  settingsPrefs.clear();

  settingsPrefs.putBool("valid", true);
  settingsPrefs.putString("ssid", ssid);
  settingsPrefs.putString("pass", password);
  settingsPrefs.putBool("ssidHidden", ssidHidden);
  settingsPrefs.putUChar("apChannel", (uint8_t)apChannel);
  settingsPrefs.putBool("apScanHidden", apScanHidden);
  settingsPrefs.putUChar("deauthReas", deauthReason);
  settingsPrefs.putUShort("attkTimeout", (uint16_t)attackTimeout);
  settingsPrefs.putUChar("pktRate", (uint8_t)attackPacketRate);
  settingsPrefs.putUChar("scanTime", (uint8_t)clientScanTime);
  settingsPrefs.putBool("useLed", useLed);
  settingsPrefs.putBool("channelHop", channelHop);
  settingsPrefs.putBool("multiAPs", multiAPs);
  settingsPrefs.putBool("multiAtt", multiAttacks);
  settingsPrefs.putUShort("macIval", (uint16_t)macInterval);
  settingsPrefs.putBool("bcnIval", beaconInterval);

  // Save MAC AP
  uint8_t macBytes[6];
  for (int i = 0; i < 6; i++) macBytes[i] = macAP._get(i);
  settingsPrefs.putBytes("macAP", macBytes, 6);
  settingsPrefs.putBool("isMacAPRand", isMacAPRand);

  settingsPrefs.putUChar("ledPin", (uint8_t)ledPin);

  settingsPrefs.end();

  if (debug) {
    info();
    Serial.println("settings saved to NVS");
  }
}

void Settings::info() {
  Serial.println("Settings:");
  Serial.println("SSID: " + ssid);
  Serial.println("SSID length: " + (String)ssidLen);
  Serial.println("SSID hidden: " + (String)ssidHidden);
  Serial.println("password: " + password);
  Serial.println("password length: " + (String)passwordLen);
  Serial.println("channel: " + (String)apChannel);
  Serial.println("Default MAC AP: " + defaultMacAP.toString());
  Serial.println("Saved MAC AP: " + macAP.toString());
  Serial.println("MAC AP random: " + (String)isMacAPRand);
  Serial.println("Scan hidden APs: " + (String)apScanHidden);
  Serial.println("deauth reason: " + (String)(int)deauthReason);
  Serial.println("attack timeout: " + (String)attackTimeout);
  Serial.println("attack packet rate: " + (String)attackPacketRate);
  Serial.println("client scan time: " + (String)clientScanTime);
  Serial.println("use built-in LED: " + (String)useLed);
  Serial.println("channel hopping: " + (String)channelHop);
  Serial.println("multiple APs: " + (String)multiAPs);
  Serial.println("multiple Attacks: " + (String)multiAttacks);
  Serial.println("mac change interval: " + (String)macInterval);
  Serial.println("1s beacon interval: " + (String)beaconInterval);
  Serial.println("LED Pin: " + (String)ledPin);
}

size_t Settings::getSize() {
  String json = "{";
  size_t jsonSize = 0;

  json += "\"ssid\":\"" + ssid + "\",";
  json += "\"ssidHidden\":" + (String)ssidHidden + ",";
  json += "\"password\":\"" + password + "\",";
  json += "\"apChannel\":" + (String)apChannel + ",";
  json += "\"macAp\":\"" + macAP.toString() + "\",";
  json += "\"randMacAp\":" + (String)isMacAPRand + ",";
  json += "\"apScanHidden\":" + (String)apScanHidden + ",";
  json += "\"deauthReason\":" + (String)(int)deauthReason + ",";
  json += "\"attackTimeout\":" + (String)attackTimeout + ",";
  json += "\"attackPacketRate\":" + (String)attackPacketRate + ",";
  json += "\"clientScanTime\":" + (String)clientScanTime + ",";
  json += "\"useLed\":" + (String)useLed + ",";
  json += "\"channelHop\":" + (String)channelHop + ",";
  json += "\"multiAPs\":" + (String)multiAPs + ",";
  json += "\"multiAttacks\":" + (String)multiAttacks + ",";
  json += "\"macInterval\":" + (String)macInterval + ",";
  json += "\"beaconInterval\":" + (String)beaconInterval + ",";
  json += "\"ledPin\":" + (String)ledPin + "}";
  jsonSize += json.length();

  return jsonSize;
}

void Settings::send() {
  if (debug) Serial.println("getting settings json");
  sendHeader(200, "text/json", getSize());

  String json = "{";
  json += "\"ssid\":\"" + ssid + "\",";
  json += "\"ssidHidden\":" + (String)ssidHidden + ",";
  json += "\"password\":\"" + password + "\",";
  json += "\"apChannel\":" + (String)apChannel + ",";
  json += "\"macAp\":\"" + macAP.toString() + "\",";
  json += "\"randMacAp\":" + (String)isMacAPRand + ",";
  json += "\"apScanHidden\":" + (String)apScanHidden + ",";
  json += "\"deauthReason\":" + (String)(int)deauthReason + ",";
  json += "\"attackTimeout\":" + (String)attackTimeout + ",";
  json += "\"attackPacketRate\":" + (String)attackPacketRate + ",";
  json += "\"clientScanTime\":" + (String)clientScanTime + ",";
  json += "\"useLed\":" + (String)useLed + ",";
  json += "\"channelHop\":" + (String)channelHop + ",";
  json += "\"multiAPs\":" + (String)multiAPs + ",";
  json += "\"multiAttacks\":" + (String)multiAttacks + ",";
  json += "\"macInterval\":" + (String)macInterval + ",";
  json += "\"beaconInterval\":" + (String)beaconInterval + ",";
  json += "\"ledPin\":" + (String)ledPin + "}";
  sendToBuffer(json);
  sendBuffer();

  if (debug) Serial.println("\ndone");
}
