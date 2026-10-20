/*
 * Local sun/moon calculations.
 *
 * The Waveshare board has no WiFi, so instead of the original
 * visualcrossing.com request this module derives the moon phase and the
 * sunrise/sunset/moonrise/moonset times from the RTC, latitude and longitude.
 *
 * The sun and moon positions use Paul Schlyter's low-precision formulae,
 * which are accurate to roughly a minute for rise/set purposes at temperate
 * latitudes.  The moon phase uses a mean synodic month anchored at the
 * 2000-01-06 18:14 UTC new moon.
 */
#include "astronomy.h"

#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "clock.h"
#include "location.h"

extern const lv_img_dsc_t *ui_imgset_moon_[30];

namespace
{
    constexpr double kPi     = 3.14159265358979323846;
    constexpr double DEG2RAD = kPi / 180.0;
    constexpr double RAD2DEG = 180.0 / kPi;

    constexpr double SYNODIC_MONTH = 29.530588853;
    constexpr double NEW_MOON_EPOCH_JD = 2451550.1; /* 2000-01-06 18:14 UTC */

    /* The 30 static moon images are treated as lunar-north-up.  The moon tile
     * is rotated by the position angle of lunar north relative to the local
     * zenith (P - q).  These constants are the calibration points for the
     * actual panel orientation and for small residual errors in the artwork. */
    constexpr double MOON_POLE_RA        = 270.0;
    constexpr double MOON_POLE_DEC       = 66.56;
    constexpr float  MOON_ASSET_NORTH_OFFSET_DEG = 0.0f;
    constexpr float  MOON_SCREEN_UP_OFFSET_DEG   = 0.0f;
    constexpr float  MOON_ROTATION_SIGN          = 1.0f;

    /* Optional per-frame correction for the static assets.  Keep this table
     * zero unless a known moon/Stellarium comparison shows that a particular
     * frame's baked terminator is not aligned with lunar north. */
    constexpr float kMoonFrameCorrectionDeg[30] = {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };

    unsigned long lastUpdateMs = 0;
    int currentFrame = 0;

    double normalizeDegrees(double d)
    {
        d = fmod(d, 360.0);
        if (d < 0.0) d += 360.0;
        return d;
    }

    double normalizeRadians(double r)
    {
        r = fmod(r, 2.0 * kPi);
        if (r < 0.0) r += 2.0 * kPi;
        return r;
    }

    double julianFromUtc(time_t utc)
    {
        return (double)utc / 86400.0 + 2440587.5;
    }

    double daysSinceEpoch2000(double jd)
    {
        /* Schlyter's day zero: 2000 Jan 0.0 = JD 2451543.5 */
        return jd - 2451543.5;
    }

    void sunPosition(double jd, double &raDeg, double &decDeg)
    {
        double d = daysSinceEpoch2000(jd);

        double w = 282.9404 + 4.70935e-5 * d;
        double e = 0.016709 - 1.151e-9 * d;
        double M = normalizeDegrees(356.0470 + 0.9856002585 * d);

        double E = M + e * RAD2DEG * sin(M * DEG2RAD) *
                       (1.0 + e * cos(M * DEG2RAD));
        for (int i = 0; i < 3; i++) {
            E = E - (E - e * RAD2DEG * sin(E * DEG2RAD) - M) /
                        (1.0 - e * cos(E * DEG2RAD));
        }

        double xv = cos(E * DEG2RAD) - e;
        double yv = sqrt(1.0 - e * e) * sin(E * DEG2RAD);
        double v  = atan2(yv, xv) * RAD2DEG;
        double r  = sqrt(xv * xv + yv * yv);

        double lon = normalizeDegrees(v + w);
        double x = r * cos(lon * DEG2RAD);
        double y = r * sin(lon * DEG2RAD);

        double obl = 23.4393 - 3.563e-7 * d;
        raDeg  = normalizeDegrees(atan2(y * cos(obl * DEG2RAD), x) * RAD2DEG);
        decDeg = atan2(y * sin(obl * DEG2RAD), sqrt(x * x + y * y)) * RAD2DEG;
    }

    void moonPosition(double jd, double &raDeg, double &decDeg)
    {
        double d = daysSinceEpoch2000(jd);

        double N = normalizeDegrees(125.1228 - 0.0529538083 * d);
        double i = 5.1454;
        double w = normalizeDegrees(318.0634 + 0.1643573223 * d);
        double a = 60.2666;
        double e = 0.054900;
        double M = normalizeDegrees(115.3654 + 13.0649929509 * d);

        double E = M + e * RAD2DEG * sin(M * DEG2RAD) *
                       (1.0 + e * cos(M * DEG2RAD));
        for (int k = 0; k < 4; k++) {
            E = E - (E - e * RAD2DEG * sin(E * DEG2RAD) - M) /
                        (1.0 - e * cos(E * DEG2RAD));
        }

        double xv = a * (cos(E * DEG2RAD) - e);
        double yv = a * sqrt(1.0 - e * e) * sin(E * DEG2RAD);
        double v  = atan2(yv, xv) * RAD2DEG;
        double r  = sqrt(xv * xv + yv * yv);

        double vw = (v + w) * DEG2RAD;
        double Nr = N * DEG2RAD;
        double ir = i * DEG2RAD;

        double xh = r * (cos(Nr) * cos(vw) - sin(Nr) * sin(vw) * cos(ir));
        double yh = r * (sin(Nr) * cos(vw) + cos(Nr) * sin(vw) * cos(ir));
        double zh = r * sin(vw) * sin(ir);

        double lon = atan2(yh, xh) * RAD2DEG;
        double lat = atan2(zh, sqrt(xh * xh + yh * yh)) * RAD2DEG;

        /* Main lunar perturbations (Schlyter). */
        double Ms = normalizeDegrees(356.0470 + 0.9856002585 * d);
        double ws = 282.9404 + 4.70935e-5 * d;
        double Ls = normalizeDegrees(Ms + ws);
        double Lm = normalizeDegrees(N + w + v);
        double D  = normalizeDegrees(Lm - Ls);
        double F  = normalizeDegrees(Lm - N);

        lon += -1.274 * sin((M - 2.0 * D) * DEG2RAD)
             +  0.658 * sin(2.0 * D * DEG2RAD)
             -  0.186 * sin(Ms * DEG2RAD)
             -  0.059 * sin((2.0 * M - 2.0 * D) * DEG2RAD)
             -  0.057 * sin((M - 2.0 * D + Ms) * DEG2RAD)
             +  0.053 * sin((M + 2.0 * D) * DEG2RAD)
             +  0.046 * sin((2.0 * D - Ms) * DEG2RAD)
             +  0.041 * sin((M - Ms) * DEG2RAD)
             -  0.035 * sin(D * DEG2RAD)
             -  0.031 * sin((M + Ms) * DEG2RAD)
             -  0.015 * sin((2.0 * F - 2.0 * D) * DEG2RAD)
             +  0.011 * sin((M - 4.0 * D) * DEG2RAD);

        lat += -0.173 * sin((F - 2.0 * D) * DEG2RAD)
             -  0.055 * sin((M - F - 2.0 * D) * DEG2RAD)
             -  0.046 * sin((M + F - 2.0 * D) * DEG2RAD)
             +  0.033 * sin((F + 2.0 * D) * DEG2RAD)
             +  0.017 * sin((2.0 * M + F) * DEG2RAD);

        double obl = 23.4393 - 3.563e-7 * d;
        double xe = cos(lon * DEG2RAD) * cos(lat * DEG2RAD);
        double ye = sin(lon * DEG2RAD) * cos(lat * DEG2RAD);
        double ze = sin(lat * DEG2RAD);

        double xeq = xe;
        double yeq = ye * cos(obl * DEG2RAD) - ze * sin(obl * DEG2RAD);
        double zeq = ye * sin(obl * DEG2RAD) + ze * cos(obl * DEG2RAD);

        raDeg  = normalizeDegrees(atan2(yeq, xeq) * RAD2DEG);
        decDeg = atan2(zeq, sqrt(xeq * xeq + yeq * yeq)) * RAD2DEG;
    }

    double localSiderealDeg(double jd, double lonDeg)
    {
        double T = (jd - 2451545.0) / 36525.0;
        double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0) +
                      0.000387933 * T * T - T * T * T / 38710000.0;
        return normalizeDegrees(gmst + lonDeg);
    }

    struct MoonAngles
    {
        double northPole;
        double zenith;
    };

    MoonAngles moonAngles()
    {
        const double jd = julianFromUtc(Clock::utcEpoch());

        double raM, decM;
        moonPosition(jd, raM, decM);

        /* Position angle of the Moon's north pole, measured from celestial
         * north toward east.  The IAU pole is close enough for a clock. */
        const double P = atan2(
            cos(MOON_POLE_DEC * DEG2RAD) * sin((MOON_POLE_RA - raM) * DEG2RAD),
            sin(MOON_POLE_DEC * DEG2RAD) * cos(decM * DEG2RAD) -
            cos(MOON_POLE_DEC * DEG2RAD) * sin(decM * DEG2RAD) *
                cos((MOON_POLE_RA - raM) * DEG2RAD)) * RAD2DEG;

        /* Parallactic angle: position angle of the local zenith at the Moon,
         * also measured from celestial north toward east. */
        double H = normalizeDegrees(
            localSiderealDeg(jd, Location::longitude()) - raM);
        if (H > 180.0) H -= 360.0;

        const double q = atan2(
            sin(H * DEG2RAD),
            tan(Location::latitude() * DEG2RAD) * cos(decM * DEG2RAD) -
            sin(decM * DEG2RAD) * cos(H * DEG2RAD)) * RAD2DEG;

        return {P, q};
    }

    int moonFrame()
    {
        const double jd = julianFromUtc(Clock::utcEpoch());
        double phase = fmod((jd - NEW_MOON_EPOCH_JD) / SYNODIC_MONTH, 1.0);
        if (phase < 0.0) phase += 1.0;

        int frame = (int)(phase * 30.0);
        if (frame < 0) frame = 0;
        if (frame > 29) frame = 29;
        return frame;
    }

    void applyMoonOrientation(int frame)
    {
        if (!ui_Screen1_Image_Moon) return;

        const MoonAngles angles = moonAngles();
        const float target = (float)(angles.northPole - angles.zenith);
        const float correction = (frame >= 0 && frame < 30)
                                     ? kMoonFrameCorrectionDeg[frame]
                                     : 0.0f;

        float rotation = target
                       - MOON_ASSET_NORTH_OFFSET_DEG
                       - MOON_SCREEN_UP_OFFSET_DEG
                       + correction;
        rotation *= MOON_ROTATION_SIGN;

        while (rotation > 180.0f) rotation -= 360.0f;
        while (rotation < -180.0f) rotation += 360.0f;

        lv_img_set_pivot(ui_Screen1_Image_Moon, 85, 85);
        lv_img_set_angle(ui_Screen1_Image_Moon,
                         (int16_t)lroundf(rotation * 10.0f));
    }

    double altitudeDeg(bool moon, time_t utc, double latDeg, double lonDeg)
    {
        double jd = julianFromUtc(utc);
        double ra, dec;
        if (moon) moonPosition(jd, ra, dec);
        else      sunPosition(jd, ra, dec);

        double lst = localSiderealDeg(jd, lonDeg);
        double H = normalizeDegrees(lst - ra);
        if (H > 180.0) H -= 360.0;

        double alt = sin(latDeg * DEG2RAD) * sin(dec * DEG2RAD) +
                     cos(latDeg * DEG2RAD) * cos(dec * DEG2RAD) *
                     cos(H * DEG2RAD);
        if (alt > 1.0)  alt = 1.0;
        if (alt < -1.0) alt = -1.0;
        return asin(alt) * RAD2DEG;
    }

    bool findEvent(bool moon, time_t start, time_t end, int stepSeconds,
                   bool rising, time_t &event)
    {
        const double threshold = moon ? 0.0 : -0.833;
        time_t prevT = start;
        double prevAlt = altitudeDeg(moon, prevT, Location::latitude(), Location::longitude());
        bool prevUp = prevAlt > threshold;

        for (time_t t = start + stepSeconds; t <= end; t += stepSeconds) {
            double alt = altitudeDeg(moon, t, Location::latitude(), Location::longitude());
            bool up = alt > threshold;

            if (up != prevUp && up == rising) {
                double denom = alt - prevAlt;
                double frac = (fabs(denom) > 1e-9) ? (threshold - prevAlt) / denom : 0.5;
                event = prevT + (time_t)(frac * (double)(t - prevT));
                return true;
            }

            prevT = t;
            prevAlt = alt;
            prevUp = up;
        }
        return false;
    }

    void setEventLabel(lv_obj_t *label, const char *eventText,
                       const char *arrow)
    {
        if (strcmp(eventText, "--") == 0) {
            lv_label_set_text(label, "--");
            return;
        }

        /* Keep a fixed-width slot for the optional +1/+2 marker so all four
         * astronomy arrows line up even when no day offset is shown. */
        char buf[24];
        snprintf(buf, sizeof(buf), "%s%s %s", eventText,
                 strchr(eventText, '+') ? "" : "  ", arrow);
        lv_label_set_text(label, buf);
    }

    int localDayOffset(time_t utcEvent, time_t localOffset,
                       const Clock::DateTime &today)
    {
        if (utcEvent == 0) return 0;

        time_t shifted = utcEvent + localOffset;
        struct tm tmValue;
        if (!gmtime_r(&shifted, &tmValue)) return 0;

        int64_t todayDays = Clock::daysFromCivil(today.year,
                                                 (unsigned)today.month,
                                                 (unsigned)today.day);
        int64_t eventDays = Clock::daysFromCivil(tmValue.tm_year + 1900,
                                                 (unsigned)tmValue.tm_mon + 1,
                                                 (unsigned)tmValue.tm_mday);
        return (int)(eventDays - todayDays);
    }

    void formatEvent(time_t utcEvent, time_t localOffset, char *out,
                     size_t outSize, int dayOffset = 0)
    {
        if (utcEvent == 0) {
            snprintf(out, outSize, "--");
            return;
        }

        time_t shifted = utcEvent + localOffset;
        struct tm tmValue;
        gmtime_r(&shifted, &tmValue);

        /* The original UI shows 24-hour times around the sun/moon icons.
         * Moon events can be on the next day (or the day after when the
         * lunar day is longer than 24 h), so mark those with +1/+2. */
        if (dayOffset > 0) {
            snprintf(out, outSize, "%02d:%02d+%d",
                     tmValue.tm_hour, tmValue.tm_min, dayOffset);
        } else {
            snprintf(out, outSize, "%02d:%02d",
                     tmValue.tm_hour, tmValue.tm_min);
        }
    }

}

namespace Astronomy
{
    void begin()
    {
        update(true);
    }

    void update(bool force)
    {
        unsigned long nowMs = millis();
        if (!force) {
            if (!Clock::dateChanged() && (nowMs - lastUpdateMs < 60000UL)) {
                return;
            }
        }

        Clock::DateTime dt = Clock::now();
        int localOffset = Location::offsetForDate(dt.year, dt.month, dt.day);
        time_t utcNow = Clock::utcEpoch();

        /* All four rise/set rows show the next event after the current
         * instant.  A 30 h window covers a full lunar day, which is slightly
         * longer than 24 h, so the next event cannot fall outside the search. */
        const time_t nextEventEnd = utcNow + 30 * 3600;

        time_t sunrise = 0, sunset = 0, moonrise = 0, moonset = 0;
        findEvent(false, utcNow, nextEventEnd, 600, true,  sunrise);
        findEvent(false, utcNow, nextEventEnd, 600, false, sunset);
        findEvent(true,  utcNow, nextEventEnd, 600, true,  moonrise);
        findEvent(true,  utcNow, nextEventEnd, 600, false, moonset);

        char sunriseText[16], sunsetText[16], moonriseText[16], moonsetText[16];
        formatEvent(sunrise,  localOffset, sunriseText,  sizeof(sunriseText),
                    localDayOffset(sunrise, localOffset, dt));
        formatEvent(sunset,   localOffset, sunsetText,   sizeof(sunsetText),
                    localDayOffset(sunset, localOffset, dt));
        formatEvent(moonrise, localOffset, moonriseText, sizeof(moonriseText),
                    localDayOffset(moonrise, localOffset, dt));
        formatEvent(moonset,  localOffset, moonsetText,  sizeof(moonsetText),
                    localDayOffset(moonset, localOffset, dt));

        currentFrame = moonFrame();

        lv_label_set_text(ui_Screen1_Label_Phase, Location::city());
        lv_img_set_src(ui_Screen1_Image_Moon,
                       (const void *)ui_imgset_moon_[currentFrame]);
        applyMoonOrientation(currentFrame);

        setEventLabel(ui_Screen1_Label_sunrise, sunriseText, "↑");
        setEventLabel(ui_Screen1_Label_sunset, sunsetText, "↓");
        setEventLabel(ui_Screen1_Label_moonrise, moonriseText, "↑");
        setEventLabel(ui_Screen1_Label_moonset, moonsetText, "↓");

        lv_obj_clear_flag(ui_Screen1_Panel_border, LV_OBJ_FLAG_HIDDEN);

        lastUpdateMs = nowMs;
    }

    float moonScreenAngleDeg()
    {
        const MoonAngles angles = moonAngles();
        return (float)(angles.northPole - angles.zenith);
    }

    void updateOrientation()
    {
        currentFrame = moonFrame();
        applyMoonOrientation(currentFrame);
    }

    void splitHundredths(double value, long &whole, long &fraction)
    {
        const long scaled = lround(value * 100.0);
        whole = scaled / 100;
        fraction = scaled % 100;
        if (fraction < 0) fraction = -fraction;
    }

    void printOrientation()
    {
        const MoonAngles angles = moonAngles();
        const float target = (float)(angles.northPole - angles.zenith);
        const int frame = moonFrame();
        const float correction = kMoonFrameCorrectionDeg[frame];
        float applied = target
                      - MOON_ASSET_NORTH_OFFSET_DEG
                      - MOON_SCREEN_UP_OFFSET_DEG
                      + correction;
        applied *= MOON_ROTATION_SIGN;

        long pWhole, pFrac, qWhole, qFrac, tWhole, tFrac;
        long oWhole, oFrac, cWhole, cFrac, aWhole, aFrac;
        splitHundredths(angles.northPole, pWhole, pFrac);
        splitHundredths(angles.zenith, qWhole, qFrac);
        splitHundredths(target, tWhole, tFrac);
        splitHundredths(MOON_ASSET_NORTH_OFFSET_DEG +
                        MOON_SCREEN_UP_OFFSET_DEG, oWhole, oFrac);
        splitHundredths(correction, cWhole, cFrac);
        splitHundredths(applied, aWhole, aFrac);

        Serial.printf("MOON frame=%d P=%ld.%02ld q=%ld.%02ld "
                      "screen=%ld.%02ld offset=%ld.%02ld "
                      "corr=%ld.%02ld applied=%ld.%02ld sign=%d\n",
                      frame, pWhole, pFrac, qWhole, qFrac,
                      tWhole, tFrac, oWhole, oFrac,
                      cWhole, cFrac, aWhole, aFrac,
                      MOON_ROTATION_SIGN < 0.0f ? -1 : 1);
    }
}
