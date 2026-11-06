// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plays arbitrary bytes as an SCI0 sound resource through sci-vm's music
// player and MIDI parser on sci-audio's AdLib driver (sci::vm::render_sound),
// twice through the song, for at most 0.2 s of output: the player must
// return the samples or an error, never fail otherwise.

#include "sci/audio/adlib.hpp"
#include "sci/audio/opl3.hpp"
#include "sci/vm/sound_render.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// NOLINTNEXTLINE(readability-identifier-naming): name fixed by libFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    sci::audio::AdlibDriver driver(std::vector<sci::audio::AdlibPatch>(96));
    [[maybe_unused]] const auto rendered =
        sci::vm::render_sound(std::span<const std::uint8_t>(data, size), 1, driver,
                              sci::vm::SoundRenderOptions{.max_frames = sci::audio::Opl3::native_rate / 5, .loop = 2});
    return 0;
}
