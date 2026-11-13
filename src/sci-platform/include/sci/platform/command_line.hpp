// SPDX-License-Identifier: GPL-3.0-or-later
//
// The program's command line (R-110, B-32): the options, their rules, the
// help text and the exit codes, apart from SDL so CTest can check them.
#pragma once

#include "sci/core/log.hpp"
#include "sci/platform/config.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace sci::platform {

struct Options {
    bool viewer = false;
    std::optional<std::filesystem::path> game_folder;
    std::optional<std::filesystem::path> replay;
    std::optional<std::uint32_t> seed;
    bool fast = false;
    std::optional<std::filesystem::path> end_state;
    std::optional<std::vector<core::LogCategory>> log;
    std::optional<std::filesystem::path> data;
};

enum class CommandAction : std::uint8_t {
    Run,     // Start with the options.
    Version, // "--version" alone: print the version, exit 0.
    Help,    // "--help" alone: print the help, exit 0.
    Usage,   // Anything not valid: print the help, exit 2.
};

struct CommandLine {
    CommandAction action = CommandAction::Run;
    Options options;
    // The exit code for Version, Help and Usage.
    int exit_code = 0;
};

// Reads the arguments after the program's name. Valid: --viewer, --fast,
// --replay <file>, --seed <n> (an unsigned 32-bit number), --end-state
// <file>, --log <categories>, --data <folder>, and one game folder (not
// starting with '-'); --viewer with any of --replay, --seed, --fast or
// --end-state is not.
[[nodiscard]] CommandLine parse_command_line(std::span<const std::string_view> args);

// The text --help and invalid usage print.
[[nodiscard]] std::string_view help_text();

// The game folder of the command line, else game/ in the data folder when
// the configuration's game_folder is empty and it holds the game; rom/ in
// the data folder when mt32_rom is empty. For this run only: neither is
// written to the ini.
void use_data_folder_defaults(Config& config, const Options& options, const std::filesystem::path& data_folder);

} // namespace sci::platform
