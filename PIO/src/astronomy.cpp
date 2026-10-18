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

    unsigned long lastUpdateMs = 0;

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

    void formatEvent(time_t utcEvent, time_t localOffset, char *out, size_t outSize)
    {
        if (utcEvent == 0) {
            snprintf(out, outSize, "--");
            return;
        }

        time_t shifted = utcEvent + localOffset;
        struct tm tmValue;
        gmtime_r(&shifted, &tmValue);

        /* The original UI shows 24-hour times around the sun/moon icons. */
        snprintf(out, outSize, "%02d:%02d", tmValue.tm_hour, tmValue.tm_min);
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
        time_t localMidnight = (time_t)(Clock::daysFromCivil(dt.year, (unsigned)dt.month, (unsigned)dt.day) * 86400);
        time_t utcStart = localMidnight - localOffset;

        time_t sunrise = 0, sunset = 0, moonrise = 0, moonset = 0;
        findEvent(false, utcStart, utcStart + 86400, 600, true,  sunrise);
        findEvent(false, utcStart, utcStart + 86400, 600, false, sunset);
        findEvent(true,  utcStart, utcStart + 86400, 600, true,  moonrise);
        findEvent(true,  utcStart, utcStart + 86400, 600, false, moonset);

        char sunriseText[16], sunsetText[16], moonriseText[16], moonsetText[16];
        formatEvent(sunrise,  localOffset, sunriseText,  sizeof(sunriseText));
        formatEvent(sunset,   localOffset, sunsetText,   sizeof(sunsetText));
        formatEvent(moonrise, localOffset, moonriseText, sizeof(moonriseText));
        formatEvent(moonset,  localOffset, moonsetText,  sizeof(moonsetText));

        time_t utcNow = Clock::utcEpoch();
        double jd = julianFromUtc(utcNow);
        double phase = fmod((jd - NEW_MOON_EPOCH_JD) / SYNODIC_MONTH, 1.0);
        if (phase < 0.0) phase += 1.0;

        int frame = (int)(phase * 30.0);
        if (frame < 0) frame = 0;
        if (frame > 29) frame = 29;

        lv_label_set_text(ui_Screen1_Label_Phase, Location::city());
        lv_img_set_src(ui_Screen1_Image_Moon, (const void *)ui_imgset_moon_[frame]);

        char eventBuf[24];
        snprintf(eventBuf, sizeof(eventBuf), "%s ↑", sunriseText);
        lv_label_set_text(ui_Screen1_Label_sunrise, eventBuf);
        snprintf(eventBuf, sizeof(eventBuf), "%s ↓", sunsetText);
        lv_label_set_text(ui_Screen1_Label_sunset, eventBuf);
        snprintf(eventBuf, sizeof(eventBuf), "%s ↑", moonriseText);
        lv_label_set_text(ui_Screen1_Label_moonrise, eventBuf);
        snprintf(eventBuf, sizeof(eventBuf), "%s ↓", moonsetText);
        lv_label_set_text(ui_Screen1_Label_moonset, eventBuf);

        lv_obj_clear_flag(ui_Screen1_Panel_border, LV_OBJ_FLAG_HIDDEN);

        lastUpdateMs = nowMs;
    }
}
