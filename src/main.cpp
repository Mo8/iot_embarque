#include <soc/rtc.h>
#include "./internal_temp.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

const char *ssid = "esp32_wifi";
const char *password = "wifi1234";
String serverip = "192.168.43.150:3000";
RTC_DATA_ATTR byte tab_temp[50];
RTC_DATA_ATTR byte counter = 0;
RTC_DATA_ATTR byte tempFreq = 10;        // frequence lecture
RTC_DATA_ATTR byte connectionFreq = 10;  // frequence envoie
RTC_DATA_ATTR byte connectionConfig = 3; // frequence envoie

WiFiClient espClient;
PubSubClient client(espClient);

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

void updateConfig(StaticJsonDocument<512> doc){

    tempFreq = doc["tempFreq"];

    connectionFreq = doc["connectionFreq"];

    connectionConfig = doc["connectionConfig"];

    Serial.println(String("tf = ") + tempFreq);
    Serial.println(String("cg = ") + connectionConfig);
    Serial.println(String("cf = ") + connectionFreq);
}


void sendDataHttp()
{
  HTTPClient http;
  String url = "http://" + serverip + "/host/api/Esp32/oui";
  http.begin(url.c_str());
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"config\":{\"tempFreq\":" + String(tempFreq) + ",\"connectionConfig\":" + String(connectionConfig) + ",\"connectionFreq\":" + String(connectionFreq) + "},\"temperatures\":[";
  for (int i = 0; i < counter; i++)
  {
    // payload += "{\"temp\":"+String(tab_temp[i].temp)+",\"timestamp\":"+String(tab_temp[i].time)+"}";
    payload += String(tab_temp[i]);
    if (i < counter - 1)
      payload += ",";
  }
  payload += "]}";
  int res = http.PUT(payload);

  if (res > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(res);
    String payload = http.getString();
    Serial.println(payload);
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.println(F("Failed to read file, using default configuration"));
      return;
    }
    updateConfig(doc);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(res);
  }
  Serial.printf("HTTP PUT result: %d\n", res);
  http.end();
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "/ynov/esp32-CAUTELA/out") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
     // digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      //digitalWrite(ledPin, LOW);
    }
  }
}

void sendDataMqtt(){
  
  client.setServer("broker.hivemq.com", 1883);
  client.setCallback(callback);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("my_idjkhjkgkjghjgh656456465")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("/ynov/esp32-CAUTELA/out");
      client.publish("/ynov/esp32-CAUTELA/in","ajdskjskdfjsjldjf");
    } else {
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
  byte temp2 = readTemp2(false);
  // unsigned long time = getTime();
  Serial.printf("Temp2 : %u, Wifi status %s\n Compteur : %u\n", temp2, WiFi.status() == WL_CONNECTED ? "connected" : "disconnected", counter);
  tab_temp[counter] = temp2;

  // start esp light sleep of 5s
  if (counter % (connectionFreq / tempFreq) == 0)
  {
    Serial.println("Sending data");
    initWifi();
    //sendDataHttp();
    sendDataMqtt();
    counter = 0;
  }
  Serial.flush();
  counter++;
  esp_sleep_enable_timer_wakeup(tempFreq * 1000000);
  esp_deep_sleep_start();
}
