// SPDX-License-Identifier: GPL-3.0-or-later

#include "review_grid.hpp"

#include "sci/core/assert.hpp"

#include <algorithm>
#include <cstddef>
#include <format>

namespace sci::tools::review {

namespace {

constexpr int midpoint_half_up(int a, int b) noexcept {
    return (a + b + 1) / 2;
}

core::Result<CellRef> invalid(std::string_view text) {
    return core::make_error(core::ErrorCode::InvalidData,
                            std::format("'{}' is not a cell (A1 to C5) or a quarter (such as B3.2)", text));
}

} // namespace

int quarter_column_line(int column) noexcept {
    SCI_ASSERT(column >= 0 && column < columns);
    const auto at = static_cast<std::size_t>(column);
    return midpoint_half_up(column_edges[at], column_edges[at + 1]);
}

int quarter_row_line(int row) noexcept {
    SCI_ASSERT(row >= 0 && row < rows);
    const auto at = static_cast<std::size_t>(row);
    return midpoint_half_up(row_edges[at], row_edges[at + 1]);
}

Rect rect_of(CellRef ref) noexcept {
    SCI_ASSERT(ref.row >= 0 && ref.row < rows && ref.column >= 0 && ref.column < columns && ref.quarter >= 0 &&
               ref.quarter <= 4);
    const auto column = static_cast<std::size_t>(ref.column);
    const auto row = static_cast<std::size_t>(ref.row);
    Rect rect{
        .left = column_edges[column],
        .top = row_edges[row],
        .right = column_edges[column + 1],
        .bottom = row_edges[row + 1],
    };
    if (ref.quarter == 0) {
        return rect;
    }
    const int x = quarter_column_line(ref.column);
    const int y = quarter_row_line(ref.row);
    if (ref.quarter == 1 || ref.quarter == 3) {
        rect.right = x;
    } else {
        rect.left = x;
    }
    if (ref.quarter == 1 || ref.quarter == 2) {
        rect.bottom = y;
    } else {
        rect.top = y;
    }
    return rect;
}

core::Result<CellRef> parse_cell(std::string_view text) {
    if (text.size() != 2 && text.size() != 4) {
        return invalid(text);
    }
    const char letter = text[0];
    CellRef ref;
    if (letter >= 'A' && letter < 'A' + rows) {
        ref.row = letter - 'A';
    } else if (letter >= 'a' && letter < 'a' + rows) {
        ref.row = letter - 'a';
    } else {
        return invalid(text);
    }
    if (text[1] < '1' || text[1] >= '1' + columns) {
        return invalid(text);
    }
    ref.column = text[1] - '1';
    if (text.size() == 4) {
        if (text[2] != '.' || text[3] < '1' || text[3] > '4') {
            return invalid(text);
        }
        ref.quarter = text[3] - '0';
    }
    return ref;
}

std::string to_string(CellRef ref) {
    std::string text{static_cast<char>('A' + ref.row), static_cast<char>('1' + ref.column)};
    if (ref.quarter != 0) {
        text += std::format(".{}", ref.quarter);
    }
    return text;
}

core::Result<Area> parse_area(std::string_view text) {
    const auto dash = text.find('-');
    const auto first = parse_cell(text.substr(0, dash));
    if (!first) {
        return std::unexpected(first.error());
    }
    Area area{.first = *first, .last = *first, .rect = rect_of(*first)};
    if (dash == std::string_view::npos) {
        return area;
    }
    const auto last = parse_cell(text.substr(dash + 1));
    if (!last) {
        return std::unexpected(last.error());
    }
    const Rect other = rect_of(*last);
    area.last = *last;
    area.rect = Rect{
        .left = std::min(area.rect.left, other.left),
        .top = std::min(area.rect.top, other.top),
        .right = std::max(area.rect.right, other.right),
        .bottom = std::max(area.rect.bottom, other.bottom),
    };
    return area;
}

std::string to_string(const Area& area) {
    return area.first == area.last ? to_string(area.first) : to_string(area.first) + "-" + to_string(area.last);
}

Rect crop_of(const Rect& area) noexcept {
    return Rect{
        .left = std::max(0, area.left - crop_margin),
        .top = std::max(0, area.top - crop_margin),
        .right = std::min(picture_width, area.right + crop_margin),
        .bottom = std::min(picture_height, area.bottom + crop_margin),
    };
}

} // namespace sci::tools::review
