/*
  FSECanada DHT11 Module


  Firmware for FSECanada DHT11 Temperature and Humidity module
  This firmware uses automatic configuration for your Wifi. Details can be found in the URL Below:
   - https://learn.fullstackelectron.com/index.php?option=com_content&view=article&id=5
   
  
  Supportted Iots:
   - Thingspeak - https://thingspeak.com/
  Created April 3, 2021 May 2014
   - V1.0 - Initial firmware
  by FSECanada

  This example code is in the public domain.

*/

#include <dht.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <FS.h>

dht DHT;

#define DHT11_PIN 2
#define NETWORK "FSEDHT11"
WiFiManager wifiManager;
WiFiManagerParameter custom_iot_server_url("iot_server_url", "Iot Server URL", "https://api.thingspeak.com", 40);
WiFiManagerParameter custom_iot_apikey_write("iot_apikey_write", "iot Apikey write", "YOUR API KEY", 40);
WiFiManagerParameter custom_device_name("device_name", "Device Name", "ESP01-DHT11 Module", 40);

extern "C" {
  #include "user_interface.h"
  }

extern "C" {
   #include "gpio.h"
 }
 
typedef struct {
  char minutes_deep_sleep[500];
  char iot_url[500];
  char iot_api_key_write[500];
  char device_name[200];
} settingsData;

settingsData settingsdata;

void settingsWrite() {
  StaticJsonDocument<200> jsonDocument;
  jsonDocument["iot_url"] = settingsdata.iot_url;
  jsonDocument["iot_api_key_write"] = settingsdata.iot_api_key_write;
  jsonDocument["minutes_deep_sleep"] = settingsdata.minutes_deep_sleep;
  jsonDocument["device_name"] = settingsdata.device_name;
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
    return;
  }

  Serial.println("Writting...");
  serializeJson(jsonDocument, Serial);
  // Serialize JSON to file
  if (serializeJson(jsonDocument, configFile) == 0) {
    Serial.println(F("Failed to write to file with code:"));
  }

  configFile.close();
}

void settingsRead() {
  if (SPIFFS.begin()) {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json")) {
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        StaticJsonDocument<500> jsonDoc;
        DeserializationError err = deserializeJson(jsonDoc, configFile);
        if (!err) {
          Serial.println("\nparsed json");
          strcpy(settingsdata.iot_url, jsonDoc["iot_url"]);
          strcpy(settingsdata.iot_api_key_write, jsonDoc["iot_api_key_write"]);
          strcpy(settingsdata.minutes_deep_sleep, jsonDoc["minutes_deep_sleep"]);
          strcpy(settingsdata.device_name, jsonDoc["device_name"]);
          Serial.print("Url settings:");
          Serial.println(settingsdata.iot_url);
          Serial.print("APIKEY settings:");
          Serial.println(settingsdata.iot_api_key_write);
          Serial.print("Minutes settings:");
          Serial.println(settingsdata.minutes_deep_sleep);
        } else {
          Serial.print("failed to load json config with code:");
          Serial.println(err.c_str());
        }
        configFile.close();
      }
    }
  }
}

void saveConfigCallback() {
  strcpy(settingsdata.iot_url, custom_iot_server_url.getValue());
  strcpy(settingsdata.iot_api_key_write, custom_iot_apikey_write.getValue());
  strcpy(settingsdata.device_name, custom_device_name.getValue());
  settingsWrite();
}

void resetFactory() {
  SPIFFS.format(); // cleaning the file system
  wifiManager.resetSettings(); // cleaning wifi stored settings
}

void initWifi() {
  if(!wifiManager.autoConnect(NETWORK)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
  }
  settingsRead();
}

void setup() {
  /* 
   * Uncomment the line below to reset the WIFI configuation 
   * This is required if you have a new WIFI configution
   */
  //resetFactory();
  Serial.begin(115200);
  Serial.println("DHT TEST PROGRAM");
  Serial.print("LIBRARY VERSION: ");
  Serial.println(DHT_LIB_VERSION);
  Serial.println();
  Serial.println("Type,\tstatus,\tHumidity (%),\tTemperature (C)");
  SPIFFS.begin();
  gpio_init(); // Initilise GPIO pins
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_iot_server_url);
  wifiManager.addParameter(&custom_iot_apikey_write);
  wifiManager.addParameter(&custom_device_name);
  wifiManager.setConfigPortalTimeout(120);
  initWifi();  
  WiFi.mode(WIFI_STA); // change wifi mode to use less power
}


String getUrl(float temp, float hum) {
  String url = "/update?api_key=";
  url += settingsdata.iot_api_key_write;
  char buff[10];
  dtostrf(temp, 2, 1, buff);
  url += "&field1=";
  url += buff;
  dtostrf(hum, 2, 1, buff);
  url += "&field2=";
  url += buff;
  url += "&field3=";
  url += settingsdata.device_name;
  Serial.print("URL,\t"); 
  Serial.println(url);
  return url;
}

void sendData(float temp, float hum) {
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  const char * host = settingsdata.iot_url;
  if (https.begin(*client, String(host) + getUrl(temp, hum))) {
  int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        // file found at server?
        if (httpCode == HTTP_CODE_OK) {
          String payload = https.getString();
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n\r", https.errorToString(httpCode).c_str());
      }

      https.end();
  }
}
 
void loop() {
  WiFi.forceSleepWake();
  initWifi();
  // READ DATA
  Serial.print("DHT11, \t");
  int chk = DHT.read11(DHT11_PIN);
  switch (chk)
  {
    case DHTLIB_OK:  
    Serial.print("OK,\t"); 
    break;
    case DHTLIB_ERROR_CHECKSUM: 
    Serial.print("Checksum error,\t"); 
    break;
    case DHTLIB_ERROR_TIMEOUT: 
    Serial.print("Time out error,\t"); 
    break;
    default: 
    Serial.print("Unknown error,\t"); 
    break;
  }
  // DISPLAY DATA
  Serial.print(DHT.humidity, 1);
  Serial.print(",\t");
  Serial.println(DHT.temperature, 1);

  sendData(DHT.temperature, DHT.humidity);
  WiFi.forceSleepBegin();
  delay(60000);
}
