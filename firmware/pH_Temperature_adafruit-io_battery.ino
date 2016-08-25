/*
This sketch for a Paritcle Photon uses a pH sensor and circuit from atlas scientific and
a DS18B20 waterproof temperature sensor to gather these measurements from
my pool.  I also leverage the photon battery shield (https://www.sparkfun.com/products/13626)
for powering this device.


(http://www.atlas-scientific.com/product_pages/circuits/ezo_ph.html)

borrowed some code snippets from:
https://learn.sparkfun.com/tutorials/photon-weather-shield-hookup-guide/-particle-libraries-and-the-particle-ide
*/
//onewire and dallas temperature code
#include "OneWire.h"
#include "spark-dallas-temperature.h"
#define ONE_WIRE_BUS D4
#define TEMPERATURE_PRECISION 11
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress inSoilThermometer = {0x28, 0x1A, 0x11, 0xAF, 0x7, 0x0, 0x0, 0x72};//Waterproof temp sensor address

// MAX17043 battery manager IC settings
#include "SparkFunMAX17043.h"
float batteryVoltage;
double batterySOC;
bool batteryAlert;

//configuration to log data to ThingSpeak
#include "ThingSpeak.h"
unsigned long myChannelNumber = <yourID>;  //e.g. 101992
const char * myWriteAPIKey = "yourkey"; // write key here, e.g. ZQV7CRQ8PLKO5QXF

TCPClient client;
String inputstring = "";                              //a string to hold incoming data from the PC
String phstring = "";                             //a string to hold the data from the Atlas Scientific product
boolean input_string_complete = false;                //have we received all the data from the PC
boolean sensor_string_complete = false;               //have we received all the data from the Atlas Scientific product
double pH = 0;                                             //used to hold a floating point number that is the pH// last time since we sent sensor readings
int lastSend = 0;                                   //last time sensor reading sent
double InTempC = 0;//original temperature in C from DS18B20
double watertempf = 0;//converted temperature in F from DS18B20
double soc;  //battery percentage

//pH circuit configuration strings
String ledconfig = "L,0";
String phReadCont = "C,0";

// connection settings
float batterySOCmin = 95.0; // minimum battery state of charge needed for short wakeup time
unsigned long wakeUpTimeoutShort = 60; // wake up every 5 mins when battery SOC > batterySOCmin
unsigned long wakeUpTimeoutLong = 600; // wake up every 10 mins during long sleep, when battery is lower
byte bssid[6];
String bssidString = "";

bool waitForUpdate = false; // for updating software
unsigned long updateTimeout = 310000; // 5 min timeout for waiting for software update, slightly offset. See below.
// wait 70 seconds before sleeping - this cannot be an increment of the measurement timer
// or a hard fault will occur.
unsigned long communicationTimeout = 70000;
unsigned long bootupStartTime;

//Timer setup
unsigned long measureInterval = 15000; // can send data to thingspeak every 15s, but give the matlab analysis a chance to add data too
bool goToSleep = false;
int count = 0;
Timer sleepy(communicationTimeout, go_to_sleep);
Timer measurement(measureInterval, doTelemetry);
Timer countup(1000, counter);

// for publish and subscribe events
String eventPrefix = "poolMon";

void update18B20Temp(DeviceAddress deviceAddress, double &tempC); //predeclare to compile

void setup() {
  // DS18B20 initialization
  sensors.begin();
  sensors.setResolution(inSoilThermometer, TEMPERATURE_PRECISION);

  //initialize pH sensor and Serial port
  Serial.begin(9600);
  Serial1.begin(9600);                                //set baud rate for software serial port_1 to 9600
  inputstring.reserve(10);                            //set aside some bytes for receiving data from the PC
  phstring.reserve(30);                           //set aside some bytes for receiving data from Atlas Scientific product

  Particle.variable("pHVar", pH);
  Particle.variable("TempVar", watertempf);
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

  ThingSpeak.begin(client);
  Particle.subscribe(eventPrefix, eventHandler);
  Particle.publish(eventPrefix + "/pHSensor/startup", "v2.1.4"); // subscribe to this with the API like: curl https://api.particle.io/v1/devices/events/temp?access_token=1234
  sleepy.start();
  measurement.start();
  countup.start();

  doTelemetry(); // always take the measurements at least once
}

//--------------------------------------------------------------------
//if the hardware serial port_0 receives a char
//read the string until we see a <CR>
//set the flag used to tell if we have received a completed string from the PC
void serialEvent() {
  inputstring = Serial.readStringUntil(13);
  input_string_complete = true;
}

//reads data from pH sensor
//read the string until we see a <CR>
//set the flag used to tell if we have received a completed string from the PC
void serialEvent1() {
  phstring = Serial1.readStringUntil(13);
  sensor_string_complete = true;
}

void go_to_sleep(){
  goToSleep = true;
}

void loop() {
  if (goToSleep){
    Serial.println("sleeping");
    //Particle.publish(eventPrefix + "/pHSensor/sleep", "true");
    count = 0;
    goToSleep = false;
    //for(uint32_t ms=millis(); millis() - ms < 5000; Particle.process());
    System.sleep(SLEEP_MODE_DEEP,wakeUpTimeoutLong);
  }
  if (input_string_complete == true) {                //if a string from the PC has been received in its entirety
    Serial1.print(inputstring);                       //send that string to the Atlas Scientific product
    Serial1.print('\r');                              //add a <CR> to the end of the string
    inputstring = "";                                 //clear the string
    input_string_complete = false;                    //reset the flag used to tell if we have received a completed string from the PC
  }
  if (sensor_string_complete == true) {               //if a string from the Atlas Scientific product has been received in its entirety
    Serial.println(phstring);                         //send that string to the PC's serial monitor
    if (isdigit(phstring[0])) {                   //if the first character in the string is a digit
      ThingSpeak.setField(2, phstring);
      Particle.publish(eventPrefix + "/pHSensor/pH", phstring);
      pH = phstring.toFloat();                    //convert the string to a floating point number so it can be evaluated
    }
    phstring = "";                                  //clear the string:
    sensor_string_complete = false;
  }
}
//helps to keep track of the timing while testing
void counter(){
  Serial.println(count++);
}
//if update flag is sent, increase communication time to allow update to be sent
//This also allows the pH circuit configurations to be sent via a publish event
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
void getTemp(){
  Serial.println("getting temperature reading");
  //get temp from DS18B20
  sensors.requestTemperatures();
  update18B20Temp(inSoilThermometer, InTempC);
  //Every so often there is an error that throws a -127.00, this compensates
  if(InTempC < -100)
    watertempf = watertempf;//push last value so data isn't out of scope
  else
    watertempf = (InTempC * 9)/5 + 32;//else grab the newest, good data

  Serial.print("  Temperature = ");
  Serial.print(InTempC);
  Serial.print(" Celsius, ");
  Serial.print(watertempf);
  Serial.println(" Fahrenheit");
}

//---------------------------------------------------------------
void update18B20Temp(DeviceAddress deviceAddress, double &tempC)
{
  tempC = sensors.getTempC(deviceAddress);
}
//---------------------------------------------------------------

void doTelemetry() {
    Particle.process();
    // publish we're still here
    Serial.println("started telemetry");
    Particle.publish(eventPrefix + "/pHSensor/online", "true");

    Serial.println("triggering reading of pH");
    Serial1.print('R');
    Serial1.print('\r');

    //reject bad readings and try again
    int tempcount = 1;
    watertempf = 0;
    while ((watertempf < 70 || watertempf > 95) && tempcount < 15) {
        tempcount++;
        getTemp();
    }
    ThingSpeak.setField(1, String(watertempf));

    // read battery states
    batteryVoltage = lipo.getVoltage();
    ThingSpeak.setField(3, batteryVoltage);
    // lipo.getSOC() returns the estimated state of charge (e.g. 79%)
    batterySOC = lipo.getSOC();
    ThingSpeak.setField(4, String(batterySOC));

    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    Particle.publish(eventPrefix + "/pHSensor/temp", String(watertempf));
    Particle.publish(eventPrefix + "/pHSensor/batterySOC", String(batterySOC));
}
