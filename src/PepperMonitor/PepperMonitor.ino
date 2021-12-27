#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_seesaw.h>

//Multithreading 
TaskHandle_t SensorReadTask;
TaskHandle_t BlinkTask;

//WiFi Information
const char* ssid     = "CHANGE ME";
const char* password = "CHANGE ME"; 
IPAddress staticIP(192,168,86,175);
IPAddress dns(172,16,1,1);
IPAddress gw(172,16,0,1);
IPAddress subnet(255,255,255,0);
int chan = 6;
byte bssid[] = {0x24, 0x05, 0x88, 0x1a, 0xe4, 0x7b};

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
#define mqttBat "peppermon/battery"

float hum, temp, heatIndex, batRaw, batV;
uint16_t moisture;

/*void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}*/

WiFiClient espClient;
PubSubClient client(mqtt_server, 1883, espClient);

void setup() {
  // Setup serial monitor
  Serial.begin(115200);
  Serial.println();
  
  // Begin Wifi connect
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
//  delay(2000);
  WiFi.config(staticIP,dns,gw,subnet);
  WiFi.begin(ssid, password,chan,bssid);
  
  while (WiFi.status() != WL_CONNECTED) {
    //delay(125);
    //Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // End Wifi connect

  client.setServer(mqtt_server, 1883);

  dht.begin();

  if (!ss.begin(0x36)) {
    Serial.println("ERROR! seesaw not found");
  } else {
    Serial.print("seesaw started! version: ");
    Serial.println(ss.getVersion(), HEX);
  }

  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(BAT_STAT,INPUT);


  // Create main task for multi-threading
  xTaskCreatePinnedToCore(
                    SensorReadTaskCode,   /* Task function. */
                    "SensorReadTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &SensorReadTask,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500);

  // Create blink task for running indicator
  xTaskCreatePinnedToCore(
                    BlinkTaskCode,   /* Task function. */
                    "BlinkTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &BlinkTask,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 0 */                  
  delay(5000);
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
  moisture = ss.touchRead(0);

  Serial.print(F("Humidity: "));
  Serial.print(hum);
  Serial.print(F("%  Temperature: "));
  Serial.print(temp);
  Serial.print(F("°F  Heat index: "));
  Serial.print(heatIndex);
  Serial.println(F("°F"));
  Serial.print("Capacitive: "); Serial.println(moisture);
  batRaw = analogRead(BAT_STAT);
  batV = (batRaw/4095)*2*3.3*1.1;  //Math required to convert analog read of battery status into correct voltage
  Serial.print(batV);
  Serial.print("V");
}


void SensorReadTaskCode( void * pvParameters ){
  for(;;){
    if (!client.connected()){
    reconnect();
  }
  getVals();
  client.publish(mqttTemp,String(temp).c_str(),true);
  client.publish(mqttHum,String(hum).c_str(),true);
  client.publish(mqttHeatIndex,String(heatIndex).c_str(),true);
  client.publish(mqttMoisture,String(moisture).c_str(),true);
  client.publish(mqttBat,String(batV).c_str(),true);
  delay(60000); //Wait 1 minute
  }
}

void BlinkTaskCode( void * pvParameters){
  for(;;){
    digitalWrite(LED_BUILTIN,HIGH);
    delay(2000);
    digitalWrite(LED_BUILTIN,LOW);
    delay(2000);
  }
}

void loop() {
}
