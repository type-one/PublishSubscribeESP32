//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//                                                                             //
// This software is provided 'as-is', without any express or implied           //
// warranty.In no event will the authors be held liable for any damages        //
// arising from the use of this software.                                      //
//                                                                             //
// Permission is granted to anyone to use this software for any purpose,       //
// including commercial applications, and to alter itand redistribute it       //
// freely, subject to the following restrictions :                             //
//                                                                             //
// 1. The origin of this software must not be misrepresented; you must not     //
// claim that you wrote the original software.If you use this software         //
// in a product, an acknowledgment in the product documentation would be       //
// appreciated but is not required.                                            //
// 2. Altered source versions must be plainly marked as such, and must not be  //
// misrepresented as being the original software.                              //
// 3. This notice may not be removed or altered from any source distribution.  //
//-----------------------------------------------------------------------------//

/**
 * @file example_common.hpp
 * @brief Shared includes and helper utilities used by the example translation units.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <numbers>
#include <ranges>
#endif

#include "tools/platform_detection.hpp"

#include "bytepack/bytepack.hpp"
#include "cjsonpp/cjsonpp.hpp"
#include "fpm/fixed.hpp"
#include "fpm/math.hpp"

#include "tools/async_observer.hpp"
#include "tools/data_task.hpp"
#include "tools/generic_task.hpp"
#include "tools/gzip_wrapper.hpp"
#include "tools/histogram.hpp"
#include "tools/lock_free_ring_buffer.hpp"
#include "tools/logger.hpp"
#include "tools/memory_pipe.hpp"
#include "tools/non_copyable.hpp"
#include "tools/periodic_task.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/ring_buffer.hpp"
#include "tools/ring_vector.hpp"
#include "tools/sync_dictionary.hpp"
#include "tools/sync_observer.hpp"
#include "tools/sync_queue.hpp"
#include "tools/sync_ring_buffer.hpp"
#include "tools/sync_ring_vector.hpp"
#include "tools/sync_time_list.hpp"
#include "tools/time_list.hpp"
#include "tools/timer_scheduler.hpp"
#include "tools/variant_overload.hpp"
#include "tools/worker_task.hpp"

#if defined(ESP_PLATFORM)
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/queue.h>
#include <hal/gpio_types.h>
#include <sdkconfig.h>
#endif

namespace example_detail
{
    /**
     * @brief Classify current heap pressure using illustrative thresholds, not allocation guarantees.
     * @param free_percent Percentage of tracked heap bytes that are free.
     * @param fragmentation_percent Percentage of free bytes outside the largest allocatable block.
     * @return The worst severity triggered by low free space or the fragmentation estimate.
     */
    inline const char* heap_diagnostic(double free_percent, double fragmentation_percent)
    {
        constexpr double critical_free_percent = 10.0;
        constexpr double warning_free_percent = 20.0;
        constexpr double critical_fragmentation_percent = 75.0;
        constexpr double warning_fragmentation_percent = 50.0;
        if (free_percent < critical_free_percent || fragmentation_percent >= critical_fragmentation_percent)
        {
            return "critical";
        }
        if (free_percent < warning_free_percent || fragmentation_percent >= warning_fragmentation_percent)
        {
            return "warning";
        }
        return "good";
    }

#if defined(ESP_PLATFORM)
    /**
     * @brief Print an aggregate snapshot of one capability-selected heap pool.
     * @param label Human-readable pool name.
     * @param capabilities ESP-IDF heap capability mask.
     *
     * Fragmentation is an estimate: separate physical heap regions also reduce largest/free.
     * Used plus free counts tracked heap bytes, not total physical RAM or allocator metadata.
     */
    inline void print_heap_stats(const char* label, std::uint32_t capabilities)
    {
        multi_heap_info_t info {};
        heap_caps_get_info(&info, capabilities);
        const auto total_bytes = info.total_allocated_bytes + info.total_free_bytes;
        if (total_bytes == 0U)
        {
            std::printf("%s: unavailable (no matching heap)\n", label);
            return;
        }
        constexpr double percent_scale = 100.0;
        const auto free_percent
            = percent_scale * static_cast<double>(info.total_free_bytes) / static_cast<double>(total_bytes);
        std::printf("%s: used=%zu B (%.1f%%), free=%zu B, min_free=%zu B, largest=%zu B, free_blocks=%zu\n", label,
            info.total_allocated_bytes, percent_scale - free_percent, info.total_free_bytes, info.minimum_free_bytes,
            info.largest_free_block, info.free_blocks);
        if (info.total_free_bytes == 0U)
        {
            std::printf("  fragmentation estimate=n/a (exhausted), diagnostic=critical\n");
            return;
        }
        const auto fragmentation_percent = percent_scale
            * (1.0 - static_cast<double>(info.largest_free_block) / static_cast<double>(info.total_free_bytes));
        std::printf("  fragmentation estimate=%.1f%%, diagnostic=%s (heuristic)\n", fragmentation_percent,
            heap_diagnostic(free_percent, fragmentation_percent));
    }
#endif
}

/** @brief Display heap health on ESP32 and the task stack low-water mark on FreeRTOS. */
inline void print_stats()
{
#if defined(ESP_PLATFORM)
    example_detail::print_heap_stats("Internal SRAM", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#if defined(CONFIG_SPIRAM)
    example_detail::print_heap_stats("PSRAM", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    std::printf("PSRAM: disabled\n");
#endif
#endif
#if defined(FREERTOS_PLATFORM)
    UBaseType_t ux_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
    std::printf("Minimum free stack size: %d bytes\n", ux_high_water_mark);
#endif
}
