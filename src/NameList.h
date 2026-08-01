#ifndef NameList_h
#define NameList_h

#include <Preferences.h>
#include "Mac.h"
#include "MacList.h"

#define listLength 50
#define nameLength 18

/*
  The NameList holds and saves all your custom device names in NVS (Preferences).
  ESP32-S3 port: Replaced EEPROM with Preferences API.
*/
extern const bool debug;

class NameList
{
  public:
    NameList();
    void load();
    void clear();
    void add(Mac client, String name);
    void edit(int num, String name);
    String get(int num);
    String getByMac(Mac client);
    int getNumByMac(Mac client);
    Mac getMac(int num);
    void remove(int num);
    int len = 0;
  private:
    void save();

    MacList clients;
    uint8_t names[listLength][nameLength];
};

#endif
