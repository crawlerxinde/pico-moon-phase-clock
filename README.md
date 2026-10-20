# Moon Phase Clock

## 移植说明（中文）

本项目已适配 **Waveshare RP2350-Touch-LCD-3.49**（RP2350B + 172×640 AXS15231B QSPI 触摸屏）。

* 原项目使用 ESP32-S3 + WiFi + visualcrossing 天气 API；该开发板**没有 WiFi**，因此改为完全离线运行。
* 时间来自板载 **PCF85063A RTC**；月相、日出/日落、月出/月落由本地天文算法计算。
* 屏幕按 **640×172 横屏** 做三栏布局：左侧 170×170 月相图，中间时间/日期/地点，右侧月历、月升月落与日出日落。
* 显示驱动使用 QSPI PIO + DMA；触摸使用 I2C0 轮询。
* 原来的 ESP32 网络代码保留在 `PIO/src/legacy/`，不参与编译。

快速使用：

```bash
cd PIO
pio run
# 生成的固件：
# PIO/.pio/build/waveshare_rp2350_touch_lcd_3_49/firmware.uf2
```

按住 **BOOT** 键连接 USB-C，出现 `RP2350`/`RPI-RP2` 磁盘后，把 `firmware.uf2` 拖进去即可。

### 烧录后自动校准 RTC

使用 `pio run -t upload` 烧录时，`PIO/scripts/post_upload.py` 会在上传完成后自动打开
USB 串口，把电脑的**本地时间**发送给板子：

```text
T YYYY-MM-DD HH:MM:SS
```

如果使用拖拽 UF2 的方式烧录，脚本不会自动运行，可在板子启动后执行：

```bash
cd PIO
pio run -t setrtc
# 或
python scripts/set_rtc.py
```

也可以手动指定端口或时间：

```bash
python scripts/set_rtc.py --port /dev/cu.usbmodem1234
python scripts/set_rtc.py --time "2026-10-17 22:30:00"
```

首次使用请修改 `PIO/src/credential.h` 中的经纬度和时区。自动校准使用电脑本地时间；
如果电脑时区与板子配置的 UTC 偏移不同，请用 `--time` 传入正确的本地时间。

串口命令：`B 40..100` 亮度、`F` 翻转显示、`X/Y/S` 触摸校准、`C` 截屏、`R` 读时间、`?` 帮助。

截屏（生成 PNG）：

```bash
cd PIO
python scripts/screenshot.py
python scripts/screenshot.py --output clock.png
```

Moon phase clock running on the **Waveshare RP2350-Touch-LCD-3.49** board.

The original project targeted an ESP32-S3 + TFT_eSPI display and fetched weather
data from visualcrossing.com.  That board is replaced here by an RP2350B with a
172x640 AXS15231B QSPI touch panel.  **The Waveshare board has no WiFi module**,
so this port runs fully offline:

* time comes from the onboard **PCF85063A RTC** (coin-cell backed),
* moon phase is calculated from a synodic-month algorithm,
* sunrise/sunset and moonrise/moonset are calculated locally from the
  latitude/longitude configured in `PIO/src/credential.h`.

The original ESP32-only networking files are kept in `PIO/src/legacy/` for
reference and are excluded from the build.

![Moon Phase Clock](Assets/Moon%20Phase%20Clock.GIF)

At boot the left 130x130 tile cycles through the 28 moon-phase frames over
three seconds, matching the original T-Display-S3 animation.  The boot screen
stays visible for one full cycle before switching to the clock.

Returning from Settings uses a shorter, more pronounced transition instead:
the current moon phase fades in, slides from -36 px and zooms from 130 to 236
over ~620 ms.  The same transition is replayed when the clock page wakes from
screen-off with the power button.  The full phase-cycle animation is
intentionally boot-only.

## Hardware / pin map

| Function | RP2350B pins |
|---|---|
| LCD QSPI (AXS15231B) | CS 25, SCLK 20, D0-D3 21-24, RST 34, PWR_EN 37 |
| LCD backlight | GPIO 36 (PWM, active low) |
| Touch (AXS15231B, I2C0, addr 0x3B) | SDA 32, SCL 33, INT 11 |
| RTC PCF85063A (I2C1, addr 0x51) | SDA 6, SCL 7 |
| Battery voltage (3:1 divider) | GPIO 40 / ADC0 |
| Audio ES8311 codec (I2C1, addr 0x18) | SDA 6, SCL 7 |
| Audio I2S (PIO) | PA 0, DOUT 1, MCLK 3, BCLK 4, LRCLK 5 |
| Power latch | SYS_EN GPIO 39 (must be driven HIGH) |
| Power button | SYS_OUT GPIO 38 (low = pressed) |

The 640x172 panel uses a three-column layout based on the minimal black
dashboard mock:

* left: about 120x120 moon phase image, inset from the screen edge,
* centre: Inter Light 66 px time, Inter Medium AM/PM, Inter Regular seconds/date,
  and a rounded location pill with a map-pin icon,
* right: Inter month calendar with today outlined, then IBM Plex Mono moonrise/
  moonset and sunrise/sunset rows with separate moon/sun icons.

The panel itself is driven in its native **172x640 portrait** orientation; the
LVGL flush callback rotates the logical buffer into 172x640 blocks in software.
This matches Waveshare's reference implementation and avoids relying on the
panel's MADCTL rotation mode.  The location pill shows the current city
(for example `Beijing` or `Paris`); the moon image itself shows the current phase.
Sunrise, sunset, moonrise and moonset all show the **next** event after the
current time.  A `+1` or `+2` suffix marks that the event falls on the next day
or the day after; this is common for moon events because a lunar day is slightly
longer than 24 hours.

## Settings page

On the clock face:

* swipe **up / down** outside the left column to increase / decrease brightness
  by 20% (clamped to 40–100%),
* swipe **left or right in the left column** to open Settings; the same gesture
  in the left column returns from Settings,
* or tap the location pill to open Settings.

The left column (`x < 180`) is the dedicated page-swipe zone.  On the clock
page it opens Settings; a left-column swipe from the Settings overview returns
to the clock.  Editor pages use a large `Back` button instead, so an accidental
swipe cannot discard a draft.

The Settings overview contains four full-card entries plus a separate
`Back to Clock` card:

* **Date & Time** (blue when focused): shows the current date and time.  Tapping
  it opens `Set Date (Step 1/2)`, where Year, Month and Day are three linked
  rollers.  `Next` opens the hour step, then the minute step, then a
  `Review Date & Time` page.  `Apply` opens the generic Apply Changes dialog;
  only its Apply action writes the RTC.  The day roller is rebuilt from the
  selected year/month, so invalid dates such as 31 February are clamped.
* **Location**: opens `Select Location` with linked Letter (`A-Z`) and City
  rollers.  The City roller shows only cities for the selected letter and the
  expected UTC offset.  `Apply` opens the Apply Changes dialog; saving preserves
  the absolute UTC instant and rewrites the RTC in the new city's local time.
  Each city has its own coordinates and timezone/DST rule (EU, US, Australia,
  New Zealand, Chile and Egypt rules are supported).  The generated table is in
  `PIO/src/location_data.h`.
* **Tick Sound**: shows `Audible` or `Muted`; the large action card immediately
  toggles the per-second tick and persists the new state.
* **Brightness**: opens a single read-only brightness bar with large `-` / `+`
  controls in 5% steps (clamped to 40–100% so the panel stays readable).
  Changes apply immediately; the bar is only an indicator and cannot be dragged.

Back from an editor with unsaved date/time or location changes opens the
`Discard Changes` dialog.  Apply and Discard dialogs both use explicit Cancel
and action buttons.  The overview cards display live device state; there is no
separate battery card.

The default city still comes from `PIO/src/credential.h`; the EEPROM selection
overrides it after the first change.

## Configure

`PIO/src/credential.h` holds the **default** location (Beijing):

```cpp
static const float myLat = 40.0434412f;
static const float myLon = 116.4152377f;
static const char *myCity = "Beijing";
static const long  gmtOffset_sec = 8 * 3600;
```

Use the Settings page to switch city at runtime.  The RTC stores **local
time**.  On the first boot (or after the RTC battery is lost) it is seeded with
the firmware build date/time.  Set it precisely over the USB serial console or
from the Settings page:

```text
T 2026-10-17 22:30:00
```

Serial commands (115200 baud):

| Command | Action |
|---|---|
| `T YYYY-MM-DD HH:MM:SS` | set the RTC (local time) |
| `G` | open/close the Settings page |
| `W` | open/close the city picker (Settings must be open) |
| `J` | open/close the date/time picker (Settings must be open) |
| `Z N` | debug Settings page 0..9 (Settings must be open) |
| `L` | list locations; `L 0..5` select a city |
| `K FIELD DELTA` | adjust date/time field (0=year .. 5=second), for diagnostics |
| `A` | print battery voltage and percentage |
| `B 40..100` | set backlight brightness (minimum 40%) |
| `F` | flip the display 180 degrees |
| `X` / `Y` / `S` | invert touch X / invert touch Y / swap touch axes |
| `C` | capture the LVGL screen as RGB565 over USB |
| `O` | print moon orientation: lunar pole P, zenith q, screen angle |
| `R` | print the current RTC date/time |
| `?` | help |

If touch does not line up with the screen, use `X`, `Y` and `S` first; the
current calibration can then be copied into `PIO/src/display.cpp`.

### Screenshot

The firmware can stream its current LVGL screen over USB.  Run the host script
to receive the 640x172 RGB565 image and save it as a PNG:

```bash
cd PIO
python scripts/screenshot.py
python scripts/screenshot.py --output clock.png
```

The script first tries the normal serial port (pyserial), then falls back to
raw USB CDC if `/dev/ttyACM*` is not accessible.  The raw USB fallback needs
`pyusb` (`pip install pyusb`).  You can force either mode:

```bash
python scripts/screenshot.py --port /dev/ttyACM0
python scripts/screenshot.py --usb
python scripts/screenshot.py --raw   # also save the RGB565 data
```

The result is a snapshot of LVGL's logical screen, not a readback of the
display controller's physical framebuffer.  It is useful for checking text,
layout, moon images and values; it does not show panel-level color or rotation
issues.

## Make targets

From the repository root:

```bash
make build       # PlatformIO build
make test        # layout/geometry tests + Python syntax checks
make flash       # build, flash, then sync the RTC from host local time
make sync_time   # sync the RTC without flashing
make sync_time TIME="2026-10-19 12:34:56"
make clean
```

`make flash` waits for the board to boot and then runs the same RTC sync as
`make sync_time`, so the clock should match the host immediately after flashing.
The sync sends a UTC epoch; the firmware applies the currently selected city's
timezone/DST before writing the RTC.  `make sync_time` retries if the board is
still in its boot animation.

Settings are snapshotted to EEPROM (location, brightness, TICK and the last
known time).  On boot they are restored; the RTC remains the primary clock,
and the snapshot time is used only if the RTC lost power.

## Build and flash

`PIO/platformio.ini` uses the Arduino-Pico fork with RP2350 support:

```bash
cd PIO
pio run
```

The UF2 is generated at:

```text
PIO/.pio/build/waveshare_rp2350_touch_lcd_3_49/firmware.uf2
```

To flash:

1. Hold **BOOT**, connect USB-C (or press RESET while holding BOOT).
2. A drive named `RP2350` / `RPI-RP2` appears.
3. Copy `firmware.uf2` onto that drive.
4. The board reboots and runs the clock.

Alternatively, with the board already in BOOTSEL mode:

```bash
cd PIO
pio run -t upload
```

### Automatic RTC calibration after upload

`pio run -t upload` runs `PIO/scripts/post_upload.py` after a successful upload.
It opens the board's USB CDC port and sends the host's local time to the RTC:

```text
T YYYY-MM-DD HH:MM:SS
```

If you flash by dragging the UF2 instead, run the calibration manually after the
board boots:

```bash
cd PIO
pio run -t setrtc
# or
python scripts/set_rtc.py
```

The helper also accepts an explicit port or time:

```bash
python scripts/set_rtc.py --port /dev/cu.usbmodem1234
python scripts/set_rtc.py --time "2026-10-17 22:30:00"
```

The automatic calibration uses the host's local clock.  If the host timezone
differs from the `gmtOffset_sec` configured in `credential.h`, pass the correct
local time with `--time`.

## Notes about the port

* **Display**: QSPI PIO + DMA driver for the AXS15231B, based on Waveshare's
  demo.  The LVGL flush callback byte-swaps RGB565 because the panel expects
  big-endian pixels while the SquareLine assets use `LV_COLOR_16_SWAP = 0`.
* **Touch**: polled I2C reads; no interrupt is required for the clock UI.
* **UI**: the original 320x170 layout was re-arranged for 640x172.  The moon
  image is about 120x120 on the left; time, date, phase and rise/set times use the
  remaining width.  The Clock page follows the v1.1 layout: larger time block,
  compact 2x2 astronomy rows with reserved room for `+1`, and a full calendar
  that supports six-row months.
* **Moon orientation**: the 30 static moon frames are treated as lunar-north-up.
  The image is rotated by `P - q`, where `P` is the position angle of the lunar
  north pole and `q` is the local parallactic angle.  This makes the terminator
  tilt depend on both the selected city and the current time.  The calibration
  constants live in `PIO/src/astronomy.cpp`; the `O` serial command prints the
  intermediate P/q values.  If the artwork is not exactly north-up, adjust
  `MOON_ASSET_NORTH_OFFSET_DEG` or the per-frame correction table.
* **Power**: `SYS_EN` is asserted at the very start of `setup()` so the board
  stays on when running from the 18650 battery.  A short press of the power
  button toggles the backlight; holding it for ~1.5 s powers the board off.
  Battery voltage is measured on GPIO40/ADC0 through the board's 3:1 divider.
* **Settings**: the selected city is persisted in the RP2350 EEPROM area
  (one flash sector).  City switching preserves UTC, so the RTC is rewritten
  to the new local time and astronomy is recalculated.
* **RTC year base**: the PCF85063A stores the year as a 2000-based value.
  Older firmware used a 1970 base (which made the chip's leap-year logic wrong
  for 2026); this firmware migrates the stored value once and records a flag
  in EEPROM.
* **No WiFi**: the weather client, WiFi manager and NTP code were removed from
  the active build.  Everything is local and works without a network.  If you
  later add an external WiFi module, the old code is in `PIO/src/legacy/` as a
  starting point.
