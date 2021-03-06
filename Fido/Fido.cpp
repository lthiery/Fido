/*
Author: Louis Thiery

COPYRIGHT
This work is released under Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
For details, visit: http://creativecommons.org/licenses/by-sa/3.0/

DESCRIPTION
This is a library written to help manage the EEPROM interactions with the Arduino's AVR chip
Handles booleans, ints, and strings. Strings were chosen for phone numbers since they can be of any relevant length.

All numbers are authorized to text for updates. Any number can text for update if option is chosen.

Use constructor to create class variables, run start() in setup to get variables from EEPROM, 
Use any of the get functions during the loop to get data from the class variables (not EEPROM) 
Any of the set methods will rewrite the EEPROM memory and refresh the class variable
*/

#include "Arduino.h"
#include "EEPROM.h"
#include "Fido.h"

#define FLAG_0 0 //first byte of EEPROM contains a set of 8 booleans
//on FLAG_0
#define ALARM 0 //first bit is if alarm is active
#define UPDATES 1 //second bit is if want updates throughout day
#define DAILY_SUM 2 //third bit is if want daily updates
#define IF_PRIVATE 3 //fourth bit is if want to allow Fido to answer any update request
//#define IF_FAHRENHEIT 4//fifth bit is if fahrenheit or not

//location of ints (16 bit) given by location of first byte (second byte is +1) 
#define FREQ 1 //in minutes (1-60)
#define UB 2 //in Fahrenheit (take range from temp sensor)
#define LB 3 //in Fahrenheit
#define UPDATE_FREQ 4 //in minutes (1-120)
#define UPDATE_BEG 5 //in hours (1-24
#define UPDATE_END 6 //in hours (1-24)
#define DAILY_SUM_TIME 7 //in hours (1-24)

#define PIN1 10 //4 char string plus #
#define PIN2 15
//there's room from 23-100
//110+16*13 is where the end of user info is

//4 bytes serving as flags to tell if there is a number somewhere
#define UFLAG 100 //will take up 2 spots
#define LENGTH_OF_USER 14 
//location of number strings, appended by their usergroup
//9=owner,0-2 levels of alarms
#define NUMBERS_LOC 110 //takes X+1 bytes, X numbers in char form appended with # 

//CONSTRUCTOR
Fido::Fido(){
  char num[LENGTH_OF_NUM];
  byte _uFlag[2];
}

void Fido::start(void){
  for(int i=0;i<2;i++) _uFlag[i]=EEPROM.read(UFLAG+i);
  getOwner();
}
//PUBLIC METHODS

//USER SYSTEM RELATED
void Fido::setOwner(const char c[]){
  EEPROM.write(NUMBERS_LOC,9);
  _writeStr(NUMBERS_LOC+1,c,LENGTH_OF_NUM);
  bitSet(_uFlag[0],0); //update bit in prog mem
  EEPROM.write(UFLAG,_uFlag[0]); //update byte in EEPROM
  getOwner();
}

void Fido::getOwner(void){
  if (bitRead(EEPROM.read(UFLAG),0)) _getNum(NUMBERS_LOC+1);//owner will always be first user
  else for(uint8_t i=0;i<LENGTH_OF_NUM;i++) num[i]='0';
}


uint8_t Fido::getUserGroup(uint8_t user){
  return EEPROM.read(_userLoc(user));
}

void Fido::getUserContact(uint8_t user){
  _getNum(_userLoc(user)+1);
}

void Fido::clearUsers(){
  //clear all user flags in progmem
  for(int i=0;i<2;i++){
    _uFlag[i]=0;
    if(i==0) bitSet(_uFlag[i],0);//keep owner
  }
  //write this to EEPROM;
  for(int i=0;i<2;i++){
    EEPROM.write(UFLAG+i,_uFlag[i]);
  }
}


void Fido::addUser(uint8_t ug,const char c[]){
  //look for first open spot
  for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(!bitRead(_uFlag[i],j)){
	EEPROM.write(_userLoc(j+i*8),ug); //write UG to first spot
	_writeStr(_userLoc(j+i*8)+1,c,LENGTH_OF_NUM+1); //follow 11 spots hold number
	bitSet(_uFlag[i],j); //update bit in prog mem
	EEPROM.write(UFLAG+i,_uFlag[i]); //update byte in EEPROM
	return;
      }
  Serial.println("No more room for users!");
}

void Fido::removeUser(uint8_t user){
  if(user==0) return; //don't remove owner or non-user
  bitClear(_uFlag[user/8],user%8);
  EEPROM.write(UFLAG+user/8,_uFlag[user/8]);
}

void Fido::removeUser(const char c[]){
  uint8_t userNum = findUser(c);
  if(userNum==255) return;
  else removeUser(userNum);
}

uint8_t Fido::findUser(const char c[]){
  for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(_uFlag[i],j)){
	getUserContact(i*8+j);
	if(strncmp(c,num,LENGTH_OF_NUM)==0) return i*8+j;
      } 
  return 255;
}  

void Fido::setPin(bool two,const char c[]){
  if(two) _writeStr(PIN2,c,4);
  else _writeStr(PIN1,c,4);
}

bool Fido::isPin(bool two,const char c[]){
  char pin[4];
  for(int i=0;i<4;i++){
    if(two) pin[i]=EEPROM.read(PIN2+i);
    else pin[i]=EEPROM.read(PIN1+i);
  }
  return (strncmp(pin,c,4)==0);
}

//STANDARD MEMORY RELATED
//bool related
bool Fido::ifAlarm(void){
  return bitRead(EEPROM.read(FLAG_0),ALARM);
}
void Fido::setIfAlarm(bool IF){
  _setBool(ALARM,IF);
}
bool Fido::ifUpdates(){
  return bitRead(EEPROM.read(FLAG_0),UPDATES);
}
void Fido::setIfUpdates(bool IF){
  _setBool(UPDATES,IF);
}
bool Fido::ifDailySummary(void){
  return bitRead(EEPROM.read(FLAG_0),DAILY_SUM);
}
void Fido::setIfDailySummary(bool IF){
  _setBool(DAILY_SUM,IF);
}
bool Fido::ifPrivate(void){
  return bitRead(EEPROM.read(FLAG_0),IF_PRIVATE);
}
void Fido::setIfPrivate(bool IF){
  _setBool(IF_PRIVATE,IF);
}

//int related
void Fido::setFreq(int num){
  _writeInt(FREQ,num);
}
uint8_t Fido::getFreq(void){
  return _readInt(FREQ);
}
void Fido::setUB(int num){
  _writeInt(UB, num);
}
uint8_t Fido::getUB(void){
  return _readInt(UB);
}
void Fido::setLB(int num){
  _writeInt(LB, num);
}
int8_t Fido::getLB(void){
  return _readInt(LB);
}
void Fido::setUpdateFreq(int num){
  _writeInt(UPDATE_FREQ, num);
}
uint8_t Fido::getUpdateFreq(void){
  return _readInt(UPDATE_FREQ);
}
void Fido::setUpdateBegin(int num){
  _writeInt(UPDATE_BEG,num);
}
uint8_t Fido::getUpdateBegin(void){
  return _readInt(UPDATE_BEG);
}
void Fido::setUpdateEnd(int num){
  _writeInt(UPDATE_END, num);
}
uint8_t Fido::getUpdateEnd(void){
  return _readInt(UPDATE_END);
}
void Fido::setSummaryTime(int num){
  _writeInt(DAILY_SUM_TIME,num);
}
uint8_t Fido::getSummaryTime(void){
  return _readInt(DAILY_SUM_TIME);
}

//PRIVATE METHODS
void Fido::_setBool(int bit, bool IF){
  byte temp = EEPROM.read(FLAG_0);
  if(IF){bitSet(temp,bit);}
  else{bitClear(temp,bit);}
  EEPROM.write(FLAG_0, temp);
}
uint8_t Fido::_readInt(int location){
  return EEPROM.read(location);
}

void Fido::_writeInt(int location, uint8_t num){
  EEPROM.write(location, num);
}

void Fido::_writeStr(int location,const char c[], uint8_t length){
  for(uint8_t i=0;i<length;i++){
    EEPROM.write(location+i, c[i]);
  }
}


void Fido::_getStr(int location, int length){
  for(uint8_t i=0;i<length;i++){
    num[i]=EEPROM.read(location+i);
  }
}

void Fido::_getNum(int location){
  _getStr(location,LENGTH_OF_NUM);
}

int Fido::_userLoc(uint8_t user){
  return NUMBERS_LOC+user*13;
}
