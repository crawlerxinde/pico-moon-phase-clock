#!/usr/bin/env python3
"""Set the clock RTC to the host's current local time over USB CDC."""

from __future__ import annotations

import argparse
import datetime as _dt
import struct
import sys
import time

try:
    import usb.core
    import usb.util
except ImportError:  # pragma: no cover
    usb = None

VID = 0x2E8A
PID = 0x000F


def sync_over_usb(epoch: int, timestamp: str, timeout: float = 4.0) -> bool:
    if usb is None:
        return False

    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        return False

    try:
        dev.set_configuration()
    except Exception:
        pass

    for interface in (0, 1):
        try:
            if dev.is_kernel_driver_active(interface):
                dev.detach_kernel_driver(interface)
        except Exception:
            pass
    for interface in (0, 1):
        try:
            usb.util.claim_interface(dev, interface)
        except Exception:
            pass

    try:
        dev.ctrl_transfer(
            0x21, 0x20, 0, 0,
            struct.pack("<IBBB", 115200, 0, 0, 8),
            timeout=1000,
        )
        dev.ctrl_transfer(0x21, 0x22, 0x0003, 0, None, timeout=1000)
    except Exception:
        pass

    cfg = dev.get_active_configuration()
    interface = cfg[(1, 0)]
    ep_out = usb.util.find_descriptor(
        interface,
        custom_match=lambda ep: usb.util.endpoint_direction(ep.bEndpointAddress)
        == usb.util.ENDPOINT_OUT,
    )
    ep_in = usb.util.find_descriptor(
        interface,
        custom_match=lambda ep: usb.util.endpoint_direction(ep.bEndpointAddress)
        == usb.util.ENDPOINT_IN,
    )
    if ep_out is None or ep_in is None:
        return False

    try:
        ep_out.write((f"U {epoch}\n").encode("ascii"), timeout=2000)
    except Exception as exc:
        print(f"[sync_time] write failed: {exc}", file=sys.stderr)
        return False

    response = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            chunk = ep_in.read(256, timeout=300)
        except usb.core.USBTimeoutError:
            continue
        except Exception:
            break
        if chunk:
            response += bytes(chunk)
            if b"RTC set" in response:
                break

    for interface in (1, 0):
        try:
            usb.util.release_interface(dev, interface)
        except Exception:
            pass
    for interface in (0, 1):
        try:
            dev.attach_kernel_driver(interface)
        except Exception:
            pass

    if b"RTC set" in response:
        print(f"[sync_time] RTC set to {timestamp}")
        return True

    text = response.decode("ascii", "replace").strip()
    print(f"[sync_time] no RTC acknowledgement: {text}", file=sys.stderr)
    return False


def fallback_serial(timestamp: str) -> bool:
    try:
        from pathlib import Path
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from set_rtc import set_rtc
    except Exception as exc:
        print(f"[sync_time] serial fallback unavailable: {exc}", file=sys.stderr)
        return False
    return set_rtc(timeout=8.0, timestamp=timestamp)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--time", dest="timestamp",
                        help='local time, format "YYYY-MM-DD HH:MM:SS"')
    args = parser.parse_args(argv)

    timestamp = args.timestamp or _dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    if args.timestamp:
        try:
            _dt.datetime.strptime(timestamp, "%Y-%m-%d %H:%M:%S")
        except ValueError:
            parser.error('--time must use the format "YYYY-MM-DD HH:MM:SS"')

    epoch = int(
        _dt.datetime.strptime(timestamp, "%Y-%m-%d %H:%M:%S").timestamp()
        if args.timestamp else time.time()
    )
    for _ in range(20):
        if sync_over_usb(epoch, timestamp):
            return 0
        time.sleep(0.5)

    if fallback_serial(timestamp):
        return 0
    print("[sync_time] failed; is the board connected and running?", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
