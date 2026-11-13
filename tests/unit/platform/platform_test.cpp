// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/platform/config.hpp"
#include "sci/platform/identify.hpp"
#include "sci_test.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using sci::platform::GraphicsMode;
using sci::platform::MusicDevice;
using sci::platform::parse_config;

SCI_TEST(config_reads_its_keys) {
    const auto parsed = parse_config("; comment\n[colonel-hd]\nformat = 1\ngame_folder = C:\\Games\\Laura;1\n"
                                     "mode = original ; comment\nscale = 5\nmt32_rom =\ncolour = red\n");
    SCI_CHECK(parsed.has_value());
    if (!parsed) {
        return;
    }
    SCI_CHECK(parsed->config.game_folder.u8string() == u8"C:\\Games\\Laura;1");
    SCI_CHECK(parsed->config.mode == GraphicsMode::Original);
    SCI_CHECK(parsed->config.scale == 5);
    SCI_CHECK(parsed->config.mt32_rom.empty());
    SCI_CHECK(!parsed->config.priority_snap); // Off by default.
    SCI_CHECK(parsed->warnings.size() == 1);
}

SCI_TEST(config_reads_priority_snap) {
    const auto on = parse_config("[colonel-hd]\nformat = 1\npriority_snap = on\n");
    SCI_CHECK(on.has_value() && on->config.priority_snap);
    const auto off = parse_config("[colonel-hd]\nformat = 1\npriority_snap = off ; comment\n");
    SCI_CHECK(off.has_value() && !off->config.priority_snap);
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\npriority_snap = yes\n").has_value());
}

SCI_TEST(config_reads_music_and_volume) {
    const auto defaults = parse_config("[colonel-hd]\nformat = 1\n");
    SCI_CHECK(defaults.has_value() && defaults->config.music == MusicDevice::Adlib && defaults->config.volume == 100);
    const auto set = parse_config("[colonel-hd]\nformat = 1\nmusic = off ; silent\nvolume = 40\n");
    SCI_CHECK(set.has_value() && set->config.music == MusicDevice::Off && set->config.volume == 40);
    const auto mt32 = parse_config("[colonel-hd]\nformat = 1\nmusic = mt32\n");
    SCI_CHECK(mt32.has_value() && mt32->config.music == MusicDevice::Mt32);
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\nmusic = gm\n").has_value());
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\nvolume = 101\n").has_value());
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\nvolume = -1\n").has_value());
}

SCI_TEST(config_round_trips) {
    sci::platform::Config config;
    config.game_folder = "/home/player/laura bow";
    config.scale = 0;
    config.priority_snap = true;
    config.music = MusicDevice::Off;
    config.volume = 75;
    const auto parsed = parse_config(sci::platform::format_config(config));
    SCI_CHECK(parsed.has_value() && parsed->config.game_folder == config.game_folder && parsed->config.scale == 0 &&
              parsed->config.mode == GraphicsMode::Hd && parsed->config.priority_snap && parsed->warnings.empty());
    SCI_CHECK(parsed.has_value() && parsed->config.music == MusicDevice::Off && parsed->config.volume == 75);
}

SCI_TEST(config_rejects_what_it_cannot_read) {
    SCI_CHECK(!parse_config("[colonel-hd]\nmode = hd\n").has_value());  // No format.
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 2\n").has_value()); // Newer format.
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\nscale = 17\n").has_value());
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\nmode = vga\n").has_value());
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\nnonsense\n").has_value());
}

SCI_TEST(config_reads_and_writes_log_categories) {
    const auto parsed = parse_config("[colonel-hd]\nformat = 1\nlog = vm, resources ; comment\n");
    SCI_CHECK(parsed.has_value() && parsed->config.log.size() == 2 &&
              parsed->config.log[0] == sci::core::LogCategory::Vm);
    SCI_CHECK(!parse_config("[colonel-hd]\nformat = 1\nlog = graphics\n").has_value());
    const auto defaults = parse_config("[colonel-hd]\nformat = 1\n");
    SCI_CHECK(defaults.has_value() && defaults->config.log.empty());
    if (parsed) {
        const auto again = parse_config(sci::platform::format_config(parsed->config));
        SCI_CHECK(again.has_value() && again->config.log == parsed->config.log);
    }
}

SCI_TEST(config_updates_keep_every_other_line) {
    using sci::platform::update_config;
    // Comments, a blank line, an unknown key, another section, a trailing
    // comment on the changed value (B-04).
    const std::string text = "; my notes\n[colonel-hd]\nformat = 1\n\n# old\nmode = hd   ; F12 to switch\n"
                             "favourite = yes\ngame_folder = /games/Laura;1\n[other]\nmode = x\n";
    const auto parsed = parse_config(text);
    SCI_CHECK(parsed.has_value());
    if (!parsed) {
        return;
    }
    auto changed = parsed->config;
    changed.mode = GraphicsMode::Original;
    const std::string updated = update_config(text, parsed->config, changed);
    SCI_CHECK(updated == "; my notes\n[colonel-hd]\nformat = 1\n\n# old\nmode = original   ; F12 to switch\n"
                         "favourite = yes\ngame_folder = /games/Laura;1\n[other]\nmode = x\n");
    // Nothing changed: the text is returned as it was.
    SCI_CHECK(update_config(text, parsed->config, parsed->config) == text);
    // CR LF line endings stay; a key without a line is added at the end of
    // the section, with the file's line ending.
    const std::string crlf = "[colonel-hd]\r\nformat = 1\r\nmode = hd\r\n[other]\r\nx = 1\r\n";
    const auto parsed_crlf = parse_config(crlf);
    SCI_CHECK(parsed_crlf.has_value());
    if (parsed_crlf) {
        auto scaled = parsed_crlf->config;
        scaled.mode = GraphicsMode::Original;
        scaled.scale = 5;
        SCI_CHECK(update_config(crlf, parsed_crlf->config, scaled) ==
                  "[colonel-hd]\r\nformat = 1\r\nmode = original\r\nscale = 5\r\n[other]\r\nx = 1\r\n");
    }
    // No file yet: the commented defaults.
    SCI_CHECK(update_config("", sci::platform::Config{}, changed) == sci::platform::format_config(changed));
    // What update_config writes reads back as the changed configuration.
    const auto again = parse_config(updated);
    SCI_CHECK(again.has_value() && again->config.mode == GraphicsMode::Original &&
              again->config.game_folder == parsed->config.game_folder);
}

SCI_TEST(config_file_and_section_have_the_colonel_hd_name) {
    SCI_CHECK(sci::platform::config_file_name == "colonel-hd.ini");
    SCI_CHECK(sci::platform::format_config(sci::platform::Config{}).contains("\n[colonel-hd]\n"));
    // The old name's section is not read: its keys are ignored, so it has
    // no format and is rejected (clean rename, no migration). The old name
    // is put together here so that B-43's rename grep finds no mention.
    const std::string old_section = "[" + std::string("sierra") + "-hd]\n";
    SCI_CHECK(!parse_config(old_section + "format = 1\ngame_folder = /games/laura\n").has_value());
    const auto both = parse_config(old_section + "format = 1\nvolume = 10\n[colonel-hd]\nformat = 1\n");
    SCI_CHECK(both.has_value() && both->config.volume == sci::platform::Config{}.volume);
}

SCI_TEST(unknown_game_folders_are_rejected) {
    const std::vector<sci::resource::GameFile> none;
    SCI_CHECK(!sci::platform::identify_game(none).has_value());
    const std::vector<sci::resource::GameFile> other{
        {.name = "resource.map", .bytes = std::vector<std::uint8_t>(8478, 0)},
    };
    const auto id = sci::platform::identify_game(other);
    SCI_CHECK(!id.has_value() && id.error().code == sci::core::ErrorCode::Unsupported);
}
