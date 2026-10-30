"""Host-side moon model used by the orientation regression test.

The formulas mirror PIO/src/astronomy.cpp.  Keeping the model here lets the
test render the 30 static frames without flashing the RP2350.
"""

from __future__ import annotations

import math
import subprocess
from datetime import datetime, timezone
from pathlib import Path

DEG = math.pi / 180.0
RAD = 180.0 / math.pi
SYNODIC_MONTH = 29.530588853
NEW_MOON_EPOCH_JD = 2451550.1
LATITUDE = 39.9289
LONGITUDE = 116.3883

# Same table as PIO/src/astronomy.cpp.
FRAME_CORRECTION_DEG = (
    0.0, 0.0, -20.0, -18.5, -15.0, -13.0, -6.5, -0.5,
    7.0, 11.5, 19.0, 21.0, 22.5, 10.5, 0.0, 0.0,
    0.0, 23.5, 20.5, 16.5, 11.0, 5.0, -1.5, -8.5,
    -15.5, -21.0, -23.5, -28.0, -30.0, 0.0,
)


def normalize_degrees(value: float) -> float:
    return value % 360.0


def signed_degrees(value: float) -> float:
    value = value % 360.0
    return value - 360.0 if value > 180.0 else value


def julian_from_utc(value: datetime) -> float:
    return value.astimezone(timezone.utc).timestamp() / 86400.0 + 2440587.5


def days_since_2000(jd: float) -> float:
    return jd - 2451543.5


def sun_position(jd: float) -> tuple[float, float]:
    d = days_since_2000(jd)
    w = 282.9404 + 4.70935e-5 * d
    e = 0.016709 - 1.151e-9 * d
    m = normalize_degrees(356.0470 + 0.9856002585 * d)
    eccentric = m + e * RAD * math.sin(m * DEG) * (1.0 + e * math.cos(m * DEG))
    for _ in range(3):
        eccentric -= (eccentric - e * RAD * math.sin(eccentric * DEG) - m) / (
            1.0 - e * math.cos(eccentric * DEG)
        )
    xv = math.cos(eccentric * DEG) - e
    yv = math.sqrt(1.0 - e * e) * math.sin(eccentric * DEG)
    v = math.atan2(yv, xv) * RAD
    longitude = normalize_degrees(v + w)
    x = math.cos(longitude * DEG)
    y = math.sin(longitude * DEG)
    obliquity = 23.4393 - 3.563e-7 * d
    ra = normalize_degrees(
        math.atan2(y * math.cos(obliquity * DEG), x) * RAD
    )
    dec = math.atan2(
        y * math.sin(obliquity * DEG), math.hypot(x, y)
    ) * RAD
    return ra, dec


def moon_position(jd: float) -> tuple[float, float]:
    d = days_since_2000(jd)
    n = normalize_degrees(125.1228 - 0.0529538083 * d)
    i = 5.1454
    w = normalize_degrees(318.0634 + 0.1643573223 * d)
    a = 60.2666
    e = 0.054900
    m = normalize_degrees(115.3654 + 13.0649929509 * d)
    eccentric = m + e * RAD * math.sin(m * DEG) * (1.0 + e * math.cos(m * DEG))
    for _ in range(4):
        eccentric -= (eccentric - e * RAD * math.sin(eccentric * DEG) - m) / (
            1.0 - e * math.cos(eccentric * DEG)
        )
    xv = a * (math.cos(eccentric * DEG) - e)
    yv = a * math.sqrt(1.0 - e * e) * math.sin(eccentric * DEG)
    v = math.atan2(yv, xv) * RAD
    r = math.hypot(xv, yv)
    vw = (v + w) * DEG
    nr = n * DEG
    ir = i * DEG
    xh = r * (math.cos(nr) * math.cos(vw) - math.sin(nr) * math.sin(vw) * math.cos(ir))
    yh = r * (math.sin(nr) * math.cos(vw) + math.cos(nr) * math.sin(vw) * math.cos(ir))
    zh = r * math.sin(vw) * math.sin(ir)
    longitude = math.atan2(yh, xh) * RAD
    latitude = math.atan2(zh, math.hypot(xh, yh)) * RAD

    ms = normalize_degrees(356.0470 + 0.9856002585 * d)
    ws = 282.9404 + 4.70935e-5 * d
    ls = normalize_degrees(ms + ws)
    lm = normalize_degrees(n + w + v)
    dd = normalize_degrees(lm - ls)
    f = normalize_degrees(lm - n)
    longitude += (
        -1.274 * math.sin((m - 2.0 * dd) * DEG)
        + 0.658 * math.sin(2.0 * dd * DEG)
        - 0.186 * math.sin(ms * DEG)
        - 0.059 * math.sin((2.0 * m - 2.0 * dd) * DEG)
        - 0.057 * math.sin((m - 2.0 * dd + ms) * DEG)
        + 0.053 * math.sin((m + 2.0 * dd) * DEG)
        + 0.046 * math.sin((2.0 * dd - ms) * DEG)
        + 0.041 * math.sin((m - ms) * DEG)
        - 0.035 * math.sin(dd * DEG)
        - 0.031 * math.sin((m + ms) * DEG)
        - 0.015 * math.sin((2.0 * f - 2.0 * dd) * DEG)
        + 0.011 * math.sin((m - 4.0 * dd) * DEG)
    )
    latitude += (
        -0.173 * math.sin((f - 2.0 * dd) * DEG)
        - 0.055 * math.sin((m - f - 2.0 * dd) * DEG)
        - 0.046 * math.sin((m + f - 2.0 * dd) * DEG)
        + 0.033 * math.sin((f + 2.0 * dd) * DEG)
        + 0.017 * math.sin((2.0 * m + f) * DEG)
    )
    obliquity = 23.4393 - 3.563e-7 * d
    xe = math.cos(longitude * DEG) * math.cos(latitude * DEG)
    ye = math.sin(longitude * DEG) * math.cos(latitude * DEG)
    ze = math.sin(latitude * DEG)
    xeq = xe
    yeq = ye * math.cos(obliquity * DEG) - ze * math.sin(obliquity * DEG)
    zeq = ye * math.sin(obliquity * DEG) + ze * math.cos(obliquity * DEG)
    ra = normalize_degrees(math.atan2(yeq, xeq) * RAD)
    dec = math.atan2(zeq, math.hypot(xeq, yeq)) * RAD
    return ra, dec


def local_sidereal_deg(jd: float) -> float:
    t = (jd - 2451545.0) / 36525.0
    gmst = (
        280.46061837
        + 360.98564736629 * (jd - 2451545.0)
        + 0.000387933 * t * t
        - t * t * t / 38710000.0
    )
    return normalize_degrees(gmst + LONGITUDE)


def angles(jd: float) -> tuple[float, float, float]:
    ra_m, dec_m = moon_position(jd)
    ra_s, dec_s = sun_position(jd)
    pole_ra = 270.0
    pole_dec = 66.56
    north = math.atan2(
        math.cos(pole_dec * DEG) * math.sin((pole_ra - ra_m) * DEG),
        math.sin(pole_dec * DEG) * math.cos(dec_m * DEG)
        - math.cos(pole_dec * DEG)
        * math.sin(dec_m * DEG)
        * math.cos((pole_ra - ra_m) * DEG),
    ) * RAD
    hour_angle = signed_degrees(local_sidereal_deg(jd) - ra_m)
    parallactic = math.atan2(
        math.sin(hour_angle * DEG),
        math.tan(LATITUDE * DEG) * math.cos(dec_m * DEG)
        - math.sin(dec_m * DEG) * math.cos(hour_angle * DEG),
    ) * RAD
    sun_angle = math.atan2(
        math.cos(dec_s * DEG) * math.sin((ra_s - ra_m) * DEG),
        math.sin(dec_s * DEG) * math.cos(dec_m * DEG)
        - math.cos(dec_s * DEG)
        * math.sin(dec_m * DEG)
        * math.cos((ra_s - ra_m) * DEG),
    ) * RAD
    return north, parallactic, sun_angle


def phase_frame(jd: float) -> int:
    phase = ((jd - NEW_MOON_EPOCH_JD) / SYNODIC_MONTH) % 1.0
    return min(29, max(0, int(phase * 30.0)))


def illumination(jd: float) -> float:
    phase = ((jd - NEW_MOON_EPOCH_JD) / SYNODIC_MONTH) % 1.0
    return (1.0 - math.cos(2.0 * math.pi * phase)) / 2.0


def screen_terms(jd: float) -> dict[str, float | int]:
    north, parallactic, sun_angle = angles(jd)
    frame = phase_frame(jd)
    terminator = signed_degrees(sun_angle - parallactic + 90.0)
    correction = FRAME_CORRECTION_DEG[frame]
    rotation = correction - terminator
    if 16 <= frame <= 28:
        rotation += 180.0
    return {
        "frame": frame,
        "north": signed_degrees(north),
        "parallactic": signed_degrees(parallactic),
        "sun": signed_degrees(sun_angle),
        "terminator": terminator,
        "correction": correction,
        "rotation": signed_degrees(rotation),
        "illumination": illumination(jd),
    }


def render_device_image(
    asset_dir: Path, jd: float, output: Path, background: str = "black"
) -> dict[str, float | int]:
    """Rotate the static asset like LVGL and write a 170x170 preview."""
    values = screen_terms(jd)
    asset = Path(asset_dir) / f"Moon_{int(values['frame']) + 1}.png"
    # ImageMagick's positive rotation matches the visual direction used by
    # LVGL's positive image angle for these upright assets.
    subprocess.run(
        [
            "magick",
            str(asset),
            "-background",
            background,
            "-alpha",
            "remove",
            "-alpha",
            "off",
            "-rotate",
            f"{values['rotation']:.4f}",
            str(output),
        ],
        check=True,
    )
    return values
