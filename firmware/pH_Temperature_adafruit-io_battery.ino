#include "OneWire.h" // Use this include for Particle Dev where everything is in one directory.
OneWire ds = OneWire(D4);  // 1-wire signal on pin D4

// device name
#define DEVICE_NAME "PHOTON-POOLv0.4.1"

// whether to use Farenheit instead of Celsius
#define USE_FARENHEIT 1

// AdaFruit integration
#define ADAFRUIT_ENABLED 1
#define ADAFRUIT_API_KEY "f23a7ccc954b857af7c88a3f1229f72b0dcd9c58"
#define ADAFRUIT_FEED_POOLTEMP "pool-temp"
#define ADAFRUIT_FEED_PH "pH"
#define ADAFRUIT_FEED_BATTERY "battery"

// Particle event
#define PARTICLE_EVENT 1
#define PARTICLE_EVENT_NAME "pool-logger"

// AdaFruit.io
#if ADAFRUIT_ENABLED
#include "Adafruit_IO_Client.h"
#include "Adafruit_IO_Arduino.h"
#endif

#include "SparkFunMAX17043.h"
// MAX17043 battery manager IC settings
float batteryVoltage;
double batterySOC;
bool batteryAlert;

#include "ThingSpeak.h"
//################### update these vars ###################
unsigned long myChannelNumber = 144215;  //e.g. 101992
const char * myWriteAPIKey = "PVANB79KBVKVMY0K"; // write key here, e.g. ZQV7CRQ8PLKO5QXF
//################### update these vars ###################

TCPClient client;

#if ADAFRUIT_ENABLED
Adafruit_IO_Client aio = Adafruit_IO_Client(client, ADAFRUIT_API_KEY);
Adafruit_IO_Feed aioFeedPoolTemp = aio.getFeed(ADAFRUIT_FEED_POOLTEMP);
Adafruit_IO_Feed aioFeedPH = aio.getFeed(ADAFRUIT_FEED_PH);
Adafruit_IO_Feed aioFeedBattery = aio.getFeed(ADAFRUIT_FEED_BATTERY);
#endif


String inputstring = "";                              //a string to hold incoming data from the PC
String phstring = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_complete = false;                //have we received all the data from the PC
boolean sensor_string_complete = false;               //have we received all the data from the Atlas Scientific product
double pH = 1;                                             //used to hold a floating point number that is the pH// last time since we sent sensor readings
int lastSend = 0;                                   //last time sensor reading sent
float celsius;
double fahrenheit;
double soc;
String ledconfig = "L,0";
String phReadCont = "C,0";

unsigned long lastMeasureTime = 0;
unsigned long measureInterval = 30000; // can send data to thingspeak every 15s, but give the matlab analysis a chance to add data too


// connection settings
float batterySOCmin = 95.0; // minimum battery state of charge needed for short wakeup time
unsigned long wakeUpTimeoutShort = 10; // wake up every 5 mins when battery SOC > batterySOCmin
unsigned long wakeUpTimeoutLong = 30; // wake up every 15 mins during long sleep, when battery is lower

// for updating software
bool waitForUpdate = false; // for updating software
unsigned long updateTimeout = 300000; // 5 min timeout for waiting for software update
unsigned long communicationTimeout = 300000; // wait 5 mins before sleeping
unsigned long bootupStartTime;

//Timer setup
Timer sleepy(communicationTimeout, go_to_sleep);
Timer measurement(measureInterval, doTelemetry);

// for HTTP POST and Particle.publish payloads
char payload[1024];

// for publish and subscribe events
//################### update these vars ###################
String eventPrefix = "poolMon"; // e.g. myFarm/waterSystem
//################### update these vars ###################

bool pumpOn;

void setup() {
    Serial.begin(9600);
    Serial1.begin(9600);                                //set baud rate for software serial port_1 to 9600
    inputstring.reserve(10);                            //set aside some bytes for receiving data from the PC
    phstring.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product

    Particle.variable("pHVar", phstring);
    Particle.variable("TempVar", fahrenheit);
    Particle.variable("BattVar", batterySOC);

    //set the LED on the pH sensor chip
    Serial1.print(ledconfig);
    Serial1.print('\r');
    Serial1.print(phReadCont);
    Serial1.print('\r');

    // Set up the MAX17043 LiPo fuel gauge:
    lipo.begin(); // Initialize the MAX17043 LiPo fuel gauge

    // Quick start restarts the MAX17043 in hopes of getting a more accurate
    // guess for the SOC.
    lipo.quickStart();

    // We can set an interrupt to alert when the battery SoC gets too low.
    // We can alert at anywhere between 1% - 32%:
    lipo.setThreshold(20); // Set alert threshold to 20%.

    #if ADAFRUIT_ENABLED
        aio.begin();
    #endif

    ThingSpeak.begin(client);

    Particle.subscribe(eventPrefix, eventHandler);
    Particle.publish(eventPrefix + "/pHSensor/startup", "true"); // subscribe to this with the API like: curl https://api.particle.io/v1/devices/events/temp?access_token=1234
    sleepy.start();
    doTelemetry(); // always take the measurements at least once
}

void serialEvent() {                                  //if the hardware serial port_0 receives a char
  inputstring = Serial.readStringUntil(13);           //read the string until we see a <CR>
  input_string_complete = true;                       //set the flag used to tell if we have received a completed string from the PC
}

//reads data from pH sensor
void serialEvent1() {                                 //if the hardware serial port_1 receives a char
  phstring = Serial1.readStringUntil(13);         //read the string until we see a <CR>
  sensor_string_complete = true;                      //set the flag used to tell if we have received a completed string from the PC
}

void go_to_sleep(){
  Serial.println("sleeping for long");
  Particle.publish(eventPrefix + "/pHSensor/sleep", "long");
  //System.sleep(SLEEP_MODE_DEEP, wakeUpTimeoutLong);
  delay(5000);
  sleepy.reset();
}

void loop() {
    if ((millis() - bootupStartTime) > updateTimeout) {
            Serial.println("waitforupdate set to false false");
            waitForUpdate = false;
    }
}


void eventHandler(String event, String data)
{
  // to publish update: curl https://api.particle.io/v1/devices/events -d "name=update" -d "data=true" -d "private=true" -d "ttl=60" -d access_token=1234
  if (event == eventPrefix + "/pHSensor/update") {
    if (data == "true"){
      waitForUpdate = true;
      sleepy.changePeriod(600000);
      Serial.println("wating for update");
      Particle.publish(eventPrefix + "/pHSensor/updateConfirm", "waiting for update");
    }
  }
  if (event == eventPrefix + "/pHSensor/cfg") {
    Serial.println("sending setting to pH sensor:" + data);
    Serial1.print(data);
  }
  Serial.print(event);
  Serial.print(", data: ");
  Serial.println(data);
}

void doTelemetry() {
    // publish we're still here
    Serial.println("started telemetry");
    Particle.publish(eventPrefix + "/pHSensor/online", "true");

    Serial.println("triggering reading of pH");
    Serial1.print('R');
    Serial1.print('\r');
    delay(2000); //wait for reading of pH sensor

    byte i;
    byte present = 0;
    byte type_s;
    byte data[12];
    byte addr[8];

    if ( !ds.search(addr)) {
      Serial.println("No more addresses.");
      Serial.println();
      ds.reset_search();
      delay(250);
      return;
    }

    // The order is changed a bit in this example
    // first the returned address is printed

    Serial.print("ROM =");
    for( i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }

    // second the CRC is checked, on fail,
    // print error and just return to try again

    if (OneWire::crc8(addr, 7) != addr[7]) {
        Serial.println("CRC is not valid!");
        return;
    }
    Serial.println();

    // we have a good address at this point
    // what kind of chip do we have?
    // we will set a type_s value for known types or just return

    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10:
        Serial.println("  Chip = DS1820/DS18S20");
        type_s = 1;
        break;
      case 0x28:
        Serial.println("  Chip = DS18B20");
        type_s = 0;
        break;
      case 0x22:
        Serial.println("  Chip = DS1822");
        type_s = 0;
        break;
      case 0x26:
        Serial.println("  Chip = DS2438");
        type_s = 2;
        break;
      default:
        Serial.println("Unknown device type.");
        return;
    }

    // got a reading from pH sensor, let's do something with it

    if (sensor_string_complete == true) {               //if a string from the Atlas Scientific product has been received in its entirety
      Serial.println(phstring);                         //send that string to the PC's serial monitor
      Particle.publish(eventPrefix + "/pHSensor/pH", phstring);
      ThingSpeak.setField(2, phstring);
      if (isdigit(phstring[0])) {                   //if the first character in the string is a digit
        pH = phstring.toFloat();                    //convert the string to a floating point number so it can be evaluated by the Arduino
        if (pH >= 7.0) {                                //if the pH is greater than or equal to 7.0
          Serial.println("high");                       //print "high" this is demonstrating that the Arduino is evaluating the pH as a number and not as a string
        }
        if (pH <= 6.99) {                               //if the pH is less than or equal to 6.99
          Serial.println("low");                        //print "low" this is demonstrating that the Arduino is evaluating the pH as a number and not as a string
        }
      }
    }
    phstring = "";                                  //clear the string:
    sensor_string_complete = false;

    // this device has temp so let's read it

    ds.reset();               // first clear the 1-wire bus
    ds.select(addr);          // now select the device we just found
    // ds.write(0x44, 1);     // tell it to start a conversion, with parasite power on at the end
    ds.write(0x44, 0);        // or start conversion in powered mode (bus finishes low)

    // just wait a second while the conversion takes place
    // different chips have different conversion times, check the specs, 1 sec is worse case + 250ms
    // you could also communicate with other devices if you like but you would need
    // to already know their address to select them.

    delay(1000);     // maybe 750ms is enough, maybe not, wait 1 sec for conversion

    // we might do a ds.depower() (parasite) here, but the reset will take care of it.

    // first make sure current values are in the scratch pad

    present = ds.reset();
    ds.select(addr);
    ds.write(0xB8,0);         // Recall Memory 0
    ds.write(0x00,0);         // Recall Memory 0

    // now read the scratch pad

    present = ds.reset();
    ds.select(addr);
    ds.write(0xBE,0);         // Read Scratchpad
    if (type_s == 2) {
      ds.write(0x00,0);       // The DS2438 needs a page# to read
    }

    // transfer and print the values

    Serial.print("  Data = ");
    Serial.print(present, HEX);
    Serial.print(" ");
    for ( i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.print(" CRC=");
    Serial.print(OneWire::crc8(data, 8), HEX);
    Serial.println();

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s == 2) raw = (data[2] << 8) | data[1];
    byte cfg = (data[4] & 0x60);

    switch (type_s) {
      case 1:
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
          // "count remain" gives full 12 bit resolution
          raw = (raw & 0xFFF0) + 12 - data[6];
        }
        celsius = (float)raw * 0.0625;
        break;
      case 0:
        // at lower res, the low bits are undefined, so let's zero them
        if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
        if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
        if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
        // default is 12 bit resolution, 750 ms conversion time
        celsius = (float)raw * 0.0625;
        break;

      case 2:
        data[1] = (data[1] >> 3) & 0x1f;
        if (data[2] > 127) {
          celsius = (float)data[2] - ((float)data[1] * .03125);
        }else{
          celsius = (float)data[2] + ((float)data[1] * .03125);
        }
    }

    fahrenheit = celsius * 1.8 + 32.0;
    Serial.print("  Temperature = ");
    Serial.print(celsius);
    Serial.print(" Celsius, ");
    Serial.print(fahrenheit);
    Serial.println(" Fahrenheit");

    ThingSpeak.setField(1, String(fahrenheit));

    // read battery states
    batteryVoltage = lipo.getVoltage();
    ThingSpeak.setField(3, batteryVoltage);
    // lipo.getSOC() returns the estimated state of charge (e.g. 79%)
    batterySOC = lipo.getSOC();
    ThingSpeak.setField(4, String(batterySOC));
    // lipo.getAlert() returns a 0 or 1 (0=alert not triggered)
    //batteryAlert = lipo.getAlert();

    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    #if ADAFRUIT_ENABLED
            aioFeedPoolTemp.send(fahrenheit);
            aioFeedPH.send(pH);
            aioFeedBattery.send(String(batterySOC));
    #endif

    #if PARTICLE_EVENT
            Particle.publish(eventPrefix + "/pHSensor/temp", String(fahrenheit));
            Particle.publish(eventPrefix + "/pHSensor/batterySOC", String(batterySOC));
            //Spark.publish(PARTICLE_EVENT_NAME, payload, 60, PRIVATE);
            //Spark.publish("pool-temp", String(fahrenheit));
    #endif

    lastMeasureTime = millis();
}
