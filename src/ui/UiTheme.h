#pragma once

#include <Arduino.h>

#ifndef RGB565
#define RGB565(r, g, b) \
  (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#endif

namespace ui {

static constexpr int16_t SCREEN_W = 480;
static constexpr int16_t SCREEN_H = 320;
static constexpr int16_t SAFE = 12;
static constexpr int16_t HEADER_H = 40;
static constexpr int16_t FOOTER_H = 62;
static constexpr int16_t FOOTER_Y = SCREEN_H - FOOTER_H;
static constexpr int16_t GAP = 10;
static constexpr int16_t RADIUS = 10;

// Exact RGB565 tokens from bike_computer_v2_exact_sourcepack/source/python/render_ui.py.
static constexpr uint16_t BG = 0x0861;
static constexpr uint16_t SURFACE = 0x10A3;
static constexpr uint16_t SURFACE_2 = 0x10E4;
static constexpr uint16_t SURFACE_ACTIVE = 0x11C6;

static constexpr uint16_t BORDER = 0x2187;
static constexpr uint16_t BORDER_SOFT = 0x2187;
static constexpr uint16_t BORDER_ACTIVE = 0x56B7;

static constexpr uint16_t TEXT = 0xF7BF;
static constexpr uint16_t TEXT_MUTED = 0x8493;
static constexpr uint16_t LABEL = 0x56B7;

static constexpr uint16_t GREEN = 0x66D1;
static constexpr uint16_t ORANGE = 0xF64B;
static constexpr uint16_t RED = 0xFB4E;
static constexpr uint16_t BLUE = 0x6DBF;
static constexpr uint16_t PURPLE = RGB565(170, 80, 255);
static constexpr uint16_t DISABLED_COLOR = 0x8493;

static constexpr uint16_t ACCENT = LABEL;
static constexpr uint16_t ACCENT_DIM = SURFACE_ACTIVE;
static constexpr uint16_t SUCCESS = GREEN;
static constexpr uint16_t WARNING = ORANGE;
static constexpr uint16_t DANGER = RED;

// Compatibility aliases for existing UI code while screens move to the clean tokens above.
static constexpr uint16_t UI_BG = BG;
static constexpr uint16_t UI_PANEL = SURFACE;
static constexpr uint16_t UI_PANEL_2 = SURFACE_2;
static constexpr uint16_t UI_PANEL_ACTIVE = SURFACE_ACTIVE;
static constexpr uint16_t UI_LINE = BORDER;
static constexpr uint16_t UI_LINE_SOFT = BORDER_SOFT;
static constexpr uint16_t UI_GRID = BORDER_SOFT;
static constexpr uint16_t UI_HIGHLIGHT = BORDER_SOFT;
static constexpr uint16_t UI_TEXT = TEXT;
static constexpr uint16_t UI_MUTED = TEXT_MUTED;
static constexpr uint16_t UI_CYAN = LABEL;
static constexpr uint16_t UI_GREEN = GREEN;
static constexpr uint16_t UI_ORANGE = ORANGE;
static constexpr uint16_t UI_RED = RED;
static constexpr uint16_t UI_DISABLED = DISABLED_COLOR;

}  // namespace ui
