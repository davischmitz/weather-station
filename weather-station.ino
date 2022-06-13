#include <Wire.h>  
#include <DHT.h>                                                
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFiMulti.h>
#include <ArduinoJson.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define WIFI_SSID "<Network Name>"                                                                                        
#define WIFI_PASSWORD "<Network Password>"                                                                               
#define INFLUXDB_URL "<INFLUXDB_URL>"                                                                                     
#define INFLUXDB_TOKEN "<INFLUXDB_TOKEN>"         
#define INFLUXDB_ORG "davi.schmitz2@gmail.com"                                                                   
#define INFLUXDB_BUCKET "weather-station"                                                                        
#define TZ_INFO "BRST+3BRDT+2,M10.3.0,M2.3.0"  

#define OPENWEATHER_SERVER "api.openweathermap.org"

#define DHT_PIN 15
#define DHT_TYPE DHT11
#define BMP280_ADDRESS 0x76
#define BMP280_PRESSURE_CALIBRATION 10
#define CURRENT_SEA_LEVEL_PRESSURE 1023

DHT dht(DHT_PIN, DHT_TYPE);                                                   
Adafruit_BMP280 bmp280;

QueueHandle_t temperatureQueue;
QueueHandle_t pressureQueue;
QueueHandle_t altitudeQueue;
QueueHandle_t humidityQueue;

QueueHandle_t forecastTemperatureQueue;
QueueHandle_t forecastHumidityQueue;
QueueHandle_t forecastPressureQueue;

void TaskReadSensors( void *pvParameters );
void TaskForecast( void *pvParameters ); 

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);                 

Point weatherInfluxPoint("weather");
Point forecastInfluxPoint("forecast");

WiFiClient wifiClient;
String CITY_ID = "3448622"; // São Leopoldo
//String CITY_ID = "3448439"; // São Paulo
//String CITY_ID = "5128581"; // New York
String OPENWEATHER_APIKEY = "5df187ee02a64af1c354a4c63b5a605f";

void setup() {
  Serial.begin(115200);                                             
  
  dht.begin();
  if(!bmp280.begin(BMP280_ADDRESS)) {                                             
    Serial.println("bmp280 init error!");
  }

  WiFi.mode(WIFI_STA);                                              
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");                               
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  weatherInfluxPoint.addTag("device", DEVICE);                                   
  weatherInfluxPoint.addTag("SSID", WIFI_SSID);

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");                 

  if (client.validateConnection()) {                                 
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } 
  else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  temperatureQueue = xQueueCreate(10, sizeof(float));
  pressureQueue = xQueueCreate(10, sizeof(float));
  altitudeQueue = xQueueCreate(10, sizeof(float));
  humidityQueue = xQueueCreate(10, sizeof(float));

  forecastTemperatureQueue = xQueueCreate(10, sizeof(int));
  forecastHumidityQueue = xQueueCreate(10, sizeof(int));
  forecastPressureQueue = xQueueCreate(10, sizeof(float));;

  
  xTaskCreate(
    TaskReadSensors
    ,  "TaskReadSensors"
    ,  configMINIMAL_STACK_SIZE * 4
    ,  NULL
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL);
  xTaskCreate(
    TaskForecast
    ,  "TaskForecast"
    ,  configMINIMAL_STACK_SIZE * 7
    ,  NULL
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL);
}

void loop() {    
  weatherInfluxPoint.clearFields();
  forecastInfluxPoint.clearFields();
  
  float temperature = 0;
  float pressure = 0;  
  float altitude = 0; 
  float humidity = 0;  

  int forecastTemperature = 0;
  int forecastHumidity = 0;
  float forecastPressure = 0;

  if (xQueueReceive(temperatureQueue, &temperature, portMAX_DELAY) == pdPASS) {
    if (temperature > 0) {
      weatherInfluxPoint.addField("temperature", temperature);   
      Serial.print(F("Temperature = "));
      Serial.print(temperature);
      Serial.println(" *C"); 
    }
  }

   if (xQueueReceive(humidityQueue, &humidity, portMAX_DELAY) == pdPASS) {
    if (!isnan(humidity) and humidity > 0) {
      weatherInfluxPoint.addField("humidity", humidity); 
      Serial.print(F("Humidity: "));
      Serial.print(humidity);
      Serial.println(" %");  
    }
  } 
      
  if (xQueueReceive(pressureQueue, &pressure, portMAX_DELAY) == pdPASS) {
    if (pressure > 0) {
      weatherInfluxPoint.addField("pressure", pressure);
      Serial.print(F("Pressure: "));
      Serial.print(pressure); 
      Serial.println(" hPa");
    }
  }

  if (xQueueReceive(altitudeQueue, &altitude, portMAX_DELAY) == pdPASS) {
      weatherInfluxPoint.addField("altitude", altitude);
      Serial.print(F("Altitude: "));
      Serial.print(altitude); 
      Serial.println(" m");
  }

  if(xQueueReceive(forecastTemperatureQueue, &forecastTemperature, portMAX_DELAY) == pdPASS) {
    forecastInfluxPoint.addField("forecasttemperature", forecastTemperature);
    Serial.print(F("Forecast Temperature: "));
    Serial.print(forecastTemperature);
    Serial.println(" *C"); 
  }

  if(xQueueReceive(forecastHumidityQueue, &forecastHumidity, portMAX_DELAY) == pdPASS) {
    forecastInfluxPoint.addField("forecasthumidity", forecastHumidity);
    Serial.print(F("Forecast Humidity: "));
    Serial.print(forecastHumidity);
    Serial.println(" %"); 
  }

  if(xQueueReceive(forecastPressureQueue, &forecastPressure, portMAX_DELAY) == pdPASS) {
    forecastInfluxPoint.addField("forecastpressure", forecastPressure);
    Serial.print(F("Forecast Pressure: "));
    Serial.print(forecastPressure);
    Serial.println(" hPa"); 
  }
    
  if (wifiMulti.run() != WL_CONNECTED)                               
    Serial.println("Wifi connection lost");

  if (!client.writePoint(weatherInfluxPoint)) {                                  
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
  if (!client.writePoint(forecastInfluxPoint)) {                                  
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(100);                                                      
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskReadSensors(void *pvParameters)
{
  (void) pvParameters;

  for (;;)
  {
    vTaskDelay(1000);
    
    float temperature = bmp280.readTemperature();                          
    float pressure = (bmp280.readPressure() / 100) + BMP280_PRESSURE_CALIBRATION;
    float altitude = bmp280.readAltitude(CURRENT_SEA_LEVEL_PRESSURE);
    float humidity = dht.readHumidity();
   
    xQueueSend(temperatureQueue, &temperature, portMAX_DELAY);
    xQueueSend(pressureQueue, &pressure, portMAX_DELAY);
    xQueueSend(altitudeQueue, &altitude, portMAX_DELAY);
    xQueueSend(humidityQueue, &humidity, portMAX_DELAY);

  }
}

void TaskForecast(void *pvParameters)
{
  (void) pvParameters;

  for (;;)
  {
    String result;
    
    if (wifiClient.connect(OPENWEATHER_SERVER, 80)) {
      wifiClient.println("GET /data/2.5/weather?id=" + CITY_ID + "&units=metric&APPID=" + OPENWEATHER_APIKEY);
      wifiClient.println("Host: api.openweathermap.org");
      wifiClient.println("User-Agent: ArduinoWiFi/1.1");
      wifiClient.println("Connection: close");
      wifiClient.println();
    }
    else {
      Serial.println("Connection failed to OpenWeather");  
    }
  
    while (wifiClient.connected() && !wifiClient.available()) {
      vTaskDelay(1);                                          
    }
    while (wifiClient.connected() || wifiClient.available()) {
      char c = wifiClient.read();                     
      result = result + c;
    }
  
    wifiClient.stop();                                     
    result.replace('[', ' ');
    result.replace(']', ' ');
    
    char jsonArray [result.length() + 1];
    result.toCharArray(jsonArray, sizeof(jsonArray));
    jsonArray[result.length() + 1] = '\0';
    StaticJsonDocument<2048> response;
    DeserializationError error = deserializeJson(response, jsonArray);
  
  
    if (error) {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }

    String country = response["sys"]["country"];
    String location = response["name"];
    int temperaturejson = response["main"]["temp"];
    int humidityjson = response["main"]["humidity"];
    float pressurejson = response["main"]["pressure"];

    xQueueSend(forecastTemperatureQueue, &temperaturejson, portMAX_DELAY);
    xQueueSend(forecastHumidityQueue, &humidityjson, portMAX_DELAY);
    xQueueSend(forecastPressureQueue, &pressurejson, portMAX_DELAY);
      
    vTaskDelay(5000);
  }
}
