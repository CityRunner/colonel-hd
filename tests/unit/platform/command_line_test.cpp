// SPDX-License-Identifier: GPL-3.0-or-later
//
// The command line (R-110, B-32): each rule of the options, the help and
// the exit codes.

#include "sci/platform/command_line.hpp"
#include "sci/platform/config.hpp"
#include "sci_test.hpp"

#include <filesystem>
#include <initializer_list>
#include <string_view>
#include <vector>

using sci::platform::CommandAction;
using sci::platform::CommandLine;

namespace {

CommandLine parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list(args);
    return sci::platform::parse_command_line(list);
}

bool is_usage(const CommandLine& command) {
    return command.action == CommandAction::Usage && command.exit_code == 2;
}

} // namespace

SCI_TEST(no_arguments_run_with_the_defaults) {
    const CommandLine command = parse({});
    SCI_CHECK(command.action == CommandAction::Run);
    SCI_CHECK(!command.options.viewer && !command.options.fast);
    SCI_CHECK(!command.options.game_folder && !command.options.replay && !command.options.seed);
    SCI_CHECK(!command.options.end_state && !command.options.log && !command.options.data);
}

SCI_TEST(version_and_help_alone_exit_0) {
    const CommandLine version = parse({"--version"});
    SCI_CHECK(version.action == CommandAction::Version && version.exit_code == 0);
    const CommandLine help = parse({"--help"});
    SCI_CHECK(help.action == CommandAction::Help && help.exit_code == 0);
    // With anything else they are invalid usage.
    SCI_CHECK(is_usage(parse({"--version", "game"})));
    SCI_CHECK(is_usage(parse({"--help", "--viewer"})));
    SCI_CHECK(sci::platform::help_text().starts_with("usage: colonel-hd"));
    SCI_CHECK(sci::platform::help_text().contains("--viewer"));
}

SCI_TEST(the_game_options_and_a_folder) {
    const CommandLine command = parse({
        "--replay",
        "r.txt",
        "--seed",
        "42",
        "--fast",
        "--end-state",
        "end.txt",
        "--log",
        "vm,resources",
        "--data",
        "d",
        "game",
    });
    SCI_CHECK(command.action == CommandAction::Run);
    const sci::platform::Options& options = command.options;
    SCI_CHECK(options.replay == std::filesystem::path("r.txt"));
    SCI_CHECK(options.seed == 42U);
    SCI_CHECK(options.fast);
    SCI_CHECK(options.end_state == std::filesystem::path("end.txt"));
    SCI_CHECK(options.log.has_value() && options.log->size() == 2);
    SCI_CHECK(options.data == std::filesystem::path("d"));
    SCI_CHECK(options.game_folder == std::filesystem::path("game"));
}

SCI_TEST(the_viewer_with_a_folder_and_the_data_and_log_options) {
    const CommandLine command = parse({"--viewer", "--data", "d", "--log", "vm", "game"});
    SCI_CHECK(command.action == CommandAction::Run && command.options.viewer);
    SCI_CHECK(command.options.game_folder == std::filesystem::path("game"));
}

SCI_TEST(the_timing_mode_needs_a_replay_and_takes_a_scale) {
    const CommandLine command = parse({"--replay", "r.txt", "--timing", "t.csv", "--scale", "10"});
    SCI_CHECK(command.action == CommandAction::Run);
    SCI_CHECK(command.options.timing == std::filesystem::path("t.csv"));
    SCI_CHECK(command.options.scale == 10);
    SCI_CHECK(parse({"--replay", "r.txt", "--timing", "t.csv"}).options.scale == std::nullopt);
    SCI_CHECK(is_usage(parse({"--timing", "t.csv"})));                 // No replay.
    SCI_CHECK(is_usage(parse({"--replay", "r.txt", "--scale", "2"}))); // No timing.
    SCI_CHECK(is_usage(parse({"--replay", "r", "--timing", "t", "--scale", "0"})));
    SCI_CHECK(is_usage(parse({"--replay", "r", "--timing", "t", "--scale", "17"})));
    SCI_CHECK(is_usage(parse({"--replay", "r", "--timing", "t", "--scale", "auto"})));
}

SCI_TEST(the_viewer_with_a_game_option_is_invalid) {
    SCI_CHECK(is_usage(parse({"--viewer", "--replay", "r.txt"})));
    SCI_CHECK(is_usage(parse({"--viewer", "--seed", "1"})));
    SCI_CHECK(is_usage(parse({"--fast", "--viewer"})));
    SCI_CHECK(is_usage(parse({"--viewer", "--end-state", "e"})));
    SCI_CHECK(is_usage(parse({"--viewer", "--replay", "r", "--timing", "t"})));
}

SCI_TEST(invalid_values_and_arguments_are_usage_errors) {
    SCI_CHECK(is_usage(parse({"--seed", "-1"})));
    SCI_CHECK(is_usage(parse({"--seed", "12x"})));
    SCI_CHECK(is_usage(parse({"--seed", "4294967296"}))); // Past 32 bits.
    SCI_CHECK(parse({"--seed", "4294967295"}).options.seed == 4294967295U);
    SCI_CHECK(is_usage(parse({"--log", "nothing"})));
    SCI_CHECK(is_usage(parse({"--replay"}))); // No value.
    SCI_CHECK(is_usage(parse({"--unknown"})));
    SCI_CHECK(is_usage(parse({"one", "two"}))); // Two folders.
}

SCI_TEST(the_data_folder_gives_game_and_rom_defaults) {
    const std::filesystem::path data = "/nonexistent/colonel-hd-data";
    sci::platform::Config config;
    sci::platform::Options options;
    options.game_folder = "/games/colonel";
    sci::platform::use_data_folder_defaults(config, options, data);
    SCI_CHECK(config.game_folder == std::filesystem::path("/games/colonel"));
    SCI_CHECK(config.mt32_rom == data / "rom");

    // A configured folder stays when the command line names none; a
    // configured ROM folder stays.
    sci::platform::Config configured;
    configured.game_folder = "/configured";
    configured.mt32_rom = "/roms";
    const sci::platform::Options no_folder;
    sci::platform::use_data_folder_defaults(configured, no_folder, data);
    SCI_CHECK(configured.game_folder == std::filesystem::path("/configured"));
    SCI_CHECK(configured.mt32_rom == std::filesystem::path("/roms"));
}
