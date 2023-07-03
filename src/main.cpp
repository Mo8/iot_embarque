#include <soc/rtc.h>
#include "./internal_temp.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoBLE.h>
#define addr_temp_freq 0
#define addr_connection_freq 1
#define addr_connection_config 2
#define EEPROM_SIZE 12

const String name = "my_name";
const char *ssid = "esp32_wifi";
const char *password = "wifi1234";
String serverip = "192.168.43.150:3000";
RTC_DATA_ATTR char tab_temp[50];
RTC_DATA_ATTR byte counter = 0;
RTC_DATA_ATTR unsigned long lastTimeConnection = 0;
RTC_DATA_ATTR unsigned long lastTimeTemp = 0;
byte tempFreq = 10;        // frequence lecture
byte connectionFreq = 30;  // frequence envoie
byte connectionConfig = 2; // frequence envoie

WiFiClient espClient;
PubSubClient client(espClient);

BLEService configService("7f48c732-1011-11ee-be56-0242ac120002");
BLEByteCharacteristic tempFreqCharacteristique("8f094da4-1011-11ee-be56-0242ac120002", BLERead | BLEWrite | BLENotify);
BLEByteCharacteristic connectionConfigCharacteristique("8f094da4-1011-11ee-be56-0242ac120004", BLERead | BLEWrite | BLENotify);
BLEByteCharacteristic connectionFreqCharacteristique("8f094da4-1011-11ee-be56-0242ac120003", BLERead | BLEWrite | BLENotify);
BLECharCharacteristic tempCharacteristique("6bdc4610-1f81-42f0-9a50-467554def186", BLERead | BLENotify);

void printConfig()
{
  Serial.println(String("tf = ") + tempFreq);
  Serial.println(String("cc = ") + connectionConfig);
  Serial.println(String("cf = ") + connectionFreq);
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

void setTempFreq(byte freq)
{
  tempFreq = freq != 10 && freq != 20 && freq != 60 ? 10 : freq;
  EEPROM.put(addr_temp_freq, tempFreq);
}

void setConnectionConfig(byte config)
{
  connectionConfig = config != 1 && config != 2 && config != 3 ? 2 : config;
  EEPROM.put(addr_connection_config, connectionConfig);
}

void setConnectionFreq(byte freq)
{
  connectionFreq = freq != 20 && freq != 30 && freq != 180 ? 20 : freq;
  EEPROM.put(addr_connection_freq, connectionFreq);
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
  // Init EEPROM
  EEPROM.begin(EEPROM_SIZE);
  byte t_tempFreq = EEPROM.read(addr_temp_freq);             // frequence lecture
  byte t_connectionFreq = EEPROM.read(addr_connection_freq); // frequence envoie
  byte t_connectionConfig = EEPROM.read(addr_connection_config);
  setTempFreq(t_tempFreq);
  setConnectionConfig(t_connectionConfig);
  setConnectionFreq(t_connectionFreq);

  printConfig();

  initWifi();
  if (!BLE.begin())
  {
    Serial.println("failed to initialize BLE!");
    while (1)
      ;
  }
  BLE.setDeviceName("esp32_ble_c");
  BLE.setLocalName("esp32_ble_local");
  configService.addCharacteristic(tempFreqCharacteristique);
  configService.addCharacteristic(connectionConfigCharacteristique);
  configService.addCharacteristic(connectionFreqCharacteristique);
  configService.addCharacteristic(tempCharacteristique);
  BLE.addService(configService);

  tempFreqCharacteristique.writeValue(tempFreq);
  connectionConfigCharacteristique.writeValue(connectionConfig);
  connectionFreqCharacteristique.writeValue(connectionFreq);
  tempCharacteristique.writeValue(0);

  BLE.advertise();

  Serial.println("advertising ...");
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

void updateConfig(String payload)
{
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.println(F("Failed to read file, using default configuration"));
    return;
  }
  setTempFreq(doc["tempFreq"]);

  setConnectionFreq(doc["connectionFreq"]);

  setConnectionConfig(doc["connectionConfig"]);

  EEPROM.commit();
  printConfig();
}

String generateJsonData()
{
  String payload = "{\"config\":{\"tempFreq\":" + String(tempFreq) + ",\"connectionConfig\":" + String(connectionConfig) + ",\"connectionFreq\":" + String(connectionFreq) + "},\"temperatures\":[";
  for (int i = 0; i < counter; i++)
  {
    // payload += "{\"temp\":"+String(tab_temp[i].temp)+",\"timestamp\":"+String(tab_temp[i].time)+"}";
    payload += String(int(tab_temp[i]));
    if (i < counter - 1)
      payload += ",";
  }
  payload += "]}";
  return payload;
}

void sendDataHttp()
{
  HTTPClient http;
  String url = "http://" + serverip + "/host/api/Esp32/"+name;
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

    //updateConfig(payload);
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
    if (client.connect("my_id75468239710"))
    {
      Serial.println("connected");
      // Subscribe
      Serial.println(client.subscribe(String("/ynov/esp32-CAUT/out/"+name).c_str()));
      delay(400);

      client.publish(String("/ynov/esp32-CAUT/in/"+name).c_str(), generateJsonData().c_str());
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
  BLE.poll();
  if (BLE.connected())
  {
    if (tempFreqCharacteristique.written())
    {
      setTempFreq(tempFreqCharacteristique.value());
      Serial.println("modif tf : " + String(tempFreq));
      EEPROM.commit();
    }
    if (connectionConfigCharacteristique.written())
    {
      setConnectionConfig(connectionConfigCharacteristique.value());
      Serial.println("modif cc : " + String(connectionConfig));
      EEPROM.commit();
    }
    if (connectionFreqCharacteristique.written())
    {
      setConnectionFreq(connectionFreqCharacteristique.value());
      Serial.println("modif cf : " + String(connectionFreq));
      EEPROM.commit();
    }
  }
  if (millis() - lastTimeTemp > tempFreq * 1000)
  {

    char temp2 = readTemp2(false);
    Serial.printf("Temp2 : %u, Wifi status %s\n Compteur : %u\n", temp2, WiFi.status() == WL_CONNECTED ? "connected" : "disconnected", counter);
    tab_temp[counter] = temp2;
    tempCharacteristique.writeValue(temp2);
    counter++;
    lastTimeTemp = millis();
  }

  // start esp light sleep of 5s
  if (millis() - lastTimeConnection > connectionFreq * 1000)
  {

    // initWifi();
    Serial.println("Sending data");
    if (connectionConfig == 1 || connectionConfig == 3)
      sendDataHttp();
    if (connectionConfig == 2 || connectionConfig == 3)
      sendDataMqtt();

    lastTimeConnection = millis();
    counter = 0;
  }
  Serial.flush();

  // esp_sleep_enable_timer_wakeup(tempFreq * 1000000);
  // esp_deep_sleep_start();
}
