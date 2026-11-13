// SPDX-License-Identifier: GPL-3.0-or-later
//
// The layers of the golden outputs of all resources (tools/golden-resources,
// review/BACKLOG_V2.md B-18): on synthetic resources, a change of one pixel
// changes the hash of every layer kind at 1x and at every N the goldens use,
// so a tool that hashed the wrong buffer would fail here.

#include "layers.hpp"
#include "sci_test.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace golden = sci::tools::golden;

namespace {

constexpr std::array scales{1, 2, 5, 10};

sci::view::Cel cel() {
    sci::view::Cel c;
    c.width = 7;
    c.height = 5;
    c.clear_key = 3;
    c.pixels.assign(std::size_t{7} * 5, 3); // Transparent.
    c.pixels[8] = 12;
    c.pixels[20] = 1;
    return c;
}

sci::text::Glyph glyph() {
    sci::text::Glyph g;
    g.width = 6;
    g.height = 8;
    g.pixels.assign(std::size_t{6} * 8, 0);
    g.pixels[13] = 1;
    return g;
}

sci::view::Cursor cursor() {
    sci::view::Cursor c;
    c.pixels.fill(sci::view::CursorPixel::Transparent);
    c.pixels[17] = sci::view::CursorPixel::Black;
    return c;
}

} // namespace

SCI_TEST(a_pixel_of_a_cel_changes_every_layer) {
    const sci::view::Cel a = cel();
    for (const std::size_t i : {std::size_t{0}, std::size_t{8}, std::size_t{34}}) {
        sci::view::Cel b = a;
        b.pixels[i] = b.pixels[i] == 9 ? 10 : 9;
        SCI_CHECK(golden::cel_1x(a) != golden::cel_1x(b));
        for (const int scale : scales) {
            SCI_CHECK(golden::cel_hd(a, scale) != golden::cel_hd(b, scale));
        }
    }
}

SCI_TEST(a_pixel_of_a_glyph_changes_every_layer) {
    const sci::text::Glyph a = glyph();
    for (const std::size_t i : {std::size_t{0}, std::size_t{13}, std::size_t{47}}) {
        sci::text::Glyph b = a;
        b.pixels[i] = b.pixels[i] == 0 ? 1 : 0;
        SCI_CHECK(golden::glyph_1x(a) != golden::glyph_1x(b));
        for (const int scale : scales) {
            SCI_CHECK(golden::glyph_hd(a, scale) != golden::glyph_hd(b, scale));
        }
    }
}

SCI_TEST(a_pixel_of_a_cursor_changes_every_layer) {
    const sci::view::Cursor a = cursor();
    for (const std::size_t i : {std::size_t{0}, std::size_t{17}, std::size_t{255}}) {
        for (const auto pixel : {sci::view::CursorPixel::Black, sci::view::CursorPixel::White}) {
            sci::view::Cursor b = a;
            if (b.pixels[i] == pixel) {
                continue;
            }
            b.pixels[i] = pixel;
            SCI_CHECK(golden::cursor_1x(a) != golden::cursor_1x(b));
            for (const int scale : scales) {
                SCI_CHECK(golden::cursor_hd(a, scale) != golden::cursor_hd(b, scale));
            }
        }
    }
}

SCI_TEST(the_layers_are_deterministic) {
    for (const int scale : scales) {
        SCI_CHECK(golden::cel_hd(cel(), scale) == golden::cel_hd(cel(), scale));
        SCI_CHECK(golden::glyph_hd(glyph(), scale) == golden::glyph_hd(glyph(), scale));
        SCI_CHECK(golden::cursor_hd(cursor(), scale) == golden::cursor_hd(cursor(), scale));
    }
}
