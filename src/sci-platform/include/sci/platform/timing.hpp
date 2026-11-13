// SPDX-License-Identifier: GPL-3.0-or-later
//
// The program's timing mode (B-10, `colonel-hd --timing <csv> --replay
// <file>`): what a run measured, and the CSV it writes
// (docs/timing.md). Apart from SDL so CTest can check the arithmetic and
// the format.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sci::platform {

// A frame time over which a 60 Hz display misses a frame (spec 10.3).
inline constexpr double frame_budget_ms = 1000.0 / 60.0;

struct TimeStats {
    std::size_t count = 0;
    double mean = 0;
    double median = 0;
    double p90 = 0;
    double p99 = 0;
    double worst = 0;
    std::size_t over_budget = 0; // Values over frame_budget_ms.
};

// Nearest-rank percentiles of `values` (milliseconds); all zero if empty.
[[nodiscard]] TimeStats time_stats(std::vector<double> values);

struct RoomEntry {
    int room = 0;  // Global 11, the room number, when the entry began.
    double ms = 0; // From the new picture's request to its HD frame.
};

class TimingLog {
public:
    // The analysis before the game: the scenes analysed (0 with a warm
    // cache) and how long it took.
    void set_first_launch(std::size_t scenes, double ms);

    // The main thread's time for one presented frame: composing it,
    // uploading the changed rectangles and presenting; and of that, the
    // composing.
    void frame(double ms, double compose_ms) {
        frames_.push_back(ms);
        composes_.push_back(compose_ms);
    }

    void room_entry(int room, double ms) { rooms_.push_back(RoomEntry{.room = room, .ms = ms}); }

    [[nodiscard]] const std::vector<double>& frames() const { return frames_; }

    [[nodiscard]] const std::vector<RoomEntry>& rooms() const { return rooms_; }

    // The CSV of the run: "key,value" lines, then one "room,<room>,<ms>"
    // line per room entry in order.
    [[nodiscard]] std::string csv(std::string_view version, int scale, std::optional<std::uint64_t> peak_kib) const;

private:
    std::size_t scenes_ = 0;
    double first_launch_ms_ = 0;
    std::vector<double> frames_;
    std::vector<double> composes_;
    std::vector<RoomEntry> rooms_;
};

// The process's peak resident memory so far, in KiB (getrusage on Linux,
// the peak working set on Windows); nothing where it cannot be read.
[[nodiscard]] std::optional<std::uint64_t> peak_memory_kib();

} // namespace sci::platform
