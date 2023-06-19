#include <soc/rtc.h>
#include "./internal_temp.h"
#include <HTTPClient.h>
#include <WiFi.h>

struct data
{
  double temp;
  unsigned long time;
};

const char *ssid = "esp32_wifi";
const char *password = "wifi1234";
String serverip = "192.168.43.150:3000";
RTC_DATA_ATTR double tab_temp[50];
RTC_DATA_ATTR unsigned int counter = 0;
RTC_DATA_ATTR unsigned int tempFreq = 10;        // frequence lecture
RTC_DATA_ATTR unsigned int connectionFreq = 30;  // frequence envoie
RTC_DATA_ATTR unsigned int connectionConfig = 3; // frequence envoie

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
  Serial.println("\nConnected to the WiFi network");
  Serial.print("\nConnected to the WiFi network");
  Serial.println(WiFi.getAutoConnect());
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void sendData()
{

  HTTPClient http;
  String url = "http://" + serverip + "/host/api/Esp32";
  http.begin(url.c_str());
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"config\":{\"tempFreq\":" + String(tempFreq) +",\"connectionString\":" + String(tempFreq) +",\"connectionFreq\":" + String(connectionFreq) + "},\"temperatures\":[";
  for (int i = 0; i < counter; i++)
  {
    // payload += "{\"temp\":"+String(tab_temp[i].temp)+",\"timestamp\":"+String(tab_temp[i].time)+"}";
    payload += String(tab_temp[i]);
    if (i < counter - 1)
      payload += ",";
  }
  payload += "]}";
  int res = http.PUT(payload);
  Serial.printf("HTTP POST result: %d\n", res);
}

void loop()
{
  float temp2 = readTemp2(false);
  // unsigned long time = getTime();
  Serial.printf("time : %u Temp2 : %f, Wifi status %s\n Compteur : %u\n", time, temp2, WiFi.status() == WL_CONNECTED ? "connected" : "disconnected", counter);
  tab_temp[counter] = temp2;

  // start esp light sleep of 5s
  if (counter % 3 == 0)
  {
    initWifi();
    sendData();
    counter = 0;
  }
  Serial.flush();
  counter++;
  esp_sleep_enable_timer_wakeup(tempFreq * 1000000);
  esp_deep_sleep_start();
}
