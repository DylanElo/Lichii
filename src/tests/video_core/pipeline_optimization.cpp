// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <spirv/unified1/spirv.hpp>

#include "video_core/renderer_vulkan/pipeline_optimization.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

TEST_CASE("The short compute build is limited to the profiled RTX 2060 and OoT programs",
          "[video_core][vulkan][pipeline_optimization]") {
    constexpr u64 oot = 0x0400486024DEC000ULL;
    constexpr auto nvidia = VK_DRIVER_ID_NVIDIA_PROPRIETARY;
    constexpr u32 gpu = 0x1f11;
    for (const u64 shader : {0xce9f15d1fe2e4101ULL, 0x74ab167d83c54073ULL,
                             0xf12162b84e9cc190ULL}) {
        CHECK(Vulkan::RaceColdOcarinaCompute(oot, nvidia, gpu, shader));
        CHECK_FALSE(Vulkan::RaceColdOcarinaCompute(oot + 1, nvidia, gpu, shader));
        CHECK_FALSE(Vulkan::RaceColdOcarinaCompute(oot, VK_DRIVER_ID_MESA_RADV, gpu, shader));
        CHECK_FALSE(Vulkan::RaceColdOcarinaCompute(oot, nvidia, gpu + 1, shader));
        CHECK_FALSE(Vulkan::RaceColdOcarinaCompute(oot, nvidia, gpu, shader ^ 1));
    }
}

TEST_CASE("Temporary compute SPIR-V changes only helper inlining", "[video_core][vulkan]") {
    constexpr u32 function = (5U << 16) | spv::OpFunction;
    constexpr u32 entry = (5U << 16) | spv::OpEntryPoint;
    constexpr u32 end = (1U << 16) | spv::OpFunctionEnd;
    constexpr u32 compute = spv::ExecutionModelGLCompute;
    std::array<u32, 22> words{
        spv::MagicNumber, 0x10000, 0, 8, 0, entry, compute, 2, 0x6e69616d, 0,
        function, 1, 3, spv::FunctionControlInlineMask | spv::FunctionControlPureMask, 4, end,
        function, 1, 2, spv::FunctionControlMaskNone, 4, end};
    auto expected = words;
    expected[13] = spv::FunctionControlDontInlineMask | spv::FunctionControlPureMask;
    REQUIRE(Vulkan::DontInlineComputeHelpers(words));
    CHECK(words == expected);
    CHECK_FALSE(Vulkan::DontInlineComputeHelpers(words));
    words[6] = spv::ExecutionModelFragment;
    CHECK_FALSE(Vulkan::DontInlineComputeHelpers(words));
    words[6] = compute;
    words[10] = 0;
    CHECK_FALSE(Vulkan::DontInlineComputeHelpers(words));
    CHECK_FALSE(Vulkan::DontInlineComputeHelpers({}));
}

TEST_CASE("Only known AMD proprietary shaders disable pipeline optimization",
          "[video_core][vulkan][pipeline_optimization]") {
    constexpr std::array<u64, 2> hashes{0xeff3ccbe1438679dULL, 0xf9ea14a22394f5b1ULL};
    for (const u64 hash : hashes) {
        CHECK(Vulkan::BuildsUnoptimized(VK_DRIVER_ID_AMD_PROPRIETARY, hash));
        for (const auto driver : {VK_DRIVER_ID_AMD_OPEN_SOURCE, VK_DRIVER_ID_MESA_RADV,
                                  VK_DRIVER_ID_NVIDIA_PROPRIETARY,
                                  VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS,
                                  VK_DRIVER_ID_QUALCOMM_PROPRIETARY, VK_DRIVER_ID_MESA_TURNIP}) {
            CHECK_FALSE(Vulkan::BuildsUnoptimized(driver, hash));
        }
        CHECK_FALSE(Vulkan::BuildsUnoptimized(VK_DRIVER_ID_AMD_PROPRIETARY, hash ^ 1));
    }
    CHECK_FALSE(Vulkan::BuildsUnoptimized(VK_DRIVER_ID_AMD_PROPRIETARY, 0));
    CHECK_FALSE(Vulkan::BuildsUnoptimized(VK_DRIVER_ID_AMD_PROPRIETARY, 0x3c2193d0166610b3ULL));
    for (const u64 hash : {0x8c29514395041853ULL, 0xc332d87961d23b13ULL,
                           0xb2f024003969e2dcULL}) {
        CHECK_FALSE(Vulkan::BuildsUnoptimized(VK_DRIVER_ID_AMD_PROPRIETARY, hash));
    }
}
