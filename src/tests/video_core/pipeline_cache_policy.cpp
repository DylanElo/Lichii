// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>

#include <catch2/catch_test_macros.hpp>

#include "video_core/pipeline_cache_policy.h"

using namespace VideoCommon;

TEST_CASE("RTX 2060 keeps costly OoT graphics prebuilds within the existing budget",
          "[video_core][pipeline_cache]") {
    // This worker build took 6,665 ms during a held OoT draw on the RTX 2060.
    constexpr u64 slow = 0x720f4634f0ac74deULL;
    CHECK(IsSlowOot2060GraphicsKey(slow));
    CHECK_FALSE(IsSlowOot2060GraphicsKey(0));
    std::array<u64, 3> keys{0, 1, slow};
    auto evict = GraphicsPrebuildsOverBudget(std::span<u64>{keys}, 1, IsSlowOot2060GraphicsKey);
    REQUIRE(evict.size() == 2);
    CHECK(keys.front() == slow);
    CHECK(GraphicsPrebuildsOverBudget(std::span<u64>{keys}, 0, IsSlowOot2060GraphicsKey).size() ==
          keys.size());
    CHECK(GraphicsPrebuildsOverBudget(std::span<u64>{keys}, keys.size(), IsSlowOot2060GraphicsKey)
              .empty());
    keys = {slow, 0x41802ca89a8918f4ULL, 1};
    evict = GraphicsPrebuildsOverBudget(std::span<u64>{keys}, 1, IsSlowOot2060GraphicsKey);
    CHECK(std::ranges::count_if(evict, IsSlowOot2060GraphicsKey) == 1);
}

TEST_CASE("RTX 2060 boot reserves slots for costly and recently used states",
          "[video_core][pipeline_cache]") {
    // Visit old records first and costly records last to exercise selection across file order.
    const auto select = [](std::array<size_t, 4> counts, size_t limit, size_t fallback = 3000) {
        constexpr std::array<u8, 4> ages{4, 1, 0, 3};
        RecentGraphicsBootBudget budget;
        for (size_t group = 0; group < counts.size(); ++group) {
            for (size_t i = 0; i < counts[group]; ++i) {
                budget.Count(group == 3, ages[group]);
            }
        }
        budget.SetLimit(limit, fallback);
        std::array<size_t, 4> taken{};
        for (size_t group = 0; group < counts.size(); ++group) {
            for (size_t i = 0; i < counts[group]; ++i) {
                taken[group] += budget.Take(group == 3, ages[group]);
            }
        }
        return taken;
    };
    // Frozen benchmark cache and the player's larger recent history on the same 16 GiB host.
    CHECK((select({7404, 508, 975, 17}, 1751) == std::array<size_t, 4>{0, 508, 975, 17}));
    CHECK((select({2454, 3488, 2934, 18}, 1751) == std::array<size_t, 4>{0, 0, 1733, 18}));
    CHECK((select({40, 0, 0, 2}, 16, 10) == std::array<size_t, 4>{8, 0, 0, 2}));
    CHECK((select({40, 0, 0, 2}, 1) == std::array<size_t, 4>{0, 0, 0, 1}));
    CHECK((select({40, 1, 1, 2}, 0) == std::array<size_t, 4>{}));

    RecentGraphicsBootBudget only_slow_recent;
    only_slow_recent.Count(false, 5);
    only_slow_recent.Count(true, 0);
    only_slow_recent.SetLimit(10, 3000);
    CHECK_FALSE(only_slow_recent.Take(false, 5));
    CHECK(only_slow_recent.Take(true, 0));
}

TEST_CASE("Graphics pipelines are evicted only past the limit", "[video_core][pipeline_cache]") {
    SECTION("within the limit nothing is evicted, however stale") {
        CHECK(GraphicsPipelinesToEvict(0, 0, 100) == 0);
        CHECK(GraphicsPipelinesToEvict(100, 100, 100) == 0);
        CHECK(GraphicsPipelinesToEvict(50, 50, 100) == 0);
    }
    SECTION("past the limit only the excess is evicted") {
        CHECK(GraphicsPipelinesToEvict(101, 101, 100) == 1);
        CHECK(GraphicsPipelinesToEvict(150, 80, 100) == 50);
    }
    SECTION("pipelines used recently are kept even past the limit") {
        CHECK(GraphicsPipelinesToEvict(150, 20, 100) == 20);
        CHECK(GraphicsPipelinesToEvict(150, 0, 100) == 0);
    }
    SECTION("a zero limit evicts every stale pipeline") {
        CHECK(GraphicsPipelinesToEvict(10, 7, 0) == 7);
    }
}

TEST_CASE("Vertex attribute mask follows the header's input map", "[video_core][pipeline_cache]") {
    std::array<std::array<bool, 4>, 32> inputs{};
    CHECK(VertexAttributeMask(inputs) == 0);

    inputs[0] = {true, false, false, false};
    inputs[3] = {false, false, false, true};
    inputs[31] = {false, true, true, false};
    CHECK(VertexAttributeMask(inputs) == ((u64{3} << 0) | (u64{3} << 6) | (u64{3} << 62)));

    inputs.fill({true, true, true, true});
    CHECK(VertexAttributeMask(inputs) == ~u64{0});
}

TEST_CASE("Pipeline keys keep only the attribute types active stages read",
          "[video_core][pipeline_cache]") {
    constexpr u64 types = 0x1234'5678'9abc'def0;
    constexpr u64 first_four = 0xff;

    SECTION("a VertexA program keeps every type") {
        const std::array<u64, 6> hashes{1, 2, 0, 0, 0, 3};
        const std::array<u64, 6> masks{0, 0, 0, 0, 0, 0};
        CHECK(KeyedAttributeTypes(types, hashes, masks) == types);
    }
    SECTION("the vertex program's input map masks the types") {
        const std::array<u64, 6> hashes{0, 2, 0, 0, 0, 3};
        const std::array<u64, 6> masks{0, first_four, 0, 0, 0, ~u64{0}};
        CHECK(KeyedAttributeTypes(types, hashes, masks) == (types & first_four));
    }
    SECTION("inactive stages do not mask") {
        const std::array<u64, 6> hashes{0, 2, 0, 0, 0, 0};
        const std::array<u64, 6> masks{0, ~u64{0}, 0, 0, 0, 0};
        CHECK(KeyedAttributeTypes(types, hashes, masks) == types);
    }
    SECTION("task and mesh programs drop every type") {
        const std::array<u64, 6> hashes{0, 7, 0, 8, 0, 3};
        const std::array<u64, 6> masks{0, 0, 0, 0, 0, ~u64{0}};
        CHECK(KeyedAttributeTypes(types, hashes, masks) == 0);
    }
    SECTION("garbage in unread registers no longer changes the key") {
        const std::array<u64, 6> hashes{0, 2, 0, 0, 0, 3};
        const std::array<u64, 6> masks{0, first_four, 0, 0, 0, ~u64{0}};
        const u64 other = (types & first_four) | 0xabcd'0000'0000'0000;
        CHECK(KeyedAttributeTypes(types, hashes, masks) ==
              KeyedAttributeTypes(other, hashes, masks));
    }
    SECTION("masking a stored key again changes nothing") {
        const std::array<u64, 6> hashes{0, 2, 0, 0, 0, 3};
        const std::array<u64, 6> masks{0, first_four, 0, 0, 0, ~u64{0}};
        const u64 once = KeyedAttributeTypes(types, hashes, masks);
        CHECK(KeyedAttributeTypes(once, hashes, masks) == once);
    }
}
