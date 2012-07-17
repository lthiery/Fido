#include <avr/pgmspace.h> //to directly access PROGMEM
#include <EEPROM.h>//EEPROM RELATED
#include <Fido.h>
#include <SoftwareSerial.h>//CELLPHONE RELATED
#include <GSMSerial.h>
#include <SD.h>//DATALOGGING SHIELD RELATED
#include <Wire.h>
#include <RTClib.h>

//pins where things should be
#define RED 3
#define GREEN 4
#define PHONE_RX 5//red
#define PHONE_TX 6//yellow
#define THERMISTORPIN A0 

//compiler variables
#define LENGTH_OF_NUM 10
#define FAHRENHEIT 1
#define MAX_PROGMEM_STR 32
//for temp
#define THERMISTORNOMINAL 10000 // resistance at 25 degrees C   
#define TEMPERATURENOMINAL 25 // temp. for nominal resistance
#define NUMSAMPLES 5// how many samples to take and average
#define BCOEFFICIENT 3950// The beta coefficient of the thermistor 
#define SERIESRESISTOR 10000 // the value of the 'other' resistor 

//freeing up some RAM
prog_char msg_1[] PROGMEM = "You are Fido's new owner!";
prog_char msg_2[] PROGMEM = "You are now friends with Fido!"; //31+1
prog_char msg_3[] PROGMEM = "Invalid msg from";
prog_char msg_4[] PROGMEM = "Current temp is ";
prog_char msg_5[] PROGMEM = "Update - current temp is ";
prog_char msg_6[] PROGMEM = "Warning - current temp is ";
prog_char msg_7[] PROGMEM = "Running on back-up battery!";

prog_char sum_0[] PROGMEM = "SUMMARY: avg = ";
prog_char sum_1[] PROGMEM = ", hi = ";
prog_char sum_2[] PROGMEM = ", lo = ";

prog_char pref_0[] PROGMEM ="FREQ: "; 
prog_char pref_1[] PROGMEM ="m, ALARM:";
prog_char pref_2[] PROGMEM =", lb: ";
prog_char pref_3[] PROGMEM =", ub: ";
prog_char pref_4[] PROGMEM =", UPDATES: ";
prog_char pref_5[] PROGMEM =", freq: ";
prog_char pref_6[] PROGMEM ="m, begin: ";
prog_char pref_7[] PROGMEM ="h, end: ";
prog_char pref_8[] PROGMEM ="h, SUM:";
prog_char pref_9[] PROGMEM ="@";

PROGMEM const char *prefs_table[]=
{   
  pref_0,pref_1,pref_2,pref_3,pref_4,pref_5,pref_6,pref_7,pref_8,pref_9,
};


#define CHIP_SELECT 10
File logfile; //logging file
RTC_DS1307 RTC; //clock object
Fido fido;
GSMSerial phone(5,6); //5 red, 6 yellow


//stamps: 0=minute of last measure
//        1=hour*60+minute of last update (eg: 8am=8*60=480)
//        2=day of last summary
int stamp[3];
//flags:  0=sent temp alarm
//        1=sent battery alarm
//        2=LED blink
byte flags=0;
DateTime now;
int samples[NUMSAMPLES];
float avg=0; int samplesNum=0;
double high; double low;

void setup(){
  Serial.begin(9600);
  Serial.println("start up");
  firstConfiguration(); //will only execute first time
  //defaultFido(); //manual reset - will "erase" Fido on start up
  Wire.begin();
  fido.start();
  RTC.begin();
  initializeStampsAndFile();
  pinMode(GREEN,OUTPUT);
  pinMode(RED,OUTPUT);
  phone.start();
  analogReference(EXTERNAL);
}

void loop(){
  //blink LED
  if(bitRead(flags,2)==0){digitalWrite(GREEN,HIGH);bitSet(flags,2);}
  else {digitalWrite(GREEN,LOW);bitClear(flags,2);}
  
  fido.getOwner(); //just to make sure fido.num is owner's num
  now = RTC.now(); double temp = getTemp(); //update loop variables

  //first check for texts
  if(phone.hasTxt()){
    digitalWrite(RED,HIGH);
    manageTxt(temp);
    phone.deleteTxts();
    digitalWrite(RED,LOW);
  }

  //now do a normal routine
  //if freq has elapsed since last log, log temp
  if(now.minute()-stamp[0]>=fido.getFreq() ||now.minute()-stamp[0]<0 || samples==0 ){
     Serial.println("logging");
     stamp[0]=now.minute();
     char date[11];char time[10];
     refreshDate(now,date,time);
     
    // if (logfile){
       logfile.print(date);
       logfile.print(',');
       logfile.print(time);
       logfile.print(',');
       logfile.println(temp);
       logfile.flush();
   //  }
   //  else Serial.println("SD card error");
     

     
     if(temp>high || samplesNum==0) high=temp;
     if(temp<low || samplesNum==0) low=temp;
     if(samplesNum==0) avg = temp;
     else avg=(avg*samplesNum+temp)/(samplesNum+1);
     samplesNum++;
     Serial.print(date);
     Serial.print(", ");
     Serial.print(time);
     Serial.print(": ");
     Serial.println(temp);     
  }
  
  //check if outside temp alarm bounds
  if(fido.ifAlarm() && bitRead(flags,0)==0 && (temp>fido.getUB() || temp<fido.getLB())){
    
    
    //send to all users!
    for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(EEPROM.read(100+i),j)){
      if(fido.getUserGroup(i*4+j)>=0){
        fido.getUserContact(i*4+j);
        
        strcpy_P(phone.buffer, msg_6);
        phone.openTxt(fido.num);
        phone.inTxt(phone.buffer);
        phone.inTxt(temp);
        phone.closeTxt();
        Serial.print("Alarm sent to: ");
        Serial.println(fido.num);
      }
    }   
    bitSet(flags,0); //flag keeps warning from being sent several times
  } //reset alarm when back within bounds +/- 2%
  else if(temp<0.98*fido.getUB() && temp>1.02*fido.getLB()) bitClear(flags,0);
  
  //if updates enabled
  if(fido.ifUpdates()){ //see when last update happened
    if(now.hour()>=fido.getUpdateBegin() && now.hour()<fido.getUpdateEnd()){
      if(now.hour()*60+now.minute()-stamp[1]>=fido.getUpdateFreq()
      || now.hour()*60+now.minute()-stamp[1]<0){  
        
        stamp[1]=now.hour()*60+now.minute();
        
        for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(EEPROM.read(100+i),j)){
          if(fido.getUserGroup(i*4+j)>=1){
            fido.getUserContact(i*4+j);
            strcpy_P(phone.buffer, msg_5);
            phone.openTxt(fido.num);
            phone.inTxt(phone.buffer);
            phone.inTxt(temp);
            phone.inTxt("F");
            phone.closeTxt(); 
            Serial.print("Update sent to: ");
            Serial.println(fido.num);
          }
        }
      }
    }
  }
  
  //if daily summary enabled
  if(fido.ifDailySummary()){ //check when last summary was sent
    if(now.hour()>=fido.getSummaryTime() && stamp[2]!=now.day()){
      stamp[2]=now.day();
      
      for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(EEPROM.read(100+i),j)){
        if(fido.getUserGroup(i*4+j)>=2){
          fido.getUserContact(i*4+j);
          strcpy_P(phone.buffer, msg_5);
          
          phone.openTxt(fido.num); strcpy_P(phone.buffer, sum_0);
          phone.inTxt(phone.buffer); strcpy_P(phone.buffer, sum_1);
          phone.inTxt(avg);
          phone.inTxt(phone.buffer); strcpy_P(phone.buffer, sum_2);
          phone.inTxt(high);
          phone.inTxt(phone.buffer);
          phone.inTxt(low);
          phone.closeTxt();
          Serial.print("Summary sent to: ");
          Serial.println(fido.num);
        }
      }
      

      samplesNum=0;
    }
    
  }
  if(!phone.isCharging() && bitRead(flags,1)==0){
    for(int i=0;i<2;i++) for(int j=0;j<8;j++) if(bitRead(EEPROM.read(100+i),j)){
      if(fido.getUserGroup(i*4+j)>=2){
        fido.getUserContact(i*4+j);
        strcpy_P(phone.buffer, msg_5);
        strcpy_P(phone.buffer, msg_7);
        phone.sendTxt(fido.num, phone.buffer);
        Serial.println("Power warning sent to: ");
        Serial.println(fido.num);
      }
    }

    bitSet(flags,1);//set flag
  }
  else if (phone.isCharging()) bitClear(flags,1);//reset flag
}

void firstConfiguration(){
  RTC.adjust(DateTime(__DATE__,__TIME__));
  if(EEPROM.read(99)==1) return;
  defaultFido();
  EEPROM.write(99,1);
}

void manageTxt(double temp){
  //the next line is to make sure we've actually received a text
  //checks the integrity of phone.origin  
  if(phone.content[1]=='\0') return;
  for(int i=0;i<LENGTH_OF_NUM;i++) if(phone.origin[i]=='\0') return;
  Serial.print("New text from: ");
  Serial.println(phone.origin);
  Serial.println(phone.content);
    
  //if it starts with # then we're doing something important 
  if(phone.content[0]=='#'){ 
    char pin[5]; 
    for(int i=0;i<5;i++) pin[i]=phone.content[i+1];
    
    if(fido.isPin(0,pin)) { //if they've given master pin
      if(strncmp(fido.num, phone.origin, LENGTH_OF_NUM)!=0){ //make them owner if not already so
        if(fido.findUser(phone.origin)!=255) fido.removeUser(fido.findUser(phone.origin)); //remove them if they are normal user
        fido.setOwner(phone.origin);
        strcpy_P(phone.buffer, msg_1);
        phone.sendTxt(phone.origin, phone.buffer);
        Serial.print("New owner: ");
        Serial.println(phone.origin);
      }
      //need new protocol to set new pin
      /*
      if(pin[5]==','){     //if they've included a comma then the rest of text should be pin
        for(int i=0;i<5;i++) {
          pin[i]=phone.content[i+5];
          if (pin[i]=='\0') return;
        }
        fido.setPin(0,pin);
        Serial.print("new pin");
        Serial.println(pin);
      }*/
    }
    
    else if(fido.isPin(1,pin) && fido.findUser(phone.origin)!=9){  //if they've given sub pin, make them user (unless they're owner!)
      if(fido.findUser(phone.origin)!=255) fido.removeUser(fido.findUser(phone.origin)); //delete them if they already exist
      strcpy_P(phone.buffer, msg_2);
      fido.addUser(atoi(phone.content[6]),phone.origin);
      phone.sendTxt(phone.origin, phone.buffer);
    }
    
    else if(strncmp(fido.num, phone.origin, LENGTH_OF_NUM)==0){      //if they're the owner, they want to program Fido

      for(uint8_t i=0;i<11*2+1;i++) if (phone.content[i]=='\0') return; //can't be valid program if terminal bit in first 23 chars
  
      int8_t prefs[11];
      programFido();     //passes them to be programmed to EEPROM
      Serial.println("Programmed");
      sendPrefsTxt();          //send confirmation text to owner
    }
 
    else{ //maybe someone is guessing pin?
      strangerDanger(); //tell owner
    }
  }
  else{
    
    if(!fido.ifPrivate() || fido.findUser(phone.origin)!=255)
    {
      strncpy(fido.num,phone.origin,LENGTH_OF_NUM);
      sendTemp(temp); //send update
      Serial.print("Temp sent to ");
      Serial.println(fido.num);
    }
    
    else{ //if we've gone this far, privacy is on and stranger has texted
      strangerDanger(); //tell owner
    }
  }
}

void strangerDanger(){
  strcpy_P(phone.buffer, msg_3);  
  phone.openTxt(fido.num);
  phone.inTxt(phone.buffer);
  phone.inTxt(phone.origin);
  phone.closeTxt();
}

void sendTemp(double temp){
  strcpy_P(phone.buffer, msg_4);
  phone.openTxt(fido.num);
  phone.inTxt(phone.buffer);
  phone.inTxt(temp);
  phone.inTxt("F");
  phone.closeTxt();
}

void programFido() {
  int8_t ints[11];
  convertStrToInts(ints);
  byte temp = 0;
  if(ints[1]==1) bitSet(temp,0);
  if(ints[4]==1) bitSet(temp,1);
  if(ints[8]==1) bitSet(temp,2);
  if(ints[10]==1) bitSet(temp,3);
  //these are all determined in Fido.cpp
  int8_t prefs[]= {temp,ints[0],ints[3],ints[2],
    ints[5],ints[6],ints[7],ints[9]};
  for(int i=0;i<9;i++) EEPROM.write(i,prefs[i]);
}

//search content for 11 ints
void convertStrToInts(int8_t prefs[]){
  uint8_t contentP = 1; //pointer for content
  uint8_t prefsP = 0;   //pointer for prefs
  char temp[4]; uint8_t tempP=0;
  while(prefsP<11){
    temp[tempP++]=phone.content[contentP++];
    if (phone.content[contentP]==',' || phone.content[contentP]=='\0'){
      temp[tempP]='\0'; tempP=0; prefs[prefsP]=0;
      while(temp[tempP]!='\0'){
        prefs[prefsP]*=10;
        prefs[prefsP]+=atoi(temp[tempP++]);
      }
      tempP=0; contentP++; prefsP++;
    }
  }
}

uint8_t atoi(char c) {return c-48;}

double getTemp(){ //thanks Adafruit!
  uint8_t i;
  float average;
 
  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
   samples[i] = analogRead(THERMISTORPIN);
   delay(10);
  }
 
  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;
 
  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;
 
  double steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  steinhart = steinhart*9/5+32;
  return steinhart;
}

void defaultFido(){
  const int8_t defaultPrefs[] = {0,5,90,32,60,10,17,19,5};
  for(uint8_t i=0;i<2;i++) EEPROM.write(100+i,0);
  fido.setPin(0,"FIDO");
  fido.setPin(1,"fido");
  Serial.println("Default Fido set");
}

void sendPrefsTxt(){
  int ints[]={fido.getFreq(),0,fido.getLB(),fido.getUB(),0,
    fido.getUpdateFreq(), fido.getUpdateBegin(), fido.getUpdateEnd(), 0, fido.getSummaryTime()};
  if(fido.ifAlarm()) ints[1]=1;
  if(fido.ifUpdates()) ints[4]=1;
  if(fido.ifDailySummary()) ints[8]=1;
  phone.openTxt(fido.num);
  Serial.println(fido.num);
  for(int i=0;i<10;i++){
    strcpy_P(phone.buffer, (char*)pgm_read_word(&(prefs_table[i])));
    phone.inTxt(phone.buffer);
    Serial.print(phone.buffer);
    char out[]="   ";
    if(i==1 || i==4 || i==8) {
      if(ints[i]==1) out[1]='T';
      else out[1]='F';
    }
    else{
      if(ints[i]>99) out[0]=itoa(ints[i]/100);
      if(ints[i]>9) out[1]=itoa(ints[i]%100/10);
      out[2]=itoa(ints[i]%100%10);
    }
    phone.inTxt(out);
    Serial.print(out);
  }
  phone.closeTxt();
}

void initializeStampsAndFile(){
  now = RTC.now();
  stamp[1]=now.hour()*60-1;
  //if its past summary time of today, put today's stamp
  if(now.hour()>=fido.getSummaryTime()) stamp[2]=now.day();
  
  pinMode(CHIP_SELECT, OUTPUT);
  // see if the card is present and can be initialized:
  
  if (!SD.begin(CHIP_SELECT)) {
    return;
  }
  // create a new file
  char filename[] = "LOGGER00.txt";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE); 
      Serial.print("logfile \'");
      Serial.print(filename);
      Serial.println("\' opened");
      break;  // leave the loop!
    }
    //else Serial.println("Failed SD intialize");
  }
}

void refreshDate(const DateTime& dt, char date[], char time[]){
  date[0]=itoa(dt.day()/10);
  date[1]=itoa(dt.day()%10);
  date[2]='/';
  date[3]=itoa(dt.month()/10);
  date[4]=itoa(dt.month()%10);
  date[5]='/';
  date[6]=itoa(dt.year()/1000);
  date[7]=itoa(dt.year()%1000/100);
  date[8]=itoa(dt.year()%1000%100/10);
  date[9]=itoa(dt.year()%1000%100%10);
  date[10]='\0';
  time[0]=itoa(dt.hour()/10);
  time[1]=itoa(dt.hour()%10);
  time[2]=':';
  time[3]=itoa(dt.minute()/10);
  time[4]=itoa(dt.minute()%10);
  time[5]=':';
  time[6]=itoa(dt.second()/10);
  time[7]=itoa(dt.second()/10);
  time[8]='\0';
}

char itoa(uint8_t c) {return c+48;}

