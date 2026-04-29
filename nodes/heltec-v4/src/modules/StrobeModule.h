#pragma once
#include "concurrency/OSThread.h"
#include "GPS.h"

/**
 * StrobeModule  —  Heltec WiFi LoRa 32 V4 variant
 * Drives a high-power 5V recovery strobe via a 2N7000 MOSFET on GPIO 5.
 *
 * Behaviour:
 *   GPS locked   : 3 blinks every 30 s  (slow — "I'm tracking fine")
 *   No GPS lock  : 3 blinks every 5 s   (fast — "I'm lost / no fix")
 *
 * LED: Jaycar ZD0290 White 5mm Cree 45000mcd (Vf=3.2V, If=100mA)
 *
 * Circuit:
 *   5V ── LED(+) ── LED(–) ──[56Ω]── MOSFET Drain
 *   MOSFET Source ── GND
 *   GPIO 5 ──[100Ω]── MOSFET Gate
 *   MOSFET Gate ──[10kΩ]── GND  (pulldown)
 *
 * PIN NOTE: GPIO 13 is LORA_BUSY (SX1262 DIO2) on the Heltec V4 — do not use.
 *           GPIO 5 is a free general-purpose I/O on Header J3 pin 16.
 */
class StrobeModule : private concurrency::OSThread
{
  public:
    StrobeModule();

  protected:
    int32_t runOnce() override;

  private:
    void doBlinks(uint8_t count);

    // GPIO driving the MOSFET gate
    // GPIO 5 (J3 pin 16) — free I/O, no LoRa or I2C conflict on Heltec V4
    static constexpr uint8_t  STROBE_PIN       = 5;

    // Blink timing (ms)
    static constexpr uint32_t BLINK_ON_MS      = 80;   // flash duration
    static constexpr uint32_t BLINK_GAP_MS     = 150;  // gap between blinks in a burst

    // Repeat interval depending on GPS state
    static constexpr uint32_t INTERVAL_LOCK_MS   = 30000; // 30 s when locked
    static constexpr uint32_t INTERVAL_NOLOCK_MS =  5000; //  5 s when no fix
};

extern StrobeModule *strobeModule;
