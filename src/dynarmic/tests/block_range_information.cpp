// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "dynarmic/backend/block_range_information.h"

TEST_CASE("Invalidating guest code removes exactly the overlapping translations", "[jit]") {
    using Dynarmic::IR::LocationDescriptor;
    using Interval = boost::icl::discrete_interval<u64>;
    Dynarmic::Backend::BlockRangeInformation<u64> index;
    constexpr u64 boundary = 0x1'0001'0000;
    const LocationDescriptor left{1}, right{2}, crosses{3}, distant{4}, last{5};
    const auto add = [&](u64 first, u64 end, LocationDescriptor key) {
        index.AddRange(Interval::closed(first, end), key);
    };
    const auto invalidate = [&](Interval range) {
        boost::icl::interval_set<u64> intervals;
        intervals.add(range);
        return index.InvalidateRanges(intervals);
    };

    add(boundary - 8, boundary - 1, left);
    add(boundary, boundary + 7, right);
    add(boundary - 4, boundary + 3, crosses);
    add(boundary + 0x20000, boundary + 0x20004, distant);
    constexpr u64 max = std::numeric_limits<u64>::max();
    add(max - 3, max, last);

    auto removed = invalidate(Interval::right_open(boundary - 1, boundary));
    REQUIRE(removed.size() == 2);
    CHECK(removed.contains(left));
    CHECK(removed.contains(crosses));
    CHECK(invalidate(Interval::closed(boundary - 1, boundary - 1)).empty());
    CHECK(invalidate(Interval::closed(max, max)).contains(last));
    add(boundary - 4, boundary + 3, crosses);
    add(boundary - 4, boundary + 3, crosses);
    boost::icl::interval_set<u64> disjoint;
    disjoint.add(Interval::closed(boundary + 1, boundary + 1));
    disjoint.add(Interval::closed(boundary + 0x20003, boundary + 0x20004));
    removed = index.InvalidateRanges(disjoint);
    REQUIRE(removed.size() == 3);
    CHECK(removed.contains(right));
    CHECK(removed.contains(crosses));
    CHECK(removed.contains(distant));
    CHECK(index.InvalidateRanges(disjoint).empty());

    add(boundary - 1, boundary, crosses);
    add(boundary, boundary, right);
    index.ClearCache();
    CHECK(invalidate(Interval::closed(0, max)).empty());
}
