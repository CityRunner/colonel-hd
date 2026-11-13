// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/platform/config.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

namespace sci::platform {

namespace {

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

// A value without a trailing comment.
std::string_view value_of(std::string_view text) {
    const auto comment = text.find_first_of(";#");
    return trim(comment == std::string_view::npos ? text : text.substr(0, comment));
}

std::optional<int> parse_int(std::string_view text) {
    int value = 0;
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), std::to_address(text.end()), value);
    if (error != std::errc{} || end != std::to_address(text.end())) {
        return std::nullopt;
    }
    return value;
}

// Sets the music or the volume; the problem, if the value is not valid.
std::optional<std::string> apply_audio(Config& config, std::string_view key, std::string_view value) {
    if (key == "music") {
        if (value == "adlib") {
            config.music = MusicDevice::Adlib;
        } else if (value == "mt32") {
            config.music = MusicDevice::Mt32;
        } else if (value == "off") {
            config.music = MusicDevice::Off;
        } else {
            return "music is adlib, mt32 or off";
        }
        return std::nullopt;
    }
    const auto volume = parse_int(value);
    if (!volume || *volume < 0 || *volume > max_volume) {
        return std::format("volume is 0 to {}", max_volume);
    }
    config.volume = *volume;
    return std::nullopt;
}

// Sets one key; the problem, if the value is not valid.
std::optional<std::string> apply(ParsedConfig& parsed, std::string_view key, std::string_view value) {
    Config& config = parsed.config;
    const auto to_path = [](std::string_view text) {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    };
    if (key == "format") {
        if (parse_int(value) != config_format) {
            return std::format("format {} is not supported (this program reads format {})", value, config_format);
        }
    } else if (key == "game_folder") {
        config.game_folder = to_path(value);
    } else if (key == "mt32_rom") {
        config.mt32_rom = to_path(value);
    } else if (key == "saves") {
        config.saves = to_path(value);
    } else if (key == "mode") {
        if (value != "hd" && value != "original") {
            return "mode is hd or original";
        }
        config.mode = value == "hd" ? GraphicsMode::Hd : GraphicsMode::Original;
    } else if (key == "scale") {
        const auto scale = value == "auto" ? std::optional<int>(0) : parse_int(value);
        if (!scale || *scale < 0 || *scale > max_scale) {
            return std::format("scale is auto or 1 to {}", max_scale);
        }
        config.scale = *scale;
    } else if (key == "music" || key == "volume") {
        return apply_audio(config, key, value);
    } else if (key == "log") {
        auto categories = core::parse_log_categories(value);
        if (!categories) {
            return "log is a comma-separated list of vm and resources, or empty";
        }
        config.log = std::move(*categories);
    } else if (key == "priority_snap") {
        if (value != "on" && value != "off") {
            return "priority_snap is on or off";
        }
        config.priority_snap = value == "on";
    } else {
        parsed.warnings.push_back(std::format("configuration: unknown key {} ignored", key));
    }
    return std::nullopt;
}

} // namespace

core::Result<ParsedConfig> parse_config(std::string_view text) {
    ParsedConfig parsed;
    bool in_section = false;
    bool has_format = false;
    int line_number = 0;
    while (!text.empty()) {
        const auto end = text.find('\n');
        const std::string_view line = trim(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        ++line_number;
        const auto fail = [&](std::string_view what) {
            return core::make_error(core::ErrorCode::InvalidData,
                                    std::format("configuration line {}: {}", line_number, what));
        };
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[') {
            in_section = line == "[colonel-hd]";
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) {
            return fail("'key = value' expected");
        }
        if (!in_section) {
            continue;
        }
        const std::string_view key = trim(line.substr(0, equals));
        const std::string_view raw = trim(line.substr(equals + 1));
        // Paths may contain ';' and '#'; only other values take comments.
        const bool is_path = key == "game_folder" || key == "mt32_rom" || key == "saves";
        const std::string_view value = is_path ? raw : value_of(raw);
        if (auto problem = apply(parsed, key, value)) {
            return fail(*problem);
        }
        has_format = has_format || key == "format";
    }
    if (!has_format) {
        return core::make_error(core::ErrorCode::InvalidData, "configuration: the format line is missing");
    }
    return parsed;
}

namespace {

std::string_view music_name(MusicDevice device) {
    switch (device) {
    case MusicDevice::Adlib:
        return "adlib";
    case MusicDevice::Mt32:
        return "mt32";
    case MusicDevice::Off:
        return "off";
    }
    return "adlib";
}

// The categories as the log key writes them: " vm,resources", or nothing.
std::string log_names(const std::vector<core::LogCategory>& categories) {
    std::string text;
    for (const core::LogCategory category : categories) {
        text += text.empty() ? " " : ",";
        text += core::to_string(category);
    }
    return text;
}

} // namespace

std::string format_config(const Config& config) {
    const auto path = [](const std::filesystem::path& p) {
        const std::u8string text = p.u8string();
        return text.empty() ? std::string() : " " + std::string(text.begin(), text.end());
    };
    return std::format("; colonel-hd configuration\n"
                       "[colonel-hd]\n"
                       "format = {}\n"
                       "game_folder ={}\n"
                       "; hd or original\n"
                       "mode = {}\n"
                       "; auto (the largest N that fits), or N from 1 to {}\n"
                       "scale = {}\n"
                       "; on: sprites pass behind the picture in whole 1x pixels, as in the original\n"
                       "priority_snap = {}\n"
                       "; folder of the MT-32 ROMs\n"
                       "mt32_rom ={}\n"
                       "; music device: adlib, mt32 (needs the ROMs, otherwise adlib) or off\n"
                       "music = {}\n"
                       "; volume in per cent, 0 to {}\n"
                       "volume = {}\n"
                       "; folder of the saved games (empty: saves next to this file)\n"
                       "saves ={}\n"
                       "; more messages on the console: vm, resources, or both (comma-separated); empty: none\n"
                       "log ={}\n",
                       config_format, path(config.game_folder), config.mode == GraphicsMode::Hd ? "hd" : "original",
                       max_scale, config.scale == 0 ? std::string("auto") : std::to_string(config.scale),
                       config.priority_snap ? "on" : "off", path(config.mt32_rom), music_name(config.music), max_volume,
                       config.volume, path(config.saves), log_names(config.log));
}

namespace {

// Every key's value as the file writes it, in format_config's order.
std::vector<std::pair<std::string_view, std::string>> key_values(const Config& config) {
    const auto path = [](const std::filesystem::path& p) {
        const std::u8string text = p.u8string();
        return std::string(text.begin(), text.end());
    };
    std::string log = log_names(config.log);
    if (!log.empty()) {
        log.erase(0, 1); // log_names puts a space first.
    }
    return {
        {"game_folder", path(config.game_folder)},
        {"mode", config.mode == GraphicsMode::Hd ? "hd" : "original"},
        {"scale", config.scale == 0 ? std::string("auto") : std::to_string(config.scale)},
        {"priority_snap", config.priority_snap ? "on" : "off"},
        {"mt32_rom", path(config.mt32_rom)},
        {"music", std::string(music_name(config.music))},
        {"volume", std::to_string(config.volume)},
        {"saves", path(config.saves)},
        {"log", log},
    };
}

bool is_path_key(std::string_view key) {
    return key == "game_folder" || key == "mt32_rom" || key == "saves";
}

// The line with the key's value replaced; a trailing comment of a value
// that takes comments stays (with the spaces before it).
std::string replace_value(std::string_view line, std::string_view key, const std::string& value) {
    const auto equals = line.find('=');
    std::string out(line.substr(0, equals + 1));
    if (!value.empty()) {
        out += " " + value;
    }
    if (!is_path_key(key)) {
        const std::string_view rest = line.substr(equals + 1);
        if (const auto comment = rest.find_first_of(";#"); comment != std::string_view::npos) {
            const auto space = rest.find_last_not_of(" \t", comment == 0 ? 0 : comment - 1);
            const std::size_t from = space == std::string_view::npos || comment == 0 ? comment : space + 1;
            out += rest.substr(from);
        }
    }
    return out;
}

} // namespace

std::string update_config(std::string_view text, const Config& before, const Config& after) {
    if (text.empty()) {
        return format_config(after);
    }
    const auto old_values = key_values(before);
    const auto new_values = key_values(after);
    std::vector<std::pair<std::string_view, std::string>> changed;
    for (std::size_t i = 0; i < new_values.size(); ++i) {
        if (old_values[i].second != new_values[i].second) {
            changed.push_back(new_values[i]);
        }
    }
    if (changed.empty()) {
        return std::string(text);
    }
    const bool crlf = text.find("\r\n") != std::string_view::npos;
    const std::string_view newline = crlf ? "\r\n" : "\n";
    std::vector<bool> written(changed.size(), false);
    std::string out;
    bool in_section = false;
    std::size_t section_end = std::string::npos; // Where missing keys go.
    std::string_view rest = text;
    while (!rest.empty()) {
        const auto end = rest.find('\n');
        std::string_view line = rest.substr(0, end == std::string_view::npos ? rest.size() : end + 1);
        rest.remove_prefix(line.size());
        std::string_view ending;
        if (line.ends_with("\r\n")) {
            ending = "\r\n";
        } else if (line.ends_with('\n')) {
            ending = "\n";
        }
        line.remove_suffix(ending.size());
        const std::string_view trimmed = trim(line);
        if (!trimmed.empty() && trimmed.front() == '[') {
            if (in_section) {
                section_end = out.size();
            }
            in_section = trimmed == "[colonel-hd]";
        } else if (in_section && !trimmed.empty() && trimmed.front() != ';' && trimmed.front() != '#') {
            const auto equals = trimmed.find('=');
            if (equals != std::string_view::npos) {
                const std::string_view key = trim(trimmed.substr(0, equals));
                for (std::size_t i = 0; i < changed.size(); ++i) {
                    if (changed[i].first == key) {
                        out += replace_value(line, key, changed[i].second);
                        out += ending;
                        written[i] = true;
                        line = {};
                        ending = {};
                        break;
                    }
                }
                if (line.empty() && ending.empty()) {
                    continue;
                }
            }
        }
        out += line;
        out += ending;
    }
    if (in_section) {
        section_end = out.size();
    }
    std::string missing;
    for (std::size_t i = 0; i < changed.size(); ++i) {
        if (!written[i]) {
            missing += std::string(changed[i].first) + " =" +
                       (changed[i].second.empty() ? "" : " " + changed[i].second) + std::string(newline);
        }
    }
    if (!missing.empty()) {
        if (section_end == std::string::npos) {
            // No [colonel-hd] section: parse_config would have refused the
            // file (no format line); write it whole.
            return format_config(after);
        }
        if (section_end > 0 && out[section_end - 1] != '\n') {
            missing.insert(0, newline);
        }
        out.insert(section_end, missing);
    }
    return out;
}

bool write_config_file(const std::filesystem::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary);
    file << text;
    file.close();
    if (!file) {
        core::log(core::LogLevel::Warning, core::LogCategory::General,
                  std::format("Cannot write the configuration {}", path.string()));
        return false;
    }
    return true;
}

std::filesystem::path saves_folder(const Config& config, const std::filesystem::path& config_folder) {
    if (config.saves.empty()) {
        return config_folder / "saves";
    }
    return config.saves.is_absolute() ? config.saves : config_folder / config.saves;
}

} // namespace sci::platform
