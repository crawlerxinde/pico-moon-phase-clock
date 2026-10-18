SHELL := /bin/bash

PIO_DIR   := PIO
PIO       ?= $(shell command -v pio 2>/dev/null || echo /tmp/pio-venv/bin/pio)
PYTHON    ?= python3
UF2       := $(PIO_DIR)/.pio/build/waveshare_rp2350_touch_lcd_3_49/firmware.uf2
PICOTOOL  ?= $(shell command -v picotool 2>/dev/null || find $(HOME)/.platformio/packages -type f -name picotool 2>/dev/null | head -n 1)

.PHONY: all help build test flash sync_time clean

all: build

help:
	@echo "Moon Phase Clock targets:"
	@echo "  make build       Build the firmware with PlatformIO"
	@echo "  make test        Run static layout/geometry tests and Python syntax checks"
	@echo "  make flash       Build, flash firmware.uf2, then sync the RTC"
	@echo "  make sync_time   Set the RTC from the host local time"
	@echo "  make sync_time TIME='2026-10-19 12:34:56'"
	@echo "  make clean       Remove PlatformIO build artifacts"

build:
	@test -x "$(PIO)" -o -n "$(shell command -v $(PIO) 2>/dev/null)" || \
		{ echo "PlatformIO not found; set PIO=/path/to/pio" >&2; exit 1; }
	cd $(PIO_DIR) && $(PIO) run

test:
	$(PYTHON) test_r4_layout.py
	$(PYTHON) -m py_compile \
		$(PIO_DIR)/scripts/flash.py \
		$(PIO_DIR)/scripts/sync_time.py \
		$(PIO_DIR)/scripts/set_rtc.py \
		$(PIO_DIR)/scripts/screenshot.py \
		$(PIO_DIR)/scripts/post_upload.py

flash: build
	$(PYTHON) $(PIO_DIR)/scripts/flash.py --uf2 "$(UF2)" --picotool "$(PICOTOOL)"
	@echo "[flash] waiting for the clock to boot, then syncing RTC..."
	@sleep 5
	$(PYTHON) $(PIO_DIR)/scripts/sync_time.py

sync_time:
	$(PYTHON) $(PIO_DIR)/scripts/sync_time.py $(if $(TIME),--time "$(TIME)",)

clean:
	cd $(PIO_DIR) && $(PIO) run -t clean
