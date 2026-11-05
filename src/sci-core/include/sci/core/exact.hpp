// SPDX-License-Identifier: GPL-3.0-or-later
//
// Exact integer arithmetic for topology (spec section 5.3): checked
// operations, rationals and the orientation predicate. Every operation
// asserts that it does not overflow; callers keep their inputs in ranges
// where that holds and state the ranges.
#pragma once

#include "sci/core/assert.hpp"

#include <compare>
#include <cstdint>
#include <numeric>

namespace sci::core {

[[nodiscard]] constexpr std::int64_t checked_add(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t result = 0;
    SCI_ASSERT(!__builtin_add_overflow(a, b, &result));
    return result;
}

[[nodiscard]] constexpr std::int64_t checked_sub(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t result = 0;
    SCI_ASSERT(!__builtin_sub_overflow(a, b, &result));
    return result;
}

[[nodiscard]] constexpr std::int64_t checked_mul(std::int64_t a, std::int64_t b) noexcept {
    std::int64_t result = 0;
    SCI_ASSERT(!__builtin_mul_overflow(a, b, &result));
    return result;
}

// -1, 0 or 1.
[[nodiscard]] constexpr int sign(std::int64_t value) noexcept {
    return static_cast<int>(value > 0) - static_cast<int>(value < 0);
}

// The exact product of two 64-bit integers as a 128-bit two's complement
// value (hi, lo), portable (no compiler extension), for comparisons whose
// products do not fit 64 bits.
struct WideProduct {
    std::int64_t hi = 0;
    std::uint64_t lo = 0;

    friend constexpr bool operator==(const WideProduct&, const WideProduct&) noexcept = default;

    friend constexpr std::strong_ordering operator<=>(const WideProduct& a, const WideProduct& b) noexcept {
        if (const auto by_hi = a.hi <=> b.hi; by_hi != 0) {
            return by_hi;
        }
        return a.lo <=> b.lo;
    }
};

[[nodiscard]] constexpr WideProduct wide_mul(std::int64_t a, std::int64_t b) noexcept {
    const bool negative = (a < 0) != (b < 0);
    // Magnitudes as unsigned values (well defined for INT64_MIN too).
    const std::uint64_t ua = a < 0 ? (~static_cast<std::uint64_t>(a)) + 1 : static_cast<std::uint64_t>(a);
    const std::uint64_t ub = b < 0 ? (~static_cast<std::uint64_t>(b)) + 1 : static_cast<std::uint64_t>(b);
    constexpr std::uint64_t mask = 0xFFFFFFFFU;
    const std::uint64_t a_lo = ua & mask;
    const std::uint64_t a_hi = ua >> 32U;
    const std::uint64_t b_lo = ub & mask;
    const std::uint64_t b_hi = ub >> 32U;
    const std::uint64_t lo_lo = a_lo * b_lo;
    const std::uint64_t hi_lo = a_hi * b_lo;
    const std::uint64_t lo_hi = a_lo * b_hi;
    const std::uint64_t hi_hi = a_hi * b_hi;
    const std::uint64_t middle = (lo_lo >> 32U) + (hi_lo & mask) + (lo_hi & mask);
    std::uint64_t lo = (middle << 32U) | (lo_lo & mask);
    std::uint64_t hi = hi_hi + (hi_lo >> 32U) + (lo_hi >> 32U) + (middle >> 32U);
    if (negative) {
        lo = ~lo + 1;
        hi = ~hi + static_cast<std::uint64_t>(lo == 0);
    }
    return {.hi = static_cast<std::int64_t>(hi), .lo = lo};
}

// a * b compared with c * d, exactly: in 64 bits where every factor is
// below 2^31 in magnitude (the products then fit), else with wide_mul.
[[nodiscard]] constexpr std::strong_ordering compare_products(std::int64_t a, std::int64_t b, std::int64_t c,
                                                              std::int64_t d) noexcept {
    constexpr std::int64_t small = std::int64_t{1} << 31;
    const auto fits = [](std::int64_t v) { return v > -small && v < small; };
    if (fits(a) && fits(b) && fits(c) && fits(d)) {
        return (a * b) <=> (c * d);
    }
    return wide_mul(a, b) <=> wide_mul(c, d);
}

// A rational number num / den with den > 0, in lowest terms, so that equal
// values have equal representations.
class Rational {
public:
    constexpr Rational() noexcept = default;

    constexpr explicit Rational(std::int64_t num) noexcept : num_(num) {}

    constexpr Rational(std::int64_t num, std::int64_t den) noexcept {
        SCI_ASSERT(den != 0);
        if (den < 0) {
            num = checked_sub(0, num);
            den = checked_sub(0, den);
        }
        const std::int64_t divisor = std::gcd(num, den); // Positive, as den > 0.
        num_ = num / divisor;
        den_ = den / divisor;
    }

    [[nodiscard]] constexpr std::int64_t num() const noexcept { return num_; }

    [[nodiscard]] constexpr std::int64_t den() const noexcept { return den_; }

    friend constexpr bool operator==(const Rational&, const Rational&) noexcept = default;

    friend constexpr std::strong_ordering operator<=>(const Rational& a, const Rational& b) noexcept {
        return compare_products(a.num_, b.den_, b.num_, a.den_);
    }

    friend constexpr Rational operator+(const Rational& a, const Rational& b) noexcept {
        return {checked_add(checked_mul(a.num_, b.den_), checked_mul(b.num_, a.den_)), checked_mul(a.den_, b.den_)};
    }

    friend constexpr Rational operator-(const Rational& a, const Rational& b) noexcept {
        return {checked_sub(checked_mul(a.num_, b.den_), checked_mul(b.num_, a.den_)), checked_mul(a.den_, b.den_)};
    }

    friend constexpr Rational operator*(const Rational& a, const Rational& b) noexcept {
        return {checked_mul(a.num_, b.num_), checked_mul(a.den_, b.den_)};
    }

    friend constexpr Rational operator/(const Rational& a, const Rational& b) noexcept {
        return {checked_mul(a.num_, b.den_), checked_mul(a.den_, b.num_)};
    }

    // The value as a double, correctly rounded when num and den are below
    // 2^53 (a single IEEE division).
    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(num_) / static_cast<double>(den_);
    }

private:
    std::int64_t num_ = 0;
    std::int64_t den_ = 1;
};

// A point with integer coordinates.
struct IntPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;

    friend constexpr bool operator==(const IntPoint&, const IntPoint&) noexcept = default;
    friend constexpr auto operator<=>(const IntPoint&, const IntPoint&) noexcept = default;
};

[[nodiscard]] constexpr IntPoint operator-(IntPoint a, IntPoint b) noexcept {
    return {.x = checked_sub(a.x, b.x), .y = checked_sub(a.y, b.y)};
}

// The z component of the cross product a x b.
[[nodiscard]] constexpr std::int64_t cross(IntPoint a, IntPoint b) noexcept {
    return checked_sub(checked_mul(a.x, b.y), checked_mul(a.y, b.x));
}

[[nodiscard]] constexpr std::int64_t dot(IntPoint a, IntPoint b) noexcept {
    return checked_add(checked_mul(a.x, b.x), checked_mul(a.y, b.y));
}

// The side of c relative to the directed line a -> b: 1 if c is on the
// side the positive cross product points to, -1 on the other side, 0 if
// the three points are collinear.
[[nodiscard]] constexpr int orientation(IntPoint a, IntPoint b, IntPoint c) noexcept {
    return sign(cross(b - a, c - a));
}

// A point with rational coordinates x / den and y / den, den > 0, in lowest
// terms (the gcd of x, y and den is 1), so that equal points compare equal.
class RationalPoint {
public:
    constexpr RationalPoint() noexcept = default;

    constexpr explicit RationalPoint(IntPoint point) noexcept : x_(point.x), y_(point.y) {}

    constexpr RationalPoint(std::int64_t x, std::int64_t y, std::int64_t den) noexcept {
        SCI_ASSERT(den != 0);
        if (den < 0) {
            x = checked_sub(0, x);
            y = checked_sub(0, y);
            den = checked_sub(0, den);
        }
        const std::int64_t divisor = std::gcd(std::gcd(x, y), den);
        x_ = x / divisor;
        y_ = y / divisor;
        den_ = den / divisor;
    }

    [[nodiscard]] constexpr std::int64_t x_num() const noexcept { return x_; }

    [[nodiscard]] constexpr std::int64_t y_num() const noexcept { return y_; }

    [[nodiscard]] constexpr std::int64_t den() const noexcept { return den_; }

    [[nodiscard]] constexpr Rational x() const noexcept { return {x_, den_}; }

    [[nodiscard]] constexpr Rational y() const noexcept { return {y_, den_}; }

    [[nodiscard]] constexpr bool is_integer() const noexcept { return den_ == 1; }

    friend constexpr bool operator==(const RationalPoint&, const RationalPoint&) noexcept = default;

    // Ordered by x, then y.
    friend constexpr std::strong_ordering operator<=>(const RationalPoint& a, const RationalPoint& b) noexcept {
        if (const auto by_x = compare_products(a.x_, b.den_, b.x_, a.den_); by_x != 0) {
            return by_x;
        }
        return compare_products(a.y_, b.den_, b.y_, a.den_);
    }

private:
    std::int64_t x_ = 0;
    std::int64_t y_ = 0;
    std::int64_t den_ = 1;
};

// The side of the rational point c relative to the directed line a -> b
// through integer points, as for orientation().
[[nodiscard]] constexpr int orientation(IntPoint a, IntPoint b, const RationalPoint& c) noexcept {
    const IntPoint direction = b - a;
    // cross(direction, c - a) scaled by c.den() > 0, which keeps the sign.
    const std::int64_t dx = checked_sub(c.x_num(), checked_mul(a.x, c.den()));
    const std::int64_t dy = checked_sub(c.y_num(), checked_mul(a.y, c.den()));
    return sign(checked_sub(checked_mul(direction.x, dy), checked_mul(direction.y, dx)));
}

} // namespace sci::core
