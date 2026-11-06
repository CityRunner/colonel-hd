// SPDX-License-Identifier: GPL-3.0-or-later

#include "layers.hpp"

#include "game_screen.hpp"
#include "sci/core/sha256.hpp"
#include "sci/render/compose.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace sci::tools::golden {

namespace {

using render::compose_height;
using render::compose_width;

constexpr std::size_t screen_pixels = std::size_t{compose_width} * compose_height;

// The low 16 bits of the value, little-endian.
void put16(std::vector<std::uint8_t>& out, int value) {
    const auto bits = static_cast<std::uint16_t>(value);
    out.push_back(static_cast<std::uint8_t>(bits & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(bits >> 8U));
}

// An empty screen: every layer zero (colour 0, owned by the picture,
// priority 0, no sprites).
struct Screen {
    std::vector<std::uint8_t> visual = std::vector<std::uint8_t>(screen_pixels, 0);
    std::vector<std::uint8_t> owner = std::vector<std::uint8_t>(screen_pixels, 0);
    std::vector<std::uint8_t> background_visual = std::vector<std::uint8_t>(screen_pixels, 0);
    std::vector<std::uint8_t> background_owner = std::vector<std::uint8_t>(screen_pixels, 0);
    std::vector<std::uint8_t> background_priority = std::vector<std::uint8_t>(screen_pixels, 0);
    std::vector<std::uint8_t> background_source = std::vector<std::uint8_t>(screen_pixels, 0);
    std::vector<std::uint32_t> sprite_layer = std::vector<std::uint32_t>(screen_pixels, 0);

    [[nodiscard]] render::ComposeInput input(std::span<const render::ComposeSpriteList> lists) const {
        return render::ComposeInput{
            .visual = visual,
            .owner = owner,
            .background_visual = background_visual,
            .background_owner = background_owner,
            .background_priority = background_priority,
            .background_source = background_source,
            .sprite_layer = sprite_layer,
            .sprite_lists = lists,
            .hd_background = nullptr,
            .hd_priority = {},
            .hd = true,
            .snap = false,
        };
    }
};

// Composes `rect` at N and hashes the N x N blocks of that rectangle, row by
// row, after its position and size.
std::string compose_rect(const render::ComposeInput& input, const render::PixelRect& rect, int scale) {
    std::vector<std::uint8_t> bytes;
    put16(bytes, rect.x);
    put16(bytes, rect.y);
    put16(bytes, rect.width);
    put16(bytes, rect.height);
    if (rect.width > 0 && rect.height > 0) {
        const auto n = static_cast<std::size_t>(scale);
        const std::size_t frame_width = std::size_t{compose_width} * n;
        std::vector<std::uint8_t> frame(frame_width * compose_height * n * 4, 0);
        render::compose(input, scale, rect, frame);
        for (std::size_t y = static_cast<std::size_t>(rect.y) * n;
             y < static_cast<std::size_t>(rect.y + rect.height) * n; ++y) {
            const auto row = frame.begin() + static_cast<std::ptrdiff_t>(
                                                 ((y * frame_width) + (static_cast<std::size_t>(rect.x) * n)) * 4);
            bytes.insert(bytes.end(), row,
                         row + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(rect.width) * n * 4));
        }
    }
    return hash(bytes);
}

} // namespace

std::string hash(std::span<const std::uint8_t> bytes) {
    return core::to_hex(core::sha256(bytes));
}

std::string cel_1x(const view::Cel& cel) {
    std::vector<std::uint8_t> bytes;
    put16(bytes, cel.width);
    put16(bytes, cel.height);
    put16(bytes, cel.displace_x);
    bytes.push_back(cel.displace_y);
    bytes.push_back(cel.clear_key);
    bytes.insert(bytes.end(), cel.pixels.begin(), cel.pixels.end());
    return hash(bytes);
}

std::string cel_hd(const view::Cel& cel, int scale) {
    Screen screen;
    const int top = render::compose_picture_top;
    const render::PixelRect rect{
        .x = 0,
        .y = top,
        .width = std::min<int>(cel.width, compose_width),
        .height = std::min<int>(cel.height, compose_height - top),
    };
    const std::array sprites{
        render::ComposeSprite{
            .left = 0,
            .top = top,
            .width = cel.width,
            .height = cel.height,
            .pixels = cel.pixels,
            .clear_key = cel.clear_key,
            .priority = 15,
            .clip_left = 0,
            .clip_top = top,
            .clip_right = compose_width,
            .clip_bottom = compose_height,
        },
    };
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        for (int x = rect.x; x < rect.x + rect.width; ++x) {
            screen.sprite_layer[(static_cast<std::size_t>(y) * compose_width) + static_cast<std::size_t>(x)] = 1;
        }
    }
    const std::array lists{
        render::ComposeSpriteList{
            .generation = 1,
            .sprites = sprites,
        },
    };
    return compose_rect(screen.input(lists), rect, scale);
}

std::string glyph_1x(const text::Glyph& glyph) {
    std::vector<std::uint8_t> bytes{glyph.width, glyph.height};
    bytes.insert(bytes.end(), glyph.pixels.begin(), glyph.pixels.end());
    return hash(bytes);
}

std::string glyph_hd(const text::Glyph& glyph, int scale) {
    Screen screen;
    const render::PixelRect rect{
        .x = 0,
        .y = 0,
        .width = std::min<int>(glyph.width, compose_width),
        .height = std::min<int>(glyph.height, compose_height),
    };
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * compose_width) + static_cast<std::size_t>(x);
            screen.owner[i] = static_cast<std::uint8_t>(render::Owner::Interface);
            screen.visual[i] =
                glyph.pixels[(static_cast<std::size_t>(y) * glyph.width) + static_cast<std::size_t>(x)] != 0 ? 15 : 0;
        }
    }
    return compose_rect(screen.input({}), rect, scale);
}

std::string cursor_1x(const view::Cursor& cursor) {
    std::vector<std::uint8_t> bytes;
    put16(bytes, cursor.hotspot_x);
    put16(bytes, cursor.hotspot_y);
    for (const view::CursorPixel pixel : cursor.pixels) {
        bytes.push_back(static_cast<std::uint8_t>(pixel));
    }
    return hash(bytes);
}

std::string cursor_hd(const view::Cursor& cursor, int scale) {
    const app::CursorImage image = app::scale_cursor(cursor, scale);
    std::vector<std::uint8_t> bytes;
    put16(bytes, image.size);
    put16(bytes, image.hotspot_x);
    put16(bytes, image.hotspot_y);
    bytes.insert(bytes.end(), image.pixels.begin(), image.pixels.end());
    return hash(bytes);
}

} // namespace sci::tools::golden
