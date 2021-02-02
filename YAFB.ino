/*
 *   This file is part of Yet Another Foxbox (YAFB).
 *   
 *   Yet Another Foxbox (YAFB) is an amateur radio fox transmitter 
 *   designed for an ESP32-S2-Saola-1 and a NiceRF SA818.
 *   
 *   Copyright (c) 2021 Gregory Stoike (KN4CK).
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

#include <Adafruit_NeoPixel.h>  // Used for the NeoPixel LED on GPIO 18
#include <FS.h>                 // For SD Card
#include <SPI.h>                // For SD Card
#include <SD.h>                 // For SD Card
#include <WiFi.h>               // For Wifi
#include <WiFiAP.h>             // For WiFi
#include <WiFiClient.h>         // For WiFi
#include <AsyncTCP.h>           // For Webserver
#include <ESPAsyncWebServer.h>  // For Webserver
#include <ESPmDNS.h>            // For DNS resolving
#include <DRA818.h>             // For SA818
#include <TimeLib.h>               // For Clock
#include "time.h"



// Initial Fox Settings if nothing set
char callsign[15] = "N0CALL";
int timebetween = 60000; // Time between transmissions in miliseconds (1 sec = 1000 miliseconds)
boolean onoff = false; // false = off, true = on
struct tm tmcurrenttime = {0};
struct tm tmstarttime = {0};
struct tm tmendtime = {0};

// Initial SA818 Settings if nothing set
float txfrequency = 146.565; // 146.565 is the normal TX frequency for foxes
float rxfrequency = 146.565; // RX frequency
byte bandwidth = 1; // Bandwidth, 0=12.5k, 1=25K
byte squelch = 1; // Squelch 0-8, 0 is listen/open
byte volume = 5; // Volume 1-8

// WiFi Settings
const char* ssid = "YAFB";
const char* password = "11111111";
const uint8_t channel = 11;
IPAddress local_IP(10,73,73,73);
IPAddress gateway(10,73,73,1);
IPAddress subnet(255,255,255,0);

// Web Server Settings
AsyncWebServer server(80);

// HTML web page
const char index_html[] PROGMEM = R"rawliteral(
  <!doctype html>
  <html lang="en">
  <head>
    <meta charset="utf-8">
    <title>Yet Another Foxbox</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
      html 
      {
        font-family: Arial, sans-serif;
      }
  
      .switch  /* The switch - the box around the slider */
      {
        position: relative;
        display: inline-block;
        width: 60px;
        height: 34px;
      }
  
      .switch input[type=checkbox]  /* Hide default HTML checkbox */
      {
        opacity: 0;
        width: 0;
        height: 0;
      }
  
      .slider  /* The slider */
      {
        position: absolute;
        cursor: pointer;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background-color: #ccc;
        -webkit-transition: .4s;
        transition: .4s;
      }
  
      .slider:before
      {
        position: absolute;
        content: "";
        height: 26px;
        width: 26px;
        left: 4px;
        bottom: 4px;
        background-color: white;
        -webkit-transition: .4s;
        transition: .4s;
      }
  
      input[type=checkbox]:checked + .slider 
      {
        background-color: #2196F3;
      }
  
      input[type=checkbox]:focus + .slider 
      {
        box-shadow: 0 0 1px #2196F3;
      }
  
      input[type=checkbox]:checked + .slider:before 
      {
        -webkit-transform: translateX(26px);
        -ms-transform: translateX(26px);
        transform: translateX(26px);
      }
  
      .slider.round  /* Rounded sliders */
      {
        border-radius: 34px;
      }
  
      .slider.round:before  /* Rounded sliders */
      {
        border-radius: 50%;
      }
  
      input[type=text], input[type=date],input[type=time], select
      {
        width: 100%;
        padding: 12px;
        border: 1px solid #ccc;
        border-radius: 4px;
        box-sizing: border-box;
        background: white;
        color: black;
      }
      
      input[type=submit] 
      {
        background-color: #2196F3;
        color: white;
        padding: 12px 20px;
        border: none;
        border-radius: 4px;
        cursor: pointer;
      }
  
      input[type=reset] 
      {
        background-color: #ccc;
        padding: 12px 20px;
        border: none;
        border-radius: 4px;
        cursor: pointer;
      }
  
      .container 
      {
        display: grid;
        grid-template-columns: max-content 150px 150px;
        grid-gap: 10px;
        justify-content: center;
        align-items: center;
      }
  
      .titlebox
      {
        grid-column: span 3;
        text-align: center;
      }

      .spantwo
      {
        grid-column: span 2;  
      }
  </style>
  
  </head>
  <body>
  
  <form action="/get">
  <div class="container">
    <div class="titlebox"><h2>Yet Another Foxbox</h2></div>
    <div>Transmitter:</div>
    <div class="spantwo"><label class="switch"><input type="checkbox" name="webonoff" ~ONOFF~><span class="slider round"></span></label></div>
  
    <div><label class="textlabel" for="callsign">Callsign:</label></div>
    <div class="spantwo"><input type="text" id="callsign" name="webcallsign" value=~CALLSIGN~></div>
  
    <div><label class="textlabel" for="txfrequency">TX Frequency:</label></div>
    <div class="spantwo"><input type="text" id="txfrequency" name="webtxfrequency" value=~TXFREQUENCY~></div>
  
    <div><label class="textlabel" for="rxfrequency">RX Frequency:</label></div>
    <div class="spantwo"><input type="text" id="rxfrequency" name="webrxfrequency" value=~RXFREQUENCY~></div>
  
    <div><label for="bandwidth">Bandwidth:</label></div>
    <div class="spantwo"><select id="bandwidth" name="webbandwidth">~BANDWIDTH~</select></div>
  
    <div><label for="volume">Volume:</label></div>
    <div class="spantwo"><select id="volume" name="webvolume">~VOLUME~</select></div>
  
    <div><label for="squelch">Squelch:</label></div>
    <div class="spantwo"><select id="squelch" name ="websquelch">~SQUELCH~</select></div>

    <div><label for="currenttime">Current time:</label></div>
    <div><input type="date" id="currentdate" name="webcurrentdate" value=~CURRENTDATE~></div>
    <div><input type="time" id="currenttime" name="webcurrenttime" value=~CURRENTTIME~></div>
  
    <div><label for="starttime">Start fox at:</label></div>
    <div><input type="date" id="startdate" name="webstartdate" value=~STARTDATE~></div>
    <div><input type="time" id="starttime" name="webstarttime" value=~STARTTIME~></div>
  
    <div><label for="stoptime">Stop fox at:</label></div>
    <div><input type="date" id="stopdate" name="webenddate" value=~ENDDATE~></div>
    <div><input type="time" id="stoptime" name="webendtime" value=~ENDTIME~></div>
  
    <div><input type="reset"></div>
    <div><input type="submit" value="Submit"></div>
  </div>
  </form>
  
  </body>
  </html>
  )rawliteral";

String processor(const String& var)
{
  if(var == "ONOFF")
  {
    if (onoff==true)
    {
      return String("checked");
    }
  }
  
  if(var == "CALLSIGN")
  {
      return callsign;  
  }
  
  if(var == "TXFREQUENCY")
  {
      return String(txfrequency,3);  
  }
  
  if(var == "RXFREQUENCY")
  {
      return String(rxfrequency,3);  
  }
  
  if(var == "BANDWIDTH")
  {
    if (bandwidth == 0)
    {
      char temp[70];
      return (String("<option value=\"0\" selected>12.5K</option><option value=\"1\">25K</option>"));
    }
    else if (bandwidth == 1)
    {
      return (String("<option value=\"0\">12.5K</option><option value=\"1\" selected>25K</option>"));
    }
  } 

  if (var == "VOLUME")
  {
    String temp;
    for (int i = 1; i <= 8; i++)
    {
      temp.concat("<option value=\"");
      temp.concat(i);
      temp.concat("\"");
      if (i == volume)
      {
        temp.concat(" selected");
      }
      temp.concat(">");
      temp.concat(i);
      temp.concat("</option>");
    }
    return temp;
  }

  if (var == "SQUELCH")
  {
    String temp;
    for (int i = 0; i <= 8; i++)
    {
      temp.concat("<option value=\"");
      temp.concat(i);
      temp.concat("\"");
      if (i == squelch)
      {
        temp.concat(" selected");
      }
      temp.concat(">");
 
      if (i == 0)
      {
        temp.concat("off");
      }
      else
      {
        temp.concat(i);
      }
      temp.concat("</option>");
    }
    return temp;
  }

  if (var == "CURRENTDATE")
  {
    char temp[20];
    strftime(temp, 20, "%Y-%m-%d", &tmcurrenttime);
    return temp;
  }

  if (var == "CURRENTTIME")
  {
    char temp[20];
    strftime(temp, 20, "%H:%M", &tmcurrenttime);
    return temp;
  }
  
  if (var == "STARTDATE")
  {
    char temp[20];
    strftime(temp, 20, "%Y-%m-%d", &tmstarttime);
    return temp;
  }
  
  if (var == "STARTTIME")
  {
    char temp[20];
    strftime(temp, 20, "%H:%M", &tmstarttime);
    return temp;
  }
  
  if (var == "ENDDATE")
  {
    char temp[20];
    strftime(temp, 20, "%Y-%m-%d", &tmendtime);
    return temp;
  }
  
  if (var == "ENDTIME")
  {
    char temp[20];
    strftime(temp, 20, "%H:%M", &tmendtime);
    return temp;
  }
 
  return String();
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

// NeoPixel Settings
#define NeoPixel_PIN 18
Adafruit_NeoPixel NeoPixel(1, NeoPixel_PIN, NEO_GRB + NEO_KHZ800);

// ESP32-S2 Settings
#define HL_Pin 0
#define PTT_Pin 1 
#define PD_Pin 2  

void setup() 
{
  // Set pins
  pinMode(PTT_Pin, OUTPUT);
  pinMode(PD_Pin, OUTPUT);
  pinMode(HL_Pin, OUTPUT);
  digitalWrite(PTT_Pin, HIGH); // LOW is RX, High is TX, it's confusing??
  digitalWrite(PD_Pin, LOW); // LOW for power down mode, HIGH for normal mode??
  digitalWrite(HL_Pin, LOW); // LOW for .5w, HIGH for 1w??
  
  // Start Serial for debugging
  Serial.begin(115200); // Serial for ESP32-S2 board
  Serial1.begin(9600, SERIAL_8N1, 7, 8); // Serial for SA818 using pins GPIO7 & GPIO8
  
  // Start NeoPixel and set it to off
  NeoPixel.setBrightness(1); // 1-255
  NeoPixel.begin();
  NeoPixel.show();
  
  // Start Wifi and put in AP mode
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password, channel);

  // Start mDNS to allow yafb.local to work
  if (!MDNS.begin("yafb")) 
  {
      Serial.println("Error setting up MDNS responder!");
      while(1) {
          delay(1000);
      }
  }

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send_P(200, "text/html", index_html, processor);
  });


  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request)
  {
    if (request->hasParam("webonoff")) 
    {
      onoff = request->getParam("webonoff")->value();
      Serial.print("onoff value: ");
      Serial.println(onoff);
    }
    else
    {
      onoff = 0; // HTML doesn't submit false with checkboxes, only true
      Serial.print("onoff value: ");
      Serial.println(onoff);
    }
    
    if (request->hasParam("webcallsign")) 
    {
      String temp;
      temp = request->getParam("webcallsign")->value();
      int stringlength = temp.length() + 1;
      temp.toCharArray(callsign, stringlength);
      Serial.print("webcallsign value: ");
      Serial.println(callsign);
    }
    
    if (request->hasParam("webtxfrequency")) 
    {
      txfrequency = (request->getParam("webtxfrequency")->value()).toFloat();
      Serial.print("webtxfrequency value: ");
      Serial.println(txfrequency, 3);
    }
    
    if (request->hasParam("webrxfrequency")) 
    {
      rxfrequency = (request->getParam("webrxfrequency")->value()).toFloat();
      Serial.print("webrxfrequency value: ");
      Serial.println(rxfrequency, 3);
    }
    
    if (request->hasParam("webbandwidth")) 
    {
      bandwidth = (request->getParam("webbandwidth")->value()).toInt();
      Serial.print("webbandwidth value: ");
      Serial.println(bandwidth);
    }
 
    if (request->hasParam("webvolume")) 
    {
      volume = (request->getParam("webvolume")->value()).toInt();
      Serial.print("webvolume value: ");
      Serial.println(volume);
    }

    if (request->hasParam("websquelch")) 
    {
      squelch = (request->getParam("websquelch")->value()).toInt();
      Serial.print("websquelch value: ");
      Serial.println(squelch);
    }

    if (request->hasParam("webcurrentdate")) 
    {
      String temp = request->getParam("webcurrentdate")->value();
      Serial.print("webcurrentdate value: ");
      Serial.println(temp);
      strptime(temp.c_str(), "%Y-%m-%d", &tmcurrenttime);
    }

    if (request->hasParam("webcurrenttime")) 
    {
      String temp = request->getParam("webcurrenttime")->value();
      Serial.print("webcurrenttime value: ");
      Serial.println(temp);
      strptime(temp.c_str(), "%H:%M", &tmcurrenttime);
    }
    
    if (request->hasParam("webstartdate")) 
    {
      String temp = request->getParam("webstartdate")->value();
      Serial.print("webstartdate value: ");
      Serial.println(temp);
      strptime(temp.c_str(), "%Y-%m-%d", &tmstarttime);
    }
    
    if (request->hasParam("webstarttime")) 
    {
      String temp = request->getParam("webstarttime")->value();
      Serial.print("webstarttime value: ");
      Serial.println(temp);
      strptime(temp.c_str(), "%H:%M", &tmstarttime);
    }
    
    if (request->hasParam("webenddate")) 
    {
      String temp = request->getParam("webenddate")->value();
      Serial.print("webenddate value: ");
      Serial.println(temp);
      strptime(temp.c_str(), "%Y-%m-%d", &tmendtime);
    }
    
    if (request->hasParam("webendtime")) 
    {
      String temp = request->getParam("webendtime")->value();
      Serial.print("webendtime value: ");
      Serial.println(temp);
      strptime(temp.c_str(), "%H:%M", &tmendtime);
    }
    
    time_t temp[30];
    strftime(temp, 30, "%H, %M, %S, %d, %m, %Y", &tmcurrenttime);
    Serial.print("test: ");
    Serial.println(temp);
    setTime(temp);

       
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field <br><a href=\"/\">Return to Home Page</a>");
  });
  
  server.onNotFound(notFound);
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void loop()
{
  delay(1000);
  printLocalTime();
}

// This function sets the NeoPixel to the RGB value given
void NeoPixelSet(uint32_t R, uint32_t G, uint32_t B)
{
    NeoPixel.setPixelColor(0, R, G, B); // 0 is for the first NeoPixel
    NeoPixel.show();
}

/*
 * These functions are for the SA818
 */

// send connnect command to SA818 
void SA818connect(void)
{
  char rxbuffer[20];  // buffer for response string
  byte rxlen=0;   // counter for received bytes
  do
  {
    Serial1.println("AT+DMOCONNECT");         // begin message
    rxlen=Serial1.readBytesUntil('\n',rxbuffer,19);
  } while(rxlen==0);    // send command until answer is received
  rxbuffer[rxlen-1]=0;  // check length of answer and remove cr character
  rxbuffer[rxlen]=0; // remove last byte and end string
  delay(1000);    // wait a little bit
}

// set txfrequency, rxfrequency, txctcss, rxctcss, bandwidth, squelch to SA818
void SA818setgroup(byte bw, float txfrequency, float rxfrequency, byte txctcss, byte squelch, byte rxctcss)
{
  Serial1.print("AT+DMOSETGROUP=");
  Serial1.print(bw);
  Serial1.print(",");
  Serial1.print(txfrequency);
  Serial1.print(",");
  Serial1.print(rxfrequency);
  Serial1.print(",00");
  if(txctcss<10) Serial1.print("0");
  Serial1.print(txctcss);
  Serial1.print(",");    
  Serial1.print(squelch);
  Serial1.println(",00");
  if(rxctcss<10) Serial1.print("0");
}

// set volume to SA818
void SA818setvolume(byte volume)
{
  Serial1.print("AT+DMOSETVOLUME=");
  Serial1.println(volume);
}

// set filters to SA818
void SA818setfilter(byte prefilter, byte highpassfilter, byte lowpassfilter)
{
  Serial1.print("AT+SETFILTER=");
  Serial1.print(prefilter);
  Serial1.print(",");
  Serial1.print(highpassfilter);
  Serial1.print(",");
  Serial1.println(lowpassfilter);
}

// set tail to SA818
void SA818settail(byte tailtone)
{
  Serial1.print("AT+SETTAIL=");
  Serial1.println(tailtone);
}
