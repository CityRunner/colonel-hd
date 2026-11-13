// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/core/log.hpp"
#include "sci_test.hpp"

#include <string>
#include <string_view>

using sci::core::LogCategory;
using sci::core::LogLevel;

namespace {

struct Received {
    int count = 0;
    std::string last_message;
};

Received& received() {
    static Received state;
    return state;
}

void count_messages(LogLevel /*level*/, LogCategory /*category*/, std::string_view message) noexcept {
    ++received().count;
    received().last_message = message;
}

} // namespace

SCI_TEST(log_messages_reach_the_sink_for_enabled_categories) {
    const sci::core::LogSink previous = sci::core::set_log_sink(&count_messages);
    received() = Received{};

    sci::core::log(LogLevel::Info, LogCategory::General, "general");
    SCI_CHECK(received().count == 1 && received().last_message == "general");

    sci::core::log(LogLevel::Info, LogCategory::Resources, "off by default");
    SCI_CHECK(received().count == 1);

    sci::core::set_log_category_enabled(LogCategory::Resources, true);
    sci::core::log(LogLevel::Warning, LogCategory::Resources, "resources");
    SCI_CHECK(received().count == 2 && received().last_message == "resources");

    sci::core::set_log_category_enabled(LogCategory::Resources, false);
    sci::core::set_log_category_enabled(LogCategory::General, false); // General stays on.
    sci::core::log(LogLevel::Error, LogCategory::General, "still on");
    SCI_CHECK(received().count == 3);

    sci::core::set_log_sink(previous);
}

SCI_TEST(log_category_lists_are_parsed) {
    using sci::core::parse_log_categories;
    const auto both = parse_log_categories(" vm , resources ");
    SCI_CHECK(both.has_value() && both->size() == 2 && (*both)[0] == LogCategory::Vm &&
              (*both)[1] == LogCategory::Resources);
    for (const std::string_view none : {"", "none"}) {
        const auto empty = parse_log_categories(none);
        SCI_CHECK(empty.has_value() && empty->empty());
    }
    SCI_CHECK(!parse_log_categories("vm,general").has_value()); // General is always on, not named.
    SCI_CHECK(!parse_log_categories("vm,").has_value());
    SCI_CHECK(!parse_log_categories("graphics").has_value());
}
