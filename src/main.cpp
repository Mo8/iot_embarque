#include <soc/rtc.h>
#include "./internal_temp.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#define addr_temp_freq 0
#define addr_connection_freq 1
#define addr_connection_config 2
#define EEPROM_SIZE 12

const char *ssid = "esp32_wifi";
const char *password = "wifi1234";
String serverip = "192.168.43.150:3000";
RTC_DATA_ATTR byte tab_temp[50];
RTC_DATA_ATTR byte counter = 0;
byte tempFreq = 10;// frequence lecture
byte connectionFreq = 30;// frequence envoie
byte connectionConfig = 2;// frequence envoie

WiFiClient espClient;
PubSubClient client(espClient);

void printConfig(){
  Serial.println(String("tf = ") + tempFreq);
  Serial.println(String("cg = ") + connectionConfig);
  Serial.println(String("cf = ") + connectionFreq);
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // configure RTC slow clock to internal oscillator, fast clock to XTAL divided by 4
  rtc_clk_slow_freq_set(RTC_SLOW_FREQ_RTC);
  rtc_clk_fast_freq_set(RTC_FAST_FREQ_XTALD4);

  // read CPU speed
  rtc_cpu_freq_config_t freq_config;
  rtc_clk_cpu_freq_get_config(&freq_config);
//Init EEPROM
  EEPROM.begin(EEPROM_SIZE);
 byte t_tempFreq = EEPROM.read(addr_temp_freq) ;        // frequence lecture
 byte t_connectionFreq = EEPROM.read(addr_connection_freq); // frequence envoie
 byte t_connectionConfig = EEPROM.read(addr_connection_config);
 
 tempFreq = t_tempFreq != 10 && t_tempFreq != 20 && t_tempFreq != 60 ? 10 : t_tempFreq;
 connectionFreq = t_connectionFreq != 20 && t_connectionFreq != 30 && t_connectionFreq != 180 ? 20 : t_connectionFreq;
 connectionConfig = t_connectionConfig != 1 && t_connectionConfig != 2 && t_connectionConfig != 3 ? 2 : t_connectionConfig;
 printConfig();
}

unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

void initWifi()
{
  WiFi.mode(WIFI_STA); // Optional
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.print("\nConnected to the WiFi network");
  Serial.println(WiFi.getAutoConnect());
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void updateConfig(String payload)
{
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.println(F("Failed to read file, using default configuration"));
    return;
  }
  tempFreq = doc["tempFreq"];
  EEPROM.put(addr_temp_freq, tempFreq);

  connectionFreq = doc["connectionFreq"];
  EEPROM.put(addr_connection_freq, connectionFreq);

  connectionConfig = doc["connectionConfig"];
  EEPROM.put(addr_connection_config, connectionConfig);

  EEPROM.commit();
  printConfig();

  
}

String generateJsonData()
{
  String payload = "{\"config\":{\"tempFreq\":" + String(tempFreq) + ",\"connectionConfig\":" + String(connectionConfig) + ",\"connectionFreq\":" + String(connectionFreq) + "},\"temperatures\":[";
  for (int i = 0; i < counter; i++)
  {
    // payload += "{\"temp\":"+String(tab_temp[i].temp)+",\"timestamp\":"+String(tab_temp[i].time)+"}";
    payload += String(tab_temp[i]);
    if (i < counter - 1)
      payload += ",";
  }
  payload += "]}";
  return payload;
}

void sendDataHttp()
{
  HTTPClient http;
  String url = "http://" + serverip + "/host/api/Esp32/oui";
  http.begin(url.c_str());
  http.addHeader("Content-Type", "application/json");
  String payload = generateJsonData();
  int res = http.PUT(payload);

  if (res > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(res);
    String payload = http.getString();
    Serial.println(payload);

    updateConfig(payload);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(res);
  }
  Serial.printf("HTTP PUT result: %d\n", res);
  http.end();
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "/ynov/esp32-CAUTELA/out")
  {
    updateConfig(messageTemp);
  }
}

void sendDataMqtt()
{

  client.setServer("broker.hivemq.com", 1883);
  client.setCallback(callback);
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("my_idjkhjkgkjghjgh656456465"))
    {
      Serial.println("connected");
      // Subscribe
      Serial.println(client.subscribe("/ynov/esp32-CAUT/out"));
      delay(400);

      client.publish("/ynov/esp32-CAUT/in", generateJsonData().c_str());
      delay(1000);
      client.loop();
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop()
{

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  byte temp2 = readTemp2(false);

  // unsigned long time = getTime();
  Serial.printf("Temp2 : %u, Wifi status %s\n Compteur : %u\n", temp2, WiFi.status() == WL_CONNECTED ? "connected" : "disconnected", counter);
  tab_temp[counter] = temp2;

  // start esp light sleep of 5s
  if (counter % (connectionFreq / tempFreq) == 0)
  {
    Serial.println("Sending data");
    initWifi();
    if (connectionConfig == 1 || connectionConfig == 3)
      sendDataMqtt();
    if (connectionConfig == 2 || connectionConfig == 3)
      sendDataHttp();

    counter = 0;
  }
  Serial.flush();
  counter++;
  esp_sleep_enable_timer_wakeup(tempFreq * 1000000);
  esp_deep_sleep_start();
}
