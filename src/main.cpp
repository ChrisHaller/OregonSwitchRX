#include <Arduino.h>
#include "RCSwitch.h"
#include <DHT.h>
#include "Settings.h"


#if defined(ESP8266)
#pragma message "ESP8266 stuff happening!"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
ESP8266WebServer webServer(80);
#elif defined(ESP32)
#pragma message "ESP32 stuff happening!"
#include <WiFi.h>
#include <WebServer.h>
WebServer webServer(80);
#else
#error "This ain't a ESP8266 or ESP32, dumbo!"
#endif


#include <PubSubClient.h>
#include <time.h> 
#include <strings.h>
#include "OregonDecoder.h"
#include "private.h"

#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"


#include <Wire.h>
#include "Adafruit_MCP9808.h"


#define SERIAL_DEBUG

//#define MY_DEBUG
#define LIGHT_SWITCH
#define MHZ_RECEIVER
//#define DHT_SENSOR
//#define BME680_SENSOR
//#define MCP9808_SENSOR

String RXId = "rx20_buero_test1";




// GPIOs



// ESP-01
#if BUILD_ENV_VAL==1
#define PIN_433MHZ_RECEIVER 3
#ifdef LIGHT_SWITCH
#define PIN_LIGHT_1 2
#define PIN_LIGHT_2 0
#endif

#endif

// nodemcu
#if BUILD_ENV_VAL==2
#define PIN_433MHZ_RECEIVER 13

#ifdef LIGHT_SWITCH
#define PIN_LIGHT_1 14
#define PIN_LIGHT_2 15
#endif

#endif



// ESP32
#if BUILD_ENV_VAL==3

#define PIN_433MHZ_RECEIVER 27
#ifdef LIGHT_SWITCH
#define PIN_LIGHT_1 26
#define PIN_LIGHT_2 25
#endif

#endif

#ifdef DHT_SENSOR
// DHT Constants
#define DHTPIN 19     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
#define MODE_0 0 // Temperature only [THN132N]
#define MODE_1 1 // Temperature + Humidity [THGR2228N]
#define MODE_2 2 // Temperature + Humidity + Baro() [BTHR918N]
#define MODE MODE_1
// End DHT Constants

DHT dht(DHTPIN, DHTTYPE);

#endif

#define SEALEVELPRESSURE_HPA (1013.25)

#ifdef BME680_SENSOR
Adafruit_BME680 bme; // I2C
#endif

#ifdef MCP9808_SENSOR
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
#endif



// WiFi

/*  in private.h
const int NUM_SIDS = 5;
const char* ssids [NUM_SIDS]
        = { "MW6-IoT", "MW6", "MW6-Keller",  };

const char* wlanPasswords [NUM_SIDS]
        = { "**", "**", "**", "**", "**" };

*/

// MQTT 

String topicOutOregon = "sensors/oregonrx/P";
String topicOutOregonBadChecksum = "sensors/oregonrx_bad_checksum/P";
String topicOutRfData = "tele/" + RXId;
String topicInGPIO = "tele/" + RXId + "/GPIO/#";


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;



/* Globals */
time_t now;                         // this is the epoch
tm tm;                              // the structure tm holds time information in a more convient way

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
OregonDecoder oregonDecoder;

RCSwitch rcSwitch = RCSwitch();
unsigned long lastValue;
unsigned long lastMicrosValueReceived;

Settings settings;

// Wird bei jedem Interrupt gerufen. Schreibt die Anzahl ms, welche seit dem letzten Interrupt vergangen sind, nach "pulse"
 IRAM_ATTR void RxInterruptHandler(void)
{
  oregonDecoder.SetPulse();
  RCSwitch::handleInterrupt();
}

#ifdef LIGHT_SWITCH

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

boolean isValidUnsignedInteger(String str)
{
  if(str == "")
    return false;

  for(byte i=0; i<str.length() ;i++)
  {
    if(!isDigit(str.charAt(i))) 
      return false;
  }
  return true;
}


// mqtt-message empfangen
// Bsp: tele/rx4/GPIO/5  payload=1
void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
  char payLoadBuffer[256];
  memset(payLoadBuffer, 0, sizeof(payLoadBuffer));

#ifdef SERIAL_DEBUG
  Serial.print("MQTT message arrived [");
  Serial.print(topic);
  Serial.print("] ");
#endif

  if(length > 0 && length < sizeof(payLoadBuffer))
  {
    for (unsigned int i = 0; i < length; i++) 
    {
      payLoadBuffer[i] = ((char)payload[i]);
    }

    String payloadValue = payLoadBuffer;
#ifdef SERIAL_DEBUG
    Serial.print(payloadValue);
    Serial.println();
#endif
    String strTopic = topic;

    if(strTopic.length() >= topicInGPIO.length())
    {
      // Beginn des Topics entfernen
      // Bsp: aus "tele/rx4/GPIO/5" wird "5"
      String subTopic = strTopic.substring(topicInGPIO.length() - 1);
#ifdef SERIAL_DEBUG
      Serial.println("Subtopic: " + subTopic);
#endif
      String strGPIO = getValue(subTopic, '/', 0);
#ifdef SERIAL_DEBUG
      Serial.println(strGPIO);
      Serial.println(payLoadBuffer);
#endif
      if(isValidUnsignedInteger(strGPIO))
      {
        int gpio = strGPIO.toInt();
        if(payloadValue == "0")
        {
          digitalWrite(gpio, LOW);
#ifdef SERIAL_DEBUG
          Serial.println("Set gpio " + String(gpio) + " to low");
#endif          
        }
        if(payloadValue == "1")
        {
          digitalWrite(gpio, HIGH);
#ifdef SERIAL_DEBUG          
          Serial.println("Set gpio " + String(gpio) + " to high");
#endif          
        }
      }
    }
  }
 }

#endif

void PrintTime()
{
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
#ifdef SERIAL_DEBUG
  Serial.printf("%4d-%02d-%02d %d:%02d:%02d\n\r", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif  
}

void ConnectWifi()
{
  bool connected = false;
  WiFi.mode(WIFI_STA);

#if defined(ESP8266)
  WiFi.hostname(RXId.c_str());
#elif defined(ESP32)
  WiFi.setHostname(RXId.c_str());
#endif


  while(!connected)
  {
    for(int i = 0; i < NUM_SIDS; i++)
    {
      WiFi.begin(ssids[i], wlanPasswords[i]);
      for(int y = 0; y < 10; y++)
      {
#ifdef SERIAL_DEBUG        
          Serial.println("Connecting to WiFi " + String(ssids[i]) + "..");
#endif          
          if(WiFi.status() == WL_CONNECTED)
          {
            connected = true;


#ifdef SERIAL_DEBUG
            Serial.println("Connected to the WiFi network " + String(ssids[i]));
#endif            
            break;
          }
          delay(500);
      }
      if(connected)
      {
        break;
      }
    }
  }
  
}


void ConnectMqtt()
{
  settings.ReadPrefs();

  mqttClient.setServer(settings.GetMQTTServername(), settings.GetMQTTPort());

  mqttClient.setKeepAlive(75);

  if (WiFi.status() != WL_CONNECTED)
  {
    ConnectWifi();
  }


  int numtries = 5;
  while (!mqttClient.connected()) 
  {
    numtries--;
    if(numtries == 0) break;

    String client_id = "oregonrx-"+RXId+"-";
    client_id += String(WiFi.macAddress());
#ifdef SERIAL_DEBUG    
    Serial.printf("The client %s connects to the mqtt broker\n", client_id.c_str());
#endif    
    if (mqttClient.connect(client_id.c_str(), settings.GetMQTTUsername(), settings.GetMQTTPassword())) 
    {
#ifdef LIGHT_SWITCH
        mqttClient.setCallback(mqttCallback);
        mqttClient.subscribe(topicInGPIO.c_str());
#ifdef SERIAL_DEBUG
        Serial.println("subscribed to " + topicInGPIO);
#endif        
#endif

#ifdef SERIAL_DEBUG
        Serial.println("mqtt broker '" + String(settings.GetMQTTServername()) + "' connected");
#endif        
    } 
    else 
    {
#ifdef SERIAL_DEBUG      
        Serial.print("mqtt connect failed with state ");
        Serial.println(mqttClient.state());
#endif        
        delay(2000);
    }
  }
}

void PublishOregonData(const char* topic, int channel, int id, float temperature, int humidity, const byte* rawData, bool ChecksumOkay, float pressure = 0, float gas = 0)
{
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  char DateAndTimeString[50];

  sprintf(DateAndTimeString, "%4d-%02d-%02d %d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);


 if(!mqttClient.connected())
  {
    ConnectMqtt();
  }


  if(!mqttClient.connected())
  {
#ifdef SERIAL_DEBUG    
    Serial.println("Error: mqttclient not connected!");
#endif    
  }


  String baseTopic = String(topic) + "/C" + String(channel) + "/ID" + String(id);
  mqttClient.publish((baseTopic + "/id").c_str(), String(id).c_str());
  mqttClient.publish((baseTopic + "/channel").c_str(), String(channel).c_str());
  mqttClient.publish((baseTopic + "/temperature_C").c_str(), String(temperature).c_str());

  if(humidity != 0)
  {
    mqttClient.publish((baseTopic + "/humidity").c_str(), String(humidity).c_str());
  }
  mqttClient.publish((baseTopic + "/time").c_str(), DateAndTimeString);
  mqttClient.publish((baseTopic + "/checksumokay").c_str(), String(ChecksumOkay).c_str());
  if(pressure != 0)
  {
    mqttClient.publish((baseTopic + "/pressure").c_str(), String(pressure).c_str());
  }

  if(pressure != 0)
  {
    mqttClient.publish((baseTopic + "/gas").c_str(), String(gas).c_str());
  }


  if(!mqttClient.publish((baseTopic + "/rxid").c_str(), RXId.c_str()))
  {
#ifdef SERIAL_DEBUG    
    Serial.println("Error: publish failed()");
#endif    
  }
  else
  {
#ifdef SERIAL_DEBUG    
    Serial.println("published to " + baseTopic);
#endif    
  }
}


void PublishRfData(const char* topic, unsigned long data)
{
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  char DateAndTimeString[50];
  char RfString[255];

  sprintf(DateAndTimeString, "%4d-%02d-%02dT%d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);


 if(!mqttClient.connected())
  {
    ConnectMqtt();
  }

  if(!mqttClient.connected())
  {
#ifdef SERIAL_DEBUG    
    Serial.println("Error: mqttclient not connected!");
#endif    
  }

  String baseTopic = String(topic) + "/RESULT";
  sprintf(RfString, "{\"Time\":\"%s\",\"RfReceived\":{\"Data\":\"%lx\"}}", DateAndTimeString, data);
  mqttClient.publish(baseTopic.c_str(), RfString);
#ifdef SERIAL_DEBUG
  Serial.println(RfString);
#endif  
}

unsigned long  lastDHTMillis = 0;

#ifdef DHT_SENSOR
void HandleDHT()
{
  unsigned long  currentMillis = millis();

  if(currentMillis - lastDHTMillis > (60 * 1000))
  {
    lastDHTMillis = millis();
    //Serial.println("get temp");

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    // Check if any reads failed and exit early (to try again).
    if (!isnan(humidity) || !isnan(temperature)) 
    {
      // Daten an mQQT Broker gesenden (wie wenn es Daten einem Oregon Sensor währen)
      PublishOregonData(topicOutOregon.c_str(), 99, 51, temperature, humidity, NULL, true);
    }
    else
    {
#ifdef SERIAL_DEBUG      
      Serial.println("invalid temp");
#endif      
    }

  }
}

#endif


// WebServer-Stuff

void webHandleRoot() {
  String ip = WiFi.localIP().toString();
  String Message = "Examples: </br>";
  Message += "Setings: http://" + ip + "/settings?mqttservername=1.2.3.4&mqttport=1883&mqttusername=mqtt&mqttpassword=joshua </br>";

  Message += "Hostname ";
  Message += RXId;
  
  Message += "  </br>";

  Message += "Buildinfo  ";

  Message +=  __DATE__ " " __TIME__;
  Message += "</br>";

#ifdef BME680_SENSOR
 Message += "BME680_SENSOR</br>";
#endif

#ifdef LIGHT_SWITCH
 Message += "LIGHT_SWITCH</br>";
#endif


#ifdef DHT_SENSOR
 Message += "DHT_SENSOR</br>";
#endif


#ifdef MHZ_RECEIVER
 Message += "MHZ_RECEIVER</br>";
#endif


#ifdef MCP9808_SENSOR
 Message += "MCP9808_SENSOR</br>";
#endif


  webServer.send(200, "text/html", Message);
}


void webHandleSettings() {
  String message = "\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";

  String newMQTTServerName = "";
  String newMQTTPort = "";
  String newMQTTUsername = "";
  String newMQTTPassword = "";
    
  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
    if(webServer.argName(i)=="mqttservername")
    {
      newMQTTServerName = webServer.arg(i);
    }

    if(webServer.argName(i)=="mqttport")
    {
      newMQTTPort = webServer.arg(i);
    }

    if(webServer.argName(i)=="mqttusername")
    {
      newMQTTUsername = webServer.arg(i);
    }

    if(webServer.argName(i)=="mqttpassword")
    {
      newMQTTPassword = webServer.arg(i);
    }

  }

  if(newMQTTServerName !="")
  {
    settings.SetMQTTServername(newMQTTServerName);
    if(mqttClient.connected())
    {
      mqttClient.disconnect();
    }
    ConnectMqtt();
  }

  webServer.send(200, "text/plain", message);
}





void webHandleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", message);
}




IRAM_ATTR void setup() {


// Wenn ESP-01
// https://arduino.stackexchange.com/questions/29938/how-to-i-make-the-tx-and-rx-pins-on-an-esp-8266-01-into-gpio-pins
//********** CHANGE PIN FUNCTION  TO GPIO **********
//GPIO 1 (TX) swap the pin to a GPIO.

//GPIO 3 (RX) swap the pin to a GPIO.

#if BUILD_ENV_VAL==1
  pinMode(1, FUNCTION_3); 
  pinMode(3, FUNCTION_3); 
#endif
//**************************************************

#ifdef SERIAL_DEBUG
  Serial.begin(19200);
#endif

  settings.Init();

#ifdef DHT_SENSOR

  pinMode(DHTPIN, INPUT);       // Input DHT Data   

  dht.begin();
#endif

  delay(500);


#ifdef SERIAL_DEBUG
 // while (!Serial);
#endif

#ifdef BME680_SENSOR
#ifdef SERIAL_DEBUG
  Serial.println(F("BME680 async test"));
#endif
  if (!bme.begin()) {
#ifdef SERIAL_DEBUG    
    Serial.println(F("Could not find a valid BME680 sensor, check wiring!"));
#endif    
    while (1);
  }
#ifdef SERIAL_DEBUG
   Serial.println("Found BME680!");
#endif
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  #endif
#ifdef MCP9808_SENSOR
  // Make sure the sensor is found, you can also pass in a different i2c
  // address with tempsensor.begin(0x19) for example, also can be left in blank for default address use
  // Also there is a table with all addres possible for this sensor, you can connect multiple sensors
  // to the same i2c bus, just configure each sensor with a different address and define multiple objects for that
  //  A2 A1 A0 address
  //  0  0  0   0x18  this is the default address
  //  0  0  1   0x19
  //  0  1  0   0x1A
  //  0  1  1   0x1B
  //  1  0  0   0x1C
  //  1  0  1   0x1D
  //  1  1  0   0x1E
  //  1  1  1   0x1F
  if (!tempsensor.begin(0x18)) {
    #ifdef SERIAL_DEBUG
    Serial.println("Couldn't find MCP9808! Check your connections and verify the address is correct.");
    #endif
    while (1);
  }
    
  #ifdef SERIAL_DEBUG
   Serial.println("Found MCP9808!");
#endif
  tempsensor.setResolution(2); // sets the resolution mode of reading, the modes are defined in the table bellow:
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms

#endif

  //Setup received data
  
  rcSwitch.enableReceive(PIN_433MHZ_RECEIVER);
  attachInterrupt(digitalPinToInterrupt(PIN_433MHZ_RECEIVER), RxInterruptHandler, CHANGE);

#ifdef LIGHT_SWITCH
  pinMode(PIN_LIGHT_1, OUTPUT);
  pinMode(PIN_LIGHT_2, OUTPUT);
#endif

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(500);

  ConnectWifi();

  //connecting to a mqtt broker

  ConnectMqtt();


  // WebServer-Stuff

  webServer.on("/", webHandleRoot);
  webServer.on("/settings", webHandleSettings);
  webServer.onNotFound(webHandleNotFound);

  webServer.begin();


}

#ifdef MCP9808_SENSOR
unsigned long  lastMPC9808illis = 0;
void HandleMCP9808()
{
  unsigned long  currentMillis = millis();
    if(currentMillis - lastMPC9808illis > (3 * 1000))
    {
      lastMPC9808illis = millis();
#ifdef SERIAL_DEBUG
      Serial.println("wake up MCP9808.... "); // wake up MCP9808 - power consumption ~200 mikro Ampere
#endif      
      tempsensor.wake();   // wake up, ready to read!

      // Read and print out the temperature, also shows the resolution mode used for reading.
      //Serial.print("Resolution in mode: ");
      //Serial.println (tempsensor.getResolution());
      float c = tempsensor.readTempC();
      //Serial.print("Temp: "); 
      //Serial.print(c, 4); 
      //Serial.println("*C"); 
      tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling


      PublishOregonData(topicOutOregon.c_str(), 98, 50, c, 0, NULL, true);

    }
}


#endif






#ifdef BME680_SENSOR

unsigned long  lastBME680Millis = 0;
void HandleBME680()
{

    unsigned long  currentMillis = millis();

    if(currentMillis - lastBME680Millis > (60 * 1000))
    {
      lastBME680Millis = millis();

    // Tell BME680 to begin measurement.
     bme.beginReading();


    if (!bme.endReading()) {

#ifdef SERIAL_DEBUG      
      Serial.println(F("Failed to complete reading :("));
#endif      
      return;
    }

    // Der BME680 liefert in meinem Fall eine zu hohe Temperatur. Deshalb wird sie hier korrigiert. Es könnte sein, dass das Problem mit einem anderen Sensor nicht auftritt. In diesem Fall könnte man die Korrektur hier entfernen.
    float realTemperature = bme.temperature - 2.8;

    // auf eine Dezimalstelle abrunden
    float roundedTemperature = static_cast<float>(static_cast<int>(realTemperature * 10.)) / 10.;
    PublishOregonData(topicOutOregon.c_str(), 99, 52, roundedTemperature, bme.humidity, NULL, true, bme.pressure / 100.0, bme.gas_resistance / 1000.0);
  }

}
#endif


  int lightSwitchDelayCounter = 0;
  int dhtDelayCounter = 0;
  int bme680DelayCounter = 0;

  int mcp9808DelayCounter = 0;

  int delayCounter = 0;

void loop() 
{


 delayCounter++;
  if(delayCounter > 1000000)
  {
    
    webServer.handleClient();
    delay(1);
  

    delayCounter = 0;
    
  }



#ifdef LIGHT_SWITCH
  // damit keine Bit-Pulse verpasst werden, darf die mqtt-Abfrage nicht bei jedem Loop ausgeführt werden
  lightSwitchDelayCounter++;
  if(lightSwitchDelayCounter > 50)
  {
    mqttClient.loop();
    lightSwitchDelayCounter = 0;
  }
#endif

#ifdef DHT_SENSOR
  // damit keine Bit-Pulse verpasst werden, darf die DHT-Abfrage nicht bei jedem Loop ausgeführt werden
  dhtDelayCounter++;
  if(dhtDelayCounter > 50)
  {
    dhtDelayCounter = 0;
    HandleDHT();
  }
#endif


#ifdef BME680_SENSOR
  // damit keine Bit-Pulse verpasst werden, darf die DHT-Abfrage nicht bei jedem Loop ausgeführt werden
  bme680DelayCounter++;
  if(bme680DelayCounter > 50)
  {
    bme680DelayCounter = 0;
    HandleBME680();
  }
#endif


#ifdef MCP9808_SENSOR
  // damit keine Bit-Pulse verpasst werden, darf die DHT-Abfrage nicht bei jedem Loop ausgeführt werden
  mcp9808DelayCounter++;
  if(mcp9808DelayCounter > 50)
  {
    mcp9808DelayCounter = 0;
    HandleMCP9808();
  }
#endif


  

  bool newPulsAvailable = false;
  word pulseWidth = oregonDecoder.GetPulseWidth();
  if(pulseWidth > 0)
  {
    newPulsAvailable = oregonDecoder.NextPulse(pulseWidth);
  }


  if (newPulsAvailable)
  {

#ifdef MY_DEBUG   
    Serial.println(' ');
    PrintTime();
    Serial.println("NewData: ");
    for (byte i = 0; i < 10; ++i) 
    {
          Serial.printf("%02x", oregonDecoder.m_Data[i]);
    }
    Serial.println(' ');
#endif

    oregonDecoder.DecodeMessage();

#ifdef MY_DEBUG
    Serial.println("NewData after reversing: ");
    for (byte i = 0; i < 10; ++i) 
    {
          Serial.printf("%02x", oregonDecoder.m_Data[i]);
    }
    Serial.println(' ');
#endif

    bool checkSumOkay = oregonDecoder.CheckChecksum();

    if(checkSumOkay)
    {
      PublishOregonData(topicOutOregon.c_str(), oregonDecoder.GetChannelNumber(), oregonDecoder.GetSensorId2(), oregonDecoder.GetTemperature(), oregonDecoder.GetHumidity(), oregonDecoder.m_Data, checkSumOkay);
    }
    else
    {
      //PublishOregonData(topicOutOregonBadChecksum.c_str(), oregonDecoder.GetChannelNumber(), oregonDecoder.GetSensorId2(), oregonDecoder.GetTemperature(), oregonDecoder.GetHumidity(), oregonDecoder.m_Data, checkSumOkay);
    }

#ifdef MY_DEBUG   
    oregonDecoder.PrintState();
#endif    

    oregonDecoder.ResetDecoder();
  }

  if (rcSwitch.available()) 
  {
    unsigned long  currentMicros = micros();
    unsigned long currentValue = rcSwitch.getReceivedValue();   

    bool acceptValue = false;
    if(currentValue != lastValue)
    {
      acceptValue = true;
    }
    else
    {
        // aktueller Wert entpricht dem letzten Wert
        unsigned long timeSinceLastValue = currentMicros - lastMicrosValueReceived;

        if(timeSinceLastValue > 1 * 1000 * 500)
        {
          acceptValue = true;
        }

    }


    if(acceptValue)
    {
#ifdef SERIAL_DEBUG      
      Serial.printf("New value received: %ld (0x%lx)\n\r", currentValue, currentValue);
#endif      
      PublishRfData(topicOutRfData.c_str(), currentValue);
    }
    else
    {
#ifdef SERIAL_DEBUG      
      Serial.printf("New value not accepted: %ld (0x%lx)\n\r", currentValue, currentValue);
#endif      
    }
    lastValue = currentValue;
    lastMicrosValueReceived = currentMicros;

    rcSwitch.resetAvailable();
  }

  
}