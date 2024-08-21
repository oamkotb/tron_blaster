#include <Arduino.h>
#include "BlasterClient.h"

BlasterClient blaster;
const uint8_t RESET_PIN = 7;

void setup()
{
  digitalWrite(RESET_PIN, HIGH);
  pinMode(RESET_PIN, OUTPUT);
  blaster.init();
}

void loop() 
{
  if (blaster.update())
    digitalWrite(RESET_PIN, LOW);
}