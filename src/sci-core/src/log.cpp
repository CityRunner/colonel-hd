// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/core/log.hpp"

#include <atomic>

namespace sci::core {

namespace {

constexpr std::uint32_t category_bit(LogCategory category) noexcept {
    return 1U << static_cast<std::uint32_t>(category);
}

std::atomic<LogSink>& log_sink() noexcept {
    static std::atomic<LogSink> sink{nullptr};
    return sink;
}

std::atomic<std::uint32_t>& enabled_categories() noexcept {
    static std::atomic<std::uint32_t> mask{category_bit(LogCategory::General)};
    return mask;
}

} // namespace

LogSink set_log_sink(LogSink sink) noexcept {
    return log_sink().exchange(sink);
}

void set_log_category_enabled(LogCategory category, bool enabled) noexcept {
    if (category == LogCategory::General) {
        return;
    }
    if (enabled) {
        enabled_categories().fetch_or(category_bit(category));
    } else {
        enabled_categories().fetch_and(~category_bit(category));
    }
}

bool log_category_enabled(LogCategory category) noexcept {
    return (enabled_categories().load() & category_bit(category)) != 0;
}

void log(LogLevel level, LogCategory category, std::string_view message) noexcept {
    const LogSink sink = log_sink().load();
    if (sink != nullptr && log_category_enabled(category)) {
        sink(level, category, message);
    }
}

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Error:
        return "error";
    case LogLevel::Warning:
        return "warning";
    case LogLevel::Info:
        return "info";
    case LogLevel::Debug:
        return "debug";
    }
    return "unknown";
}

std::string_view to_string(LogCategory category) noexcept {
    switch (category) {
    case LogCategory::General:
        return "general";
    case LogCategory::Resources:
        return "resources";
    case LogCategory::Vm:
        return "vm";
    }
    return "unknown";
}

std::optional<std::vector<LogCategory>> parse_log_categories(std::string_view list) {
    const auto trim = [](std::string_view text) {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
            text.remove_prefix(1);
        }
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
            text.remove_suffix(1);
        }
        return text;
    };
    std::vector<LogCategory> categories;
    list = trim(list);
    if (list.empty() || list == "none") {
        return categories;
    }
    while (true) {
        const auto comma = list.find(',');
        const std::string_view name = trim(list.substr(0, comma));
        if (name == to_string(LogCategory::Resources)) {
            categories.push_back(LogCategory::Resources);
        } else if (name == to_string(LogCategory::Vm)) {
            categories.push_back(LogCategory::Vm);
        } else {
            return std::nullopt;
        }
        if (comma == std::string_view::npos) {
            return categories;
        }
        list.remove_prefix(comma + 1);
    }
}

} // namespace sci::core
