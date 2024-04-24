#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const int dry = 645;
const int wet = 273;
const char* webhookUrl = "https://chat.googleapis.com/v1/spaces/AAAA-rlSoZ4/messages?key=AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqqsHI&token=sldHI4qyra9fV1DtL_RJUlL5VMRGQ15E3kIfbYc33n4";
const char* testWebhookUrl = "https://chat.googleapis.com/v1/spaces/AAAA0XqLY3c/messages?key=AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqqsHI&token=Vuxxm21ZhPYxhXvPD8SkYSTkLnK74sbcXqqGNo-NJPs";
const char* weatherApiUrl = "http://api.openweathermap.org/data/2.5/forecast?lat=26.9298&lon=-82.0454&appid=fdc168625df716de0f81572b81cbcede"; // Punta Gorda's Geo coords are [26.9298, -82.0454]
const char* ssid = "CenturyLink0C01";
const char* password = "6442bcace3bf98";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  timeClient.begin();
  timeClient.setTimeOffset(-14400);

  // connect to Wifi
  WiFi.begin(ssid, password);

  Serial.println("Connecting to Wifi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("Connected to Wifi.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void loop() {
  // update the NTPClient to get current time
  timeClient.update();
  // get the current hour
  int currentHour = timeClient.getHours();
  Serial.println(currentHour);

  if (currentHour >= 8 && currentHour <= 17){
      // read analog value
    int rawValue = analogRead(A0);
    // map value to percentage
    int percentage = map(rawValue, wet, dry, 100, 0);
    // send Soil Data to Google Chat
    sendSoilDatatToGoogleChat(percentage);
    // send Rain and Wind Data to Google Chat
    float rainData = getRainDataFromAPI();
    sendRainDataToGoogleChat(rainData);
    float windData = getWindSpeedFromAPI();
    sendWindDataToGoogleChat(windData);
    
    //  print out value and percentage
    Serial.print(rawValue);
    Serial.println(" - Raw Value");
    Serial.print(percentage);
    Serial.println("%");
    

  }
  // delay 1 day
  delay(86400000);
}

void sendSoilDatatToGoogleChat(int value)
{
  // initial http client
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.begin(client, testWebhookUrl);
  // add http header
  http.addHeader("Content-Type", "application/json");
  
  // construct the message as json format
  String message = "";
  if (value >= 99)
  {
    message = "{\"text\": \"Soil sensor: The water depth is more than 2 inches.\"}";
  }
  else if (value < 99 && value >= 84)
  {
    message = "{\"text\": \"Soil sensor: The water depth is between 1 and 2 inches.\"}";
  }
  else if (value < 84 && value > 33)
  {
    message = "{\"text\": \"Soil sensor: The water depth is between 1 and 1/2 inch.\"}";
  }
  else if (value <= 33)
  {
    message = "{\"text\": \"Soil sensor: The water depth is less than half an inch.\"}";
  }

  // send json message to webhookUrl
  http.POST(message);

  Serial.println("Message sent.");
  Serial.println(message);

  // wait for server's response
  while (client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.println(line);
    }
  }

  // Close connection
  client.stop();
  http.end();
  
}

void sendRainDataToGoogleChat(float rainData) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  http.begin(client, testWebhookUrl);
  http.addHeader("Content-Type", "application/json");

  String message = "{\"text\": \"Rain data: " + String(rainData) + " \"}";
  http.POST(message);

  Serial.println("Rain data sent.");
  Serial.println(message);

  while (client.connected()){
    if (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.println(line);
    }
  }

  http.end();
}

void sendWindDataToGoogleChat(float windData){
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.begin(client, testWebhookUrl);
  http.addHeader("Content-Type", "application/json");

  String message = "{\"text\": \"Wind data: " + String(windData) + "\"}";

  http.POST(message);
  Serial.println("Wind data sent.");
  Serial.println(message);

  while (client.connected()){
    if (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.println(line);
    }
  }

  http.end();
}

float getRainDataFromAPI() {
  HTTPClient http;
  WiFiClient client;
  http.begin(client, weatherApiUrl);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    JsonArray fiveDaysData = doc["list"].as<JsonArray>();
    JsonObject todayData = fiveDaysData[0].as<JsonObject>();

    // check if rain data is available
    if (todayData.containsKey("rain")) {
      if (todayData["rain"].containsKey("3h")){
        float rainData = todayData["rain"]["3h"]; 
        return rainData;
      } else {
      Serial.println("No rain data found.");
      return 0.0;
      }  
    } else {
    Serial.println("No rain data found.");
    return 0.0;
    }
  }
  
  return 0.0;
  http.end();
}

float getWindSpeedFromAPI() {
  HTTPClient http;
  WiFiClient client;
  http.begin(client, weatherApiUrl);
  int httpCode = http.GET();

  if (httpCode > 0){
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    JsonArray fiveDaysData = doc["list"].as<JsonArray>();
    JsonObject todayData = fiveDaysData[0].as<JsonObject>();

    if (todayData.containsKey("wind")){
      float windSpeed = todayData["wind"]["speed"];
      return windSpeed;
    } else {
      Serial.println("No wind data found.");
      return 0.0;
    }
  } else {
    Serial.println("Failed to get wind data from API.");
    return 0.0;
  }

  http.end();
}
