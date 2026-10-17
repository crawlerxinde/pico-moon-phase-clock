"""
PlatformIO post-upload hook: calibrate the PCF85063A RTC to the host clock.

PlatformIO runs this after a successful `pio run -t upload`.  The board
re-enumerates as a USB CDC device after flashing, so set_rtc() retries for a
while before giving up.

It can also be triggered without flashing:

    pio run -t setrtc
"""

import os
import sys

Import("env")  # noqa: F821 - provided by PlatformIO/SCons

# Make scripts/set_rtc.py importable regardless of the current directory.
_project_dir = env.subst("$PROJECT_DIR") or os.getcwd()
_scripts_dir = os.path.join(_project_dir, "scripts")
if _scripts_dir not in sys.path:
    sys.path.insert(0, _scripts_dir)

from set_rtc import set_rtc  # noqa: E402


def _post_upload(source, target, env):
    port = env.subst("$UPLOAD_PORT") or env.subst("$MONITOR_PORT")
    print("[set_rtc] calibrating RTC to host local time...")
    set_rtc(port=port, timeout=25.0)


def _set_rtc_target(source, target, env):
    port = env.subst("$MONITOR_PORT") or env.subst("$UPLOAD_PORT")
    if not set_rtc(port=port, timeout=15.0):
        print("[set_rtc] RTC was not set; is the board connected and running?")


env.AddPostAction("upload", _post_upload)  # noqa: F821

env.AddCustomTarget(  # noqa: F821
    name="setrtc",
    dependencies=None,
    actions=[_set_rtc_target],
    title="Set RTC",
    description="Send the host's local time to the board RTC over USB serial",
)
