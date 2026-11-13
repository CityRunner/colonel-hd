// SPDX-License-Identifier: GPL-3.0-or-later
//
// The window's scale (R-112, B-32).

#include "sci/platform/config.hpp"
#include "sci/platform/window_scale.hpp"
#include "sci_test.hpp"

using sci::platform::DisplaySize;

SCI_TEST(auto_is_the_largest_scale_that_fits_capped) {
    SCI_CHECK(sci::platform::fit_scale(1920, 1080) == 5); // 6 x 320 fits, 6 x 200 does not.
    SCI_CHECK(sci::platform::fit_scale(320, 200) == 1);
    SCI_CHECK(sci::platform::fit_scale(100, 100) == 1); // Smaller than 1x: still 1.
    SCI_CHECK(sci::platform::fit_scale(20000, 20000) == sci::platform::max_scale);
    SCI_CHECK(sci::platform::scale_for_window(0, 1920, 1080) == 5);
}

SCI_TEST(a_configured_scale_is_reduced_to_what_fits) {
    SCI_CHECK(sci::platform::scale_for_window(4, 1920, 1080) == 4);
    SCI_CHECK(sci::platform::scale_for_window(8, 1920, 1080) == 5);
    SCI_CHECK(sci::platform::scale_for_window(2, 300, 150) == 1);
}

SCI_TEST(the_window_opens_at_three_for_auto_else_as_configured_where_it_fits) {
    SCI_CHECK(sci::platform::initial_window_scale(0, DisplaySize{.width = 1920, .height = 1040}) == 3);
    SCI_CHECK(sci::platform::initial_window_scale(0, std::nullopt) == 3);
    SCI_CHECK(sci::platform::initial_window_scale(4, DisplaySize{.width = 1920, .height = 1040}) == 4);
    SCI_CHECK(sci::platform::initial_window_scale(8, DisplaySize{.width = 1920, .height = 1040}) == 5);
    SCI_CHECK(sci::platform::initial_window_scale(8, std::nullopt) == 8); // Display unknown.
}
