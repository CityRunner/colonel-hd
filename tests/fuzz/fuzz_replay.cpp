// SPDX-License-Identifier: GPL-3.0-or-later
//
// Parses arbitrary text as a replay file: the parser must return the events
// or an error, never fail otherwise.

#include "sci/vm/events.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

// NOLINTNEXTLINE(readability-identifier-naming): name fixed by libFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // NOLINTNEXTLINE(*-reinterpret-cast): libFuzzer bytes as text.
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    [[maybe_unused]] const auto parsed = sci::vm::parse_replay(text);
    return 0;
}
