/*
 *   This file is part of Yet Another Foxbox (YAFB).
 *   
 *   Yet Another Foxbox (YAFB) is an amateur radio fox transmitter 
 *   designed for an ESP32-DevKitM-1 and a NiceRF SA818.
 *   
 *   Copyright (c) 2021-2023 Gregory Stoike (N8HR).
 *   
 *   YAFB is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   YAFB is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with YAFB.  If not, see <https://www.gnu.org/licenses/>.
 *   
 */


/****************************************************************************** 
 *   Todo:
 *   
 *   General
 *    * Implement Start/Stop time function
 *    *
 ******************************************************************************/


#include <Preferences.h>        // For storing variables that survive a reset
#include "RTClib.h"             // For DS3231 RTC Module
#include <Wire.h>               // For I2C Communication (DS3231)

// Preferences Settings
Preferences preferences;

// Declare some initial variables
//  This is needed due to AsyncWebServer needing them before we can pull them from memory
//  So we declare these and change them in setup via preferences if set
String callmessage = "N0CALL Fox";
String morse = "-. ----- -.-. .- .-.. .-.. / ..-. --- -..-";
int timebetween = 57000; // Time between transmissions in miliseconds (1 sec = 1000 miliseconds)
boolean onoff = false; // false = off, true = on
struct tm tmcurrenttime = {0};
struct tm tmstarttime = {0};
struct tm tmendtime = {0};
float txfrequency = 146.565; // 146.565 is the normal TX frequency for foxes
float rxfrequency = 146.565; // RX frequency
byte bandwidth = 1; // Bandwidth, 0=12.5k, 1=25K
byte squelch = 1; // Squelch 0-8, 0 is listen/open
byte volume = 5; // Volume 1-8

// ESP32-S2 Settings
#define PTT_Pin 2 
#define PD_Pin 3
#define HL_Pin 4
#define SCL_Pin 5
#define SDA_Pin 6
#define SATX_Pin 33
#define SARX_Pin 34

// DS3231 Settings
RTC_DS3231 rtc;

// Misc Variables
unsigned long oneminmillis = 10000000;
unsigned long tenminmillis = 1000000;
unsigned long radiomillis = 1000000;
unsigned long tempmillis = 1000000;
boolean updateSAsettingsFlag = false;

HardwareSerial SASerial(1);

/****************************************************************************** 
 * Standard Setup Function
 ******************************************************************************/
void setup() 
{
  // Start Serial for debugging
  Serial.begin(115200); // Serial for ESP32-S2 board
  SASerial.begin(9600, SERIAL_8N1, SARX_Pin, SATX_Pin); // Serial for SA818 using pins GPIO5 (rx) & GPIO6 (tx)
  delay(1000);
    
  // Replace fox settings from memory
  preferences.begin("settings", true);
    timebetween = preferences.getInt("timebetween", 60000);
    onoff = preferences.getBool("onoff", false);
    txfrequency = preferences.getFloat("txfrequency", 146.565);
    rxfrequency = preferences.getFloat("rxfrequency", 146.565);
    bandwidth = preferences.getChar("bandwidth", 1);
    squelch = preferences.getChar("squelch", 1);
    volume = preferences.getChar("volume", 5);
    
    time_t starttemp = preferences.getULong("starttime", 500001720);
    localtime_r(&starttemp, &tmstarttime);
    
    time_t endtemp = preferences.getULong("endtime", 1704067200);
    localtime_r(&endtemp, &tmendtime);

    callmessage = preferences.getString("callmessage", "N0CALL Fox");
    morse = preferences.getString("morse", "-. ----- -.-. .- .-.. .-.. / ..-. --- -..-");
  preferences.end();
  
  // Set pins
  pinMode(PTT_Pin, OUTPUT);
  pinMode(PD_Pin, OUTPUT);
  pinMode(HL_Pin, OUTPUT);
  digitalWrite(PTT_Pin, LOW); // LOW is RX, High is TX, it's confusing??
  digitalWrite(PD_Pin, LOW); // LOW for power down mode, HIGH for normal mode??
  digitalWrite(HL_Pin, LOW); // LOW for .5w, HIGH for 1w??
  
  // Start the I2C interface
  Wire.begin(SDA_Pin, SCL_Pin);

/*  
    Stored for later use

    preferences.putBool("onoff", onoff);
    preferences.putBool("onoff", onoff);
    preferences.putString("callmessage", callmessage);
    preferences.putString("morse", morse);
    preferences.putFloat("txfrequency", txfrequency);
    preferences.putFloat("rxfrequency", rxfrequency);
    preferences.putChar("bandwidth", bandwidth);
    preferences.putChar("volume", volume);
    preferences.putChar("squelch", squelch);
    preferences.putChar("timebetween", timebetween);
    preferences.putULong("starttime", temp);
    preferences.putULong("endtime", temp);
*/

  // Set RTC time if needed
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }

  // If power was lost, set it to the compiled date & time. About 30-40sec slow?
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); 
  }
  
  updateSysTime();
  updateSAsettings();

}

/****************************************************************************** 
 * Standard Loop Function
 * 
 * Periodically update the ESP32 time from the RTC
 * Play the audio melody then play the identification in morse code then wait
 ******************************************************************************/
void loop()
{
// Runs updateSysTime() every 10 minutes to keep the ESP32 in sync with the RTC in case of drift
  unsigned long currentMillis = millis();
  if(currentMillis - tenminmillis >= 600000)
  {
    tenminmillis = currentMillis;   
    updateSysTime();
  }

  // Starts transmitting with a time gap of 'timebetween' && if 'onoff' is set to true
  currentMillis = millis();
  if(currentMillis - radiomillis >= timebetween && onoff)
  {
    digitalWrite(HL_Pin, HIGH); // HIGH for .5w, LOW for 1w??

    // Set pins to be ready for transmit
    digitalWrite(PD_Pin, HIGH); // Take the SA818 out of PD state
    digitalWrite(PTT_Pin, HIGH); // Put the SA818 in TX mode
    delay(3000);  // Takes about 3 seconds for SA818 to come out of PD state

    // Play the song
    playMelody();
    delay(700); // Just a slight break between
    // Play the ID in morse
    playMorse();

    // Set pins to stop transmit and power down the SA818 for power saving
    digitalWrite(PTT_Pin, LOW); // Put the SA818 in RX mode
    digitalWrite(PD_Pin, LOW); // Put the SA818 back into PD state
    // End Transmittion Block

    // Set for gap between tranmissions
    radiomillis = millis();
  }
 
  currentMillis = millis();
  if(currentMillis - tempmillis >= 1000)
  {
    String content = "";  //null string constant ( an empty string )
    char character;
    while(SASerial.available()) 
    {
      character = SASerial.read();
      content.concat(character);
      content.trim();
    }
    if (content != "") 
    {
      Serial.println("***SA818**********************************************************************");
      Serial.print("Response: ");
      Serial.println(content);
      Serial.println("******************************************************************************");
      Serial.println();
    }
    
    tempmillis = currentMillis;
  }

  if (updateSAsettingsFlag)
  {
    updateSAsettingsFlag = false;
    updateSAsettings();

  }
}

/****************************************************************************** 
 * Misc Time Functions
 * 
 * updateSysTime
 *    Updates the ESP32 sytem time using the external RTC
 ******************************************************************************/

void updateSysTime()
{  
  // Update the system time from RTC time
  DateTime now = rtc.now();
  struct timeval tv{ .tv_sec = now.unixtime() };
  settimeofday(&tv, NULL);
  getLocalTime(&tmcurrenttime);
  
  Serial.println("***updateSysTime**************************************************************");
  Serial.println("Synced the ESP32 time from the RTC time.");
  Serial.print("\tCurrent time: ");
  Serial.println(&tmcurrenttime, "%B %d %Y %H:%M:%S");
  Serial.println("******************************************************************************");
  Serial.println();
}

/****************************************************************************** 
 * These functions are for the SA818
 * 
 * SA818Connect
 *    Used for initial handshake with the SA818
 * 
 * SA818SetGroup
 *    Sets the Bandwidth, TX Frequency, RX Frequency, CTCSS tones, and Squelch
 * 
 * SA818SetVolume 
 *    Sets the output volume of the SA818
 * 
 * SA818SetFilter
 *    Sets the filters of the SA818
 * 
 * SA818SetTail
 *    Sets what kind of tail you want after tx
 ******************************************************************************/

void updateSAsettings(void)
{
  SA818SetGroup();
}

void SA818Connect(void)
{
  char rxbuffer[20];  // buffer for response string
  byte rxlen=0;   // counter for received bytes
  do
  {
    SASerial.println("AT+DMOCONNECT");         // begin message
    rxlen=SASerial.readBytesUntil('\n',rxbuffer,19);
  } while(rxlen==0);    // send command until answer is received
  rxbuffer[rxlen-1]=0;  // check length of answer and remove cr character
  rxbuffer[rxlen]=0; // remove last byte and end string
  delay(1000);    // wait a little bit
}

void SA818SetGroup()
{
  digitalWrite(PD_Pin, HIGH); // LOW for power down mode, HIGH for normal mode
  digitalWrite(PTT_Pin, LOW); // LOW is RX, High is TX
  delay(3000);  // Takes about 3 seconds for SA818 to come out of PD state

  SA818Connect();
  preferences.begin("settings", true);
    txfrequency = preferences.getFloat("txfrequency");
    rxfrequency = preferences.getFloat("rxfrequency");
    bandwidth = preferences.getChar("bandwidth");
    squelch = preferences.getChar("squelch");
  preferences.end();
  byte txctcss = 0;
  byte rxctcss = 0;
  
  SASerial.print("AT+DMOSETGROUP=");
  SASerial.print(bandwidth);
  SASerial.print(",");
  SASerial.print(txfrequency, 4);
  SASerial.print(",");
  SASerial.print(rxfrequency, 4);
  SASerial.print(",00");
  if(txctcss<10) SASerial.print("0");
  SASerial.print(txctcss);
  SASerial.print(",");    
  SASerial.print(squelch);
  SASerial.print(",00");
  if(rxctcss<10) SASerial.print("0");
  SASerial.println(txctcss);
  delay(1000);
  digitalWrite(PD_Pin, LOW); // Put the SA818 back into PD state
}

/*
void SA818SetVolume()
{
  digitalWrite(PD_Pin, HIGH); // LOW for power down mode, HIGH for normal mode
  digitalWrite(PTT_Pin, LOW); // LOW is RX, High is TX
  delay(3000);  // Takes about 3 seconds for SA818 to come out of PD state
  
  SA818Connect();
  preferences.begin("settings", true);
    volume = preferences.getChar("volume", 5);
  preferences.end();
  SASerial.print("AT+DMOSETVOLUME=");
  SASerial.println(volume);
  digitalWrite(PD_Pin, LOW); // Put the SA818 back into PD state
}

void SA818SetFilter(byte prefilter, byte highpassfilter, byte lowpassfilter)
{
  SA818Connect();
  SASerial.print("AT+SETFILTER=");
  SASerial.print(prefilter);
  SASerial.print(",");
  SASerial.print(highpassfilter);
  SASerial.print(",");
  SASerial.println(lowpassfilter);
}

void SA818SetTail(byte tailtone)
{
  SA818Connect();
  SASerial.print("AT+SETTAIL=");
  SASerial.println(tailtone);
}
*/

/****************************************************************************** 
 * These functions are for debugging or displaying info
 * 
 * printVars
 *    Prints out the variables and their values
 *    
 * printTimes
 *    Outputs various times: System time, Unix time, RTC time   
 ******************************************************************************/


void printVars()
{
  Serial.println("***printVars******************************************************************");
  Serial.print("Call: ");
  Serial.println(callmessage);
  Serial.print("Morse: ");
  Serial.println(morse);
  Serial.print("Time Between: ");
  Serial.println(timebetween);
  Serial.print("OnOff: ");
  Serial.println(onoff);
  Serial.print("TX: ");
  Serial.println(txfrequency, 3);
  /*
   * Serial.print("RX: ");  
   * Serial.println(rxfrequency, 3);
   * Serial.print("Bandwidth: ");
   * Serial.println(bandwidth);
   * Serial.print("Squelch: ");
   * Serial.println(squelch);  
   * Serial.print("Volume: ");  
   * Serial.println(volume);
  */
  Serial.print("Current: ");
  Serial.println(&tmcurrenttime, "%B %d %Y %H:%M:%S");
  Serial.print("Start: ");
  Serial.println(&tmstarttime, "%B %d %Y %H:%M:%S");
  Serial.print("End: ");
  Serial.println(&tmendtime, "%B %d %Y %H:%M:%S");
  Serial.println("******************************************************************************");
  Serial.println();
}

void printTimes()
{
  Serial.println("***printTimes*****************************************************************");

  if(!getLocalTime(&tmcurrenttime)){
    Serial.println("Failed to obtain time");
    return;
  }

  Serial.print("Systime: ");
  Serial.println(&tmcurrenttime, "%B %d %Y %H:%M:%S");
  //  Serial.print("Starttime: ");
  //  Serial.println(&tmstarttime, "%B %d %Y %H:%M:%S");
  //  Serial.print("Endtime: ");
  //  Serial.println(&tmendtime, "%B %d %Y %H:%M:%S");

  DateTime now = rtc.now();
  //  Serial.print("Unix: ");
  //  Serial.println(now.unixtime());
  
  Serial.print("RTC: ");    
  Serial.printf("%02d/%02d/%04d (%02d:%02d:%02d)\n", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
  Serial.println("******************************************************************************");
  Serial.println();
}

void printTemp()
{
    Serial.println("***Temperature****************************************************************");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");
    Serial.print(rtc.getTemperature()*1.8+32);
    Serial.println(" F");
    Serial.println("******************************************************************************");
    Serial.println();
}
