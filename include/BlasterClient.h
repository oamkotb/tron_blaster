#pragma once

#include <DFRobotDFPlayerMini.h>
#include <LowPower.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <stdint.h>

#define FPSerial softSerial

// Class definition for BlasterClient
class BlasterClient
{
public:
  // Initialize the BlasterClient
  void init();

  // Update the BlasterClient state
  void update();

private:

/*** Main Blaster Functions ***/

  // Function to handle disarmed state
  void disarmed();

  // Function to handle armed state
  void armed();

  // Function to read and process data from an RFID card
  void readCard();

/** Blaster Helper Functions ***/

  // Function to add ammo based on RFID card data
  void addAmmo(char data[16]);

/*** Card Reader Helper Functions ***/

  // Function to fill a buffer with a block of data from an RFID card
  void readBlock(byte block, byte buffer[18], byte buffer_size);

  // Check if the data represents an ammo card
  bool isAmmo(char data[16]);
  bool isReset(char data[16]);

  const uint32_t DEFAULT_INTERVAL = 100;
  uint32_t current_interval = 100;
  unsigned long last_update;
  
  bool _armed;               // Indicates whether the blaster is armed
  int16_t _bullets;           // Number of bullets left (-1 for infinite)
  bool _last_trigger_state;
  bool _trigger_pressed;     // Indicates whether the trigger is pressed
  
  MFRC522 _rfreader;         // RFID reader object
  uint16_t _cards[100];       // Array to store used card IDs
  uint16_t _card_count;       // Count of used cards

  DFRobotDFPlayerMini _dfplayer;
};
