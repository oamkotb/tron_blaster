#include <Arduino.h>
#include "BlasterClient.h"

BlasterClient blaster;
const uint8_t RESET_PIN = 7;

void setup()
{
  // remove card reset due to unreliability
  blaster.init();
}

void loop() 
{
  blaster.update();
}