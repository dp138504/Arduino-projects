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
//IPAddress staticIP(192,168,86,215);
//IPAddress dns(172,16,1,1);
//IPAddress gw(192,168,86,1);
//IPAddress subnet(255,255,255,0);
//int chan = 6;
//byte bssid[] = {0x24, 0x05, 0x88, 0x1a, 0xe4, 0x7b};

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
#define mqttTemp "peppermonTest/temp"
#define mqttHum "peppermonTest/hum"
#define mqttHeatIndex "peppermonTest/HI"
#define mqttMoisture "peppermonTest/moisture"
#define mqttBat "peppermonTest/battery"

//Variable declaration
float hum, temp, heatIndex, batRaw, batV;
uint16_t rawMoisture, moisture;
const int moistureDry = 340; 
const int moistureWet = 1015;

/*void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}*/

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
    if (counter==5){
      ESP.restart();
    }
    counter+=1;
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect

    //client.setKeepAlive(300);
    
    if (client.connect("peppermon", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
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
  //WiFi.config(staticIP,gw,subnet,dns);
  //WiFi.begin(ssid,password,chan,bssid);
  WiFi.begin(ssid,password);

  //Conditional logic to work around hangs on WiFi connect
  Serial.print("WiFi connection attempt: ");
  for(int i = 0; ; i++){
    if(WiFi.status() != WL_CONNECTED && i<20){
      Serial.print(i+1);
      Serial.print(" ");
      delay(3000);
    } else if(WiFi.status() == WL_CONNECTED){
      Serial.println("\nWiFi connected");  
      Serial.print("IP address: "); Serial.print(WiFi.localIP()); 
      Serial.print(" Subnet: "); Serial.print(WiFi.subnetMask()); 
      Serial.print(" Gateway: "); Serial.println(WiFi.gatewayIP());
      break;
    } else {
      Serial.println("\nCouldn't connect to WiFi. Restarting...");
      ESP.restart();
    }
  }

  Serial.println("");
  
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
  client.publish(mqttBat,String(batV).c_str(),true);
  delay(5000);
  
  client.disconnect();
  delay(1000);
  
  WiFi.disconnect();
  esp_err_t esp_wifi_stop();
  Serial.print("Disconnecting WiFi");
  while (WiFi.status() != WL_DISCONNECTED) {
    Serial.print(".");
    delay(50);
  }
  Serial.println("\nDisconnected");
  delay(1000);
  
  Serial.println("Going to sleep now");
  Serial.flush(); 
  
  esp_deep_sleep_start();
}

void loop() {
  //Never runs in deep sleep.
}
