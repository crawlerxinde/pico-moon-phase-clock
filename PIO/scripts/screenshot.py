#!/usr/bin/env python3
"""
Capture the Moon Phase Clock LVGL screen over USB.

The firmware command is:

    C

It responds with a text header followed by raw RGB565 pixels:

    SHOT 640 172 RGB565LE 220160\n
    <220160 bytes>

This script saves the result as a PNG.  It first tries the normal serial port
(pyserial) and falls back to raw USB CDC bulk transfers when the serial device
is not accessible (for example when the user is not in the ``dialout`` group).

Examples:

    python scripts/screenshot.py
    python scripts/screenshot.py --output clock.png
    python scripts/screenshot.py --port /dev/ttyACM0
    python scripts/screenshot.py --usb

Note: this is a snapshot of LVGL's logical 640x172 screen, not a readback of
the display controller's physical framebuffer.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import os
import struct
import sys
import time
import zlib

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - only when pyserial is missing
    serial = None
    list_ports = None

try:
    import usb.core
    import usb.util
except ImportError:  # pragma: no cover - only when pyusb is missing
    usb = None


WAVESHARE_VID = 0x2E8A
WAVESHARE_PID = 0x000F


class ScreenshotError(RuntimeError):
    pass


def _png_chunk(tag: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def write_png(path: str, width: int, height: int, fmt: str, pixels: bytes) -> None:
    """Convert an RGB565 buffer to an RGB888 PNG."""
    expected = width * height * 2
    if len(pixels) < expected:
        raise ScreenshotError(
            f"short pixel buffer: got {len(pixels)}, expected {expected}"
        )

    lut5 = [(i * 255 + 15) // 31 for i in range(32)]
    lut6 = [(i * 255 + 31) // 63 for i in range(64)]
    raw = bytearray()

    big_endian = fmt == "RGB565BE"
    offset = 0
    for _y in range(height):
        raw.append(0)  # PNG filter type: none
        for _x in range(width):
            lo = pixels[offset]
            hi = pixels[offset + 1]
            offset += 2
            value = (lo << 8) | hi if big_endian else lo | (hi << 8)
            raw.append(lut5[(value >> 11) & 0x1F])
            raw.append(lut6[(value >> 5) & 0x3F])
            raw.append(lut5[value & 0x1F])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + _png_chunk(b"IEND", b"")
    )
    with open(path, "wb") as fh:
        fh.write(png)


def _parse_header(line: str) -> tuple[int, int, str, int]:
    parts = line.strip().split()
    if len(parts) != 5 or parts[0] != "SHOT":
        raise ScreenshotError(f"unexpected screenshot header: {line!r}")
    try:
        width = int(parts[1])
        height = int(parts[2])
        fmt = parts[3].upper()
        size = int(parts[4])
    except ValueError as exc:
        raise ScreenshotError(f"bad screenshot header: {line!r}") from exc

    if fmt not in ("RGB565LE", "RGB565BE"):
        raise ScreenshotError(f"unsupported pixel format: {fmt}")
    if width <= 0 or height <= 0 or size != width * height * 2:
        raise ScreenshotError(
            f"inconsistent screenshot geometry: {width}x{height}, {size} bytes"
        )
    return width, height, fmt, size


class SerialTransport:
    def __init__(self, port: str, timeout: float):
        if serial is None:
            raise ScreenshotError("pyserial is not installed")
        self.port = port
        self.timeout = timeout
        self.ser = None

    def open(self) -> None:
        self.ser = serial.Serial(
            self.port,
            115200,
            timeout=0.5,
            write_timeout=2,
            dsrdtr=False,
            rtscts=False,
        )
        time.sleep(0.2)
        try:
            self.ser.reset_input_buffer()
        except Exception:
            pass

    def send(self, data: bytes) -> None:
        assert self.ser is not None
        self.ser.write(data)
        self.ser.flush()

    def read_line(self, deadline: float) -> bytes:
        assert self.ser is not None
        buf = bytearray()
        while time.time() < deadline:
            chunk = self.ser.read(1)
            if chunk:
                buf += chunk
                if chunk == b"\n":
                    return bytes(buf)
        raise ScreenshotError("timeout waiting for screenshot header")

    def read_exact(self, count: int, deadline: float) -> bytes:
        assert self.ser is not None
        buf = bytearray()
        while len(buf) < count:
            if time.time() > deadline:
                raise ScreenshotError(
                    f"timeout receiving pixels ({len(buf)}/{count})"
                )
            chunk = self.ser.read(min(4096, count - len(buf)))
            if chunk:
                buf += chunk
        return bytes(buf)

    def close(self) -> None:
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None


class UsbCdcTransport:
    """Raw USB CDC transport, useful when /dev/ttyACM* is not accessible."""

    def __init__(self, timeout: float):
        if usb is None:
            raise ScreenshotError("pyusb is not installed")
        self.timeout = timeout
        self.dev = None
        self.ep_in = None
        self.ep_out = None
        self._rx = bytearray()

    def open(self) -> None:
        self.dev = usb.core.find(idVendor=WAVESHARE_VID, idProduct=WAVESHARE_PID)
        if self.dev is None:
            raise ScreenshotError(
                "RP2350 CDC device not found (is the board running the clock?)"
            )

        try:
            self.dev.set_configuration()
        except Exception:
            pass

        for number in (0, 1):
            try:
                if self.dev.is_kernel_driver_active(number):
                    self.dev.detach_kernel_driver(number)
            except Exception:
                pass

        for number in (0, 1):
            try:
                usb.util.claim_interface(self.dev, number)
            except Exception as exc:
                raise ScreenshotError(
                    f"could not claim USB interface {number}: {exc}"
                ) from exc

        # Standard CDC ACM line coding: 115200 8N1, DTR+RTS asserted.
        try:
            self.dev.ctrl_transfer(
                0x21,
                0x20,
                0,
                0,
                struct.pack("<IBBB", 115200, 0, 0, 8),
                timeout=1000,
            )
            self.dev.ctrl_transfer(0x21, 0x22, 0x0003, 0, None, timeout=1000)
        except Exception:
            pass

        cfg = self.dev.get_active_configuration()
        intf = cfg[(1, 0)]
        self.ep_out = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_direction(
                ep.bEndpointAddress
            )
            == usb.util.ENDPOINT_OUT,
        )
        self.ep_in = usb.util.find_descriptor(
            intf,
            custom_match=lambda ep: usb.util.endpoint_direction(
                ep.bEndpointAddress
            )
            == usb.util.ENDPOINT_IN,
        )
        if self.ep_in is None or self.ep_out is None:
            raise ScreenshotError("CDC bulk endpoints not found")

        # Discard any boot/help text that was queued before we connected.
        self._drain(0.2)

    def _drain(self, seconds: float) -> None:
        end = time.time() + seconds
        while time.time() < end:
            try:
                chunk = self.ep_in.read(64, timeout=100)
            except usb.core.USBTimeoutError:
                continue
            except Exception:
                break
            if chunk:
                self._rx += bytes(chunk)
        self._rx.clear()

    def _read_more(self, deadline: float) -> bytes:
        while time.time() < deadline:
            try:
                chunk = self.ep_in.read(64, timeout=200)
            except usb.core.USBTimeoutError:
                continue
            except Exception as exc:
                raise ScreenshotError(f"USB read failed: {exc}") from exc
            if chunk:
                return bytes(chunk)
        raise ScreenshotError("timeout waiting for USB data")

    def send(self, data: bytes) -> None:
        self.ep_out.write(data, timeout=2000)

    def read_line(self, deadline: float) -> bytes:
        while b"\n" not in self._rx:
            self._rx += self._read_more(deadline)
        index = self._rx.index(b"\n") + 1
        line = bytes(self._rx[:index])
        del self._rx[:index]
        return line

    def read_exact(self, count: int, deadline: float) -> bytes:
        buf = bytearray()
        while len(buf) < count:
            if self._rx:
                take = min(count - len(buf), len(self._rx))
                buf += self._rx[:take]
                del self._rx[:take]
                continue
            self._rx += self._read_more(deadline)
        return bytes(buf)

    def close(self) -> None:
        if self.dev is None:
            return
        for number in (1, 0):
            try:
                usb.util.release_interface(self.dev, number)
            except Exception:
                pass
        # Rebind the kernel driver so /dev/ttyACM* works again.
        for number in (0, 1):
            try:
                self.dev.attach_kernel_driver(number)
            except Exception:
                pass
        self.dev = None


def _is_serial_path(path: str) -> bool:
    return path.startswith("/dev/") or path.upper().startswith("COM")


def candidate_ports(preferred: str | None = None) -> list[str]:
    if preferred:
        return [preferred] if _is_serial_path(preferred) else []

    found: list[str] = []
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

    if sys.platform == "darwin":
        cu_ports = {p for p in found if p.startswith("/dev/cu.")}
        if cu_ports:
            found = [
                p
                for p in found
                if not (
                    p.startswith("/dev/tty.")
                    and p.replace("/dev/tty.", "/dev/cu.") in cu_ports
                )
            ]

    result: list[str] = []
    seen: set[str] = set()
    for port in found:
        if port and port not in seen:
            seen.add(port)
            result.append(port)
    return result


def _open_usb_transport(timeout: float) -> UsbCdcTransport:
    transport = UsbCdcTransport(timeout)
    try:
        transport.open()
    except Exception:
        # Make sure a detached kernel driver is rebound if open() failed
        # part-way through.
        transport.close()
        raise
    return transport


def open_transport(args: argparse.Namespace):
    errors: list[str] = []

    if args.usb:
        return _open_usb_transport(args.timeout), "usb-cdc"

    if not args.serial_only and serial is not None:
        for port in candidate_ports(args.port):
            try:
                transport = SerialTransport(port, args.timeout)
                transport.open()
                return transport, port
            except Exception as exc:
                errors.append(f"{port}: {exc}")

    if not args.serial_only and usb is not None:
        try:
            transport = _open_usb_transport(args.timeout)
            return transport, "usb-cdc"
        except Exception as exc:
            errors.append(f"usb-cdc: {exc}")

    if args.port and not args.usb:
        raise ScreenshotError(
            f"could not open serial port {args.port}: "
            + ("; ".join(errors) if errors else "unknown error")
        )
    raise ScreenshotError(
        "could not connect to the board: "
        + ("; ".join(errors) if errors else "no serial/USB transport available")
    )


def capture(args: argparse.Namespace) -> str:
    transport = None
    try:
        transport, label = open_transport(args)
        print(f"[screenshot] connected via {label}")
        transport.send(b"C\n")

        deadline = time.time() + args.timeout
        while True:
            line = transport.read_line(deadline)
            text = line.decode("ascii", "replace").strip()
            if text.startswith("SHOT "):
                width, height, fmt, size = _parse_header(text)
                break
            if text.startswith("ERR "):
                raise ScreenshotError(f"device error: {text}")
            if "Unknown command" in text:
                raise ScreenshotError(
                    "firmware does not support screenshots; flash the latest build"
                )
            if text:
                print(f"[screenshot] device: {text}", file=sys.stderr)

        print(
            f"[screenshot] receiving {width}x{height} {fmt} "
            f"({size} bytes)"
        )
        pixels = transport.read_exact(size, time.time() + args.timeout)

        output = args.output
        if output is None:
            stamp = _dt.datetime.now().strftime("%Y%m%d_%H%M%S")
            output = f"screenshot_{stamp}.png"
        write_png(output, width, height, fmt, pixels)

        if args.raw:
            raw_path = os.path.splitext(output)[0] + ".rgb565"
            with open(raw_path, "wb") as fh:
                fh.write(pixels)
            print(f"[screenshot] raw pixels: {raw_path}")

        print(f"[screenshot] saved {output}")
        return output
    finally:
        if transport is not None:
            transport.close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Capture the Moon Phase Clock screen as a PNG"
    )
    parser.add_argument(
        "--port",
        help="serial port, e.g. /dev/ttyACM0 or COM5 (default: auto-detect)",
    )
    parser.add_argument(
        "--output",
        help="PNG output path (default: screenshot_YYYYmmdd_HHMMSS.png)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=15.0,
        help="seconds to wait for the board / pixels (default: 15)",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="also save the raw RGB565 data next to the PNG",
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--usb",
        action="store_true",
        help="use raw USB CDC instead of /dev/ttyACM*",
    )
    group.add_argument(
        "--serial-only",
        action="store_true",
        help="only use the serial port, do not fall back to raw USB",
    )
    args = parser.parse_args(argv)

    try:
        capture(args)
    except KeyboardInterrupt:
        print("\n[screenshot] cancelled", file=sys.stderr)
        return 130
    except Exception as exc:
        print(f"[screenshot] error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
