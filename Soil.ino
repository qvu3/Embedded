#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const int dry = 645;
const int wet = 273;
const char* webhookUrl = "https://chat.googleapis.com/v1/spaces/AAAA-rlSoZ4/messages?key=AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqqsHI&token=sldHI4qyra9fV1DtL_RJUlL5VMRGQ15E3kIfbYc33n4";
const char* test_webhookUrl = "https://chat.googleapis.com/v1/spaces/AAAA0XqLY3c/messages?key=AIzaSyDdI0hCZtE6vySjMm-WEfRq3CPzqKqqsHI&token=Vuxxm21ZhPYxhXvPD8SkYSTkLnK74sbcXqqGNo-NJPs";
const char* forecastApiUrl = "http://api.openweathermap.org/data/2.5/forecast?lat=26.9298&lon=-82.0454&appid=fdc168625df716de0f81572b81cbcede&units=imperial"; // Punta Gorda's Geo coords are [26.9298, -82.0454], lat=26.9298&lon=-82.0454
const char* weatherApiUrl = "http://api.openweathermap.org/data/2.5/weather?lat=26.9298&lon=-82.0454&appid=fdc168625df716de0f81572b81cbcede&units=imperial";
// const char* ssid = "TP-Link_AP_0F44";
// const char* password = "Happyfarm24";
const char* ssid = "CenturyLink0C01";
const char* password = "6442bcace3bf98";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long lastExecutionTime = 0;
const unsigned long interval = 3 * 60 * 60 * 1000; // 3 hours in milliseconds

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
  unsigned long currentTime = millis();
  Serial.println("Current time: " + String(currentTime));
  Serial.println("Intervel: " + String(interval));
  Serial.println("Last exec time: " + String(lastExecutionTime));

  // update the NTPClient to get currentHour
  timeClient.update();
  int currentHour = timeClient.getHours();
  Serial.println("Current Hour is: " + String(currentHour));

  if (currentHour >= 8 && currentHour <= 19){
    if (currentTime - lastExecutionTime >= interval){
      lastExecutionTime = currentTime;
      Serial.println("Updated \"lastExecutionTime\"");

      // read analog value
      int rawValue = analogRead(A0);

      // map value to percentage
      int percentage = map(rawValue, wet, dry, 100, 0);

      //  print out value and percentage
      Serial.print(rawValue);
      Serial.println(" - Raw Value");
      Serial.print(percentage);
      Serial.println("%");
      
      // Fetch data from Weather API
      float windData = getWindSpeedFromAPI();
      float rainData = getRainDataFromAPI();

      // send data to Google Chat
      sendDataToGoogleChat(percentage, windData, rainData);
    } 
  }
  // delay 120 seconds
  delay(120000);

}

void sendDataToGoogleChat(int value, float windData, float rainData){
  // // initial http client
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.begin(client, test_webhookUrl);
  // add http header
  http.addHeader("Content-Type", "application/json");
  
  // construct the message as json format
  String message = "";
  if (value >= 99)
  {
    message = "{\"text\": \"Soil moisture sensor: The water depth is more than 2 inches.\n";
  }
  else if (value < 99 && value >= 84)
  {
    message = "{\"text\": \"Soil moisture sensor: The water depth is between 1 and 2 inches.\n";
  }
  else if (value < 84 && value > 33)
  {
    message = "{\"text\": \"Soil moisture sensor: The water depth is between 1 and 1/2 inch.\n";
  }
  else if (value <= 33)
  {
    message = "{\"text\": \"Soil moisture sensor: The water depth is less than half an inch.\n";
  }

  // append Wind data to message
  if (windData > 10.0 && windData <= 20.0){
    message = message + "Wind speed is more than 10 miles/hour.\n";
  } else if (windData > 20.0 && windData <= 30.0){
    message = message + "Wind speed is more than 20 miles/hour.\n";
  } else if (windData > 30.0 && windData <= 40.0){
    message = message + "Wind speed is more than 30 miles/hour.\n";
  } else if (windData > 40.0 && windData <= 50.0){
    message = message + "Wind speed is more than 40 miles/hour.\n";
  } else if (windData > 50.0){
    message = message + "Wind speed is more than 50 miles/hour.\n";
  } else {
    message = message + "Current wind speed is " + String(windData) + " miles/hour.\n";
  }

  // append Rain data to message
  if (rainData > 0.0){
    message = message + "Rain amount in last three hours is " + String(rainData) + " mm\"}";
  } else {
    message = message + "There was no rain in the last three hours.\"}";
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

float getRainDataFromAPI() {
  HTTPClient http;
  WiFiClient client;
  http.begin(client, forecastApiUrl);
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

    // JsonArray fiveDaysData = doc["list"].as<JsonArray>();
    // JsonObject todayData = fiveDaysData[0].as<JsonObject>();

    if (doc.containsKey("wind")){
      float windSpeed = doc["wind"]["speed"];
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



