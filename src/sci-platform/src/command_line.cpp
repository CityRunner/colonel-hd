// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/platform/command_line.hpp"

#include "sci/platform/config.hpp"
#include "sci/platform/data_folder.hpp"

#include <charconv>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

namespace sci::platform {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view help_text_lines =
    "usage: colonel-hd [--viewer] [--data <folder>] [--log <categories>] [game folder]\n"
    "       colonel-hd [--replay <file>] [--seed <n>] [--fast] [--end-state <file>]\n"
    "                 [--timing <file> [--scale <n>]] [game folder]\n"
    "       colonel-hd --version | --help\n"
    "\n"
    "--replay plays the input of a replay file\n"
    "(docs/formats/replay-and-trace.md) into the game, with the\n"
    "random seed of --seed (default 0); keyboard and mouse still work.\n"
    "--fast runs the game as fast as it can instead of in real time\n"
    "(the game plays the same; the screen is shown about 60 times a\n"
    "second of real time and most of the music is dropped).\n"
    "--data names the folder for colonel-hd.ini, game/, rom/, saves/,\n"
    "cache/ and files/. Without it: the program's folder if it holds\n"
    "colonel-hd.ini or game/ and can be written, else the per-user folder\n"
    "(%APPDATA%\\colonel-hd on Windows, ~/.local/share/colonel-hd on Linux).\n"
    "--log shows more messages on the console, for these categories\n"
    "(comma-separated): vm (script problems the interpreter works\n"
    "around), resources (resource loading); it overrides the log key of\n"
    "colonel-hd.ini for this run.\n"
    "--timing plays the --replay file as fast as it can, presenting\n"
    "every frame, and writes the frame times, room entries, first\n"
    "launch and peak memory to the file (docs/timing.md); --scale sets\n"
    "the N it measures at (1 to 16, also beyond the window's size).\n"
    "--end-state writes the game's tick, globals and state hash to\n"
    "the file when it ends (as sci-vm-trace --globals and\n"
    "--state-hash write them at a tick past the end).\n"
    "\n"
    "In the game:\n"
    "  F12               HD or original mode\n"
    "  F11, Alt+Enter    fullscreen\n"
    "\n"
    "In the room viewer (--viewer):\n"
    "  Left, Right       previous or next picture, view or font\n"
    "  Up, Down          views: previous or next loop\n"
    "  Tab               pictures, views, fonts\n"
    "  H                 HD or original mode\n"
    "  P                 pictures: visual or priority map\n"
    "  + and -           scale N up or down; 0: the largest N that fits\n"
    "  Space             views: stop or start the animation\n"
    "  F11, Alt+Enter    fullscreen\n"
    "  Escape            quit\n";

// Sets an option that takes a value: nothing if `arg` is not one, false if
// the value is not valid.
std::optional<bool> value_option(Options& options, std::string_view arg, std::string_view value) {
    if (arg == "--replay") {
        options.replay = fs::path(value);
    } else if (arg == "--data") {
        options.data = fs::path(value);
    } else if (arg == "--end-state") {
        options.end_state = fs::path(value);
    } else if (arg == "--timing") {
        options.timing = fs::path(value);
    } else if (arg == "--scale") {
        int scale = 0;
        const auto [end, error] = std::from_chars(std::to_address(value.begin()), std::to_address(value.end()), scale);
        if (error != std::errc{} || end != std::to_address(value.end()) || scale < 1 || scale > max_scale) {
            return false;
        }
        options.scale = scale;
    } else if (arg == "--log") {
        options.log = core::parse_log_categories(value);
        return options.log.has_value();
    } else if (arg == "--seed") {
        std::uint32_t seed = 0;
        const auto [end, error] = std::from_chars(std::to_address(value.begin()), std::to_address(value.end()), seed);
        if (error != std::errc{} || end != std::to_address(value.end())) {
            return false;
        }
        options.seed = seed;
    } else {
        return std::nullopt;
    }
    return true;
}

// The options, or nothing if they are not valid.
std::optional<Options> parse_options(std::span<const std::string_view> args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "--viewer") {
            options.viewer = true;
        } else if (arg == "--fast") {
            options.fast = true;
        } else if (const auto taken =
                       i + 1 < args.size() ? value_option(options, arg, args[i + 1]) : std::optional<bool>()) {
            if (!*taken) {
                return std::nullopt;
            }
            ++i;
        } else if (!arg.starts_with('-') && !options.game_folder) {
            options.game_folder = fs::path(arg);
        } else {
            return std::nullopt;
        }
    }
    if (options.viewer && (options.replay || options.seed || options.fast || options.end_state || options.timing)) {
        return std::nullopt;
    }
    // The timing mode measures a replay; --scale only sets its N.
    if ((options.timing && !options.replay) || (options.scale && !options.timing)) {
        return std::nullopt;
    }
    return options;
}

} // namespace

CommandLine parse_command_line(std::span<const std::string_view> args) {
    CommandLine command;
    if (args.size() == 1 && args[0] == "--version") {
        command.action = CommandAction::Version;
    } else if (auto options = parse_options(args)) {
        command.options = std::move(*options);
    } else if (args.size() == 1 && args[0] == "--help") {
        command.action = CommandAction::Help;
    } else {
        command.action = CommandAction::Usage;
        command.exit_code = 2;
    }
    return command;
}

std::string_view help_text() {
    return help_text_lines;
}

void use_data_folder_defaults(Config& config, const Options& options, const fs::path& data_folder) {
    if (options.game_folder) {
        config.game_folder = *options.game_folder;
    } else if (config.game_folder.empty()) {
        if (auto game_folder = data_game_folder(data_folder)) {
            config.game_folder = std::move(*game_folder);
        }
    }
    if (config.mt32_rom.empty()) {
        config.mt32_rom = data_folder / "rom";
    }
}

} // namespace sci::platform
