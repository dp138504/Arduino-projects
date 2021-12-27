#include <RTClib.h>
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

RTC_PCF8523 rtc;
SdFat root;
SdFile logfile;
Adafruit_BME280 bme280;

void setup () {
  Serial.begin(115200);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    //
    // Note: allow 2 seconds after inserting battery or applying external power
    // without battery before calling adjust(). This gives the PCF8523's
    // crystal oscillator time to stabilize. If you call adjust() very quickly
    // after the RTC is powered, lostPower() may still return true.
  }
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  rtc.start();
  
   // The PCF8523 can be calibrated for:
  //        - Aging adjustment
  //        - Temperature compensation
  //        - Accuracy tuning
  // The offset mode to use, once every two hours or once every minute.
  // The offset Offset value from -64 to +63. See the Application Note for calculation of offset values.
  // https://www.nxp.com/docs/en/application-note/AN11247.pdf
  // The deviation in parts per million can be calculated over a period of observation. Both the drift (which can be negative)
  // and the observation period must be in seconds. For accuracy the variation should be observed over about 1 week.
  // Note: any previous calibration should cancelled prior to any new observation period.
  // Example - RTC gaining 43 seconds in 1 week
  float drift = 0; // seconds plus or minus over oservation period - set to 0 to cancel previous calibration.
  float period_sec = (7 * 86400);  // total obsevation period in seconds (86400 = seconds in 1 day:  7 days = (7 * 86400) seconds )
  float deviation_ppm = (drift / period_sec * 1000000); //  deviation in parts per million (μs)
  float drift_unit = 4.34; // use with offset mode PCF8523_TwoHours
  // float drift_unit = 4.069; //For corrections every min the drift_unit is 4.069 ppm (use with offset mode PCF8523_OneMinute)
  int offset = round(deviation_ppm / drift_unit);
  // rtc.calibrate(PCF8523_TwoHours, offset); // Un-comment to perform calibration once drift (seconds) and observation period (seconds) are correct
  rtc.calibrate(PCF8523_TwoHours, 0); // Un-comment to cancel previous calibration

  Serial.print("Offset is "); Serial.println(offset); // Print to control offset

  Serial.print("Initializing SD card...");

  if (!SdFat.begin(33)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  root = SD.open("/");
  printDirectory(root, 0);

  Serial.println("done!");

  bme280.begin();
}

void loop() {
  DateTime now = rtc.now();
  char timeStamp[25];
  char sensorReadings[41];

  sprintf(timeStamp,"[%i-%i-%i %02d:%02d:%02d]",now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  sprintf(sensorReadings,"Temperature: %.2f°C | Humidity: %.2f%%", bme280.readTemperature(), bme280.readHumidity());

  logData(timeStamp,sensorReadings,true);
  
  delay(5000);
}

void printDirectory(SdFile dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void logData(char timestamp[], char logData[], boolean printToSerial){
  logfile = SD.open("/test.txt", FILE_APPEND);
  if(logfile) {
    logfile.print(timestamp); logfile.print(" ");
    logfile.print(logData); logfile.print("\n");
  } else {
    Serial.println("Unable to open file");
  }

  if (printToSerial){
    Serial.print(timestamp); Serial.print(" ");
    Serial.print(logData); Serial.print("\n");
  }

  logfile.close();
}