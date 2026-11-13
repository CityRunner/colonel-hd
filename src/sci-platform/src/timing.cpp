// SPDX-License-Identifier: GPL-3.0-or-later

#include "sci/platform/timing.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>

#ifdef _WIN32
#include <windows.h>
// Needs windows.h first.
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace sci::platform {

namespace {

// The nearest-rank percentile p (0 to 100) of sorted values.
double percentile(const std::vector<double>& sorted, double p) {
    const auto rank = static_cast<std::size_t>(std::ceil(p / 100.0 * static_cast<double>(sorted.size())));
    return sorted[std::clamp<std::size_t>(rank, 1, sorted.size()) - 1];
}

void line(std::string& out, std::string_view key, double value) {
    out += std::format("{},{:.3f}\n", key, value);
}

void line(std::string& out, std::string_view key, std::uint64_t value) {
    out += std::format("{},{}\n", key, value);
}

} // namespace

TimeStats time_stats(std::vector<double> values) {
    TimeStats stats;
    if (values.empty()) {
        return stats;
    }
    std::ranges::stable_sort(values); // Equal values are equal: the order among them does not matter.
    stats.count = values.size();
    stats.mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    stats.median = percentile(values, 50);
    stats.p90 = percentile(values, 90);
    stats.p99 = percentile(values, 99);
    stats.worst = values.back();
    stats.over_budget =
        static_cast<std::size_t>(std::ranges::count_if(values, [](double value) { return value > frame_budget_ms; }));
    return stats;
}

void TimingLog::set_first_launch(std::size_t scenes, double ms) {
    scenes_ = scenes;
    first_launch_ms_ = ms;
}

std::string TimingLog::csv(std::string_view version, int scale, std::optional<std::uint64_t> peak_kib) const {
    std::string out = "colonel-hd timing,1\n";
    out += std::format("version,{}\n", version);
    line(out, "scale", static_cast<std::uint64_t>(scale));
    line(out, "first_launch_scenes", static_cast<std::uint64_t>(scenes_));
    line(out, "first_launch_ms", first_launch_ms_);
    const TimeStats frames = time_stats(frames_);
    line(out, "frames", static_cast<std::uint64_t>(frames.count));
    line(out, "frame_ms_mean", frames.mean);
    line(out, "frame_ms_median", frames.median);
    line(out, "frame_ms_p90", frames.p90);
    line(out, "frame_ms_p99", frames.p99);
    line(out, "frame_ms_worst", frames.worst);
    line(out, "frames_over_16.7ms", static_cast<std::uint64_t>(frames.over_budget));
    const TimeStats composes = time_stats(composes_);
    line(out, "compose_ms_median", composes.median);
    line(out, "compose_ms_p99", composes.p99);
    line(out, "compose_ms_worst", composes.worst);
    std::vector<double> entries;
    entries.reserve(rooms_.size());
    for (const RoomEntry& room : rooms_) {
        entries.push_back(room.ms);
    }
    const TimeStats rooms = time_stats(entries);
    line(out, "room_entries", static_cast<std::uint64_t>(rooms.count));
    line(out, "room_entry_ms_median", rooms.median);
    line(out, "room_entry_ms_worst", rooms.worst);
    if (peak_kib) {
        line(out, "peak_memory_kib", *peak_kib);
    }
    for (const RoomEntry& room : rooms_) {
        out += std::format("room,{},{:.3f}\n", room.room, room.ms);
    }
    return out;
}

std::optional<std::uint64_t> peak_memory_kib() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(counters.PeakWorkingSetSize / 1024);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access): glibc declares the field in a union.
    return static_cast<std::uint64_t>(usage.ru_maxrss); // KiB on Linux.
#endif
}

} // namespace sci::platform
