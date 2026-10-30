// SPDX-License-Identifier: GPL-3.0-or-later
//
// Development tool (milestone 10): the visual review sheets of A4 part B
// (docs/SPEC_ADDENDA.md) for a picture stack.
//
//   sci-review-sheet <game folder> <output folder> --stack <n>[+<n>...]
//                    [--room <n>] [--scale <n>] [--sheet-version <n>]
//                    [--date <yyyy-mm-dd>] [--hints <directory> | --no-hints]
//                    [--cache <directory>]
//                    [--crop <ref>[,<ref>...]] [--crop-scale <n>]
//                    [--crop-layout side|stacked]
//
// Without --crop it writes the overview sheet (B.1: HD on top, original
// below, at N, each 320N x 200N with the blank menu bar, with the grid over
// the picture area and a header) as <name>-sheet.png and the same pair
// without the grid as <name>-plain.png. With --crop it writes, for every
// cell, quarter or range named ("B3", "B3.2", "A2-A4"), the enlarged crop
// of B.4 (the area plus half a cell around it, clamped to the picture, at
// the crop N, HD and original, with the grid) as <name>-crop-<ref>.png.
// <name> is pic-<n>[+<n>...]. The frames are composed by the program's own
// code (sci/platform/game_frame.hpp) from the stack as the interpreter
// shows it alone (tools/common/stack_screen.hpp); the grid's geometry is
// tools/common/review_grid.hpp. The text is a small built-in bitmap font,
// so the sheets depend on no font of the game or of the system.

#include "game_folder.hpp"
#include "png.hpp"
#include "review_grid.hpp"
#include "sci/platform/analysis_cache.hpp"
#include "sci/platform/config.hpp"
#include "sci/platform/game_frame.hpp"
#include "sci/platform/hd_pictures.hpp"
#include "sci/platform/thread_executor.hpp"
#include "stack_screen.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace review = sci::tools::review;
using sci::platform::GameFrame;
using sci::platform::GraphicsMode;
using sci::tools::Image;
using sci::tools::Rgb;

constexpr int screen_width = 320;
constexpr int screen_height = 200;
constexpr int menu_rows = 10;

constexpr Rgb sheet_background{.r = 0x40, .g = 0x40, .b = 0x40};
constexpr Rgb header_background{.r = 0x20, .g = 0x20, .b = 0x20};
constexpr Rgb header_ink{.r = 0xF0, .g = 0xF0, .b = 0xF0};
constexpr Rgb white{.r = 0xFF, .g = 0xFF, .b = 0xFF};
constexpr Rgb black{.r = 0x00, .g = 0x00, .b = 0x00};
constexpr int panel_gap = 8;

struct Options {
    fs::path game;
    fs::path output;
    std::vector<std::uint16_t> stack;
    std::optional<int> room;
    int scale = 4;
    int crop_scale = 8;
    bool crop_stacked = false;
    int sheet_version = 1;
    std::string date;
    std::optional<fs::path> hints = fs::path("hints");
    std::optional<fs::path> cache;
    std::vector<review::Area> crops;
};

std::optional<std::uint32_t> parse_number(std::string_view text) {
    std::uint32_t value = 0;
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

// "yyyy-mm-dd" naming a real day.
bool valid_date(std::string_view text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return false;
    }
    const auto year = parse_number(text.substr(0, 4));
    const auto month = parse_number(text.substr(5, 2));
    const auto day = parse_number(text.substr(8, 2));
    if (!year || !month || !day) {
        return false;
    }
    return std::chrono::year_month_day{std::chrono::year{static_cast<int>(*year)}, std::chrono::month{*month},
                                       std::chrono::day{*day}}
        .ok();
}

std::string today() {
    const auto now = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    return std::format("{:%F}", std::chrono::year_month_day{now});
}

int usage() {
    std::println(stderr, "usage: sci-review-sheet <game folder> <output folder> --stack <n>[+<n>...]\n"
                         "                        [--room <n>] [--scale <n>] [--sheet-version <n>]\n"
                         "                        [--date <yyyy-mm-dd>] [--hints <directory> | --no-hints]\n"
                         "                        [--cache <directory>]\n"
                         "                        [--crop <ref>[,<ref>...]] [--crop-scale <n>]\n"
                         "                        [--crop-layout side|stacked]\n"
                         "  <ref>: a cell (B3), a quarter (B3.2) or a range (A2-A4)");
    return 2;
}

bool parse_crops(std::string_view text, std::vector<review::Area>& crops) {
    while (true) {
        const auto comma = text.find(',');
        const auto area = review::parse_area(text.substr(0, comma));
        if (!area) {
            std::println(stderr, "{}", area.error().message);
            return false;
        }
        crops.push_back(*area);
        if (comma == std::string_view::npos) {
            return true;
        }
        text = text.substr(comma + 1);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): argument parsing.
std::optional<Options> parse_options(std::span<char*> args) {
    if (args.size() < 3) {
        return std::nullopt;
    }
    Options options;
    options.game = args[1];
    options.output = args[2];
    for (std::size_t i = 3; i < args.size(); ++i) {
        const std::string_view flag = args[i];
        if (flag == "--no-hints") {
            options.hints.reset();
            continue;
        }
        if (i + 1 >= args.size()) {
            return std::nullopt;
        }
        const std::string_view value = args[++i];
        if (flag == "--stack") {
            std::string_view rest = value;
            while (true) {
                const auto plus = rest.find('+');
                const auto number = parse_number(rest.substr(0, plus));
                if (!number || *number > 999) {
                    return std::nullopt;
                }
                options.stack.push_back(static_cast<std::uint16_t>(*number));
                if (plus == std::string_view::npos) {
                    break;
                }
                rest = rest.substr(plus + 1);
            }
        } else if (flag == "--hints") {
            options.hints = fs::path(value);
        } else if (flag == "--cache") {
            options.cache = fs::path(value);
        } else if (flag == "--date") {
            if (!valid_date(value)) {
                return std::nullopt;
            }
            options.date = std::string(value);
        } else if (flag == "--crop") {
            if (!parse_crops(value, options.crops)) {
                return std::nullopt;
            }
        } else if (flag == "--crop-layout") {
            if (value != "side" && value != "stacked") {
                return std::nullopt;
            }
            options.crop_stacked = value == "stacked";
        } else if (const auto number = parse_number(value)) {
            const bool scale = *number >= 1 && *number <= sci::platform::max_scale;
            if (flag == "--scale" && scale) {
                options.scale = static_cast<int>(*number);
            } else if (flag == "--crop-scale" && scale) {
                options.crop_scale = static_cast<int>(*number);
            } else if (flag == "--room" && *number <= 999) {
                options.room = static_cast<int>(*number);
            } else if (flag == "--sheet-version" && *number <= 999) {
                options.sheet_version = static_cast<int>(*number);
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    if (options.stack.empty()) {
        return std::nullopt;
    }
    if (options.date.empty()) {
        options.date = today();
    }
    return options;
}

// A 3 x 5 bitmap font for the labels and the header: digits, capitals and
// a few signs; each row's bits are 4 (left), 2, 1 (right).
struct Glyph {
    char c;
    std::array<std::uint8_t, 5> rows;
};

constexpr std::array<Glyph, 48> font{{
    {.c = '0', .rows = {7, 5, 5, 5, 7}}, {.c = '1', .rows = {2, 6, 2, 2, 7}}, {.c = '2', .rows = {7, 1, 7, 4, 7}},
    {.c = '3', .rows = {7, 1, 7, 1, 7}}, {.c = '4', .rows = {5, 5, 7, 1, 1}}, {.c = '5', .rows = {7, 4, 7, 1, 7}},
    {.c = '6', .rows = {7, 4, 7, 5, 7}}, {.c = '7', .rows = {7, 1, 1, 1, 1}}, {.c = '8', .rows = {7, 5, 7, 5, 7}},
    {.c = '9', .rows = {7, 5, 7, 1, 7}}, {.c = 'A', .rows = {2, 5, 7, 5, 5}}, {.c = 'B', .rows = {6, 5, 6, 5, 6}},
    {.c = 'C', .rows = {3, 4, 4, 4, 3}}, {.c = 'D', .rows = {6, 5, 5, 5, 6}}, {.c = 'E', .rows = {7, 4, 6, 4, 7}},
    {.c = 'F', .rows = {7, 4, 6, 4, 4}}, {.c = 'G', .rows = {3, 4, 5, 5, 3}}, {.c = 'H', .rows = {5, 5, 7, 5, 5}},
    {.c = 'I', .rows = {7, 2, 2, 2, 7}}, {.c = 'J', .rows = {1, 1, 1, 5, 2}}, {.c = 'K', .rows = {5, 5, 6, 5, 5}},
    {.c = 'L', .rows = {4, 4, 4, 4, 7}}, {.c = 'M', .rows = {5, 7, 5, 5, 5}}, {.c = 'N', .rows = {6, 5, 5, 5, 5}},
    {.c = 'O', .rows = {2, 5, 5, 5, 2}}, {.c = 'P', .rows = {6, 5, 6, 4, 4}}, {.c = 'Q', .rows = {2, 5, 5, 7, 3}},
    {.c = 'R', .rows = {6, 5, 6, 5, 5}}, {.c = 'S', .rows = {3, 4, 2, 1, 6}}, {.c = 'T', .rows = {7, 2, 2, 2, 2}},
    {.c = 'U', .rows = {5, 5, 5, 5, 7}}, {.c = 'V', .rows = {5, 5, 5, 5, 2}}, {.c = 'W', .rows = {5, 5, 5, 7, 5}},
    {.c = 'X', .rows = {5, 5, 2, 5, 5}}, {.c = 'Y', .rows = {5, 5, 2, 2, 2}}, {.c = 'Z', .rows = {7, 1, 2, 4, 7}},
    {.c = ' ', .rows = {0, 0, 0, 0, 0}}, {.c = '-', .rows = {0, 0, 7, 0, 0}}, {.c = '.', .rows = {0, 0, 0, 0, 2}},
    {.c = '+', .rows = {0, 2, 7, 2, 0}}, {.c = ':', .rows = {0, 2, 0, 2, 0}}, {.c = '/', .rows = {1, 1, 2, 4, 4}},
    {.c = '=', .rows = {0, 7, 0, 7, 0}}, {.c = ',', .rows = {0, 0, 0, 2, 4}}, {.c = '(', .rows = {1, 2, 2, 2, 1}},
    {.c = ')', .rows = {4, 2, 2, 2, 4}}, {.c = '?', .rows = {7, 1, 2, 0, 2}}, {.c = '_', .rows = {0, 0, 0, 0, 7}},
}};

const Glyph& glyph_of(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    const auto* const found = std::ranges::find(font, c, &Glyph::c);
    return found != font.end() ? *found : *std::ranges::find(font, '?', &Glyph::c);
}

// Text at scale s: 4s per character (one column of space), 5s high.
int text_width(std::string_view text, int s) { return text.empty() ? 0 : (static_cast<int>(text.size()) * 4 * s) - s; }

void fill(Image& image, int left, int top, int width, int height, Rgb color) {
    for (int y = top; y < top + height; ++y) {
        for (int x = left; x < left + width; ++x) {
            image.set(x, y, color);
        }
    }
}

// `color` over the pixel with alpha/255.
void blend(Image& image, int x, int y, Rgb color, unsigned alpha) {
    if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
        return;
    }
    const Rgb under = image.at(x, y);
    const auto mix = [alpha](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(((a * alpha) + (b * (255U - alpha)) + 127U) / 255U);
    };
    image.set(x, y, Rgb{.r = mix(color.r, under.r), .g = mix(color.g, under.g), .b = mix(color.b, under.b)});
}

void draw_text(Image& image, int left, int top, std::string_view text, int s, Rgb ink) {
    for (std::size_t k = 0; k < text.size(); ++k) {
        const Glyph& glyph = glyph_of(text[k]);
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                if ((glyph.rows[static_cast<std::size_t>(row)] & (4U >> static_cast<unsigned>(column))) != 0) {
                    fill(image, left + (static_cast<int>(k) * 4 * s) + (column * s), top + (row * s), s, s, ink);
                }
            }
        }
    }
}

// A rendered frame (320N x 200N RGBA) in the sheet: the part of the
// picture area `view` (with `menu` the menu bar's rows above it) at
// (left, top).
void copy_frame(Image& image, const std::vector<std::uint8_t>& frame, int scale, const review::Rect& view, bool menu,
                int left, int top) {
    const int width = screen_width * scale;
    const int first_row = (menu ? 0 : (view.top + menu_rows) * scale);
    const int last_row = (view.bottom + menu_rows) * scale;
    for (int y = first_row; y < last_row; ++y) {
        for (int x = view.left * scale; x < view.right * scale; ++x) {
            const std::size_t i =
                ((static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) + static_cast<std::size_t>(x)) * 4;
            image.set(left + x - (view.left * scale), top + y - first_row,
                      Rgb{.r = frame[i], .g = frame[i + 1], .b = frame[i + 2]});
        }
    }
}

// The grid of B.1 over a panel showing `view` of the picture area at N, its
// top left corner at (left, top). Every pixel is blended once, with the
// topmost of: cell line core, cell line outline, quarter line core,
// quarter line outline.
void draw_grid(Image& image, const review::Rect& view, int scale, int left, int top) {
    enum class Layer : std::uint8_t { None, QuarterOutline, QuarterCore, CellOutline, CellCore };
    const int width = view.width() * scale;
    const int height = view.height() * scale;
    std::vector<Layer> mask(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), Layer::None);
    const auto mark = [&](int x, int y, Layer layer) {
        if (x >= 0 && y >= 0 && x < width && y < height) {
            auto& at = mask[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) + static_cast<std::size_t>(x)];
            at = std::max(at, layer);
        }
    };
    const int cell_half = std::max(1, scale / 4); // Core 2 * cell_half wide.
    const int quarter_core = std::max(1, scale / 4);
    const int dash = 2 * scale;
    // A line at original coordinate `at` across the panel: vertical when
    // `vertical`, its core from `from` to `to` (panel pixels on its axis)
    // with an outline of one pixel on both sides.
    const auto line = [&](int at, bool vertical, int from, int to, Layer outline, Layer core, bool dashed) {
        const int position = (at - (vertical ? view.left : view.top)) * scale;
        const int length = vertical ? height : width;
        for (int t = 0; t < length; ++t) {
            if (dashed && (t / dash) % 2 != 0) {
                continue;
            }
            for (int d = from - 1; d < to + 1; ++d) {
                const Layer layer = (d < from || d >= to) ? outline : core;
                if (vertical) {
                    mark(position + d, t, layer);
                } else {
                    mark(t, position + d, layer);
                }
            }
        }
    };
    for (int column = 0; column < review::columns; ++column) {
        const int x = review::quarter_column_line(column);
        if (x > view.left && x < view.right) {
            line(x, true, -(quarter_core / 2), quarter_core - (quarter_core / 2), Layer::QuarterOutline, Layer::QuarterCore, true);
        }
    }
    for (int row = 0; row < review::rows; ++row) {
        const int y = review::quarter_row_line(row);
        if (y > view.top && y < view.bottom) {
            line(y, false, -(quarter_core / 2), quarter_core - (quarter_core / 2), Layer::QuarterOutline, Layer::QuarterCore, true);
        }
    }
    for (const int x : review::column_edges) {
        if (x >= view.left && x <= view.right) {
            line(x, true, -cell_half, cell_half, Layer::CellOutline, Layer::CellCore, false);
        }
    }
    for (const int y : review::row_edges) {
        if (y >= view.top && y <= view.bottom) {
            line(y, false, -cell_half, cell_half, Layer::CellOutline, Layer::CellCore, false);
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            switch (mask[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) + static_cast<std::size_t>(x)]) {
            case Layer::QuarterOutline:
                blend(image, left + x, top + y, black, 110);
                break;
            case Layer::QuarterCore:
                blend(image, left + x, top + y, white, 150);
                break;
            case Layer::CellOutline:
                blend(image, left + x, top + y, black, 170);
                break;
            case Layer::CellCore:
                blend(image, left + x, top + y, white, 170);
                break;
            default:
                break;
            }
        }
    }
    // The cell labels, small, on a backing in the top left corner of every
    // cell (of its part in view).
    const int s = std::max(1, scale / 2);
    const int inset = cell_half + 2;
    for (int row = 0; row < review::rows; ++row) {
        for (int column = 0; column < review::columns; ++column) {
            const review::CellRef ref{.row = row, .column = column};
            const review::Rect cell = review::rect_of(ref);
            if (cell.right <= view.left || cell.left >= view.right || cell.bottom <= view.top ||
                cell.top >= view.bottom) {
                continue;
            }
            const std::string label = review::to_string(ref);
            const int x = left + ((std::max(cell.left, view.left) - view.left) * scale) + inset;
            const int y = top + ((std::max(cell.top, view.top) - view.top) * scale) + inset;
            const int backing_width = text_width(label, s) + (2 * s);
            const int backing_height = (5 * s) + (2 * s);
            for (int dy = 0; dy < backing_height; ++dy) {
                for (int dx = 0; dx < backing_width; ++dx) {
                    blend(image, x + dx, y + dy, black, 190);
                }
            }
            draw_text(image, x + s, y + s, label, s, white);
        }
    }
}

// The frames of the stack in both modes at N, as the program composes them.
struct Frames {
    std::vector<std::uint8_t> hd;
    std::vector<std::uint8_t> original;
};

Frames compose(sci::platform::HdPictures& pictures, const sci::vm::ShownScreen& shown, int scale) {
    const auto frame = [&](GraphicsMode mode) {
        GameFrame game_frame([&pictures](auto stack, auto lines, int at) { return pictures.get(stack, lines, at); });
        game_frame.update(shown, mode, scale, false);
        return game_frame.frame();
    };
    return Frames{.hd = frame(GraphicsMode::Hd), .original = frame(GraphicsMode::Original)};
}

std::string stack_text(std::span<const std::uint16_t> stack) {
    std::string text;
    for (std::size_t i = 0; i < stack.size(); ++i) {
        text += std::format("{}{}", i == 0 ? "" : "+", stack[i]);
    }
    return text;
}

std::string file_stem(std::span<const std::uint16_t> stack) {
    std::string name = "pic";
    for (std::size_t i = 0; i < stack.size(); ++i) {
        name += std::format("{}{:03}", i == 0 ? "-" : "+", stack[i]);
    }
    return name;
}

constexpr int header_scale = 3;
constexpr int header_line = 8 * header_scale;
constexpr int header_padding = 4 * header_scale;

// The header's height: the lines, without the last one's spacing, and the
// padding.
int header_height(std::size_t lines) {
    return (static_cast<int>(lines) * header_line) - (3 * header_scale) + (2 * header_padding);
}

// A sheet: the header lines on top, then the panels' area.
Image make_sheet(const std::vector<std::string>& header, int panels_width, int panels_height) {
    int width = panels_width;
    for (const std::string& line : header) {
        width = std::max(width, text_width(line, header_scale) + (2 * header_padding));
    }
    const int height = header_height(header.size());
    Image image(width, height + panels_height, sheet_background);
    fill(image, 0, 0, width, height, header_background);
    for (std::size_t i = 0; i < header.size(); ++i) {
        draw_text(image, header_padding, header_padding + (static_cast<int>(i) * header_line), header[i], header_scale,
                  header_ink);
    }
    return image;
}

std::string title(const Options& options) {
    const int room = options.room.value_or(options.stack.front());
    return std::format("ROOM {}   PICTURE{} {}   SHEET V{}   {}", room, options.stack.size() > 1 ? "S" : "",
                       stack_text(options.stack), options.sheet_version, options.date);
}

bool write(const Image& image, const fs::path& path) {
    if (!image.write_png(path)) {
        std::println(stderr, "cannot write {}", path.string());
        return false;
    }
    std::println("{}", path.string());
    return true;
}

// B.1: the overview sheet with the grid and the same pair without it.
bool write_overview(const Options& options, const Frames& frames) {
    const int width = screen_width * options.scale;
    const int height = screen_height * options.scale;
    const review::Rect all{.left = 0, .top = 0, .right = review::picture_width, .bottom = review::picture_height};
    const int picture_offset = menu_rows * options.scale;
    for (const bool grid : {true, false}) {
        const std::vector<std::string> header{
            title(options),
            std::format("N = {}   TOP: HD   BOTTOM: ORIGINAL{}", options.scale,
                        grid ? "   GRID: COLUMNS 1-5, ROWS A-C, QUARTERS .1-.4" : "   WITHOUT GRID"),
        };
        Image image = make_sheet(header, width, (2 * height) + panel_gap);
        const int top = header_height(header.size());
        const int left = (image.width() - width) / 2;
        copy_frame(image, frames.hd, options.scale, all, true, left, top);
        copy_frame(image, frames.original, options.scale, all, true, left, top + height + panel_gap);
        if (grid) {
            draw_grid(image, all, options.scale, left, top + picture_offset);
            draw_grid(image, all, options.scale, left, top + height + panel_gap + picture_offset);
        }
        const fs::path path = options.output / (file_stem(options.stack) + (grid ? "-sheet.png" : "-plain.png"));
        if (!write(image, path)) {
            return false;
        }
    }
    return true;
}

// B.4: the enlarged crop of every area asked for.
bool write_crops(const Options& options, const Frames& frames) {
    const int scale = options.crop_scale;
    for (const review::Area& area : options.crops) {
        const review::Rect crop = review::crop_of(area.rect);
        const int width = crop.width() * scale;
        const int height = crop.height() * scale;
        const std::string name = review::to_string(area);
        const std::vector<std::string> header{
            title(options),
            std::format("{}   PICTURE X {}-{}, Y {}-{}   N = {}   {}", name, crop.left, crop.right - 1, crop.top,
                        crop.bottom - 1, scale, options.crop_stacked ? "TOP: HD   BOTTOM: ORIGINAL"
                                                                     : "LEFT: HD   RIGHT: ORIGINAL"),
        };
        const int panels_width = options.crop_stacked ? width : (2 * width) + panel_gap;
        const int panels_height = options.crop_stacked ? (2 * height) + panel_gap : height;
        Image image = make_sheet(header, panels_width, panels_height);
        const int top = header_height(header.size());
        const int left = (image.width() - panels_width) / 2;
        const int second_left = options.crop_stacked ? left : left + width + panel_gap;
        const int second_top = options.crop_stacked ? top + height + panel_gap : top;
        copy_frame(image, frames.hd, scale, crop, false, left, top);
        copy_frame(image, frames.original, scale, crop, false, second_left, second_top);
        draw_grid(image, crop, scale, left, top);
        draw_grid(image, crop, scale, second_left, second_top);
        const fs::path path = options.output / std::format("{}-crop-{}.png", file_stem(options.stack), name);
        if (!write(image, path)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const auto options = parse_options(std::span(argv, static_cast<std::size_t>(argc)));
    if (!options) {
        return usage();
    }
    sci::tools::install_stderr_log();
    const auto manager = sci::tools::open_game(options->game);
    if (!manager) {
        return 1;
    }
    std::optional<fs::path> hints = options->hints;
    if (hints && !fs::is_directory(*hints)) {
        std::println(stderr, "{} not found; pictures are analysed without hints", hints->string());
        hints.reset();
    }
    const auto screen = sci::tools::stack_screen(*manager, options->stack);
    if (!screen) {
        return 1;
    }
    std::error_code error;
    fs::create_directories(options->output, error);
    sci::platform::ThreadExecutor executor;
    sci::platform::AnalysisCache analysis(*manager, hints, options->cache, executor);
    sci::platform::HdPictures pictures(analysis, executor);
    const sci::vm::ShownScreen shown = screen->shown();
    if (options->crops.empty()) {
        return write_overview(*options, compose(pictures, shown, options->scale)) ? 0 : 1;
    }
    return write_crops(*options, compose(pictures, shown, options->crop_scale)) ? 0 : 1;
}
