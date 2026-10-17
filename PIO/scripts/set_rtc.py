#!/usr/bin/env python3
"""
Set the Moon Phase Clock RTC to the host's local time.

The firmware accepts this command on its USB CDC serial console:

    T YYYY-MM-DD HH:MM:SS

This helper is used automatically after `pio run -t upload` (see
scripts/post_upload.py).  It can also be run manually:

    python scripts/set_rtc.py
    python scripts/set_rtc.py --port /dev/cu.usbmodem1234
    python scripts/set_rtc.py --time "2026-10-17 22:30:00"
"""

from __future__ import annotations

import argparse
import datetime as _dt
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - only happens outside PlatformIO
    serial = None
    list_ports = None


WAVESHARE_VID = 0x2E8A  # Raspberry Pi / RP2350
WAVESHARE_PID = 0x000F  # Pico 2 / RP2350 USB CDC in this project


def _is_serial_port(path: str) -> bool:
    if not path:
        return False
    return path.startswith("/dev/") or path.upper().startswith("COM")


def _candidate_ports(preferred: str | None = None) -> list[str]:
    """Return likely board serial ports, preferred first."""
    found: list[str] = []

    if _is_serial_port(preferred or ""):
        found.append(preferred)  # type: ignore[arg-type]

    if list_ports is not None:
        for info in list_ports.comports():
            vid = getattr(info, "vid", None)
            pid = getattr(info, "pid", None)
            product = (getattr(info, "product", "") or "").lower()
            description = (getattr(info, "description", "") or "").lower()

            if vid == WAVESHARE_VID and pid == WAVESHARE_PID:
                found.append(info.device)
            elif vid == WAVESHARE_VID:
                found.append(info.device)
            elif "rp2350" in product or "rp2350" in description:
                found.append(info.device)
            elif "pico" in product or "pico" in description:
                found.append(info.device)

    # On macOS, opening /dev/tty.* can block; prefer /dev/cu.*.
    if sys.platform == "darwin":
        cu_ports = {p for p in found if p.startswith("/dev/cu.")}
        if cu_ports:
            found = [p for p in found
                     if not (p.startswith("/dev/tty.") and
                             p.replace("/dev/tty.", "/dev/cu.") in cu_ports)]

    # Deduplicate while preserving order.
    result: list[str] = []
    seen: set[str] = set()
    for port in found:
        if port and port not in seen:
            seen.add(port)
            result.append(port)
    return result


def _try_port(port: str, timestamp: str, verbose: bool) -> bool:
    if serial is None:
        return False

    try:
        with serial.Serial(port, 115200, timeout=1, write_timeout=2,
                           dsrdtr=False, rtscts=False) as ser:
            # Give the firmware time to finish booting / enumerate.
            time.sleep(1.0)
            try:
                ser.reset_input_buffer()
            except Exception:
                pass

            command = f"T {timestamp}\n".encode("ascii")
            ser.write(command)
            ser.flush()

            deadline = time.time() + 3.0
            response = b""
            while time.time() < deadline:
                chunk = ser.read(256)
                if chunk:
                    response += chunk
                if b"RTC set" in response:
                    if verbose:
                        print(f"[set_rtc] RTC set via {port}: {timestamp}")
                    return True

            if verbose and response:
                text = response.decode("ascii", errors="replace").strip()
                print(f"[set_rtc] {port}: {text}")
    except Exception as exc:
        if verbose:
            print(f"[set_rtc] {port}: {exc}")
    return False


def set_rtc(port: str | None = None,
            timeout: float = 25.0,
            timestamp: str | None = None,
            verbose: bool = True) -> bool:
    """Set the board RTC.  Returns True when the firmware acknowledges."""
    if serial is None:
        if verbose:
            print("[set_rtc] pyserial is not installed; skip RTC calibration")
        return False

    if timestamp is None:
        timestamp = _dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    deadline = time.time() + timeout
    last_ports: list[str] = []

    while time.time() < deadline:
        ports = _candidate_ports(port)
        last_ports = ports
        if ports:
            for candidate in ports:
                if _try_port(candidate, timestamp, verbose):
                    return True
        time.sleep(0.5)

    if verbose:
        if last_ports:
            print(f"[set_rtc] could not reach the board on {last_ports}")
        else:
            print("[set_rtc] no RP2350 serial port found")
        print(f'[set_rtc] open the serial monitor and run: T {timestamp}')
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Set the clock RTC over USB serial")
    parser.add_argument("--port", help="serial port, e.g. /dev/cu.usbmodem1234 or COM5")
    parser.add_argument("--timeout", type=float, default=25.0,
                        help="seconds to wait for the board (default: 25)")
    parser.add_argument("--time", dest="timestamp",
                        help='local time to set, format "YYYY-MM-DD HH:MM:SS"')
    args = parser.parse_args(argv)

    if args.timestamp:
        try:
            _dt.datetime.strptime(args.timestamp, "%Y-%m-%d %H:%M:%S")
        except ValueError:
            parser.error('--time must use the format "YYYY-MM-DD HH:MM:SS"')

    ok = set_rtc(port=args.port, timeout=args.timeout, timestamp=args.timestamp)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
