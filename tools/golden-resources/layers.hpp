// SPDX-License-Identifier: GPL-3.0-or-later
//
// The layers the golden outputs of all resources hash (review/BACKLOG_V2.md
// B-18, stage 5 step 0.3): each function returns the SHA-256 (hex) of one
// image as the program would show it. Kept apart from main.cpp so that a
// CI test on synthetic resources can check that a one-pixel change changes
// every layer's hash (tests/golden/layers_test.cpp).
#pragma once

#include "sci/text/font.hpp"
#include "sci/view/cursor.hpp"
#include "sci/view/view.hpp"

#include <cstdint>
#include <span>
#include <string>

namespace sci::tools::golden {

// SHA-256 of the bytes, lower-case hex.
[[nodiscard]] std::string hash(std::span<const std::uint8_t> bytes);

// A cel at 1x: its size, displacement, transparent colour and pixels.
[[nodiscard]] std::string cel_1x(const view::Cel& cel);

// A cel through the HD sprite path (sci::render::compose, HD mode, no HD
// background): drawn at the top left of the picture area with priority 15
// over an empty picture (colour 0, priority 0); the hash covers the N x N
// blocks of the cel's 1x rectangle (clipped to the screen).
[[nodiscard]] std::string cel_hd(const view::Cel& cel, int scale);

// A glyph at 1x: its size and pixels.
[[nodiscard]] std::string glyph_1x(const text::Glyph& glyph);

// A glyph as the interpreter draws text (colour 15 on colour 0, owned by the
// interface) composed at N in HD mode: the blocks of its rectangle.
[[nodiscard]] std::string glyph_hd(const text::Glyph& glyph, int scale);

// A cursor at 1x: hotspot and shape.
[[nodiscard]] std::string cursor_1x(const view::Cursor& cursor);

// A cursor scaled as the program shows it (the app's scale_cursor): size,
// hotspot and RGBA pixels.
[[nodiscard]] std::string cursor_hd(const view::Cursor& cursor, int scale);

} // namespace sci::tools::golden
