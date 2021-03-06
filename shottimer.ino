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
#include "Sensor.h"

// comment in for use with relais
//#include <Bounce.h>

Display display; // LCD display wrapper

// setup for the tsic sensors on pin 2 and 3
Sensor groupSensor(2);   // only Signalpin, VCCpin unused by default
Sensor boilerSensor(3);  // only Signalpin, VCCpin unused by default

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
bool isSleep = true;
bool isHold = true;
const char* version[] = {"Espresso", "Shot Timer", "v1.006.045"};
long previous;
//long onTIME;

bool isSecondTime = false;
bool isFirstTime = false;
bool isThirdTime = false;

float timerValue;
int firstTIME;
int secondTIME;

enum class Configuration {
  None = 0,
  GroupOnly = 1,
  BoilerOnly = 2,
  Both = 3
};
Configuration configuration = Configuration::None;

bool requestT;
bool prepareTemp = true;

// light
bool turnLightOn = false;
bool IsLightOn = false;
bool turnLightOff = false;
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
  if (groupSensor.IsConnected() && boilerSensor.IsConnected()) {
    configuration = Configuration::Both;
  }
  else if (groupSensor.IsConnected()) {
    configuration = Configuration::GroupOnly;
  }
  else if (boilerSensor.IsConnected()) {
    configuration = Configuration::BoilerOnly;
  }
}

void loop() {

  //check for signal
  int klick1 = !digitalRead(btnSTARTpin);
  int klick2 = !digitalRead(btnPUMPpin);


  // active signal on P1 start timer
  if (klick1) {
    if (!timerRUN) {
      MsTimer2::set(10, Tick);
      MsTimer2::start();
      // reset lcd
      display.Clear();
      // set variable
      timerRUN = true;
      count = 0;
      timerValue = 0;
      firstTIME = 0;
      secondTIME = 0;
      isSleep = false;
      sleeptimerRUN = false;
      isHold = true;
      turnLightOn = true;
    }
  }

  // timer is running
  if (timerRUN && klick1) {
    float countF = count;
    int TIME = countF / 100;
    
	// get first time
    if (!isFirstTime) {
      isFirstTime = true;
    }
    
	// get second time
    if (klick1 && klick2 && !isSecondTime) {
      firstTIME = TIME;
      isSecondTime = true;
      count = 0;
    }
    
	// get third time
    if (isFirstTime && isSecondTime && !isThirdTime && !klick2) {
      secondTIME = TIME;
      TIME = 0;
      count = 0;
      isThirdTime = true;
    }
    
	// seven segment
    if (!isThirdTime) {
      display.DrawTimer(TIME);
    }
    
    if (firstTIME != 0 && isSecondTime) {
      display.DrawPreInfusion(5, 0, firstTIME);
    }
    if (TIME > 0 && isThirdTime ) {
      display.DrawPreInfusion(70, 0, TIME);
    }
  }
  
  // no active signal
  if (timerRUN && !klick1) {
    MsTimer2::stop();
    float countF = count;
    timerValue = countF / 100;
    timerRUN = false;
    isSleep = true;
    count = 0;
    //      tcount = 0;
    isFirstTime = false;
    isSecondTime = false;
    isThirdTime = false;
  }

  if (isSleep) {
    if (!sleeptimerRUN) {
      MsTimer2::set(10, Tick);
      MsTimer2::start();
      sleeptimerRUN = true;
    }
  }

  if (isSleep && sleeptimerRUN) {
    if ((count > showLastShot) || ((timerValue + firstTIME + secondTIME) < 8)) {
      MsTimer2::stop();
      display.Clear();
      sleeptimerRUN = false;
      count = 0;
      isSleep = false;
      isHold = false;
      turnLightOff = true;
      isFirstTime = false;
      isSecondTime = false;
      isThirdTime = false;
      requestT = 1;
    }
  }
  // get and display temperature runs only if tsic is present on startup
  if (configuration != Configuration::None && !timerRUN && !klick1 && !isHold) {
    if ( requestT) {
      requestT = 0;
    }
    if (!requestT && tcount >= 20000) {
      requestT = 1;
      tcount = 0;

      if(configuration == Configuration::Both){
        groupSensor.ReadNext();
        boilerSensor.ReadNext();
        display.DrawBothTemperatures(groupSensor.GetValue(), boilerSensor.GetValue());
      }
      else if (configuration == Configuration::BoilerOnly){
        boilerSensor.ReadNext();
        display.DrawBoilerTemperature(boilerSensor.GetValue());
      }
      else{
        groupSensor.ReadNext();
        display.DrawGroupTemperature(groupSensor.GetValue());
      }

      Serial.print(" group temperature: ");
      Serial.print(groupSensor.GetValue());
      Serial.print(" raw1: ");
      Serial.print(groupSensor.GetRawValue());
      Serial.print("-");
      //Serial.print(tmp);
      Serial.print("-");
      Serial.print(groupSensor.previousValue);
      Serial.print("  boiler temperature: ");
      Serial.print(boilerSensor.GetValue());
      Serial.print(" raw2: ");
      Serial.print(boilerSensor.GetRawValue());
      Serial.print("-");
      Serial.print(boilerSensor.previousValue); 
      Serial.print("-");
      Serial.print(boilerSensor.errorCount);
      Serial.print("-");
      Serial.println(static_cast<int>(configuration));
    }                   

    tcount++;
  }
  
  if (turnLightOn && !IsLightOn) {
    // turn BaristaLight on
    IsLightOn = true;
    turnLightOff = false;
    if (dimm < 100) {
      dimm = 100;
    }
    turnOffDelay = 0;
  }
  
  if (IsLightOn) {
    if (dimm < brightness) {
      unsigned long current = millis();
      if (current - previous > 10) {
        previous = current;
        dimm = dimm + 5;
      }
    }
    analogWrite(baristaLightPWM, dimm);
  }
  
  if (turnLightOff && IsLightOn && dimm == brightness) {
    // turn BaristaLight off
    turnLightOn = false;
    IsLightOn = false;
    turnOffDelay = millis();
  }
  
  if (!IsLightOn) {
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
        isHold = false;
      }
      analogWrite(baristaLightPWM, dimm);
    }
  }
}

void Tick() {
  count++;
}
