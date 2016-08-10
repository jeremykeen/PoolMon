/*

Modified from https://github.com/Hotaman/OneWireSpark and the example
file (arduino_mega_pH_sample_code.ino) from http://www.atlas-scientific.com/ph.html

Use this sketch to read the temperature from 1-Wire devices
you have attached to your Particle device (core, p0, p1, photon, electron)
and the pH from Atlas Scientific pH sensor.  Temp and pH are sent to adafruit.io
and Particle publish.  Sparkfun battery shield measured and reported.

Temperature is read from: DS18S20, DS18B20, DS1822, DS2438

Expanding on the enumeration process in the address scanner, this example
reads the temperature and outputs it from known device types as it scans.

I/O setup:
These made it easy to just 'plug in' my 18B20

D3 - 1-wire ground, or just use regular pin and comment out below.
D4 - 1-wire signal, 2K-10K resistor to D5 (3v3)
D5 - 1-wire power, ditto ground comment.

A pull-up resistor is required on the signal line. The spec calls for a 4.7K.
I have used 1K-10K depending on the bus configuration and what I had out on the
bench. If you are powering the device, they all work. If you are using parisidic
power it gets more picky about the value.

*/

//change

// Only include One of the following depending on your environment!
// #include "OneWire/OneWire.h"  // Use this include for the Web IDE:
#include "OneWire.h" // Use this include for Particle Dev where everything is in one directory.

#include "SparkFunMAX17043.h" // Include the SparkFun MAX17043 library

// Comment this out for normal operation
// SYSTEM_MODE(SEMI_AUTOMATIC);  // skip connecting to the cloud for (Electron) testing

// device name
#define DEVICE_NAME "PHOTON-POOLv0.4.1"

//frequency to send data
#define SEND_INTERVAL 30

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

TCPClient tcpClient;

String inputstring = "";                              //a string to hold incoming data from the PC
String phstring = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_complete = false;                //have we received all the data from the PC
boolean sensor_string_complete = false;               //have we received all the data from the Atlas Scientific product
float pH;                                             //used to hold a floating point number that is the pH// last time since we sent sensor readings
int lastSend = 0;                                   //last time sensor reading sent
float celsius;
double fahrenheit;
double soc;

#if ADAFRUIT_ENABLED
Adafruit_IO_Client aio = Adafruit_IO_Client(tcpClient, ADAFRUIT_API_KEY);
Adafruit_IO_Feed aioFeedPoolTemp = aio.getFeed(ADAFRUIT_FEED_POOLTEMP);
Adafruit_IO_Feed aioFeedPH = aio.getFeed(ADAFRUIT_FEED_PH);
Adafruit_IO_Feed aioFeedBattery = aio.getFeed(ADAFRUIT_FEED_BATTERY);
#endif

OneWire ds = OneWire(D4);  // 1-wire signal on pin D4

// for HTTP POST and Particle.publish payloads
char payload[1024];

unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);                                //set baud rate for software serial port_1 to 9600
  inputstring.reserve(10);                            //set aside some bytes for receiving data from the PC
  phstring.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product

  Particle.variable("pHVar", phstring);
  Particle.variable("TempVar", fahrenheit);
  Particle.variable("BattVar", soc);

  Serial1.print('C,1');
  Serial1.print('\r');
  Serial1.print('L,1');
  Serial1.print('\r');


// Set up the MAX17043 LiPo fuel gauge:
    lipo.begin(); // Initialize the MAX17043 LiPo fuel gauge

    // Quick start restarts the MAX17043 in hopes of getting a more accurate
    // guess for the SOC.
    lipo.quickStart();

#if ADAFRUIT_ENABLED
    aio.begin();
#endif

#if PARTICLE_EVENT
    // startup event
    sprintf(payload,
            "{\"device\":\"%s\",\"state\":\"starting\"}",
            DEVICE_NAME);

    Spark.publish(PARTICLE_EVENT_NAME, payload, 60, PRIVATE);
#endif
}
//read serial input to send commands to pH sensor

void serialEvent() {                                  //if the hardware serial port_0 receives a char
  inputstring = Serial.readStringUntil(13);           //read the string until we see a <CR>
  input_string_complete = true;                       //set the flag used to tell if we have received a completed string from the PC
}

//reads data from pH sensor
void serialEvent1() {                                 //if the hardware serial port_1 receives a char
  phstring = Serial1.readStringUntil(13);         //read the string until we see a <CR>
  sensor_string_complete = true;                      //set the flag used to tell if we have received a completed string from the PC
}

void loop(void) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  int now = Time.now();

  if (input_string_complete == true) {                //if a string from the PC has been received in its entirety
    Serial1.print(inputstring);                       //send that string to the Atlas Scientific product
    Serial1.print('\r');                              //add a <CR> to the end of the string
    inputstring = "";                                 //clear the string
    input_string_complete = false;                    //reset the flag used to tell if we have received a completed string from the PC
  }

  // only run every SEND_INTERVAL seconds
  if (now - lastSend < SEND_INTERVAL) {
      return;
  }


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
    Spark.publish("pool-ph", phstring);
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

  soc = lipo.getSOC(); //get state-of-charge of battery

// do something with the temp and pH data
#if ADAFRUIT_ENABLED
        aioFeedPoolTemp.send(fahrenheit);
        aioFeedPH.send(pH);
        aioFeedBattery.send(String(soc));
#endif

  sprintf(payload,
      "{\"device\":\"%s\",\"TempF\":%.2f,\"pH\":%.2f}",
      DEVICE_NAME,
      fahrenheit,
      pH);

#if PARTICLE_EVENT
        Spark.publish(PARTICLE_EVENT_NAME, payload, 60, PRIVATE);
        Spark.publish("pool-temp", String(fahrenheit));
#endif

  lastSend = now;

}
