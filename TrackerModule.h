#pragma once
#include "SinglePortModule.h"
#include "OSTimer.h"
#include <Wire.h>

/**
 * TrackerModule
 * Sends GPS position + compass heading over a private channel every 30 seconds.
 * Flashes TX LED on each packet send.
 *
 * Hardware:
 *   GPS  : u-blox M8Q via UART (Serial2, GPIO 12 RX / 15 TX)
 *   Mag  : QMC5883L via I2C   (SDA=21, SCL=22)
 *   Power LED : GPIO 2  (always on)
 *   TX LED    : GPIO 4  (flashes 200 ms per packet)
 */
class TrackerModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    TrackerModule();

  protected:
    // OSThread: called on the 30s interval
    int32_t runOnce() override;

  private:
    void sendPosition();
    void flashTxLed();

    // Read heading from QMC5883L; returns degrees 0-359, or -1 on error
    float readHeadingDeg();

    // QMC5883L I2C address (fixed in hardware)
    static constexpr uint8_t QMC_ADDR      = 0x0D;

    // GPIO pins
    static constexpr uint8_t POWER_LED_PIN = 2;
    static constexpr uint8_t TX_LED_PIN    = 4;

    // Timing
    static constexpr uint32_t TX_LED_MS    = 200;
    static constexpr uint32_t INTERVAL_MS  = 30000;

    // Magnetic declination for your area (degrees, + east / - west)
    // Brisbane, AU ≈ +11.5°  — adjust for your deployment location
    // Find yours at: https://www.magnetic-declination.com
    static constexpr float MAG_DECLINATION = 11.5f;

    bool compassOk = false;
};

extern TrackerModule *trackerModule;
