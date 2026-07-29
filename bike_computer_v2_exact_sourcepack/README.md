# Bike Computer 2.0 — exact render/source package

This package deliberately contains **two exact paths**:

1. `source/layouts/*.json` + `source/python/render_ui.py` regenerate the 24-bit reference PNGs.
2. `source/esp32/` contains actual ESP32/TFT_eSPI code and RLE-compressed RGB565 screen assets generated from the same source renders.

## Folders

- `reference_png/` — original 480×320 renders.
- `rgb565_expected/` — what the same screens look like after the real RGB565 color conversion used by the TFT.
- `source/layouts/` — complete layout source including base-screen composition and dim overlays.
- `source/python/render_ui.py` — deterministic source renderer.
- `source/esp32/generated/` — actual per-screen RLE565 arrays stored in PROGMEM.
- `source/esp32/include/ui_exact/` and `source/esp32/src/ui_exact/` — TFT_eSPI exact renderer.
- `verification/report.json` — pixel-comparison report.
- `tools/verify_source_render.py` — regenerates screens and checks every pixel.

## Important distinction

The ESP exact renderer draws the supplied screens pixel-for-pixel in RGB565. It is intended as the visual baseline and regression mode. It does not magically make demo text dynamic. For the production UI, Codex should preserve this renderer as a comparison/reference mode and replace only declared dynamic regions while keeping the same background color, geometry, icon assets and typography strategy.

## Background rule

Every full screen and every sprite must be cleared with the same color:

```cpp
0x0841 // RGB565 for #090C0F
```

Do not mix it with `TFT_BLACK` (`0x0000`). That was the cause of the split black/green background.

## Build integration

Copy the contents of `source/esp32/include` into the project's include path and `source/esp32/src` + `source/esp32/generated` into the firmware source path.

Example:

```cpp
ui_exact::ExactScreenRenderer renderer(tft);
renderer.draw(ui_exact::ScreenId::SCREEN_11_RIDE_SPEED_ACTIVE);
```

The generated assets occupy approximately 1277.0 KiB before compiler/linker overhead.

## Verification commands

```bash
python tools/verify_source_render.py
python tools/verify_esp_assets.py
```

Both commands must return exit code 0. The package currently contains zero pixel mismatches for all screens.

## Fonts

Font binaries are intentionally not included. The reference generator uses the system DejaVu Sans regular and bold fonts. Exact SHA-256 requirements are recorded in `verification/font_requirements.json`. The ESP pixel-exact path does not need these fonts because it draws generated RGB565 assets directly.

## Why the ESP path is exact

The generated C++ files contain the actual 480×320 RGB565 pixels compressed as run-length pairs. `ExactScreenRenderer` sends each logical RGB565 color with `TFT_eSPI::pushColor()`. Codex does not reinterpret coordinates, icons or font weights in this mode.
