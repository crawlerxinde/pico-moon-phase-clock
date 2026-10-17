// credential.h
//
// The RP2350-Touch-LCD-3.49 has no WiFi module.  The clock therefore runs
// offline: it reads the onboard PCF85063A RTC and calculates the moon phase,
// sunrise/sunset and moonrise/moonset locally from the location below.

#ifndef CREDENTIAL_H
#define CREDENTIAL_H

#include <stdint.h>

// Location used for the local sun/moon calculations.
// Positive latitude = north, positive longitude = east.
static const float myLat = 40.0434412f;
static const float myLon = 116.4152377f;

// Local UTC offset in seconds.
// e.g. UTC+3:  3 * 3600     UTC-5:  -5 * 3600
// The RTC is expected to hold local time.  daylightOffset_sec is added when
// computing UTC for the astronomy routines; set it to 3600 during DST or 0.
static const long gmtOffset_sec      = 8 * 3600; // Asia/Shanghai (UTC+8)
static const int  daylightOffset_sec = 0;        // China does not use DST

// Default backlight brightness (0..100).  Can also be changed at runtime with
// the serial command "B 80".
static const uint8_t defaultBrightness = 100;

#endif
