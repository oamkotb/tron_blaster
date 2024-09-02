#include "BlasterClient.h"

#include <IRremote.hpp>

#define POWER_UP_SFX   1
#define ACTIVATED_SFX  2
#define EMPTY_SFX      3
#define FIRE_SFX       4
#define DEPLETED_SFX   5
#define ACQUIRED_SFX   6
#define POWER_DOWN_SFX 7

// Const Pin definitions
static const uint8_t IR_SEND_PIN = 3;
static const uint8_t LASER_PIN = 4;
static const uint8_t FPSERIAL_RX = 5;
static const uint8_t FPSERIAL_TX = 6;
static const uint8_t TRIGGER_PIN = 8;
static const uint8_t RST_PIN = 9;
static const uint8_t SS_PIN = 10;


SoftwareSerial softSerial(FPSERIAL_RX, FPSERIAL_TX);

// Initialize the BlasterClient
void BlasterClient::init()
{
  // Initialize Serial Communication
  FPSerial.begin(9600);
  Serial.begin(9600);

  // Initialize pins
  pinMode(LASER_PIN, OUTPUT); // Laser output pin
  pinMode(TRIGGER_PIN, INPUT_PULLUP); // Trigger input pin

  // Initialize SPI communication
  SPI.begin();
  
  // Initialize RFID reader
  _rfreader.PCD_Init(SS_PIN, RST_PIN);
  delay(4);

  // Initialize IR sender
  IrSender.begin(IR_SEND_PIN);
  
  // Initialize DFPlayer
  Serial.println("Initializing DFPlayer Mini...");
  if (!_dfplayer.begin(FPSerial))
  {
    Serial.println("DFPlayer Mini failed to begin:");
    Serial.println("1. Please recheck the connection!");
    Serial.println("2. Insert the SD card!");
    while(true);
  }
  else
    Serial.println("DFPlayer Mini online.");
  _dfplayer.volume(30);

  // Initialize variables
  _is_reset = false;
  _bullets = 0;
  _armed = false;
  _trigger_pressed = false;
  _last_trigger_state = HIGH;

  _dfplayer.play(POWER_UP_SFX);

  // Flash to indicate startup
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(LASER_PIN, HIGH);
    delay(200);
    digitalWrite(LASER_PIN, LOW);
    delay(200);
  }
  _dfplayer.play(ACTIVATED_SFX);
}

// Update the BlasterClient state
bool BlasterClient::update()
{
  // Non-blocking timing of update
  if (millis() - last_update >= UPDATE_INTERVAL)
    last_update = millis();
  else
  {
    if (_is_reset)
    {
      _dfplayer.play(POWER_DOWN_SFX);
      delay(5000);
    }
    return _is_reset;
  }

  // Check if the trigger is pressed 
  // Read the current state of the trigger pin
  int trigger_state = digitalRead(TRIGGER_PIN);

  // Check for trigger press (active high) with debounce
  if (trigger_state == LOW && !_trigger_pressed)
  {
      _trigger_pressed = true;
      // Serial.println("Trigger pressed.");      
  }
  else if (trigger_state == HIGH)
    _trigger_pressed = false;

  // Handle the current state (armed or disarmed)
  if (_armed)
    armed();
  else
    disarmed();

  _last_trigger_state = trigger_state;

  return _is_reset;
}

/*** Main Blaster Functions ***/

// Function to handle disarmed state
void BlasterClient::disarmed()
{
  // Turn off the laser
  digitalWrite(LASER_PIN, LOW);

  // Check for RFID cards and process their data
  readCard();

  if (_trigger_pressed)
  {
    _dfplayer.play(EMPTY_SFX);
    delay(900);
  }

  if (_bullets != 0)
    _armed = true;
}

// Function to handle armed state
void BlasterClient::armed()
{
  // Turn on the laser
  digitalWrite(LASER_PIN, HIGH);
  
  // Check for RFID cards and process their data
  readCard();

  // Check if there are bullets and the trigger is pressed
  if (!(_bullets && _trigger_pressed && _last_trigger_state == HIGH))
    return;

  // Decrement bullets if not infinite
  if (_bullets != -1)
    --_bullets;
  
  // Send IR signal
  _dfplayer.play(FIRE_SFX);
  IrSender.sendNECRaw(3016);
  Serial.println("IR signal sent");
  Serial.print("Bullet count: ");
  Serial.println(_bullets);

  // delay(900);

  if (_bullets == 0)
  {
    _armed = false;
    _dfplayer.play(DEPLETED_SFX);

    delay(2000);
  }
}

void BlasterClient::readCard()
{
  // Buffer to hold RFID card data
  byte buffer[18];

  // Check if a new RFID card is present
  if (!_rfreader.PICC_IsNewCardPresent())
    return;
  
  if(!_rfreader.PICC_ReadCardSerial())
    return;
  // Read data from the card
  readBlock(4, buffer, sizeof(buffer));

  // Convert data to characters
  char data[16];
  for (size_t i = 0; i < 16; ++i)
    data[i] = (char)buffer[i];

  Serial.print("Card data: ");
  Serial.println(data);

  // Check if the card is an ammo card
  if (isAmmo(data))
  {
    bool used_card = false;
    uint16_t id = 100 * (data[1] - '0') + 10 * (data[2] - '0') + (data[3] - '0');
    
    // Check if the card has been used
    for (size_t i = 0; i < _card_count; ++i)
    {
      if (_cards[i] == id)
      {
        used_card = true;
        break;
      }
    }

    // If the card has not been used, update bullets
    if (!used_card)
    {
      addAmmo(data);
      _cards[_card_count++] = id;  // Mark the card as used
    }
  }
  else if (isReset(data))
  {
    _is_reset = true;
    Serial.println("Reset called!");
  }
}

/** Blaster Helper Functions ***/

// Function to add ammo based on RFID card data
void BlasterClient::addAmmo(char data[16])
{
  if (data[4] == 'I' && data[5] == 'N' && data[6] == 'F')
    _bullets = -1;  // Infinite ammo
  else
  {
    int amount = 100 * (data[4] - '0') + 10 * (data[5] - '0') + (data[6] - '0');
    _bullets += amount;
    Serial.print("Added bullets: ");
    Serial.println(amount);
  }

  _dfplayer.play(ACQUIRED_SFX);
  delay(2000);
}

/*** Card Reader Helper Functions ***/

// Function to read data from an RFID card
void BlasterClient::readBlock(byte block, byte buffer[18], byte buffer_size)
{
  MFRC522::PICC_Type piccType = _rfreader.PICC_GetType(_rfreader.uid.sak);

  // Check for compatibility
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println("This is not a MIFARE Classic card.");
    return;
  }
  
  // Authenticate using key A
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++)
    key.keyByte[i] = 0xFF;  // Default key for MIFARE Classic cards

  MFRC522::StatusCode status = _rfreader.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(_rfreader.uid)
  );

  if (status != MFRC522::STATUS_OK)
  {
    Serial.print("PCD_Authenticate() failed: ");
    Serial.println(_rfreader.GetStatusCodeName(status));
    return;
  }

  // Read data from the card
  status = _rfreader.MIFARE_Read(block, buffer, &buffer_size);
  if (status != MFRC522::STATUS_OK)
  {
    Serial.print("MIFARE_Read() failed: ");
    Serial.println(_rfreader.GetStatusCodeName(status));
  }

  // Halt PICC
  _rfreader.PICC_HaltA();
  // Stop encryption on PCD
  _rfreader.PCD_StopCrypto1();
}

// Check if the data represents an ammo card
bool BlasterClient::isAmmo(char data[16])
{
  if (data[0] == 'A')
    return true;
  return false;
}

// Check if the data represents a reset card
bool BlasterClient::isReset(char data[16])
{
  if (data[0] == 'R')
    return true;
  return false;
}