// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "common/common_types.h"

namespace VideoCommon {

/// How many graphics pipelines to evict, least recently used first.
/// @param cached Graphics pipelines in the cache.
/// @param stale  Pipelines among them unused for long enough to be evicted.
/// @param limit  Pipelines kept before any is evicted.
/// @return Zero within the limit, else the excess, at most `stale`.
constexpr size_t GraphicsPipelinesToEvict(size_t cached, size_t stale, size_t limit) {
    if (cached <= limit) {
        return 0;
    }
    return (std::min)(stale, cached - limit);
}

// Full graphics state keys whose builds took at least a second during three OoT frame stalls
// on the RTX 2060. Only the existing records for these keys can be replayed at boot.
constexpr bool IsSlowOot2060GraphicsKey(u64 hash) {
    constexpr std::array<u64, 18> slow{
        0x09c28e46e2e0ea1fULL, 0x35aebd45ca79c895ULL, 0x3913fd31bf7ff84bULL,
        0x3c0901e1b88120ffULL, 0x41802ca89a8918f4ULL, 0x602d4915a15a27d0ULL,
        0x67c5a5a588e317fbULL, 0x6cd2af8515c329d0ULL, 0x720f4634f0ac74deULL,
        0x765cffd97abe7e49ULL, 0x8066f93ee1895cdcULL, 0x8db6ed93d41531f0ULL,
        0xa0309ab0d1f493ebULL, 0xaae9d246871df463ULL, 0xc3d160e1b65d58b3ULL,
        0xe296a4668dfc7f38ULL, 0xfc3df13f1a0bf681ULL, 0xfebcf12fcfa6107bULL,
    };
    static_assert(std::ranges::is_sorted(slow));
    return std::ranges::binary_search(slow, hash);
}

// Fill existing retention slots with expensive prebuilds first. The remainder may be evicted.
template <typename T, typename Predicate>
constexpr std::span<T> GraphicsPrebuildsOverBudget(std::span<T> prebuilt, size_t keep,
                                                   Predicate expensive) {
    if (prebuilt.size() <= keep) {
        return {};
    }
    if (keep != 0) {
        std::ranges::partition(prebuilt, expensive);
    }
    return prebuilt.subspan(keep);
}

/// Attribute mask of a native vertex program, two bits per attribute as in
/// FixedPipelineState::attribute_types.
/// @param input_generic Components of each generic attribute in the program header's input map.
constexpr u64 VertexAttributeMask(std::span<const std::array<bool, 4>> input_generic) {
    u64 mask = 0;
    for (size_t index = 0; index < input_generic.size(); ++index) {
        if (std::ranges::any_of(input_generic[index], [](bool component) { return component; })) {
            mask |= u64{3} << (index * 2);
        }
    }
    return mask;
}

/// Vertex attribute types a graphics pipeline key keeps.
/// @param attribute_types Types of every attribute register.
/// @param unique_hashes   Program hash of each stage, zero when the stage is off.
/// @param attribute_masks ShaderInfo::vertex_attribute_mask of each stage.
/// @return The types masked by every active stage, or all of them when VertexA is on.
constexpr u64 KeyedAttributeTypes(u64 attribute_types, std::span<const u64> unique_hashes,
                                  std::span<const u64> attribute_masks) {
    if (unique_hashes.empty() || unique_hashes[0] != 0) {
        return attribute_types;
    }
    for (size_t stage = 1; stage < unique_hashes.size(); ++stage) {
        if (unique_hashes[stage] != 0) {
            attribute_types &= attribute_masks[stage];
        }
    }
    return attribute_types;
}

} // namespace VideoCommon
