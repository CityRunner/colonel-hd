// SPDX-License-Identifier: GPL-3.0-or-later
//
// The timing mode's statistics and CSV (B-10).

#include "sci/platform/timing.hpp"
#include "sci_test.hpp"

#include <string>
#include <vector>

SCI_TEST(time_stats_are_nearest_rank) {
    std::vector<double> values;
    for (int i = 100; i >= 1; --i) {
        values.push_back(static_cast<double>(i));
    }
    const sci::platform::TimeStats stats = sci::platform::time_stats(values);
    SCI_CHECK(stats.count == 100);
    SCI_CHECK(stats.median == 50.0);
    SCI_CHECK(stats.p90 == 90.0);
    SCI_CHECK(stats.p99 == 99.0);
    SCI_CHECK(stats.worst == 100.0);
    SCI_CHECK(stats.mean == 50.5);
    SCI_CHECK(stats.over_budget == 84); // 17 to 100.
    const sci::platform::TimeStats none = sci::platform::time_stats({});
    SCI_CHECK(none.count == 0 && none.worst == 0.0);
    const sci::platform::TimeStats one = sci::platform::time_stats({3.0});
    SCI_CHECK(one.median == 3.0 && one.p99 == 3.0);
}

SCI_TEST(the_csv_holds_every_value_and_the_rooms_in_order) {
    sci::platform::TimingLog log;
    log.set_first_launch(176, 1824.5);
    log.frame(1.0, 0.5);
    log.frame(2.0, 1.5);
    log.frame(20.0, 12.0);
    log.room_entry(1, 150.25);
    log.room_entry(7, 90.0);
    log.slow_frame(
        sci::platform::SlowFrame{.tick = 1234, .room = 7, .ms = 20.0, .compose_ms = 12.0, .uploaded = 64000});
    const std::string csv = log.csv("1.0.0", 10, 210924);
    SCI_CHECK(csv.starts_with("colonel-hd timing,1\nversion,1.0.0\nscale,10\n"));
    SCI_CHECK(csv.contains("first_launch_scenes,176\nfirst_launch_ms,1824.500\n"));
    SCI_CHECK(csv.contains("frames,3\n"));
    SCI_CHECK(csv.contains("frame_ms_median,2.000\n"));
    SCI_CHECK(csv.contains("frame_ms_worst,20.000\n"));
    SCI_CHECK(csv.contains("frames_over_16.7ms,1\n"));
    SCI_CHECK(csv.contains("compose_ms_median,1.500\ncompose_ms_p99,12.000\ncompose_ms_worst,12.000\n"));
    SCI_CHECK(csv.contains("room_entries,2\nroom_entry_ms_median,90.000\nroom_entry_ms_worst,150.250\n"));
    SCI_CHECK(csv.contains("peak_memory_kib,210924\n"));
    SCI_CHECK(csv.ends_with("room,1,150.250\nroom,7,90.000\nslow,1234,7,20.000,12.000,64000\n"));
    SCI_CHECK(!log.csv("1.0.0", 1, std::nullopt).contains("peak_memory_kib"));
}

SCI_TEST(peak_memory_is_read) {
    const auto peak = sci::platform::peak_memory_kib();
    SCI_CHECK(peak.has_value() && *peak > 0);
}
