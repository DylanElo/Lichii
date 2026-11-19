// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan.h"

namespace Vulkan {

constexpr bool RaceColdOcarinaCompute(u64 title, VkDriverId driver, u32 device, u64 shader) {
    constexpr std::array<u64, 3> slow{
        0xce9f15d1fe2e4101ULL, 0x74ab167d83c54073ULL, 0xf12162b84e9cc190ULL};
    return title == 0x0400486024DEC000ULL && driver == VK_DRIVER_ID_NVIDIA_PROPRIETARY &&
           device == 0x1f11 && std::ranges::find(slow, shader) != slow.end();
}

inline bool BuildsUnoptimized(VkDriverId driver_id, u64 shader_hash) {
    // The Legend of Zelda: Ocarina of Time (0400486024DEC000), driver 2.0.353 on an RX 6600 XT
    static constexpr std::array<u64, 2> amd_proprietary{
        0xeff3ccbe1438679dULL, // compute shader built as the AV1 intro starts: 6 min
        0xf9ea14a22394f5b1ULL, // compute shader built as the AV1 intro starts: 18 min
    };
    return driver_id == VK_DRIVER_ID_AMD_PROPRIETARY &&
           std::ranges::find(amd_proprietary, shader_hash) != amd_proprietary.end();
}

} // namespace Vulkan
