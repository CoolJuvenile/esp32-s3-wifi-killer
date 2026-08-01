#include "NameList.h"

static Preferences namePrefs;

NameList::NameList() {
  // NVS has much more space than EEPROM, no overflow concern
}

void NameList::load() {
  namePrefs.begin("namelist", false);
  len = namePrefs.getUChar("len", 0);
  if (len < 0 || len > listLength) {
    len = 0;
    namePrefs.end();
    NameList::clear();
    namePrefs.begin("namelist", false);
  }

  Mac _client;
  for (int i = 0; i < len; i++) {
    char key[20];
    // Read MAC
    snprintf(key, sizeof(key), "mac%d", i);
    uint8_t macBytes[6];
    size_t macLen = namePrefs.getBytesLength(key);
    if (macLen == 6) {
      namePrefs.getBytes(key, macBytes, 6);
      for (int h = 0; h < 6; h++) _client.setAt(macBytes[h], h);
    }
    // Read name
    snprintf(key, sizeof(key), "name%d", i);
    size_t nameLen = namePrefs.getBytesLength(key);
    if (nameLen > 0) {
      uint8_t nameBuf[nameLength];
      memset(nameBuf, 0, nameLength);
      namePrefs.getBytes(key, nameBuf, min((size_t)nameLength, nameLen));
      for (int h = 0; h < nameLength; h++) names[i][h] = nameBuf[h];
    }
    clients.add(_client);
  }
  namePrefs.end();
}

void NameList::clear() {
  namePrefs.begin("namelist", false);
  namePrefs.clear();
  namePrefs.end();
  len = 0;
  clients._clear();
  Serial.println("nameList cleared");
}

void NameList::save() {
  namePrefs.begin("namelist", false);
  namePrefs.clear();  // Clear all keys before rewriting
  namePrefs.putUChar("len", (uint8_t)len);

  for (int i = 0; i < len; i++) {
    char key[20];
    // Save MAC
    snprintf(key, sizeof(key), "mac%d", i);
    uint8_t macBytes[6];
    for (int h = 0; h < 6; h++) macBytes[h] = clients._get(i)._get(h);
    namePrefs.putBytes(key, macBytes, 6);
    // Save name
    snprintf(key, sizeof(key), "name%d", i);
    namePrefs.putBytes(key, names[i], nameLength);
  }
  namePrefs.end();
}

void NameList::add(Mac client, String name) {
  if (clients.add(client) >= 0) len++;
  else if (clients.getNum(client) < 0) Serial.println("WARNING: name list is full!");

  uint8_t _buf[nameLength];
  memset(_buf, 0, nameLength);
  name.getBytes(_buf, nameLength);
  int idx = clients.getNum(client);
  if (idx >= 0) {
    for (int i = 0; i < nameLength; i++) {
      if (i < name.length()) names[idx][i] = _buf[i];
      else names[idx][i] = 0x00;
    }
  }
  NameList::save();
}

String NameList::get(int num) {
  String returnStr;
  if (num >= 0 && num < len) {
    for (int h = 0; h < nameLength; h++) {
      if (names[num][h] != 0x00) returnStr += (char)names[num][h];
    }
    returnStr.trim();
    return returnStr;
  }
  return "";
}

Mac NameList::getMac(int num) {
  return clients._get(num);
}

String NameList::getByMac(Mac client) {
  String returnStr;
  int clientNum = clients.getNum(client);
  if (clientNum > -1) {
    for (int h = 0; h < nameLength; h++) {
      if (names[clientNum][h] != 0x00) returnStr += (char)names[clientNum][h];
    }
  }
  return returnStr;
}

int NameList::getNumByMac(Mac client) {
  return clients.getNum(client);
}

void NameList::remove(int num) {
  if (num < 0 || num >= len) return;
  for (int i = num; i < len - 1; i++) {
    clients.set(i, clients._get(i + 1));
    for (int h = 0; h < nameLength; h++) names[i][h] = names[i + 1][h];
  }
  clients.remove(clients._get(len - 1));
  clients.num--;
  len--;
  save();
}

void NameList::edit(int num, String name) {
  if (num < 0 || num >= len) return;
  for (int i = 0; i < nameLength; i++) {
    if (i < name.length()) names[num][i] = name[i];
    else names[num][i] = 0x00;
  }
  save();
}
