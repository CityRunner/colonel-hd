// SPDX-License-Identifier: GPL-3.0-or-later
//
// A picture stack (a picture with overlays drawn over it) as the
// interpreter would show it with nothing else on screen: drawn by path 1,
// the picture's everywhere, the menu bar's rows the interface's (blank, as
// the game shows them while the menu bar is hidden). sci-hd-frames --stack
// and sci-review-sheet compose it through the program's own frame
// (sci/platform/game_frame.hpp).

#pragma once

#include "sci/resource/resource_manager.hpp"
#include "sci/vm/game.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace sci::tools {

struct StackScreen {
    std::vector<std::uint8_t> visual;
    std::vector<std::uint8_t> priority;
    std::vector<std::uint8_t> owner;
    std::vector<std::uint8_t> source;
    std::vector<std::uint32_t> layer;
    std::vector<vm::ShownPicture> stack;

    // Refers to the vectors above.
    [[nodiscard]] vm::ShownScreen shown() const;
};

// Errors go to stderr.
[[nodiscard]] std::optional<StackScreen> stack_screen(const resource::ResourceManager& manager,
                                                      std::span<const std::uint16_t> numbers);

} // namespace sci::tools
