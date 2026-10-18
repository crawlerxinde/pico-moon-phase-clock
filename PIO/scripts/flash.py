#!/usr/bin/env python3
"""Flash the RP2350 from an already-built UF2.

The board normally runs as a USB CDC device.  We first perform the standard
1200-baud touch to reboot it into BOOTSEL, then invoke picotool.  If the board
is already in BOOTSEL mode, the touch step is skipped.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import time

try:
    import usb.core
    import usb.util
except ImportError:  # pragma: no cover
    usb = None

VID = 0x2E8A
PID = 0x000F


def enter_bootsel() -> bool:
    if usb is None:
        print("[flash] pyusb is not installed; cannot send 1200-baud touch",
              file=sys.stderr)
        return False

    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        print("[flash] app device not found; assuming BOOTSEL is already active")
        return True

    for interface in (0, 1):
        try:
            if dev.is_kernel_driver_active(interface):
                dev.detach_kernel_driver(interface)
        except Exception:
            pass

    try:
        dev.ctrl_transfer(
            0x21, 0x20, 0, 0,
            struct.pack("<IBBB", 1200, 0, 0, 8),
            timeout=1000,
        )
        time.sleep(0.2)
        dev.ctrl_transfer(0x21, 0x22, 0x0000, 0, None, timeout=1000)
    except Exception as exc:
        print(f"[flash] 1200-baud touch failed: {exc}", file=sys.stderr)
        return False

    try:
        usb.util.dispose_resources(dev)
    except Exception:
        pass
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uf2", required=True, help="path to firmware.uf2")
    parser.add_argument("--picotool", default="picotool",
                        help="picotool executable (default: picotool)")
    parser.add_argument("--no-touch", action="store_true",
                        help="do not send the 1200-baud reboot touch")
    args = parser.parse_args(argv)

    if not args.no_touch and not enter_bootsel():
        return 1

    # Give the ROM bootloader time to enumerate before picotool scans for it.
    time.sleep(2.0)

    cmd = [args.picotool, "load", "-x", "-v", args.uf2]
    for attempt in range(3):
        print("[flash] " + " ".join(cmd))
        result = subprocess.run(cmd)
        if result.returncode == 0:
            return 0
        if attempt == 2:
            return result.returncode
        time.sleep(1.0)

    return 1


if __name__ == "__main__":
    raise SystemExit(main())
