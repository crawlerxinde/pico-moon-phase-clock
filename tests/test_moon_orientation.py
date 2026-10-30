#!/usr/bin/env python3
"""Compare generated equipment moon images with timeanddate renderings.

The reference PNGs are captured once from timeanddate's moon.php renderer by
tests/fetch_moon_references.py.  This test does not flash the board; it applies
the same frame selection/rotation logic to the static assets and checks the
lit-limb direction across a full lunar month.
"""

from __future__ import annotations

import json
import math
import subprocess
import sys
import tempfile
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import moon_model  # noqa: E402

REFERENCE_DIR = ROOT / "moon_reference"
MANIFEST = REFERENCE_DIR / "manifest.json"
ASSET_DIR = ROOT.parent / "Assets" / "Moon_Phase"
BJT = timezone(timedelta(hours=8))


def image_stats(path: Path, size: int = 120) -> tuple[float, float, float]:
    """Return (bright-vector angle, vector length, bright fraction)."""
    raw = subprocess.check_output(
        [
            "magick",
            str(path),
            "-resize",
            f"{size}x{size}!",
            "-background",
            "black",
            "-alpha",
            "remove",
            "-colorspace",
            "Gray",
            "-depth",
            "8",
            "gray:-",
        ]
    )
    pixels = list(raw)
    points = [(x, y) for y in range(size) for x in range(size) if pixels[y * size + x] > 8]
    if not points:
        raise AssertionError(f"no moon pixels in {path}")
    xmin = min(x for x, _ in points)
    xmax = max(x for x, _ in points)
    ymin = min(y for _, y in points)
    ymax = max(y for _, y in points)
    cx = (xmin + xmax) / 2.0
    cy = (ymin + ymax) / 2.0
    radius = max(1.0, (xmax - xmin + ymax - ymin) / 4.0)

    weight = 0.0
    wx = 0.0
    wy = 0.0
    bright = 0
    for y in range(ymin, ymax + 1):
        for x in range(xmin, xmax + 1):
            value = pixels[y * size + x]
            if value <= 8:
                continue
            weight += value
            wx += value * x
            wy += value * y
            if value > 110:
                bright += 1
    if weight == 0:
        raise AssertionError(f"empty moon in {path}")
    dx = (wx / weight - cx) / radius
    dy = (wy / weight - cy) / radius
    return math.degrees(math.atan2(dx, -dy)), math.hypot(dx, dy), bright / max(1, len(points))


def angle_error(a: float, b: float) -> float:
    return abs((a - b + 180.0) % 360.0 - 180.0)


def main() -> int:
    if not MANIFEST.exists():
        raise SystemExit(f"missing {MANIFEST}; run tests/fetch_moon_references.py")
    entries = json.loads(MANIFEST.read_text())
    failures = []
    checked = 0
    with tempfile.TemporaryDirectory(prefix="moon-orientation-") as tmp:
        tmp_path = Path(tmp)
        for entry in entries:
            local = datetime.fromisoformat(entry["local"])
            jd = moon_model.julian_from_utc(local)
            generated = tmp_path / entry["reference"]
            values = moon_model.render_device_image(ASSET_DIR, jd, generated)
            reference = REFERENCE_DIR / entry["reference"]
            if not reference.exists():
                failures.append(f"{entry['reference']}: missing reference")
                continue

            ref_angle, ref_len, ref_frac = image_stats(reference)
            gen_angle, gen_len, gen_frac = image_stats(generated)
            illumination = float(values["illumination"])
            # At new/full the centroid direction is not meaningful.  The
            # image is still generated so this loop covers every frame.
            if not 0.15 < illumination < 0.85:
                continue
            checked += 1
            error = angle_error(ref_angle, gen_angle)
            if error > 35.0:
                failures.append(
                    f"{entry['reference']}: lit-side angle {gen_angle:.1f}° != "
                    f"timeanddate {ref_angle:.1f}° (error {error:.1f}°)"
                )
            print(
                f"{entry['local']}: frame={values['frame']:2d} "
                f"illum={illumination:.3f} ref={ref_angle:7.1f}° "
                f"gen={gen_angle:7.1f}° err={error:5.1f}°"
            )
    if checked < 8:
        failures.append(f"only {checked} illuminated phases checked; expected at least 8")
    if failures:
        print("\nFAILURES:")
        print("\n".join(failures))
        return 1
    print(f"\nOK: compared {checked} illuminated phases across the lunation")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
