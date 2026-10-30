// SPDX-License-Identifier: GPL-3.0-or-later

#include "stack_screen.hpp"

#include "picture.hpp"

#include <cstddef>
#include <utility>

namespace sci::tools {

vm::ShownScreen StackScreen::shown() const {
    return vm::ShownScreen{
        .visual = visual,
        .serial = 1,
        .owner = owner,
        .background_visual = visual,
        .background_owner = owner,
        .background_priority = priority,
        .background_source = source,
        .sprite_layer = layer,
        .sprite_lists = {},
        .pictures = stack,
        .lines = {},
        .cursor = -1,
        .mouse = vm::Point{.x = 0, .y = 0},
    };
}

std::optional<StackScreen> stack_screen(const resource::ResourceManager& manager,
                                        std::span<const std::uint16_t> numbers) {
    auto drawn = draw_pictures(manager, numbers);
    if (!drawn) {
        return std::nullopt;
    }
    StackScreen screen{
        .visual = std::move(drawn->maps.visual),
        .priority = std::move(drawn->maps.priority),
        .owner = std::vector<std::uint8_t>(pic::map_size, static_cast<std::uint8_t>(vm::Owner::Picture)),
        .source = std::vector<std::uint8_t>(pic::map_size, vm::priority_from_picture),
        .layer = std::vector<std::uint32_t>(pic::map_size, 0),
        .stack = {},
    };
    for (std::size_t i = 0; i < static_cast<std::size_t>(pic::screen_width) * pic::picture_top; ++i) {
        screen.owner[i] = static_cast<std::uint8_t>(vm::Owner::Interface);
    }
    screen.stack.reserve(numbers.size());
    for (const std::uint16_t number : numbers) {
        screen.stack.push_back(vm::ShownPicture{.number = number, .mirrored = false, .ega_palette = 0});
    }
    return screen;
}

} // namespace sci::tools
