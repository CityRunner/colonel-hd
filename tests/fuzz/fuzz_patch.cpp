// SPDX-License-Identifier: GPL-3.0-or-later
//
// Parses arbitrary bytes as the music driver's patch resources: an AdLib
// instrument bank (patch.003) and an MT-32 patch (patch.001). The parsers
// must return the patches or an error, never fail otherwise.

#include "sci/audio/adlib.hpp"
#include "sci/audio/mt32_driver.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// NOLINTNEXTLINE(readability-identifier-naming): name fixed by libFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::span<const std::uint8_t> bytes(data, size);
    [[maybe_unused]] const auto adlib = sci::audio::parse_adlib_patches(bytes);
    [[maybe_unused]] const auto mt32 = sci::audio::parse_mt32_patch(bytes);
    return 0;
}
