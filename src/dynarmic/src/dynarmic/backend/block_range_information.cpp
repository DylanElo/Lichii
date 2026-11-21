// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/backend/block_range_information.h"

#include <algorithm>

#include <boost/icl/interval_set.hpp>
#include "common/common_types.h"
#include "common/container/unordered_set.h"

namespace Dynarmic::Backend {

template<typename P>
void BlockRangeInformation<P>::AddRange(boost::icl::discrete_interval<P> range, IR::LocationDescriptor location) {
    if (boost::icl::is_empty(range)) {
        return;
    }
    const P first = boost::icl::first(range);
    const P last = boost::icl::last(range);
    if ((first >> RegionBits) == (last >> RegionBits)) {
        regions[first >> RegionBits].push_back({static_cast<u16>(first), static_cast<u16>(last), location});
    } else {
        spanning.push_back({first, last, location});
    }
}

template<typename P>
void BlockRangeInformation<P>::ClearCache() {
    regions.clear();
    spanning.clear();
}

template<typename P>
::Common::unordered_set<IR::LocationDescriptor> BlockRangeInformation<P>::InvalidateRanges(const boost::icl::interval_set<P>& invalidated) {
    ::Common::unordered_set<IR::LocationDescriptor> erase_locations;
    for (const auto& interval : invalidated) {
        const P first = boost::icl::first(interval);
        const P last = boost::icl::last(interval);
        auto it = regions.lower_bound(first >> RegionBits);
        while (it != regions.end() && it->first <= (last >> RegionBits)) {
            const P base = it->first << RegionBits;
            std::erase_if(it->second, [&](const Range<u16>& range) {
                if (base + range.first > last || base + range.last < first) {
                    return false;
                }
                erase_locations.insert(range.location);
                return true;
            });
            if (it->second.empty()) {
                it = regions.erase(it);
            } else {
                ++it;
            }
        }
    }
    std::erase_if(spanning, [&](const Range<P>& range) {
        for (const auto& interval : invalidated) {
            if (range.first <= boost::icl::last(interval) && boost::icl::first(interval) <= range.last) {
                erase_locations.insert(range.location);
                return true;
            }
        }
        return false;
    });
    return erase_locations;
}

template class BlockRangeInformation<u32>;
template class BlockRangeInformation<u64>;

}  // namespace Dynarmic::Backend
