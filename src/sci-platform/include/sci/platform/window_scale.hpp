// SPDX-License-Identifier: GPL-3.0-or-later
//
// The scale N of the program's window (R-112, B-32): always an integer;
// "auto" (0) is the largest N that fits, capped at max_scale; a configured
// N is reduced to what fits. Apart from SDL so CTest can check them.
#pragma once

#include <optional>

namespace sci::platform {

// The largest N at which the 320 x 200 screen fits in width x height
// pixels: at least 1, at most max_scale.
[[nodiscard]] int fit_scale(int width, int height);

// The N to draw at in a window of width x height pixels: the configured N
// (1 to max_scale) as far as it fits, or fit_scale for auto (0).
[[nodiscard]] int scale_for_window(int configured, int width, int height);

struct DisplaySize {
    int width = 0;
    int height = 0;
};

// The N the window opens at: 3 for auto; the configured N as far as the
// display's usable area allows, or the configured N when it is not known.
[[nodiscard]] int initial_window_scale(int configured, std::optional<DisplaySize> usable);

} // namespace sci::platform
