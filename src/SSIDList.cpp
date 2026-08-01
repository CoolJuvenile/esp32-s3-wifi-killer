#include "SSIDList.h"

static Preferences ssidPrefs;

SSIDList::SSIDList() {
  // NVS has plenty of space for SSID list
}

void SSIDList::load() {
  ssidPrefs.begin("ssidlist", false);
  len = ssidPrefs.getUChar("len", 0);
  if (len < 0 || len > SSIDListLength) {
    clear();
    save();
  }
  for (int i = 0; i < len; i++) {
    char key[16];
    snprintf(key, sizeof(key), "ssid%d", i);
    String saved = ssidPrefs.getString(key, "");
    for (int h = 0; h < SSIDLength; h++) {
      if (h < saved.length()) names[i][h] = saved[h];
      else names[i][h] = 0x00;
    }
    snprintf(key, sizeof(key), "enc%d", i);
    encrypted[i] = ssidPrefs.getBool(key, false);
  }
  ssidPrefs.end();
}

void SSIDList::clear() {
  len = 0;
}

void SSIDList::add(String name, bool enc) {
  if (len < SSIDListLength) {
    for (int i = 0; i < SSIDLength; i++) {
      if (i < name.length()) names[len][i] = name[i];
      else names[len][i] = 0x00;
    }
    encrypted[len] = enc;
    len++;
  }
}

void SSIDList::addClone(String name, int num, bool enc) {
  int _restSSIDLen = SSIDLength - name.length();
  String _apName;

  for (int c = 0; c < SSIDListLength && c < num; c++) {
    _apName = name;

    if (c < _restSSIDLen) {
      for (int d = 0; d < _restSSIDLen - c; d++) _apName += " ";
    } else if (c < _restSSIDLen * 2) {
      _apName = "​" + _apName;
      for (int d = 0; d < (_restSSIDLen - 3) - c / 2; d++) _apName += " ";
    } else if (c < _restSSIDLen * 3) {
      _apName = "." + _apName;
      for (int d = 0; d < (_restSSIDLen - 1) - c / 3; d++) _apName += " ";
    } else {
      for (int d = 0; d < _restSSIDLen - 2; d++) _apName += " ";
      _apName += (String)c;
    }
    add(_apName, enc);
  }
}

String SSIDList::get(int num) {
  String _name = "";
  for (int i = 0; i < SSIDLength; i++) {
    if (names[num][i] != 0x00) _name += names[num][i];
  }
  return _name;
}

bool SSIDList::isEncrypted(int num) {
  return encrypted[num];
}

void SSIDList::remove(int num) {
  if (num >= 0 && num < len) {
    for (int i = num; i < len - 1; i++) {
      for (int h = 0; h < SSIDLength; h++) {
        names[i][h] = names[i + 1][h];
      }
      encrypted[i] = encrypted[i + 1];
    }
    len--;
  }
}

void SSIDList::save() {
  if (debug) Serial.print("saving SSIDList...");
  ssidPrefs.begin("ssidlist", false);
  ssidPrefs.clear();
  ssidPrefs.putUChar("len", (uint8_t)len);
  for (int i = 0; i < len; i++) {
    char key[16];
    String ssidStr = get(i);
    snprintf(key, sizeof(key), "ssid%d", i);
    ssidPrefs.putString(key, ssidStr);
    snprintf(key, sizeof(key), "enc%d", i);
    ssidPrefs.putBool(key, encrypted[i]);
  }
  ssidPrefs.end();
  if (debug) Serial.println("done");
}

void SSIDList::_random() {
  String _rName;
  for (int i = len; i < SSIDListLength; i++) {
    _rName = "";
    for (int h = 0; h < SSIDLength; h++) _rName += letters[random(0, (int)sizeof(letters))];
    add(_rName, random(2) > 0.5);
  }
}
