/*
  simple shot timer with 7-Seg style graphic output, temprature display and
  signaloutput for a relais for baristlights during shot (version 06/06/2016)
  to use with an OLED diplay via I2C.
  by David Kißling (user mactree of http://www.kaffee-netz.de )
  BIG THANK TO:
  magicbugsbunny - for code snippets
  blu - for optimizing the code
  Torsten Wedemeyer - input and optimizing the code
  kn-forum - all the help

  changelog

v1.005.003 - change tsic libraray
   004.014 - delay check Temp 
   004.005 - delete Bounce 
   004.004 - change pin mapping 
   003.010 - change to Tsic temperature sensor
   002.030 - added "°" to the library font which makes the temperature display a lot easier
   002.026 - slow down the temperature request set resolution again to 12
   002.025 - change behaviour when sensor looses connection
   002.025 - change temp output
   002.020 - set temperature wait for conversion, slows down but stablelize the output
   002.016 - set temperature wait for conversion, slows down but stablelize the output
   002.014 - set temperature resolution 12 (max)
   002.012 - code rewrite, bugfixing
   002.011 - code rewrite
   002.010 - code rewrite
   002.008 - code rewrite, renamed variables, change behavior when P2 is connected but no PI is used
   001.030 - fix temperature output
   001.028 - change behavior of Baristalight
   001.024 - change pins for mv and pump
   001.023 - add PIN13 for baristlight and PIN10 for baristalight with pwm, added millis for better dimming
   001.011 - code rewrite, post infusion in upper reight corner
   001.001 - code rewrite
   000.010 - change relais pin to 10 for pwm, add postinfusion,
       009 - change behavior of signalinput P1 ist starting the timer and P2 is switchich the PI
       008 - bugfix: degree when temperature sinks under 100°
       004 - change temprature display
       003 - release

*/

// libraries
#include <MsTimer2.h> // https://github.com/PaulStoffregen/MsTimer2
#include <Arduino.h>
//#include <Wire.h>
#include "Display.h"
#include "TSIC.h" // https://github.com/Schm1tz1/arduino-tsic

// comment in for use with relais
//#include <Bounce.h>

// setup for the tsic sensors on pin 2 and 3
TSIC groupSensor(2);    // only Signalpin, VCCpin unused by default
TSIC boilerSensor(3);    // only Signalpin, VCCpin unused by default

Display display;

// define signal input
const int btnPUMPpin = 7;
const int btnSTARTpin = 5;

// define pin => connect to IRLIZ44N (GATE) with PWM
int baristaLightPWM = 6;


// define how long the last shot-time will be shown
int showLastShot = 1000; // 500 => 5s; 1000 => 10s

// define global variables
int count = 0;
int tcount = 0;
bool timerRUN = false;
bool sleeptimerRUN = false;
bool sleep = true;
bool hold = true;
const char* version[] = {"Espresso", "Shot Timer", "v1.006.045"};
long previous;
long onTIME;


bool getSecondTime = false;
bool getFirstTime = false;
bool getThirdTime = false;

float TIME;
int firstTIME;
int secondTIME;

// sensors
enum class Mode {
  None = 0,
  GroupOnly = 1,
  BoilerOnly = 2,
  Both = 3
};
Mode checkTemp = Mode::None;

struct SensorData {
  static const int maxValues = 10;
  uint16_t values[maxValues];      // the readings from the analog input
  unsigned int currentIndex = 0;              // the index of the current reading
  int total = 0;
  uint16_t previousValue = 0;
  unsigned int errorCount = 0;
  float temperatureValue = 0;

  uint16_t GetCurrentValue() const { return values[currentIndex]; }

  void reset() {
    for(auto& value : values) value = 0;    
  }
};

SensorData groupData;
SensorData boilerData;

void getTemperature(TSIC& sensor, SensorData &sensorData) {
  uint16_t data = 0;

  if(sensor.getTemperature(&data)) {
    auto tmp = data;
    int r = tmp - sensorData.previousValue;
    if( r < 0) {
      r = -r;
    }
    
    if (r < 50 || sensorData.errorCount >= 5) {
      sensorData.previousValue = tmp;
      sensorData.errorCount = 0;
    }
    else{
      tmp = sensorData.previousValue;
      sensorData.errorCount++;
    }
    
    sensorData.total -= sensorData.GetCurrentValue();
    sensorData.values[sensorData.currentIndex] = tmp;  
    sensorData.total += sensorData.GetCurrentValue();
    
    ++sensorData.currentIndex;
    
    if (sensorData.currentIndex >= SensorData::maxValues) {
      sensorData.currentIndex = 0;
    }
    
    data = sensorData.total / SensorData::maxValues;
  }
  
  sensorData.temperatureValue = sensor.calc_Celsius(&data);
}

bool requestT;
bool prepareTemp = true;

// light
bool turnLightOn = false;
bool IsLightOn = false;
bool turnLightOff = false;
bool IsLightOff = false;
int dimm = 0;
int brightness = 200; // 255 => max 127 => 50%
unsigned long turnOffDelay;

void setup() {
  //Serial.begin(9600);
  
  pinMode(btnSTARTpin, INPUT);
  digitalWrite(btnSTARTpin, HIGH);
  pinMode(btnPUMPpin, INPUT);
  digitalWrite(btnPUMPpin, HIGH);
  pinMode(baristaLightPWM, OUTPUT);
  digitalWrite(baristaLightPWM, LOW);
  delay(40);
  display.Setup(version);
      
  delay(3000);

  // check if temperature sensor tsic is connected runs only once
  uint16_t dummyData = 0;
  groupSensor.getTemperature(&dummyData);
  auto dummyTemperature = groupSensor.calc_Celsius(&dummyData);
  Serial.println(dummyData);
  Serial.println(dummyTemperature);
  bool useGroupSensor = dummyTemperature > 0;
  
  dummyData = 0;
  boilerSensor.getTemperature(&dummyData);
  dummyTemperature = boilerSensor.calc_Celsius(&dummyData);  
  Serial.println(dummyData);
  Serial.println(dummyTemperature);
  bool useBoilerSensor = dummyTemperature > 0;
  
  if (useGroupSensor && useBoilerSensor > 0) {
    checkTemp = Mode::Both;
  }
  else if (useGroupSensor) {
    checkTemp = Mode::GroupOnly;
  }
  else if (useBoilerSensor > 0){
    checkTemp = Mode::BoilerOnly;
  }

  // reset groupTemperatureData data
  groupData.reset();
  boilerData.reset();
}

void loop() {

  //check for signal
  int klick1 = !digitalRead(btnSTARTpin);
  int klick2 = !digitalRead(btnPUMPpin);


  // active signal on P1 start timer
  if (klick1) {
    if (!timerRUN) {
      MsTimer2::set(10, zeitLaeuft);
      MsTimer2::start();
      // reset lcd
      display.Clear();
      // set variable
      timerRUN = true;
      count = 0;
      TIME = 0;
      firstTIME = 0;
      secondTIME = 0;
      sleep = false;
      sleeptimerRUN = false;
      hold = true;
      turnLightOn = true;
    }
  }

  // timer is running
  if (timerRUN && klick1) {
    float countF = count;
    int TIME = countF / 100;
    // get first time
    if (!getFirstTime) {
      getFirstTime = true;
    }
    // get second time
    if (klick1 && klick2 && !getSecondTime) {
      firstTIME = TIME;
      getSecondTime = true;
      count = 0;
    }
    // get third time
    if (getFirstTime && getSecondTime && !getThirdTime && !klick2) {
      secondTIME = TIME;
      TIME = 0;
      count = 0;
      getThirdTime = true;
    }
    // seven segment
    if (!getThirdTime) {
      display.DrawTimer(TIME);
    }
    
    if (firstTIME != 0 && getSecondTime) {
      display.DrawPI(5, 0, firstTIME);
    }
    if (TIME > 0 && getThirdTime ) {
      display.DrawPI(70, 0, TIME);
    }
  }
  // no active signal
  if (timerRUN && !klick1) {
    MsTimer2::stop();
    float countF = count;
    TIME = countF / 100;
    timerRUN = false;
    sleep = true;
    count = 0;
    //      tcount = 0;
    getFirstTime = false;
    getSecondTime = false;
    getThirdTime = false;
  }
  if (sleep) {
    if (!sleeptimerRUN) {
      MsTimer2::set(10, zeitLaeuft);
      MsTimer2::start();
      sleeptimerRUN = true;
    }
  }
  if (sleep && sleeptimerRUN) {
    if ((count > showLastShot) || ((TIME + firstTIME + secondTIME) < 8)) {
      MsTimer2::stop();
      display.Clear();
      sleeptimerRUN = false;
      count = 0;
      sleep = false;
      hold = false;
      turnLightOff = true;
      getFirstTime = false;
      getSecondTime = false;
      getThirdTime = false;
      requestT = 1;
    }
  }
  // get and display temperature runs only if tsic is present on startup
  if (checkTemp != Mode::None && !timerRUN && !klick1 && !hold) {
    if ( requestT) {
      requestT = 0;
    }
    if (!requestT && tcount >= 20000) {
      requestT = 1;
      tcount = 0;
 
      
      if(checkTemp == Mode::Both){
        getTemperature(groupSensor, groupData);
        getTemperature(boilerSensor, boilerData);
        display.DrawBothTemperatures(groupData.temperatureValue, boilerData.temperatureValue);
      }
      else if( checkTemp == Mode::BoilerOnly){
        getTemperature(boilerSensor, boilerData);
        display.DrawBoilerTemperature(boilerData.temperatureValue);
      }
      else{
        getTemperature(groupSensor, groupData);
        display.DrawGroupTemperature(groupData.temperatureValue);
      }

      Serial.print(" group temperature: ");
      Serial.print(groupData.temperatureValue);
      Serial.print(" raw1: ");
      Serial.print(groupData.GetCurrentValue());
      Serial.print("-");
//      Serial.print(tmp);
      Serial.print("-");
      Serial.print(groupData.previousValue);
      Serial.print("  boiler temperature: ");
      Serial.print(boilerData.temperatureValue);   
      Serial.print(" raw2: ");
      Serial.print(boilerData.GetCurrentValue());
      Serial.print("-");
      Serial.print(boilerData.previousValue); 
      Serial.print("-");
      Serial.print(boilerData.errorCount);
      Serial.print("-");
      Serial.println(static_cast<int>(checkTemp));
    }                   

    tcount++;
  }
  if (turnLightOn && !IsLightOn) {
    // turn BaristaLight on
    IsLightOn = true;
    IsLightOff = false;
    turnLightOff = false;
    if (dimm < 100) {
      dimm = 100;
    }
    turnOffDelay = 0;
  }
  if (IsLightOn) {
    if ( dimm < brightness) {
      unsigned long current = millis();
      if (current - previous > 10) {
        previous = current;
        dimm = dimm + 5;
      }
    }
    analogWrite(baristaLightPWM, dimm);
  }
  if (turnLightOff && !IsLightOff && dimm == brightness) {
    // turn BaristaLight off
    turnLightOn = false;
    IsLightOn = false;
    IsLightOff = true;
    turnOffDelay = millis();
  }
  if (IsLightOff) {
    unsigned long current = millis();

    if (current - turnOffDelay > 90000) {
      if (dimm > 0 ) {
        //unsigned long current = millis();
        if (current - previous > 10) {
          previous = current;
          dimm = dimm - 5;
        }
      }
      if (dimm < 10) {
        hold = false;
      }
      analogWrite(baristaLightPWM, dimm);
    }
  }
}



void zeitLaeuft() {
  count++;
}
