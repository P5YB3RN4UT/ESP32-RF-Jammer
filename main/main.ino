#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <ezButton.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
// WARNING: GPIO 0 is a strapping pin on the ESP32. 
// If held LOW during boot/reset, the ESP32 will enter Flash Download Mode.
// If you experience boot issues, change BUTTON_PIN to 15 or 32.
#define BUTTON_PIN 0
#define LED_PIN    12

// SPI Pins (Standard VSPI for ESP32)
#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23
#define PIN_SS   5

// GDO Pins
#define PIN_GDO0 2
#define PIN_GDO2 4

// ==========================================
// GLOBAL VARIABLES
// ==========================================
ezButton buttonPause(BUTTON_PIN);
bool isPaused = true;
const float frequency = 433.92;
const byte packetSize = 60; // Max CC1101 FIFO is 64 bytes
byte txBuffer[packetSize];

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // 1. FIX: Use ESP32 hardware RNG for true randomness, called ONCE in setup
  randomSeed(esp_random()); 
  
  // Initialize button and LED
  buttonPause.setDebounceTime(30);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 2. Initialize CC1101 SPI and GDO pins
  ELECHOUSE_cc1101.setSpiPin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
  ELECHOUSE_cc1101.setGDO(PIN_GDO0, PIN_GDO2);
  ELECHOUSE_cc1101.Init();

  // 3. Verify connection
  if (ELECHOUSE_cc1101.getCC1101()) {
    Serial.println("CC1101 initialized. Connection OK");
  } else {
    Serial.println("CC1101 connection error! Check wiring.");
    while (1); // Halt if module is not found
  }

  // 4. OPTIMIZATION: Configure CC1101 for maximum spectral occupancy (Noise)
  ELECHOUSE_cc1101.setCCMode(1);          // 1 = Transmit mode
  ELECHOUSE_cc1101.setMHZ(frequency);     // Target frequency
  
  // Modulation: 0 = OOK (On-Off Keying), 1 = 2-FSK, 2 = GFSK
  // OOK (0) is highly effective at disrupting standard 433MHz ASK/OOK receivers.
  // If targeting FSK receivers, change to 1 and increase deviation.
  ELECHOUSE_cc1101.setModulation(0);      
  
  ELECHOUSE_cc1101.setDeviation(47.60);   // Keep high deviation for spectral splatter
  ELECHOUSE_cc1101.setChannel(0);
  ELECHOUSE_cc1101.setChsp(199.95);
  ELECHOUSE_cc1101.setRxBW(812.50);
  
  // OPTIMIZATION: Higher data rate spreads the signal energy over a wider bandwidth
  ELECHOUSE_cc1101.setDRate(38.4);        // Increased from 9.6 for wider noise footprint
  
  // OPTIMIZATION: Maximize transmit power (11 or 12 is max for most CC1101 modules)
  ELECHOUSE_cc1101.setPA(11);             
  
  // OPTIMIZATION: Use Fixed Packet Length for efficiency and predictability
  ELECHOUSE_cc1101.setLengthConfig(0);    // 0 = Fixed packet length
  ELECHOUSE_cc1101.setPacketLength(packetSize);
  
  // Disable unnecessary packet handling features to reduce overhead
  ELECHOUSE_cc1101.setSyncMode(0);        // No sync word needed for raw noise
  ELECHOUSE_cc1101.setCrc(0);             // Disable CRC
  ELECHOUSE_cc1101.setWhiteData(0);       // Disable data whitening (we want raw random data)
  ELECHOUSE_cc1101.setManchester(0);      // Disable Manchester encoding
  ELECHOUSE_cc1101.setFEC(0);             // Disable Forward Error Correction

  Serial.println("RF Configuration complete. Press BOOT button to start.");
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  buttonPause.loop();

  // Handle pause/resume toggle
  if (buttonPause.isPressed()) {
    isPaused = !isPaused;
    if (isPaused) {
      Serial.println("JAMMING PAUSED");
      digitalWrite(LED_PIN, LOW);
    } else {
      Serial.println("JAMMING ACTIVE");
      digitalWrite(LED_PIN, HIGH);
    }
    // Small delay to prevent button bounce from triggering multiple toggles
    delay(200); 
  }

  // Active jamming loop
  if (!isPaused) {
    // FIX: random(256) generates 0-255 (full byte range). Original used 255 (0-254).
    for (int i = 0; i < packetSize; i++) {
      txBuffer[i] = (byte)random(256);
    }

    // Transmit the random payload
    ELECHOUSE_cc1101.SendData(txBuffer, packetSize);
    
    // FIX: Crucial delay to allow the CC1101 state machine to reset between 
    // TX bursts. Without this, SPI buffer overflows and transmission failures occur.
    delay(1); 
  }
}