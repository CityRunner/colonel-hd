// SPDX-License-Identifier: GPL-3.0-or-later
//
// The grid of the visual review format (A4 part B,
// tools/common/review_grid.hpp): the cells and quarters on whole original
// pixels as docs/ASSUMPTIONS.md reads them, and the references "B3",
// "B3.2" and "A2-A4" with the crop around them.

#include "review_grid.hpp"
#include "sci_test.hpp"

#include <string_view>

using sci::tools::review::CellRef;
using sci::tools::review::crop_of;
using sci::tools::review::parse_area;
using sci::tools::review::parse_cell;
using sci::tools::review::quarter_column_line;
using sci::tools::review::quarter_row_line;
using sci::tools::review::Rect;
using sci::tools::review::rect_of;
using sci::tools::review::to_string;

SCI_TEST(quarter_lines_are_the_midpoints_rounded_half_up) {
    SCI_CHECK(quarter_column_line(0) == 32);
    SCI_CHECK(quarter_column_line(1) == 96);
    SCI_CHECK(quarter_column_line(2) == 160);
    SCI_CHECK(quarter_column_line(3) == 224);
    SCI_CHECK(quarter_column_line(4) == 288);
    SCI_CHECK(quarter_row_line(0) == 32);
    SCI_CHECK(quarter_row_line(1) == 95);
    SCI_CHECK(quarter_row_line(2) == 159);
}

SCI_TEST(cells_tile_the_picture_area) {
    SCI_CHECK(rect_of({.row = 0, .column = 0}) == Rect{.left = 0, .top = 0, .right = 64, .bottom = 63});
    SCI_CHECK(rect_of({.row = 1, .column = 2}) == Rect{.left = 128, .top = 63, .right = 192, .bottom = 127});
    SCI_CHECK(rect_of({.row = 2, .column = 4}) == Rect{.left = 256, .top = 127, .right = 320, .bottom = 190});
    int area = 0;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 5; ++column) {
            const Rect cell = rect_of({.row = row, .column = column});
            area += cell.width() * cell.height();
            int quarters = 0;
            for (int quarter = 1; quarter <= 4; ++quarter) {
                const Rect part = rect_of({.row = row, .column = column, .quarter = quarter});
                quarters += part.width() * part.height();
                SCI_CHECK(part.left >= cell.left && part.right <= cell.right);
                SCI_CHECK(part.top >= cell.top && part.bottom <= cell.bottom);
            }
            SCI_CHECK(quarters == cell.width() * cell.height());
        }
    }
    SCI_CHECK(area == 320 * 190);
}

SCI_TEST(quarters_are_numbered_left_to_right_top_to_bottom) {
    SCI_CHECK(rect_of({.row = 1, .column = 2, .quarter = 1}) ==
              Rect{.left = 128, .top = 63, .right = 160, .bottom = 95});
    SCI_CHECK(rect_of({.row = 1, .column = 2, .quarter = 2}) ==
              Rect{.left = 160, .top = 63, .right = 192, .bottom = 95});
    SCI_CHECK(rect_of({.row = 1, .column = 2, .quarter = 3}) ==
              Rect{.left = 128, .top = 95, .right = 160, .bottom = 127});
    SCI_CHECK(rect_of({.row = 1, .column = 2, .quarter = 4}) ==
              Rect{.left = 160, .top = 95, .right = 192, .bottom = 127});
}

SCI_TEST(parses_cells_and_quarters) {
    SCI_CHECK(parse_cell("B3") == CellRef{.row = 1, .column = 2, .quarter = 0});
    SCI_CHECK(parse_cell("b3") == CellRef{.row = 1, .column = 2, .quarter = 0});
    SCI_CHECK(parse_cell("B3.2") == CellRef{.row = 1, .column = 2, .quarter = 2});
    SCI_CHECK(parse_cell("A1") == CellRef{.row = 0, .column = 0, .quarter = 0});
    SCI_CHECK(parse_cell("C5.4") == CellRef{.row = 2, .column = 4, .quarter = 4});
    SCI_CHECK(to_string(CellRef{.row = 1, .column = 2, .quarter = 2}) == "B3.2");
    SCI_CHECK(to_string(CellRef{.row = 2, .column = 0, .quarter = 0}) == "C1");
}

SCI_TEST(rejects_what_is_not_a_cell) {
    for (const std::string_view text : {"", "B", "3B", "D1", "B0", "B6", "B3.", "B3.0", "B3.5", "B3,2", "B32", " B3",
                                        "B3 ", "B3.2.1", "A2-", "-A4", "A2-A4-B1", "A2--A4", "A2-D4"}) {
        SCI_CHECK(!parse_area(text).has_value());
    }
    SCI_CHECK(parse_area("Z9").error().code == sci::core::ErrorCode::InvalidData);
}

SCI_TEST(parses_ranges_as_the_rectangle_spanning_both_ends) {
    const auto row = parse_area("A2-A4");
    SCI_CHECK(row.has_value());
    SCI_CHECK(row->rect == Rect{.left = 64, .top = 0, .right = 256, .bottom = 63});
    SCI_CHECK(to_string(*row) == "A2-A4");
    const auto block = parse_area("C4-B2");
    SCI_CHECK(block->rect == Rect{.left = 64, .top = 63, .right = 256, .bottom = 190});
    const auto quarters = parse_area("B3.2-B4.1");
    SCI_CHECK(quarters->rect == Rect{.left = 160, .top = 63, .right = 224, .bottom = 95});
    const auto single = parse_area("B3.2");
    SCI_CHECK(single->first == single->last);
    SCI_CHECK(to_string(*single) == "B3.2");
}

SCI_TEST(crops_add_half_a_cell_clamped_to_the_picture) {
    SCI_CHECK(crop_of(parse_area("B3")->rect) == Rect{.left = 96, .top = 31, .right = 224, .bottom = 159});
    SCI_CHECK(crop_of(parse_area("A1")->rect) == Rect{.left = 0, .top = 0, .right = 96, .bottom = 95});
    SCI_CHECK(crop_of(parse_area("C5.4")->rect) == Rect{.left = 256, .top = 127, .right = 320, .bottom = 190});
    SCI_CHECK(crop_of(parse_area("A1-C5")->rect) == Rect{.left = 0, .top = 0, .right = 320, .bottom = 190});
}
