# R4 right-panel readability refinement

Based on the R3 full source archive and the flashed 640x172 screenshot.

- Calendar day numerals: Inter Regular 11 (per Instruction.md); the 7x6 label grid keeps the columns aligned.
- Date cell height / today's rounded outline: 14 px, outline only (no fill).
- Five-row month stepping: 14 px; six-row month stepping: 11 px so all rows remain above y=118 divider.
- Astro vertical divider: y=124..163, lower contrast (#383838).
- Astro dashed divider: 3 px segments instead of 4 px, lower contrast (#383838).
- Kept 12 px astronomy labels and 14 px icons after inspecting actual glyph and column widths; raising font size would risk clipping the right-hand column.
- No changes to left moon, clock, RTC, astronomy calculations or display driver. No calendar navigation arrows.

## Verification
- PASS: static calendar grid bounds for every month between 2024 and 2040 (204 cases)
- NOT RUN: PlatformIO firmware compilation (PlatformIO unavailable here)
- NOT RUN: native LVGL framebuffer rendering / RP2350 hardware confirmation

This package is not an assertion of pixel-perfect correspondence with the design target until flashed and compared on hardware.
