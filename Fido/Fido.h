//Author: Louis Thiery

//COPYRIGHT
//This work is released under Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
//For details, visit: http://creativecommons.org/licenses/by-sa/3.0/

//Please visit .cpp file for description

#ifndef Fido_h
#define Fido_h

#include "Arduino.h"
#include "EEPROM.h"

#define LENGTH_OF_NUM 11

class Fido {
 public:
  Fido();
  char num[LENGTH_OF_NUM+1];
  void start(void);
  void debug(void);
  bool ifAlarm(void);
  void setIfAlarm(bool IF);
  bool ifUpdates(void);
  void setIfUpdates(bool IF);
  bool ifDailySummary(void);
  void setIfDailySummary(bool IF);
  bool ifPrivate(void);
  void setIfPrivate(bool IF);
  void setFreq(int num);
  uint8_t getFreq(void);
  void setUB(int num);
  uint8_t getUB(void);
  void setLB(int num);
  int8_t getLB(void);
  void setUpdateFreq(int num);
  uint8_t getUpdateFreq(void);
  void setUpdateBegin(int num);
  uint8_t getUpdateBegin(void);
  void setUpdateEnd(int num);
  uint8_t getUpdateEnd(void);
  void setSummaryTime(int num);
  uint8_t getSummaryTime(void);

  void setOwner(const char c[]);
  void getOwner(void);
  void clearUsers(void);
  void addUser(uint8_t ug,const char c[]);
  uint8_t getUserGroup(uint8_t user);
  void getUserContact(uint8_t user);
  void removeUser(uint8_t user);
  void removeUser(const char c[]);
  uint8_t findUser(const char c[]);
  void setPin(bool two,const char c[]);
  bool isPin(bool two,const char c[]);

 private:
  byte _uFlag[2];

  void _getFlags(void);
  void _getNumberLocations(void);
  void _setBool(int bit, bool IF);
  uint8_t _readInt(int location);
  void _writeInt(int location,uint8_t num);
  void _writeStr(int location, const char c[], uint8_t length);
  void _getStr(int location, int length);
  void _getNum(int location);
  int _userLoc(uint8_t user);
};
  
#endif
