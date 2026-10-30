// SPDX-License-Identifier: GPL-3.0-or-later
//
// The grid of the visual review format (docs/SPEC_ADDENDA.md, A4 part B):
// the picture area (320 x 190 original pixels, below the menu bar) in 5
// columns 1-5 and 3 rows A-C, each cell in quarters .1 top left, .2 top
// right, .3 bottom left, .4 bottom right; and the references to them that
// the review sheets and the customer's answers use ("B3", "B3.2",
// "A2-A4"). The row boundaries and the quarter lines follow the reading in
// docs/ASSUMPTIONS.md, "Milestone numbers after A4". sci-review-sheet
// draws the grid; the unit tests check the geometry and the parsing.

#pragma once

#include "sci/core/error.hpp"

#include <array>
#include <string>
#include <string_view>

namespace sci::tools::review {

inline constexpr int picture_width = 320;
inline constexpr int picture_height = 190;
inline constexpr int columns = 5;
inline constexpr int rows = 3;

// Cell boundaries in original pixels of the picture area (y = 0 is screen
// row 10).
inline constexpr std::array<int, columns + 1> column_edges{0, 64, 128, 192, 256, 320};
inline constexpr std::array<int, rows + 1> row_edges{0, 63, 127, 190};

// A rectangle in original pixels of the picture area, right and bottom
// exclusive.
struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    [[nodiscard]] int width() const noexcept { return right - left; }

    [[nodiscard]] int height() const noexcept { return bottom - top; }

    friend bool operator==(const Rect&, const Rect&) = default;
};

// A cell (quarter 0) or one of its quarters (1-4); row 0 is A, column 0 is 1.
struct CellRef {
    int row = 0;
    int column = 0;
    int quarter = 0;

    friend bool operator==(const CellRef&, const CellRef&) = default;
};

// The quarter line of a cell: its midpoint rounded half up to a whole
// original pixel.
[[nodiscard]] int quarter_column_line(int column) noexcept;
[[nodiscard]] int quarter_row_line(int row) noexcept;

// The rectangle of a cell or quarter; `ref` must be valid.
[[nodiscard]] Rect rect_of(CellRef ref) noexcept;

// The quarter a pixel of the picture area lies in (0 <= x < 320,
// 0 <= y < 190).
[[nodiscard]] CellRef quarter_at(int x, int y) noexcept;

// "B3" or "B3.2" (the row letter in either case); `ref` back as text.
[[nodiscard]] core::Result<CellRef> parse_cell(std::string_view text);
[[nodiscard]] std::string to_string(CellRef ref);

// What a reference names: one cell or quarter, or a range "A2-A4" (the
// rectangle spanning both ends, which may be cells or quarters).
struct Area {
    CellRef first;
    CellRef last; // Equal to first for a single cell or quarter.
    Rect rect;
};

[[nodiscard]] core::Result<Area> parse_area(std::string_view text);
[[nodiscard]] std::string to_string(const Area& area);

// The enlarged crop of B.4: the area plus half a cell (32 original pixels)
// on every side, clamped to the picture area.
inline constexpr int crop_margin = 32;
[[nodiscard]] Rect crop_of(const Rect& area) noexcept;

} // namespace sci::tools::review
