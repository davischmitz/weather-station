#include <Wire.h>                                                  
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#define WIFI_SSID "<Network Name>"                                                                                        
#define WIFI_PASSWORD "<Network Password>"                                                                               
#define INFLUXDB_URL "<INFLUXDB_URL>"                                                                                     
#define INFLUXDB_TOKEN "<INFLUXDB_TOKEN>"         
#define INFLUXDB_ORG "davi.schmitz2@gmail.com"                                                                   
#define INFLUXDB_BUCKET "weather-station"                                                                        
#define TZ_INFO "BRST+3BRDT+2,M10.3.0,M2.3.0"  

#define DHT_PIN 15
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);                                                   
Adafruit_BMP085 bmp180;

SemaphoreHandle_t xMutex1;
QueueHandle_t temperatureQueue;
QueueHandle_t humidityQueue;
QueueHandle_t pressureQueue;

void TaskReadSensors( void *pvParameters );

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);                 

Point sensor("weather");

void setup() {
  Serial.begin(115200);                                             
  
  dht.begin();                                                      
  if(!bmp180.begin()) {                                             
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

  sensor.addTag("device", DEVICE);                                   
  sensor.addTag("SSID", WIFI_SSID);

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");                 

  if (client.validateConnection()) {                                 
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } 
  else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  xMutex1 = xSemaphoreCreateMutex();
  temperatureQueue = xQueueCreate(10, sizeof(int));
  humidityQueue = xQueueCreate(10, sizeof(int));
  pressureQueue = xQueueCreate(10, sizeof(int));

  xTaskCreatePinnedToCore(
    TaskReadSensors
    ,  "TaskReadSensors"
    ,  1024  
    ,  NULL
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL
    ,  ARDUINO_RUNNING_CORE);
}

void loop() {    
  sensor.clearFields();   

  int temp = 0;
  int humid = 0;
  int pressure = 0;                           

  if (xQueueReceive(temperatureQueue, &temp, portMAX_DELAY) == pdPASS) {
    sensor.addField("temperature", temp);   
    Serial.print("Temp: ");                                            
    Serial.println(temp);        
  }
  
  if (xQueueReceive(humidityQueue, &humid, portMAX_DELAY) == pdPASS) {
    sensor.addField("humidity", humid); 
    Serial.print("Humidity: ");
    Serial.println(humid);
  }
      
  if (xQueueReceive(pressureQueue, &pressure, portMAX_DELAY) == pdPASS) {
    sensor.addField("pressure", pressure);
    Serial.print("Pressure: ");
    Serial.println(pressure); 
  }
    
  if (wifiMulti.run() != WL_CONNECTED)                               
    Serial.println("Wifi connection lost");

  if (!client.writePoint(sensor)) {                                  
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(1000);                                                      
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskReadSensors(void *pvParameters)
{
  (void) pvParameters;

  for (;;)
  {
//    if (xSemaphoreTake(xMutex1, 100) == pdTRUE) {
     
      int temp = dht.readTemperature();                                      
      int humid = dht.readHumidity();                                        
      int pressure = bmp180.readPressure() / 100; 

      xQueueSend(temperatureQueue, &temp, portMAX_DELAY);
      xQueueSend(humidityQueue, &humid, portMAX_DELAY);
      xQueueSend(pressureQueue, &pressure, portMAX_DELAY);

//      xSemaphoreGive(xMutex1);
//    }

    vTaskDelay(10); 
  }
}
