/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.
 
  This example code is in the public domain.
 */

// Pin 13 has an LED connected on most Arduino boards.
// Pin 11 has the LED on Teensy 2.0
// Pin 6  has the LED on Teensy++ 2.0
// Pin 13 has the LED on Teensy 3.0
// Pin 8 has LED 0 on Naze
// Pin 16 has LED 0 on Naze
// give it a name:
int led = 16;

#include <Arduino.h>

// the setup routine runs once when you press reset:
void setup() {                
    Serial.begin(115200);
    // initialize the digital pin as an output.
    pinMode(led, OUTPUT);     
}

// the loop routine runs over and over again forever:
void loop() {
    digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);               // wait for a second
    digitalWrite(led, LOW);    // turn the LED off by making the voltage LOW
    delay(1000);               // wait for a second

    static int count;
    Serial.printf("%d\n", count++);
}

void serialEvent1()
{
    static int count;
    Serial.printf("%d\n", count++);
}
