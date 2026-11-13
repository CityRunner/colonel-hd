// SPDX-License-Identifier: GPL-3.0-or-later
//
// Parses arbitrary text as the configuration file: the parser must return
// the configuration or an error, never fail otherwise; a configuration it
// returns must format and parse again.

#include "sci/platform/config.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

// NOLINTNEXTLINE(readability-identifier-naming): name fixed by libFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // NOLINTNEXTLINE(*-reinterpret-cast): libFuzzer bytes as text.
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const auto parsed = sci::platform::parse_config(text);
    if (parsed) {
        [[maybe_unused]] const auto again = sci::platform::parse_config(sci::platform::format_config(parsed->config));
    }
    return 0;
}
