#include <Wire.h>                                                  
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <WiFiMulti.h>
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

DHT dht(15,DHT11);                                                   
Adafruit_BMP085 bmp180;

int temp = 0;                                                       
int humid = 0;
int pressure = 0;

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
}

void loop() {                                                        
  temp = dht.readTemperature();                                      
  humid = dht.readHumidity();                                        
  pressure = bmp180.readPressure()/100;                              

  sensor.clearFields();                                              

  sensor.addField("temperature", temp);                              
  sensor.addField("humidity", humid);                                
  sensor.addField("pressure", pressure);                             

    
  if (wifiMulti.run() != WL_CONNECTED)                               
    Serial.println("Wifi connection lost");

  if (!client.writePoint(sensor)) {                                  
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
  
  Serial.print("Temp: ");                                            
  Serial.println(temp);
  Serial.print("Humidity: ");
  Serial.println(humid);
  Serial.print("Pressure: ");
  Serial.println(pressure);
  delay(1000);                                                      
}
