#include <Atlas.h>
#include <AltSoftSerial.h>

Atlas A = Atlas();
String pHString;

void setup() {
  Serial.begin( 9600 );
  // get version strings for each sensor
  Serial.println( A.version() );
  Particle.variable("pH", pHString);
}

void loop() {
  // get temperature corrected readings
  pHString = A.read( 28 );
  Serial.println( A.read( 28 ) );
}
