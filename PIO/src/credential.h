// credential.h
//
// The RP2350-Touch-LCD-3.49 has no WiFi module.  The clock therefore runs
// offline: it reads the onboard PCF85063A RTC and calculates the moon phase,
// sunrise/sunset and moonrise/moonset locally from the location below.

#ifndef CREDENTIAL_H
#define CREDENTIAL_H

#include <stdint.h>

// Default location used for the local sun/moon calculations.
// Positive latitude = north, positive longitude = east.
//
// These values are the power-on default.  The Settings page can switch to
// Paris, London, New York, Tokyo or Sydney at runtime; the selection is stored
// in EEPROM.  Changing city keeps the same UTC instant and rewrites the RTC
// in the new city's local time.
static const float myLat = 40.0434412f;
static const float myLon = 116.4152377f;

// Default name shown in the location pill.
static const char *myCity = "Beijing";

// Default local UTC offset in seconds.
// e.g. UTC+3:  3 * 3600     UTC-5:  -5 * 3600
// Runtime offsets (including DST) live in location.cpp.
static const long gmtOffset_sec = 8 * 3600; // Asia/Shanghai (UTC+8)

// Default backlight brightness (0..100).  Can also be changed at runtime with
// the serial command "B 80".
static const uint8_t defaultBrightness = 100;

#endif
