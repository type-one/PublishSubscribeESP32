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
#include "tools/timer_scheduler.hpp"
#include "tools/variant_overload.hpp"
#include "tools/worker_task.hpp"

#if defined(ESP_PLATFORM)
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_system.h>
#include <freertos/queue.h>
#include <hal/gpio_types.h>
#include <sdkconfig.h>
#endif

inline void print_stats()
{
#if defined(ESP_PLATFORM)
    std::printf("Current free heap size: %" PRIu32 " bytes\n", esp_get_free_heap_size());
    std::printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
#endif
#if defined(FREERTOS_PLATFORM)
    UBaseType_t ux_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
    std::printf("Minimum free stack size: %d bytes\n", ux_high_water_mark);
#endif
}
