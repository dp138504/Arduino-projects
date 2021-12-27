#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_seesaw.h>

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 1800         /* Time ESP32 will go to sleep (in seconds)  1800 = 30 min*/
RTC_DATA_ATTR int bootCount = 0;

//WiFi Information
const char* ssid     = "CHANGE ME";
const char* password = "CHANGE ME"; 

//MQTT server and authentication settings
#define mqtt_server "CHANGE ME"
#define mqtt_user "CHANGE ME"
#define mqtt_pass "CHANGE ME"

//Sensor definitions
#define DHTPIN 21
#define DHTTYPE DHT22
#define BAT_STAT A13
DHT dht(DHTPIN, DHTTYPE);
Adafruit_seesaw ss;

//MQTT Topics
#define mqttTemp "peppermon/temp"
#define mqttHum "peppermon/hum"
#define mqttHeatIndex "peppermon/HI"
#define mqttMoisture "peppermon/moisture"
#define mqttRawMoisture "peppermon/moisture_raw"
#define mqttBat "peppermon/battery"

//Variable declaration
float hum, temp, heatIndex, batRaw, batV;
uint16_t rawMoisture, moisture;
const int moistureDry = 340; 
const int moistureWet = 1015;

WiFiClient espClient;
PubSubClient client(mqtt_server, 1883, espClient);

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void reconnect() {
  // Loop until we're reconnected
  int counter = 0;
  while (!client.connected()) {
    if (counter==20){
      ESP.restart();
    }
    counter+=1;
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    
    if (client.connect("peppermon", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
    }
  }
}

void getVals() {
  hum = dht.readHumidity();
  temp = dht.readTemperature(true); //readTemperature(ture) for Farenheit
  heatIndex = dht.computeHeatIndex(temp, hum);
  rawMoisture = ss.touchRead(0);

  moisture = map(rawMoisture, moistureDry, moistureWet, 0, 100);

  if(moisture >= 100){
    moisture = 100;
  } else if(moisture <= 0){
    moisture = 0;
  }

  Serial.println("Humidity: " + (String)hum + "%");
  Serial.println("Temperature: " + (String)temp + "°F");
  Serial.println("Heat index: " + (String)heatIndex + "°F");
  Serial.println("Raw Moisture: " + (String)rawMoisture);
  Serial.println("Moisture Percent: " + (String)moisture + "%");
  batRaw = analogRead(BAT_STAT);
  batV = (batRaw/4095)*2*3.3*1.0534;  //Math required to convert analog read of battery status into correct voltage
  Serial.println("Battery Raw: " + (String)batRaw);
  Serial.println("Battery Voltage: " + (String)batV + "V");
}

void setup() {
  
  // Setup serial monitor
  Serial.begin(115200);
  Serial.println();

  pinMode(BAT_STAT,INPUT);
  pinMode(LED_BUILTIN,OUTPUT);

  bootCount++;
  Serial.println("Boot number: " + String(bootCount));
  print_wakeup_reason();

  esp_sleep_enable_timer_wakeup((uint64_t)TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep every " + String(TIME_TO_SLEEP) + " Seconds" + " (" + String(TIME_TO_SLEEP/60) + " Minutes)");
  
  // Begin Wifi connect
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid,password);

  //Conditional logic to work around hangs on WiFi connect
  Serial.print("WiFi connection attempt: ");
  for(int i = 0; ; i++){
    if(WiFi.status() != WL_CONNECTED && i<20){
      Serial.print(i+1);
      Serial.print(" ");
      delay(3000);
    } else if(WiFi.status() == WL_CONNECTED){
      Serial.println("...WiFi connected");  
      Serial.print("IP address: "); Serial.print(WiFi.localIP()); 
      Serial.print(" Subnet: "); Serial.print(WiFi.subnetMask()); 
      Serial.print(" Gateway: "); Serial.println(WiFi.gatewayIP());
      break;
    } else {
      Serial.println("\nCouldn't connect to WiFi. Restarting...");
      ESP.restart();
    }
  }
  // End Wifi connect

  client.setServer(mqtt_server, 1883);

  dht.begin();

  if (!ss.begin(0x36)) {
    Serial.println("ERROR! seesaw not found");
  } else {
    Serial.print("seesaw started! version: ");
    Serial.println(ss.getVersion(), HEX);
  }

  //Main Code
  if (!client.connected()){
    reconnect();
  }
  digitalWrite(LED_BUILTIN,HIGH);
  getVals();
  delay(5000);
  client.publish(mqttTemp,String(temp).c_str(),true);
  client.publish(mqttHum,String(hum).c_str(),true);
  client.publish(mqttHeatIndex,String(heatIndex).c_str(),true);
  client.publish(mqttMoisture,String(moisture).c_str(),true);
  client.publish(mqttRawMoisture,String(rawMoisture).c_str(),true);
  client.publish(mqttBat,String(batV).c_str(),true);
  delay(5000);
  
  client.disconnect(); 
  WiFi.disconnect();

  Serial.print("Disconnecting WiFi");
  while (WiFi.status() != WL_DISCONNECTED) {
    Serial.print(".");
    delay(1);
  }
  Serial.println("Disconnected");
  
  Serial.println("Going to sleep now");
  Serial.flush(); 
  digitalWrite(LED_BUILTIN,LOW);
  
  delay(1000);
  esp_deep_sleep_start();
}

void loop() {
  //Never runs in deep sleep.
}
