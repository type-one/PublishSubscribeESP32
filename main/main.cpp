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

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
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

#if defined(CPP_EXCEPTIONS_ENABLED)
#include <exception>
#endif

#include "CException/CException.h"
#include "bytepack/bytepack.hpp"
#include "cjsonpp/cjsonpp.hpp"

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

//--------------------------------------------------------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer()
{
    LOG_INFO("-- ring buffer --");
    print_stats();

    constexpr const std::size_t queue_size = 64U;
    auto str_queue = std::make_unique<tools::ring_buffer<std::string, queue_size>>();

    str_queue->push_range({ "toto1", "toto2", "toto3" });

    constexpr const std::array<std::string_view, 2U> extra_values = {
        "toto4",
        "toto5"
    };
    str_queue->push_range(extra_values);

    constexpr const std::size_t pop_batch_size = 2U;
    std::array<std::string, pop_batch_size> popped_batch = { "", "" };
    const std::size_t popped_count = str_queue->pop_range(popped_batch.begin(), popped_batch.end());
    auto* popped_it = popped_batch.data();
    for (std::size_t remaining = popped_count; remaining > 0U; --remaining)
    {
        std::printf("%s\n", popped_it->c_str());
        ++popped_it;
    }

    auto item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer_perfect_forwarding()
{
    LOG_INFO("-- ring buffer perfect forwarding --");
    print_stats();

    {
        constexpr const std::size_t queue_size = 8U;
        constexpr const std::size_t repeated_char_count = 4U;
        tools::ring_buffer<std::string, queue_size> str_queue;

        std::string lvalue = "lvalue-string";
        str_queue.push(lvalue);                       // lvalue path
        str_queue.push(std::string("rvalue-string")); // rvalue path
        str_queue.emplace(repeated_char_count, 'x');   // in-place args forwarding path

        while (!str_queue.empty())
        {
            std::printf("%s\n", str_queue.front().c_str());
            str_queue.pop();
        }
    }

    {
        // move-only type: validates forwarding to push/emplace without copy requirements
        constexpr const std::size_t queue_size = 4U;
        tools::ring_buffer<std::unique_ptr<std::string>, queue_size> ptr_queue;

        auto val = std::make_unique<std::string>("move-only-lvalue-as-rvalue");
        ptr_queue.push(std::move(val));
        ptr_queue.emplace(std::make_unique<std::string>("move-only-rvalue"));

        while (!ptr_queue.empty())
        {
            auto item = ptr_queue.pop_move();
            if (item.has_value() && item.value())
            {
                std::printf("%s\n", item.value()->c_str());
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_vector()
{
    LOG_INFO("-- ring vector --");
    print_stats();

    constexpr const std::size_t queue_size = 64U;
    auto str_queue = std::make_unique<tools::ring_vector<std::string>>(queue_size);

    str_queue->emplace("toto1");
    str_queue->emplace("toto2");
    str_queue->emplace("toto3");

    auto item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_vector_perfect_forwarding()
{
    LOG_INFO("-- ring vector perfect forwarding --");
    print_stats();

    {
        constexpr const std::size_t queue_size = 8U;
        constexpr const std::size_t repeated_char_count = 4U;
        tools::ring_vector<std::string> str_queue(queue_size);

        std::string lvalue = "ring-vector-lvalue";
        str_queue.push(lvalue);
        str_queue.push(std::string("ring-vector-rvalue"));
        str_queue.emplace(repeated_char_count, 'v');

        constexpr const std::array<std::string_view, 2U> range_values = {
            "ring-vector-range-1",
            "ring-vector-range-2"
        };
        str_queue.push_range(range_values);
        str_queue.push_range({ "ring-vector-brace-range-1", "ring-vector-brace-range-2" });

        constexpr const std::size_t pop_batch_size = 2U;
        std::array<std::string, pop_batch_size> popped_batch = { "", "" };
        const std::size_t popped_count = str_queue.pop_range(popped_batch.begin(), popped_batch.end());
        auto* popped_it = popped_batch.data();
        for (std::size_t remaining = popped_count; remaining > 0U; --remaining)
        {
            std::printf("%s\n", popped_it->c_str());
            ++popped_it;
        }

        while (!str_queue.empty())
        {
            std::printf("%s\n", str_queue.front().c_str());
            str_queue.pop();
        }
    }

    {
        constexpr const std::size_t queue_size = 4U;
        tools::ring_vector<std::unique_ptr<std::string>> ptr_queue(queue_size);

        auto val = std::make_unique<std::string>("ring-vector-move-only-push");
        ptr_queue.push(std::move(val));
        ptr_queue.emplace(std::make_unique<std::string>("ring-vector-move-only-emplace"));

        while (!ptr_queue.empty())
        {
            auto item = ptr_queue.pop_move();
            if (item.has_value() && item.value())
            {
                std::printf("%s\n", item.value()->c_str());
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_vector_resize()
{
    LOG_INFO("-- ring vector resize --");
    print_stats();

    constexpr const std::size_t queue_size = 3U;
    auto str_queue = std::make_unique<tools::ring_vector<std::string>>(queue_size);

    str_queue->emplace("toto1");
    str_queue->emplace("toto2");
    str_queue->emplace("toto3");
    constexpr const std::size_t new_queue_size = 5U;
    str_queue->resize(new_queue_size);

    str_queue->emplace("toto4");
    str_queue->emplace("toto5");

    auto item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    str_queue->emplace("tito1");
    str_queue->emplace("tito2");
    str_queue->emplace("tito3");

    str_queue->emplace("tito4");
    str_queue->emplace("tito5");

    str_queue->resize(3);

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    std::printf("expect is empty: %s\n", str_queue->empty() ? "empty" : "not empty");
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer_iteration()
{
    LOG_INFO("-- ring buffer iteration --");
    print_stats();

    {
        constexpr const std::size_t queue_size = 64U;
        auto str_queue = std::make_unique<tools::ring_buffer<std::string, queue_size>>();

        str_queue->emplace("toto1");
        str_queue->emplace("toto2");
        str_queue->emplace("toto3");
        str_queue->emplace("toto4");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        str_queue->push("toto5");
        str_queue->push("toto6");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        int cnt = 0;
        while (!str_queue->full())
        {
            str_queue->emplace("tintin" + std::to_string(cnt));
            ++cnt;
        }

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        constexpr const std::size_t nb_of_items_to_keep = 5U;
        const std::size_t remove_count = str_queue->size() - nb_of_items_to_keep;
        for (std::size_t i = 0U; i < remove_count; ++i)
        {
            str_queue->pop();
        }

        str_queue->push("toutou1");
        str_queue->push("toutou2");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }
    }

    {
        constexpr const std::size_t queue_size = 64U;
        auto val_queue = std::make_unique<tools::ring_buffer<double, queue_size>>();

        val_queue->emplace(5.6);  // NOLINT test value
        val_queue->emplace(7.2);  // NOLINT test value
        val_queue->emplace(1.2);  // NOLINT test value
        val_queue->emplace(9.1);  // NOLINT test value
        val_queue->emplace(10.1); // NOLINT test value
        val_queue->emplace(7.5);  // NOLINT test value

        std::printf("content\n");

        for (std::size_t i = 0U; i < val_queue->size(); ++i)
        {
            std::printf("%f\n", (*val_queue)[i]);
        }

        std::size_t cnt = val_queue->size() - 1U; // NOLINT

        for (std::size_t i = 0U; i < cnt; ++i)
        {
            std::printf("compute\n");

            std::vector<double> snapshot;
            snapshot.reserve(val_queue->size());
            for (std::size_t i = 0U; i < val_queue->size(); ++i)
            {
                snapshot.emplace_back((*val_queue)[i]);
            }

            for (const auto& item : snapshot)
            {
                std::printf("%f\n", item);
            }
            const auto avg_val = std::accumulate(snapshot.cbegin(), snapshot.cend(), 0.0, std::plus<>())
                / static_cast<double>(snapshot.size());
            std::printf("avg: %f\n", avg_val);

            const auto sqsum_val = std::transform_reduce(snapshot.begin(), snapshot.end(), 0.0, std::plus<>(),
                [&avg_val](const auto& val)
                {
                    const auto delta = (val - avg_val);
                    return delta * delta;
                });

            const auto variance_val = std::sqrt(sqsum_val / static_cast<double>(snapshot.size()));
            std::printf("variance: %f\n", variance_val);

            const auto [min_val_it, max_val_it] = std::minmax_element(snapshot.cbegin(), snapshot.cend());

            std::printf("min: %f\n", *min_val_it);
            std::printf("max: %f\n", *max_val_it);

            std::printf("pop front\n");
            val_queue->pop();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_vector_iteration()
{
    LOG_INFO("-- ring vector iteration --");
    print_stats();

    {
        constexpr const std::size_t array_size = 64U;
        auto str_queue = std::make_unique<tools::ring_vector<std::string>>(array_size);

        str_queue->emplace("toto1");
        str_queue->emplace("toto2");
        str_queue->emplace("toto3");
        str_queue->emplace("toto4");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        str_queue->push("toto5");
        str_queue->push("toto6");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        int cnt = 0;
        while (!str_queue->full())
        {
            str_queue->emplace("tintin" + std::to_string(cnt));
            ++cnt;
        }

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        const std::size_t remove_count = str_queue->size() - 5U;
        for (std::size_t i = 0U; i < remove_count; ++i)
        {
            str_queue->pop();
        }

        str_queue->push("toutou1");
        str_queue->push("toutou2");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }
    }

    {
        constexpr const std::size_t array_size = 64U;
        auto val_queue = std::make_unique<tools::ring_vector<double>>(array_size);

        val_queue->emplace(5.6);  // NOLINT test value
        val_queue->emplace(7.2);  // NOLINT test value
        val_queue->emplace(1.2);  // NOLINT test value
        val_queue->emplace(9.1);  // NOLINT test value
        val_queue->emplace(10.1); // NOLINT test value
        val_queue->emplace(7.5);  // NOLINT test value

        std::printf("content\n");

        for (std::size_t i = 0U; i < val_queue->size(); ++i)
        {
            std::printf("%f\n", (*val_queue)[i]);
        }

        std::size_t cnt = val_queue->size() - 1U;

        for (std::size_t i = 0U; i < cnt; ++i)
        {
            std::printf("compute\n");

            std::vector<double> snapshot;
            snapshot.reserve(val_queue->size());
            for (std::size_t i = 0U; i < val_queue->size(); ++i)
            {
                snapshot.emplace_back((*val_queue)[i]);
            }

            for (const auto& item : snapshot)
            {
                std::printf("%f\n", item);
            }

            const auto avg_val = std::accumulate(snapshot.cbegin(), snapshot.cend(), 0.0, std::plus<>())
                / static_cast<double>(snapshot.size());
            std::printf("avg: %f\n", avg_val);

            const auto sqsum_val = std::transform_reduce(snapshot.cbegin(), snapshot.cend(), 0.0, std::plus<>(),
                [&avg_val](const auto& val)
                {
                    const auto delta = (val - avg_val);
                    return delta * delta;
                });

            const auto variance_val = std::sqrt(sqsum_val / static_cast<double>(snapshot.size()));
            std::printf("variance: %f\n", variance_val);

            const auto [min_val_it, max_val_it] = std::minmax_element(snapshot.cbegin(), snapshot.cend());

            std::printf("min: %f\n", *min_val_it);
            std::printf("max: %f\n", *max_val_it);

            std::printf("pop front\n");
            val_queue->pop();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_lock_free_ring_buffer()
{
    LOG_INFO("-- lock free ring buffer --");
    print_stats();

    constexpr const std::size_t queue_size_pow2 = 3U;
    auto str_queue = std::make_unique<tools::lock_free_ring_buffer<std::uint8_t, queue_size_pow2>>();
    bool result = false;

    result = str_queue->push(1U);           // NOLINT test value
    result = result && str_queue->push(2U); // NOLINT test value
    result = result && str_queue->push(3U); // NOLINT test value

    std::uint8_t val = 0;

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->pop(val);
    std::printf("expect failure - %s\n", result ? "success" : "failure");

    result = str_queue->push(1U);           // NOLINT test value
    result = result && str_queue->push(2U); // NOLINT test value
    result = result && str_queue->push(3U); // NOLINT test value
    result = result && str_queue->push(4U); // NOLINT test value

    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(5U); // NOLINT test value
    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(6U); // NOLINT test value
    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(7U); // NOLINT test value
    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(8U); // NOLINT test value
    std::printf("expect failure - %s\n", result ? "success" : "failure");

    result = str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    std::printf("expect success - %s\n", result ? "success" : "failure");
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_lock_free_ring_buffer_perfect_forwarding()
{
    LOG_INFO("-- lock free ring buffer perfect forwarding --");
    print_stats();

    {
        // exact-T rvalue push and optional-pop on a scalar buffer
        constexpr const std::size_t queue_size_pow2 = 2U;
        constexpr const std::uint16_t val_first = 10U;
        constexpr const std::uint16_t val_second = 20U;
        constexpr const std::uint16_t val_third = 30U;
        tools::lock_free_ring_buffer<std::uint16_t, queue_size_pow2> buf;

        std::uint16_t lvalue = val_first;
        (void)buf.push(lvalue);                                  // exact-T lvalue overload
        (void)buf.push(std::uint16_t(val_second));               // exact-T rvalue overload
        (void)buf.push(static_cast<std::uint16_t>(val_third));   // forwarding conversion path

        while (true)
        {
            auto item = buf.pop_opt();                 // optional-based pop
            if (!item.has_value())
            {
                break;
            }
            std::printf("%u\n", static_cast<unsigned>(item.value()));
        }
    }

    {
        // forwarding template conversion: push int literal into float buffer
        constexpr const std::size_t queue_size_pow2 = 2U;
        tools::lock_free_ring_buffer<float, queue_size_pow2> float_buf;

        (void)float_buf.push(1);   // NOLINT int-to-float conversion forwarding path
        (void)float_buf.push(2);   // NOLINT int-to-float conversion forwarding path
        (void)float_buf.push(3);   // NOLINT int-to-float conversion forwarding path

        while (true)
        {
            auto item = float_buf.pop_opt();
            if (!item.has_value())
            {
                break;
            }
            std::printf("%.1f\n", item.value());
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_lock_free_ring_buffer_range()
{
    LOG_INFO("-- lock free ring buffer range --");
    print_stats();

    constexpr const std::size_t queue_size_pow2 = 3U;
    tools::lock_free_ring_buffer<int, queue_size_pow2> buffer;

    const std::size_t pushed_init = buffer.push_range({ 10, 20, 30 });
    std::printf("push_range init-list pushed: %zu\n", pushed_init);

    const std::vector<int> extra_values = { 40, 50 };
    const std::size_t pushed_vec = buffer.push_range(extra_values);
    std::printf("push_range vector pushed: %zu\n", pushed_vec);

    constexpr const std::size_t batch_size = 3U;
    std::array<int, batch_size> batch = {};
    const std::size_t popped_count = buffer.pop_range(batch.begin(), batch.end());
    std::printf("pop_range iterator popped: %zu\n", popped_count);

    auto* batch_ptr = batch.data();
    for (std::size_t idx = 0U; idx < popped_count; ++idx)
    {
        std::printf("  batch[%zu] = %d\n", idx, batch_ptr[idx]);  // NOLINT
    }

    while (true)
    {
        auto item = buffer.pop_opt();
        if (!item.has_value())
        {
            break;
        }
        std::printf("  remaining: %d\n", item.value());
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_lock_free_ring_buffer_task_stress()
{
    LOG_INFO("-- lock free ring buffer task stress --");
    print_stats();

    constexpr const std::size_t stress_buffer_pow2 = 10U;

    struct stress_context
    {
        tools::lock_free_ring_buffer<std::uint32_t, stress_buffer_pow2> buffer;
        std::atomic<std::uint64_t> producer_sum = { 0U };
        std::atomic<std::uint64_t> consumer_sum = { 0U };
        std::atomic<std::uint32_t> consumed_count = { 0U };
    };

    using stress_task = tools::generic_task<stress_context>;
    auto context = std::make_shared<stress_context>();

    constexpr const std::uint32_t item_count = 200000U;
    constexpr const std::size_t task_stack = 4096U;

    const auto begin = std::chrono::steady_clock::now();

    {
        stress_task producer_task(
            [](const std::shared_ptr<stress_context>& shared_context, const std::string& task_name)
            {
                (void)task_name;

                std::uint64_t local_sum = 0U;
                for (std::uint32_t value = 1U; value <= item_count; ++value)
                {
                    while (!shared_context->buffer.push(value))
                    {
                        tools::yield();
                    }
                    local_sum += value;
                }
                shared_context->producer_sum.store(local_sum);
            },
            context, "lf_rb_stress_producer", task_stack);

        stress_task consumer_task(
            [](const std::shared_ptr<stress_context>& shared_context, const std::string& task_name)
            {
                (void)task_name;

                std::uint64_t local_sum = 0U;
                for (std::uint32_t expected = 1U; expected <= item_count; ++expected)
                {
                    std::uint32_t popped_value = 0U;
                    while (!shared_context->buffer.pop(popped_value))
                    {
                        tools::yield();
                    }

                    if (popped_value != expected)
                    {
                        std::printf("stress mismatch: expected=%" PRIu32 " got=%" PRIu32 "\n", expected, popped_value);
                    }

                    local_sum += popped_value;
                    shared_context->consumed_count.fetch_add(1U);
                }
                shared_context->consumer_sum.store(local_sum);
            },
            context, "lf_rb_stress_consumer", task_stack);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

    std::printf("stress consumed=%" PRIu32 "/%" PRIu32 " in %lld ms\n",
        context->consumed_count.load(), item_count, static_cast<long long>(elapsed_ms));
    std::printf("stress checksum producer=%" PRIu64 " consumer=%" PRIu64 "\n",
        context->producer_sum.load(), context->consumer_sum.load());
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_ring_buffer()
{
    LOG_INFO("-- sync ring buffer --");
    print_stats();

    {
        constexpr const std::size_t queue_size = 64U;
        constexpr const std::size_t repeated_char_count = 5U;
        tools::sync_ring_buffer<std::string, queue_size> str_queue;

        std::string lvalue = "sync-lvalue";
        str_queue.push(lvalue);                     // lvalue forwarding
        str_queue.push(std::string("sync-rvalue")); // rvalue forwarding
        str_queue.emplace(repeated_char_count, 'z'); // variadic emplace forwarding
        str_queue.push_range({ "sync-range-1", "sync-range-2" });

        constexpr const std::array<std::string_view, 2U> ar_values = {
            "sync-ar-range-1",
            "sync-ar-range-2"
        };
        str_queue.push_range(ar_values);

        constexpr const std::size_t pop_batch_size = 2U;
        std::array<std::string, pop_batch_size> popped_batch = { "", "" };
        const std::size_t popped_count = str_queue.pop_range(popped_batch.begin(), popped_batch.end());
        auto* popped_it = popped_batch.data();
        for (std::size_t remaining = popped_count; remaining > 0U; --remaining)
        {
            std::printf("%s\n", popped_it->c_str());
            ++popped_it;
        }

        while (!str_queue.empty())
        {
            auto item = str_queue.front_pop();
            if (item.has_value())
            {
                std::printf("%s\n", item->c_str());
            }
        }
    }

    {
        constexpr const std::size_t queue_size = 8U;
        tools::sync_ring_buffer<std::unique_ptr<std::string>, queue_size> ptr_queue;

        auto ptr = std::make_unique<std::string>("sync-move-only-push");
        ptr_queue.push(std::move(ptr));
        ptr_queue.emplace(std::make_unique<std::string>("sync-move-only-emplace"));

        while (!ptr_queue.empty())
        {
            auto item = ptr_queue.front_pop_move();
            if (item.has_value() && item.value())
            {
                std::printf("%s\n", item.value()->c_str());
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_ring_vector()
{
    LOG_INFO("-- sync ring vector --");
    print_stats();

    constexpr const std::size_t queue_size = 64U;
    tools::sync_ring_vector<std::string> str_queue(queue_size);

    str_queue.emplace("toto");
    str_queue.push_range({ "sync-ring-vector-basic-range-1", "sync-ring-vector-basic-range-2" });

    constexpr const std::array<std::string_view, 2U> ar_values = {
        "sync-ring-vector-basic-ar-1",
        "sync-ring-vector-basic-ar-2"
    };
    str_queue.push_range(ar_values);

    constexpr const std::size_t pop_batch_size = 2U;
    std::array<std::string, pop_batch_size> popped_batch = { "", "" };
    const std::size_t popped_count = str_queue.pop_range(popped_batch.begin(), popped_batch.end());
    auto* popped_it = popped_batch.data();
    for (std::size_t remaining = popped_count; remaining > 0U; --remaining)
    {
        std::printf("%s\n", popped_it->c_str());
        ++popped_it;
    }

    auto item = str_queue.front_pop();

    if (item.has_value())
    {
        std::printf("%s\n", item->c_str());
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_ring_vector_perfect_forwarding()
{
    LOG_INFO("-- sync ring vector perfect forwarding --");
    print_stats();

    {
        constexpr const std::size_t queue_size = 64U;
        constexpr const std::size_t repeated_char_count = 5U;
        tools::sync_ring_vector<std::string> str_queue(queue_size);

        std::string lvalue = "sync-ring-vector-lvalue";
        str_queue.push(lvalue);
        str_queue.push(std::string("sync-ring-vector-rvalue"));
        str_queue.emplace(repeated_char_count, 's');
        str_queue.push_range({ "sync-ring-vector-range-1", "sync-ring-vector-range-2" });

        constexpr const std::array<std::string_view, 2U> ar_values = {
            "sync-ring-vector-ar-range-1",
            "sync-ring-vector-ar-range-2"
        };
        str_queue.push_range(ar_values);

        constexpr const std::size_t pop_batch_size = 2U;
        std::array<std::string, pop_batch_size> popped_batch = { "", "" };
        const std::size_t popped_count = str_queue.pop_range(popped_batch.begin(), popped_batch.end());
        auto* popped_it = popped_batch.data();
        for (std::size_t remaining = popped_count; remaining > 0U; --remaining)
        {
            std::printf("%s\n", popped_it->c_str());
            ++popped_it;
        }

        while (!str_queue.empty())
        {
            auto item = str_queue.front_pop();
            if (item.has_value())
            {
                std::printf("%s\n", item->c_str());
            }
        }
    }

    {
        constexpr const std::size_t queue_size = 8U;
        tools::sync_ring_vector<std::unique_ptr<std::string>> ptr_queue(queue_size);

        auto ptr = std::make_unique<std::string>("sync-ring-vector-move-only-push");
        ptr_queue.push(std::move(ptr));
        ptr_queue.emplace(std::make_unique<std::string>("sync-ring-vector-move-only-emplace"));

        while (!ptr_queue.empty())
        {
            auto item = ptr_queue.front_pop_move();
            if (item.has_value() && item.value())
            {
                std::printf("%s\n", item.value()->c_str());
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_queue()
{
    LOG_INFO("-- sync queue --");
    print_stats();

    tools::sync_queue<std::string> str_queue;

    str_queue.emplace("toto");

    auto item = str_queue.front_pop();

    if (item.has_value())
    {
        std::printf("%s\n", item->c_str());
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_queue_perfect_forwarding()
{
    LOG_INFO("-- sync queue perfect forwarding --");
    print_stats();

    {
        tools::sync_queue<std::string> str_queue;

        std::string lvalue = "sync-queue-lvalue";
        str_queue.push(lvalue);                             // exact-T lvalue overload path
        str_queue.push(std::string("sync-queue-rvalue")); // exact-T rvalue overload path
        str_queue.push("sync-queue-conversion");          // forwarding template conversion path
        str_queue.push({ "sync-queue-brace-init" });      // brace-init compatibility path
        str_queue.emplace(4U, 'q');                         // in-place args forwarding path

        constexpr const std::array<std::string_view, 2U> range_lvalue = {
            "sync-queue-range-lvalue-1",
            "sync-queue-range-lvalue-2"
        };
        constexpr const std::array<std::string_view, 2U> range_ar = {
            "sync-queue-range-ar-1",
            "sync-queue-range-ar-2"
        };

        str_queue.push_range(range_lvalue);
        str_queue.push_range(range_ar);
        str_queue.push_range({ "sync-queue-brace-range-1", "sync-queue-brace-range-2" });

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        constexpr const std::array<std::size_t, 2U> suffixes = { 1U, 2U };
        const auto transformed = suffixes
            | std::views::transform([](const std::size_t suffix)
              {
                  return std::string("sync-queue-ranges-view-") + std::to_string(suffix);
              });
        str_queue.push_range(transformed);
#else
        const std::array<std::string, 2U> transformed = {
            "sync-queue-ranges-fallback-1",
            "sync-queue-ranges-fallback-2"
        };
        str_queue.push_range(transformed);
#endif

        constexpr const std::size_t pop_batch_size = 2U;
        std::array<std::string, pop_batch_size> popped_batch = { "", "" };
        const std::size_t popped_count = str_queue.pop_range(popped_batch.begin(), popped_batch.end());
        auto* popped_it = popped_batch.data();
        for (std::size_t remaining = popped_count; remaining > 0U; --remaining)
        {
            std::printf("%s\n", popped_it->c_str());
            ++popped_it;
        }

        while (!str_queue.empty())
        {
            auto item = str_queue.front_pop();
            if (item.has_value())
            {
                std::printf("%s\n", item->c_str());
            }
        }
    }

    {
        tools::sync_queue<std::unique_ptr<std::string>> ptr_queue;

        auto ptr = std::make_unique<std::string>("sync-queue-move-only-push");
        ptr_queue.push(std::move(ptr));
        ptr_queue.emplace(std::make_unique<std::string>("sync-queue-move-only-emplace"));

        while (!ptr_queue.empty())
        {
            auto item = ptr_queue.front_pop_move();
            if (item.has_value() && item.value())
            {
                std::printf("%s\n", item.value()->c_str());
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_dictionary()
{
    LOG_INFO("-- sync dictionary --");
    print_stats();

    tools::sync_dictionary<std::string, std::string> str_dict;

    std::string key_lvalue = "key-lvalue";
    std::string value_lvalue = "value-lvalue";

    str_dict.add(key_lvalue, value_lvalue); // exact-T lvalue overload path
    str_dict.add(std::string("key-rvalue"), std::string("value-rvalue")); // exact-T rvalue overload path
    str_dict.add("key-conversion", "value-conversion"); // forwarding conversion path
    str_dict.add("key-conversion", std::string("value-updated")); // update via forwarding path

    const std::vector<std::pair<std::string, std::string>> range_values = {
        { "key-range-1", "value-range-1" },
        { "key-range-2", "value-range-2" }
    };
    str_dict.add_range(range_values);
    str_dict.add_range({ { "key-brace-1", "value-brace-1" }, { "key-brace-2", "value-brace-2" } });

    auto result_lvalue = str_dict.find("key-lvalue");
    auto result_rvalue = str_dict.find("key-rvalue");
    auto result_conversion = str_dict.find("key-conversion");

    if (result_lvalue.has_value())
    {
        std::printf("%s\n", (*result_lvalue).c_str());
    }

    if (result_rvalue.has_value())
    {
        std::printf("%s\n", (*result_rvalue).c_str());
    }

    if (result_conversion.has_value())
    {
        std::printf("%s\n", (*result_conversion).c_str());
    }

    std::printf("contains key-range-1: %s\n", str_dict.contains("key-range-1") ? "yes" : "no");
    std::printf("contains missing-key: %s\n", str_dict.contains("missing-key") ? "yes" : "no");

    str_dict.remove("key-lvalue");
    str_dict.remove("key-rvalue");
    str_dict.remove("key-conversion");
    str_dict.remove_collection(std::vector<std::string> { "key-range-1", "key-brace-1" });
    str_dict.remove_collection({ "key-range-2", "key-brace-2" });
}

//--------------------------------------------------------------------------------------------------------------------------------

enum class my_topic : std::uint8_t
{
    generic,
    system,
    external
};

struct my_event
{
    enum class type : std::uint8_t
    {
        notification,
        failure
    } type = {type::notification};

    std::string description;
};

using base_observer = tools::sync_observer<my_topic, my_event>;
class my_observer : public base_observer // NOLINT inherits from non copyable and non movable class
{
public:
    my_observer() = default;
    ~my_observer() override = default;

    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        std::printf("sync [topic %d] received: event (%s) from %s\n",
            static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());
    }

private:
};

// using base_async_observer = tools::async_observer<my_topic, my_event, tools::sync_queue>;
using base_async_observer = tools::async_observer<my_topic, my_event, tools::sync_ring_vector>;

static constexpr const std::size_t base_async_observer_queue_depth = 256U;
class my_async_observer : public base_async_observer // NOLINT inherits from non copyable and non movable
{
public:
    my_async_observer()
        : base_async_observer(base_async_observer_queue_depth)
        , m_task_loop([this]() { handle_events(); })
    {
    }

    ~my_async_observer() override
    {
        m_stop_task.store(true);
        m_task_loop.join();
    }

    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        std::printf("async/push [topic %d] received: event (%s) from %s\n",
            static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());

        base_async_observer::inform(topic, event, origin);
    }

private:
    void handle_events()
    {
        const auto timeout = std::chrono::duration<std::uint64_t, std::micro>(1000);

        while (!m_stop_task.load())
        {
            wait_for_events(timeout);

            while (number_of_events() > 0)
            {
                auto entry = pop_first_event();
                if (entry.has_value())
                {
                    auto& [topic, event, origin] = *entry;

                    std::printf("async/pop [topic %d] received: event (%s) from %s\n",
                        static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(),
                        origin.c_str());
                }
            }
        }
    }

    std::thread m_task_loop;
    std::atomic_bool m_stop_task = false;
};

using base_subject = tools::sync_subject<my_topic, my_event>;
class my_subject : public base_subject // NOLINT inherits from non_copyable and non_movable
{
public:
    my_subject() = delete;
    my_subject(const std::string& name)
        : base_subject(name)
    {
    }

    ~my_subject() override = default;

    void publish(const my_topic& topic, const my_event& event) override
    {
        std::printf("publish: event (%s) to %s\n", event.description.c_str(), name().c_str());
        base_subject::publish(topic, event);
    }

private:
};

void test_publish_subscribe()
{
    LOG_INFO("-- publish subscribe --");
    print_stats();

    auto observer1 = std::make_shared<my_observer>();
    auto observer2 = std::make_shared<my_observer>();
    auto async_observer = std::make_shared<my_async_observer>();
    auto subject1 = std::make_shared<my_subject>("source1");
    auto subject2 = std::make_shared<my_subject>("source2");

    subject1->subscribe(my_topic::generic, observer1);
    subject1->subscribe(my_topic::generic, observer2);
    subject1->subscribe(my_topic::system, observer2);
    subject1->subscribe(my_topic::generic, async_observer);

    subject2->subscribe(my_topic::generic, observer1);
    subject2->subscribe(my_topic::generic, observer2);
    subject2->subscribe(my_topic::system, observer2);
    subject2->subscribe(my_topic::generic, async_observer);

    subject1->subscribe(my_topic::generic, "loose_coupled_handler_1",
        [](const my_topic& topic, const my_event& event, const std::string& origin)
        {
            std::printf("handler [topic %d] received: event (%s) from %s\n",
                static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());
        });

    subject1->publish(my_topic::generic, my_event { my_event::type::notification, "toto" });

    subject1->unsubscribe(my_topic::generic, observer1);

    subject1->publish(my_topic::generic, my_event { my_event::type::notification, "titi" });

    subject1->publish(my_topic::system, my_event { my_event::type::notification, "tata" });

    subject1->unsubscribe(my_topic::generic, "loose_coupled_handler_1");

    constexpr const int wait_time_500ms = 500;
    tools::sleep_for(wait_time_500ms);

    subject1->publish(my_topic::generic, my_event { my_event::type::notification, "tintin" });

    subject2->publish(my_topic::generic, my_event { my_event::type::notification, "tonton" });
    subject2->publish(my_topic::system, my_event { my_event::type::notification, "tantine" });
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_observer_perfect_forwarding()
{
    LOG_INFO("-- sync observer perfect forwarding --");
    print_stats();

    tools::sync_subject<std::string, std::string> subject("forwarding_subject");

    std::string topic_lvalue = "demo-topic";
    std::string handler_name_lvalue = "demo-handler";
    std::string payload_lvalue = "payload-lvalue";

    subject.subscribe(topic_lvalue, handler_name_lvalue,
        [](const std::string& topic, const std::string& event, const std::string& origin)
        {
            std::printf("sync-fwd [topic %s] event (%s) from %s\n", topic.c_str(), event.c_str(), origin.c_str());
        });

    subject.publish(topic_lvalue, payload_lvalue);                          // exact lvalue path
    subject.publish(std::string("demo-topic"), std::string("payload-rvalue")); // exact rvalue path
    subject.publish("demo-topic", "payload-conversion");                   // conversion forwarding path
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_async_observer_perfect_forwarding()
{
    LOG_INFO("-- async observer perfect forwarding --");
    print_stats();

    tools::async_observer<std::string, std::string, tools::sync_queue> observer;

    std::string topic_lvalue = "demo-topic";
    std::string event_lvalue = "payload-lvalue";
    std::string origin_lvalue = "demo-origin";

    observer.inform(topic_lvalue, event_lvalue, origin_lvalue); // exact lvalue path
    observer.inform(std::string("demo-topic"), std::string("payload-rvalue"), std::string("demo-origin"));
    observer.inform("demo-topic", "payload-conversion", "demo-origin"); // conversion forwarding path

    auto events = observer.pop_all_events();
    for (const auto& [topic, event, origin] : events)
    {
        std::printf("async-fwd [topic %s] event (%s) from %s\n", topic.c_str(), event.c_str(), origin.c_str());
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

struct my_generic_task_context
{
    std::atomic<bool> stop_tasks;
};

using my_generic_task = tools::generic_task<my_generic_task_context>;

static void generic_function(const std::shared_ptr<my_generic_task_context>& context, const std::string& task_name)
{
    std::printf("starting generic task %s\n", task_name.c_str());

    constexpr const int sleeping_time_ms = 250;

    while (!context->stop_tasks.load())
    {
        tools::sleep_for(sleeping_time_ms);
        std::printf("loop generic task %s\n", task_name.c_str());
        tools::sleep_for(sleeping_time_ms);
    }

    std::printf("ending generic task %s\n", task_name.c_str());
}

void test_generic_task()
{
    LOG_INFO("-- generic task --");
    print_stats();

    auto lambda = [](const std::shared_ptr<my_generic_task_context>& context, const std::string& task_name)
    {
        std::printf("starting generic task %s\n", task_name.c_str());

        constexpr const int sleeping_time_ms = 500;

        while (!context->stop_tasks.load())
        {
            std::printf("loop generic task %s\n", task_name.c_str());
            tools::sleep_for(sleeping_time_ms);
        }

        std::printf("ending generic task %s\n", task_name.c_str());
    };

    auto context = std::make_shared<my_generic_task_context>();
    context->stop_tasks.store(false);

    constexpr const std::size_t stack_size = 2048U;

    my_generic_task task1(std::move(lambda), context, "my_generic_task1", stack_size);
    my_generic_task task2(std::move(generic_function), context, "my_generic_task2", stack_size);

    constexpr const int wait_tasks_time_ms = 2000;
    tools::sleep_for(wait_tasks_time_ms);

    context->stop_tasks.store(true);

    constexpr const int wait_join_ms = 1000;
    tools::sleep_for(wait_join_ms);

    std::printf("join tasks\n");
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_generic_task_perfect_forwarding()
{
    LOG_INFO("-- generic task perfect forwarding --");
    print_stats();

    auto context = std::make_shared<my_generic_task_context>();
    context->stop_tasks.store(false);

    my_generic_task::call_back callback_lvalue
        = [](const std::shared_ptr<my_generic_task_context>& ctx, const std::string& task_name)
    {
        std::printf("generic forwarding start %s\n", task_name.c_str());
        ctx->stop_tasks.store(true);
    };

    constexpr const std::size_t stack_size = 2048U;

    my_generic_task task_lvalue(callback_lvalue, context, std::string("generic_forward_lvalue"), stack_size);

    auto context_conversion = std::make_shared<my_generic_task_context>();
    context_conversion->stop_tasks.store(false);
    my_generic_task task_conversion(generic_function, context_conversion, "generic_forward_conversion", stack_size);

    constexpr const int wait_join_ms = 200;
    tools::sleep_for(wait_join_ms);
    context_conversion->stop_tasks.store(true);
    tools::sleep_for(wait_join_ms);
}

//--------------------------------------------------------------------------------------------------------------------------------

struct my_periodic_task_context
{
    std::atomic<int> loop_counter = 0;
    tools::sync_queue<std::chrono::high_resolution_clock::time_point> time_points;
};

using my_periodic_task = tools::periodic_task<my_periodic_task_context>;

void test_periodic_task()
{
    LOG_INFO("-- periodic task --");
    print_stats();

    auto lambda = [](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter += 1;
        context->time_points.emplace(std::chrono::high_resolution_clock::now());
    };

    auto startup = [](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter = 0;
    };

    auto context = std::make_shared<my_periodic_task_context>();

    constexpr const int period_20ms = 20000;
    constexpr const auto period = std::chrono::duration<std::uint64_t, std::micro>(period_20ms);
    const auto start_timepoint = std::chrono::high_resolution_clock::now();

    constexpr const std::size_t stack_size = 2048U;
    my_periodic_task task1(startup, lambda, context, "my_periodic_task", period, stack_size);

    // sleep 2 sec
    constexpr const int wait_task_startup_ms = 2000;
    tools::sleep_for(wait_task_startup_ms);

    std::printf("nb of periodic loops = %d\n", context->loop_counter.load());

    auto previous_timepoint = start_timepoint;
    while (!context->time_points.empty())
    {
        const auto measured_timepoint = context->time_points.front_pop();

        if (measured_timepoint.has_value())
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                measured_timepoint.value() - previous_timepoint);
            std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
            previous_timepoint = measured_timepoint.value();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_periodic_task_perfect_forwarding()
{
    LOG_INFO("-- periodic task perfect forwarding --");
    print_stats();

    auto context = std::make_shared<my_periodic_task_context>();

    tools::periodic_task<my_periodic_task_context>::call_back startup_lvalue
        = [](const std::shared_ptr<my_periodic_task_context>& ctx, const std::string& task_name)
    {
        (void)task_name;
        ctx->loop_counter.store(0);
    };

    auto periodic_rvalue = [](const std::shared_ptr<my_periodic_task_context>& ctx, const std::string& task_name)
    {
        (void)task_name;
        ctx->loop_counter.fetch_add(1);
    };

    constexpr std::size_t stack_size = 2048U;
    constexpr const int period_ms = 20;
    tools::periodic_task<my_periodic_task_context> task(startup_lvalue, std::move(periodic_rvalue), context,
        "my_periodic_forwarding_task", std::chrono::milliseconds(period_ms), stack_size);

    constexpr const int wait_time_ms = 200;
    tools::sleep_for(wait_time_ms);
    std::printf("periodic forwarding loop counter = %d\n", context->loop_counter.load());
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_histogram_perfect_forwarding()
{
    LOG_INFO("-- histogram perfect forwarding --");
    print_stats();

    tools::histogram<double> hist;

    constexpr const double value_exact = 2.5;
    constexpr const int value_int_conversion = 2;
    constexpr const float value_float_conversion = 2.5F;
    constexpr const std::array<double, 2U> repeated_values = { 3.0, 3.0 };

    double lvalue_value = value_exact;
    hist.add(lvalue_value);              // exact-T lvalue overload path
    hist.add(value_exact);               // exact-T rvalue overload path
    hist.add(value_int_conversion);      // forwarding conversion path (int -> double)
    hist.add(value_float_conversion);    // forwarding conversion path (float -> double)

    const std::vector<int> range_values = { 2, 3, 3 };
    hist.add_range(range_values);
    hist.add_range(repeated_values);

    std::printf("histogram total=%d top=%f occ=%d\n", hist.total_count(), hist.top(), hist.top_occurence());
}

//--------------------------------------------------------------------------------------------------------------------------------

class my_collector : public base_observer // NOLINT inherits from non copyable and non movable class
{
public:
    my_collector() = default;
    ~my_collector() override = default;

    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        (void)topic;
        (void)origin;

        m_histogram.add(std::strtod(event.description.c_str(), nullptr));
    }

    void display_stats()
    {
        auto top = m_histogram.top();
        std::printf("\nvalue %f appears %d times\n", top, m_histogram.top_occurence());
        auto avg = m_histogram.average();
        std::printf("average value is %f\n", avg);
        std::printf("median value is %f\n", m_histogram.median());
        auto variance = m_histogram.variance(avg);
        std::printf("variance is %f\n", variance);
        auto std_deviation = m_histogram.standard_deviation(variance);
        std::printf("standard deviation is %f\n", std_deviation);
        std::printf("gaussian density of %f is %f\n", top, m_histogram.gaussian_density(top, avg, std_deviation));
        std::printf("gaussian probability of [%f, %f] is %f\n", 0.5 * top, top,         // NOLINT test value
            m_histogram.gaussian_probability(0.5 * top, top, avg, std_deviation, 100)); // NOLINT test value
    }

private:
    tools::histogram<double> m_histogram;
};

//--------------------------------------------------------------------------------------------------------------------------------

void test_periodic_publish_subscribe()
{
    LOG_INFO("-- periodic publish subscribe --");
    print_stats();

    auto monitoring = std::make_shared<my_async_observer>();
    auto data_source = std::make_shared<my_subject>("data_source");
    auto histogram_feeder = std::make_shared<my_collector>();

    auto sampler
        = [&data_source](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter += 1;

        // mocked signal
        double signal = std::sin(context->loop_counter.load());

        // emit "signal" as a 'string' event
        data_source->publish(my_topic::external, my_event { my_event::type::notification, std::to_string(signal) });
    };

    auto startup = [](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter = 0;
    };

    data_source->subscribe(my_topic::external, monitoring);
    data_source->subscribe(my_topic::external, histogram_feeder);

    // "sample" with a 100 ms period
    auto context = std::make_shared<my_periodic_task_context>();
    constexpr const std::size_t stack_size = 4096U;
    const auto period = std::chrono::duration<std::uint64_t, std::milli>(100);
    {
        my_periodic_task periodic_task(startup, sampler, context, "sampler_task", period, stack_size);

        constexpr const std::size_t wait_task = 2000;
        tools::sleep_for(wait_task);
    }

    histogram_feeder->display_stats();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_queued_commands()
{
    LOG_INFO("-- queued commands --");
    print_stats();

    tools::sync_queue<std::function<void()>> commands_queue;

    commands_queue.emplace([]() { std::printf("hello\n"); });

    commands_queue.emplace([]() { std::printf("world\n"); });

    while (!commands_queue.empty())
    {
        auto call = commands_queue.front_pop();
        if (call.has_value())
        {
            call.value()();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer_commands()
{
    LOG_INFO("-- ring buffer commands --");
    print_stats();

    constexpr const std::size_t commands_queue_depth = 128U;
    tools::sync_ring_buffer<std::function<void()>, commands_queue_depth> commands_queue;

    commands_queue.emplace([]() { std::printf("hello\n"); });

    commands_queue.emplace([]() { std::printf("world\n"); });

    while (!commands_queue.empty())
    {
        auto call = commands_queue.front_pop();
        if (call.has_value())
        {
            call.value()();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

struct my_worker_task_context
{
    std::atomic<int> loop_counter = 0;
    tools::sync_queue<std::chrono::high_resolution_clock::time_point> time_points;
};

using my_worker_task = tools::worker_task<my_worker_task_context>;

void test_worker_tasks()
{
    LOG_INFO("-- worker tasks --");
    print_stats();

    auto startup1 = [](const std::shared_ptr<my_worker_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("welcome 1\n");
        std::printf("task %s started\n", task_name.c_str());
    };

    auto startup2 = [](const std::shared_ptr<my_worker_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("welcome 2\n");
        std::printf("task %s started\n", task_name.c_str());
    };

    auto context = std::make_shared<my_worker_task_context>();

    constexpr const std::size_t stack_size = 4096U;
    auto task1 = std::make_unique<my_worker_task>(std::move(startup1), context, "worker_1", stack_size);
    auto task2 = std::make_unique<my_worker_task>(std::move(startup2), context, "worker_2", stack_size);

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 1);
    std::array<std::unique_ptr<my_worker_task>, 2> tasks = { std::move(task1), std::move(task2) };

    constexpr const int wait_tasks_ms = 100;
    tools::sleep_for(wait_tasks_ms);

    const auto start_timepoint = std::chrono::high_resolution_clock::now();
    constexpr const int nb_loops = 20;

    for (int i = 0; i < nb_loops; ++i)
    {
        auto idx = distribution(generator);

        const std::array<my_worker_task::call_back, 2> work_batch = {
            [](const auto& local_context, const auto& local_task_name)
            {
                std::printf("job %d on worker task %s\n", local_context->loop_counter.load(), local_task_name.c_str());
                local_context->loop_counter++;
                local_context->time_points.emplace(std::chrono::high_resolution_clock::now());
            },
            [](const auto& local_context, const auto& local_task_name)
            {
                std::printf("job %d on worker task %s\n", local_context->loop_counter.load(), local_task_name.c_str());
                local_context->loop_counter++;
                local_context->time_points.emplace(std::chrono::high_resolution_clock::now());
            }
        };

        tasks.at(idx)->delegate_range(work_batch);

        std::this_thread::yield();
    }

    constexpr const int wait_loops_ms = 2000;
    tools::sleep_for(wait_loops_ms);

    std::printf("nb of loops = %d\n", context->loop_counter.load());

    auto previous_timepoint = start_timepoint;
    while (!context->time_points.empty())
    {
        const auto measured_timepoint = context->time_points.front_pop();

        if (measured_timepoint.has_value())
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                measured_timepoint.value() - previous_timepoint);
            std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
            previous_timepoint = measured_timepoint.value();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_worker_task_perfect_forwarding()
{
    LOG_INFO("-- worker task perfect forwarding --");
    print_stats();

    auto context = std::make_shared<my_worker_task_context>();

    my_worker_task::call_back startup_lvalue
        = [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
    {
        std::printf("startup on %s\n", task_name.c_str());
        ctx->loop_counter.store(0);
    };

    std::string task_name_lvalue = "worker_forwarding";
    constexpr const std::size_t stack_size = 4096U;
    my_worker_task worker(startup_lvalue, context, task_name_lvalue, stack_size);

    my_worker_task::call_back delegate_lvalue
        = [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
    {
        std::printf("delegate-lvalue on %s\n", task_name.c_str());
        ctx->loop_counter.fetch_add(1);
    };

    worker.delegate(delegate_lvalue); // exact lvalue callback path
    worker.delegate(
        [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
        {
            std::printf("delegate-rvalue on %s\n", task_name.c_str());
            ctx->loop_counter.fetch_add(1);
        }); // exact rvalue callback path
    worker.delegate([](auto ctx, const auto& task_name)
    {
        std::printf("delegate-conversion on %s\n", task_name.c_str());
        ctx->loop_counter.fetch_add(1);
    }); // forwarding conversion path

    const std::vector<my_worker_task::call_back> work_batch = {
        [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
        {
            std::printf("delegate-range on %s\n", task_name.c_str());
            ctx->loop_counter.fetch_add(1);
        },
        [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
        {
            std::printf("delegate-range on %s\n", task_name.c_str());
            ctx->loop_counter.fetch_add(1);
        }
    };
    worker.delegate_range(work_batch);
    worker.delegate_range({
        my_worker_task::call_back(
            [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
            {
                std::printf("delegate-range-init on %s\n", task_name.c_str());
                ctx->loop_counter.fetch_add(1);
            })
    });

    constexpr const int wait_time_ms = 200;
    tools::sleep_for(wait_time_ms);
    std::printf("worker forwarding loop counter = %d\n", context->loop_counter.load());
}

//--------------------------------------------------------------------------------------------------------------------------------

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))

enum class message_type : std::uint8_t
{
    temperature = 1,
    manufacturing = 2,
    freetext = 3,
    aggregat = 4
};

struct temperature_sensor
{
    double cpu_temperature;
    double gpu_temperature;
    double room_temperature;

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(cpu_temperature, gpu_temperature, room_temperature);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(cpu_temperature, gpu_temperature, room_temperature);
        return true; // pretend success
    }
};

struct manufacturing_info
{
    std::string vendor_name;
    std::string serial_number;
    std::string manufacturing_date;
    std::array<char, 2> tag;

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(vendor_name, serial_number, manufacturing_date, tag);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(vendor_name, serial_number, manufacturing_date, tag);
        return true; // pretend success
    }
};

struct free_text
{
    std::string text;

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(text);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(text);
        return true; // pretend success
    }
};

void test_queued_bytepack_data()
{
    LOG_INFO("-- queued bytepack data --");
    print_stats();

    constexpr const std::size_t queue_depth = 128U;
    tools::sync_ring_vector<std::vector<std::uint8_t>> data_queue(queue_depth);
    // tools::sync_ring_buffer<std::vector<std::uint8_t>, queue_depth> data_queue;
    // tools::sync_queue<std::vector<std::uint8_t>> data_queue;

    free_text message1 = { "jojo rabbit" };
    manufacturing_info message2 = { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } };
    temperature_sensor message3 = { 45.2, 51.72, 21.5 }; // NOLINT tests values

    constexpr const std::size_t buffer_size = 1024U;
    std::vector<std::uint8_t> buffer(buffer_size);
    bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer.data(), buffer.size()) };

    binary_stream.reset();
    binary_stream.write(message_type::freetext);
    message1.serialize(binary_stream);
    auto buffer1 = binary_stream.data();
    std::vector<std::uint8_t> encoded1 = {};
    encoded1.resize(buffer1.size());
    std::memcpy(encoded1.data(), buffer1.as<void>(), buffer1.size());
    data_queue.emplace(std::move(encoded1));

    binary_stream.reset();
    binary_stream.write(message_type::manufacturing);
    message2.serialize(binary_stream);
    auto buffer2 = binary_stream.data();
    std::vector<std::uint8_t> encoded2 = {};
    encoded2.resize(buffer2.size());
    std::memcpy(encoded2.data(), buffer2.as<void>(), buffer2.size());
    data_queue.emplace(std::move(encoded2));

    binary_stream.reset();
    binary_stream.write(message_type::temperature);
    message3.serialize(binary_stream);
    auto buffer3 = binary_stream.data();
    std::vector<std::uint8_t> encoded3 = {};
    encoded3.resize(buffer3.size());
    std::memcpy(encoded3.data(), buffer3.as<void>(), buffer3.size());
    data_queue.emplace(std::move(encoded3));

    while (!data_queue.empty())
    {
        auto data_packed = data_queue.front_pop();
        if (!data_packed.has_value())
        {
            break;
        }

        bytepack::binary_stream stream(bytepack::buffer_view(data_packed->data(), data_packed->size()));

        message_type discriminant = {};
        stream.read(discriminant);

        switch (discriminant)
        {
            case message_type::freetext:
            {
                free_text text = {};
                text.deserialize(stream);
                std::printf("%s\n", text.text.c_str());
            }
            break;

            case message_type::manufacturing:
            {
                manufacturing_info info = {};
                info.deserialize(stream);
                std::printf("%s\n%s\n%s\n%c %c\n", info.vendor_name.c_str(), info.serial_number.c_str(),
                    info.manufacturing_date.c_str(), info.tag[0], info.tag[1]);
            }
            break;

            case message_type::temperature:
            {
                temperature_sensor temp = {};
                temp.deserialize(stream);
                std::printf("%f\n%f\n%f\n", temp.cpu_temperature, temp.gpu_temperature, temp.room_temperature);
            }
            break;

            case message_type::aggregat:
                break;
        };
    }
}

//--------------------------------------------------------------------------------------------------------------------------------
struct aggregated_info
{
    std::map<std::string, temperature_sensor> m_dictionary;
    std::vector<manufacturing_info> m_list;
    bool m_status = false;
    std::vector<double> m_values;

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(static_cast<std::uint32_t>(m_dictionary.size()));
        for (const auto& elt : m_dictionary)
        {
            stream.write(elt.first);
            elt.second.serialize(stream);
        }

        stream.write(static_cast<std::uint32_t>(m_list.size()));
        for (const auto& elt : m_list)
        {
            elt.serialize(stream);
        }

        stream.write(m_status, m_values);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        std::uint32_t len = 0U;
        std::string key;
        temperature_sensor sensor = {};
        manufacturing_info manuf = {};

        m_dictionary.clear();
        m_list.clear();

        stream.read(len);

        for (std::uint32_t i = 0U; i < len; ++i)
        {
            stream.read(key);
            sensor.deserialize(stream);
            m_dictionary.emplace(key, sensor);
        }

        len = 0U;
        stream.read(len);
        for (std::uint32_t i = 0U; i < len; ++i)
        {
            manuf.deserialize(stream);
            m_list.emplace_back(manuf);
        }

        stream.read(m_status, m_values);

        return true; // pretend success
    }
};

void test_aggregated_bytepack_data()
{
    LOG_INFO("-- test aggregated bytepack data --");
    print_stats();

    aggregated_info aggr = {};
    aggr.m_dictionary
        = { { "sensor1", { 45.2, 51.72, 21.5 } }, { "sensor2", { 17.2, 34.7, 18.3 } } }; // NOLINT test values
    aggr.m_list = { { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } },
        { { "AMD" }, { "HTX-7788-22-5" }, { "12/05/07" }, { 'B', 'Z' } } };
    aggr.m_status = false;
    aggr.m_values = { 0.7, 1.5, 2.1, -0.5 }; // NOLINT test values

    auto sens = aggr.m_dictionary.find("sensor2");
    std::printf(
        "%f %f %f \n", sens->second.cpu_temperature, sens->second.gpu_temperature, sens->second.room_temperature);
    std::printf("%s %s %s\n", aggr.m_list[1].manufacturing_date.c_str(), aggr.m_list[1].serial_number.c_str(),
        aggr.m_list[1].vendor_name.c_str());
    std::printf("%f %f %f\n", aggr.m_values[0], aggr.m_values[1], aggr.m_values[2]);

    constexpr const std::size_t buffer_size = 1024U;
    std::vector<std::uint8_t> buffer(buffer_size);
    bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer.data(), buffer.size()) };

    binary_stream.reset();
    binary_stream.write(message_type::aggregat);
    aggr.serialize(binary_stream);

    // deserialize

    aggregated_info aggr_dup;
    auto bin_buf = binary_stream.data();
    std::vector<std::uint8_t> data_packed(bin_buf.size());
    std::memcpy(data_packed.data(), bin_buf.as<void>(), bin_buf.size());

    bytepack::binary_stream stream(bytepack::buffer_view(data_packed.data(), data_packed.size()));
    message_type typ = {};
    stream.read(typ);
    if (message_type::aggregat == typ)
    {
        aggr_dup.deserialize(stream);

        auto sensor = aggr_dup.m_dictionary.find("sensor2");
        std::printf("%f %f %f \n", sensor->second.cpu_temperature, sensor->second.gpu_temperature,
            sensor->second.room_temperature);
        std::printf("%s %s %s %c %c \n", aggr_dup.m_list[1].manufacturing_date.c_str(),
            aggr_dup.m_list[1].serial_number.c_str(), aggr_dup.m_list[1].vendor_name.c_str(), aggr_dup.m_list[1].tag[0],
            aggr_dup.m_list[1].tag[1]);
        std::printf("%f %f %f\n", aggr_dup.m_values[0], aggr_dup.m_values[1], aggr_dup.m_values[2]);
    }
}

#endif // #if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))

//--------------------------------------------------------------------------------------------------------------------------------

struct data_task_context
{
};

constexpr const std::size_t binary_msg_size = 128U;
using binary_msg = std::array<std::uint8_t, binary_msg_size>;

using my_data_task = tools::data_task<data_task_context, binary_msg>;
using my_data_periodic_task = tools::periodic_task<data_task_context>;

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
namespace
{
    constexpr const auto task_startup
        = [](const std::shared_ptr<data_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("starting %s\n", task_name.c_str());
    };

    void task_1_processing(
        const std::shared_ptr<data_task_context>& context, binary_msg data_packed, const std::string& task_name)
    {
        (void)context;
        (void)task_name;

        bytepack::binary_stream stream(bytepack::buffer_view(data_packed.data(), data_packed.size()));

        message_type discriminant = {};
        stream.read(discriminant);

        switch (discriminant)
        {
            case message_type::freetext:
            {
                free_text text = {};
                text.deserialize(stream);
                std::printf("%s\n", text.text.c_str());
            }
            break;

            case message_type::manufacturing:
            {
                manufacturing_info info = {};
                info.deserialize(stream);
                std::printf("%s\n%s\n%s\n%c %c\n", info.vendor_name.c_str(), info.serial_number.c_str(),
                    info.manufacturing_date.c_str(), info.tag[0], info.tag[1]);
            }
            break;

            case message_type::temperature:
            {
                temperature_sensor temp = {};
                temp.deserialize(stream);
                std::printf("%f\n%f\n%f\n", temp.cpu_temperature, temp.gpu_temperature, temp.room_temperature);
            }
            break;

            case message_type::aggregat:
                break;
        }
    };
}

void test_bytepack_data_task()
{
    LOG_INFO("-- test bytepack data task --");
    print_stats();

    auto context = std::make_shared<data_task_context>();

    constexpr const std::size_t queue_depth = 128U;
    constexpr const std::size_t stack_size = 4096U;

    auto task_1 = std::make_unique<my_data_task>(
        std::move(task_startup), std::move(task_1_processing), context, queue_depth, "task 1", stack_size);

    binary_msg buffer;

    auto task_2_periodic
        = [&task_1, &buffer](const std::shared_ptr<data_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("periodic %s\n", task_name.c_str());

        temperature_sensor message1 = { 45.2, 51.72, 21.5 }; // NOLINT test values
        manufacturing_info message2 = { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } };
        free_text message3 = { "jojo rabbit" };

        bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer) };

        binary_stream.reset();
        binary_stream.write(message_type::temperature);
        message1.serialize(binary_stream);
        task_1->submit(buffer);

        binary_stream.reset();
        binary_stream.write(message_type::manufacturing);
        message2.serialize(binary_stream);
        task_1->submit(buffer);

        binary_stream.reset();
        binary_stream.write(message_type::freetext);
        message3.serialize(binary_stream);
        task_1->submit(buffer);
    };

    constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(500);
    auto task_2 = std::make_unique<my_data_periodic_task>(
        std::move(task_startup), std::move(task_2_periodic), context, "task 2", period, stack_size);

    constexpr const std::size_t wait_task_ms = 2500;
    tools::sleep_for(wait_task_ms);
}
#endif // #if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))

//--------------------------------------------------------------------------------------------------------------------------------

void test_data_task_perfect_forwarding()
{
    LOG_INFO("-- data task perfect forwarding --");
    print_stats();

    auto context = std::make_shared<data_task_context>();

    constexpr const std::size_t queue_depth = 4U;
    constexpr const std::size_t stack_size = 2048U;
    constexpr const int first_value = 1;            // NOLINT test/demo values
    constexpr const int second_value = 2;           // NOLINT test/demo values
    constexpr const int function_pointer_value = 42; // NOLINT test/demo values
    constexpr const int wait_processing_ms = 100;   // NOLINT test/demo values

    using simple_data_task = tools::data_task<data_task_context, int>;

    // lvalue call_back + lvalue data_call_back + lvalue std::string task name
    simple_data_task::call_back startup_lvalue
        = [](const std::shared_ptr<data_task_context>&, const std::string& task_name)
    {
        std::printf("starting %s\n", task_name.c_str());
    };

    simple_data_task::data_call_back process_lvalue
        = [](const std::shared_ptr<data_task_context>&, const int& data, const std::string& task_name)
    {
        std::printf("%s received %d\n", task_name.c_str(), data);
    };

    {
        std::string name_lvalue = "data_fwd_lvalue";
        auto task = std::make_unique<simple_data_task>(
            startup_lvalue, process_lvalue, context, queue_depth, name_lvalue, stack_size);
        task->submit(first_value);
        tools::sleep_for(wait_processing_ms);
    }

    // rvalue lambda startup + rvalue lambda process + rvalue std::string task name
    {
        auto task = std::make_unique<simple_data_task>(
            [](const std::shared_ptr<data_task_context>&, const std::string& task_name)
            {
                std::printf("starting %s\n", task_name.c_str());
            },
            [](const std::shared_ptr<data_task_context>&, const int& data, const std::string& task_name)
            {
                std::printf("%s received %d\n", task_name.c_str(), data);
            },
            context, queue_depth, std::string("data_fwd_rvalue"), stack_size);
        task->submit(second_value);
        tools::sleep_for(wait_processing_ms);
    }

    // function pointer conversion (captureless lambda cast to function pointer)
    {
        void (*startup_fp)(const std::shared_ptr<data_task_context>&, const std::string&)
            = [](const std::shared_ptr<data_task_context>&, const std::string& task_name)
        {
            std::printf("starting %s\n", task_name.c_str());
        };
        void (*process_fp)(const std::shared_ptr<data_task_context>&, const int&, const std::string&)
            = [](const std::shared_ptr<data_task_context>&, const int& data, const std::string& task_name)
        {
            std::printf("%s received %d\n", task_name.c_str(), data);
        };

        auto task = std::make_unique<simple_data_task>(
            startup_fp, process_fp, context, queue_depth, "data_fwd_fp", stack_size);
        task->submit(function_pointer_value);
        tools::sleep_for(wait_processing_ms);
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_json()
{
    LOG_INFO("-- json serialization/deserialization --");
    print_stats();

    {
        // create an empty structure (null)
        cjsonpp::JSONObject obj = {};
        cjsonpp::JSONObject obj2 = {};
        cjsonpp::JSONObject obj3 = {};
        cjsonpp::JSONObject obj4 = {};

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        constexpr const double number_pi = std::numbers::pi;
#else
        constexpr const double number_pi = 3.141592;
#endif
        obj.try_set("pi", number_pi);                  // add a number that is stored as double
        obj.try_set("happy", true);                    // add a Boolean that is stored as bool
        obj.try_set("name", "Niels");                  // add a string that is stored as std::string
        obj.try_set("nothing", cjsonpp::nullObject()); // add another null object by passing nullptr

        // add an array that is stored as std::vector (using an initializer list)
        std::vector<int> val = { 0, 1, 2 };
        obj.try_set("list", val);

        // add an object inside the object
        obj2.try_set("everything", 42); // NOLINT test value
        obj.try_set("answer", obj2);

        // add another object (using an initializer list of pairs)
        obj3.try_set("currency", "USD");
        obj4.try_set("value", 42.99); // NOLINT test value

        cjsonpp::JSONObject arr = cjsonpp::arrayObject();
        arr.try_add(obj3);
        arr.try_add(obj4);

        obj.try_set("object", arr);

        const auto str = obj.print();
        std::printf("%s\n", str.c_str());
    }

    {
        // create object from string literal
        std::string jsonstr = R"({ "happy": true, "pi": 3.141 })";
        auto parse_result = cjsonpp::parse_result(jsonstr);
        if (!parse_result)
        {
            LOG_ERROR("json parse failed: %s", parse_result.error().message.c_str());
            return;
        }

        cjsonpp::JSONObject obj = parse_result.value();

        const auto str = obj.print();
        std::printf("%s\n", str.c_str());
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_queued_json_data()
{
    LOG_INFO("-- queued json data --");
    print_stats();

    constexpr const std::size_t queue_depth = 128U;
    auto data_queue = std::make_unique<tools::sync_ring_buffer<std::string, queue_depth>>();

    {
        cjsonpp::JSONObject json = {};
        json.try_set("msg_type", "sensor");
        json.try_set("sensor_name", "indoor_temperature");
        json.try_set("temp", 19.47); // NOLINT test value
        json.try_set("activity", true);

        cjsonpp::JSONObject json_answer = {};
        json_answer.try_set("everything", 42); // NOLINT test value
        json.try_set("answer", json_answer);

        std::printf("%s\n", json.print(true).c_str());
        data_queue->emplace(json.print(false));
    }

    {
        cjsonpp::JSONObject json = {};
        json.try_set("msg_type", "time");
        json.try_set("yyyy_mm_dd", "2025/01/13");
        json.try_set("hh_mm_ss", "23:05:12");
        json.try_set("time_zone", "GMT+2");

        std::printf("%s\n", json.print(true).c_str());
        data_queue->emplace(json.print(false));
    }

    while (!data_queue->empty())
    {
        auto data = data_queue->front_pop();
        if (!data.has_value())
        {
            break;
        }

        auto parse_result = cjsonpp::parse_result(data.value());
        if (!parse_result)
        {
            LOG_ERROR("queue json parse failed: %s", parse_result.error().message.c_str());
            continue;
        }

        cjsonpp::JSONObject json = parse_result.value();

        const auto discriminant_result = json.try_get<std::string>("msg_type");
        if (!discriminant_result)
        {
            LOG_ERROR("missing discriminant: %s", discriminant_result.error().message.c_str());
            continue;
        }

        const auto discriminant = discriminant_result.value();

        if (discriminant == "sensor")
        {
            const auto name_result = json.try_get<std::string>("sensor_name");
            const auto temp_result = json.try_get<double>("temp");
            const auto activity_result = json.try_get<bool>("activity");
            const auto answer_obj_result = json.try_get<cjsonpp::JSONObject>("answer");
            if (!name_result || !temp_result || !activity_result || !answer_obj_result)
            {
                LOG_ERROR("sensor payload invalid");
                continue;
            }

            const auto answer_result = answer_obj_result.value().try_get<int>("everything");
            if (!answer_result)
            {
                LOG_ERROR("sensor answer invalid: %s", answer_result.error().message.c_str());
                continue;
            }

            const auto name = name_result.value();
            const auto temp = temp_result.value();
            const auto activity = activity_result.value();
            const auto answer = answer_result.value();
            std::printf(
                "sensor: %s - temp %f - %s - answer (%d)\n", name.c_str(), temp, activity ? "on" : "off", answer);
        }
        else if (discriminant == "time")
        {
            const auto time_date_result = json.try_get<std::string>("yyyy_mm_dd");
            const auto time_clock_result = json.try_get<std::string>("hh_mm_ss");
            const auto time_zone_result = json.try_get<std::string>("time_zone");
            if (!time_date_result || !time_clock_result || !time_zone_result)
            {
                LOG_ERROR("time payload invalid");
                continue;
            }

            const auto time_date = time_date_result.value();
            const auto time_clock = time_clock_result.value();
            const auto time_zone = time_zone_result.value();
            std::printf("time: %s - %s - %s\n", time_date.c_str(), time_clock.c_str(), time_zone.c_str());
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_packing_unpacking_json_data()
{
    LOG_INFO("-- packing/unpacking json data --");
    print_stats();

    // example taken from https://www.iotforall.com/10-jsonata-examples
    static const std::string json_str1 = R"(
    {
      "device": "dev:5c0272356817",
      "when": 1643398446,
      "body": {
        "humid": 56.23,
        "temp": 35.52
      },
      "best_location_type": "triangulated",
      "tower_country": "US",
      "tower_lat": 44.9288392,
      "tower_lon": -84.9283943,
      "tower_location": "Grand Ledge, MI",
      "tower_id": "310,410,25878,88213007",
      "tri_country": "US",
      "tri_lat": 44.76386883,
      "tri_lon": -83.64839822,
      "tri_location": "Lansing, MI",
      "settings": [
        { "name": "power_saving", "value": false },
        { "name": "detect_motion", "value": true },
        { "name": "sample_interval", "value": 5 }
      ]
    })";

    // example taken from https://json.org/example.html
    static const std::string json_str2 = R"(
    {
        "glossary": {
            "title": "example glossary",
            "GlossDiv": {
                "title": "S",
                "GlossList": {
                    "GlossEntry": {
                        "ID": "SGML",
                        "SortAs": "SGML",
                        "GlossTerm": "Standard Generalized Markup Language",
                        "Acronym": "SGML",
                        "Abbrev": "ISO 8879:1986",
                        "GlossDef": {
                            "para": "A meta-markup language, used to create markup languages such as DocBook.",
                            "GlossSeeAlso": ["GML", "XML"]
                        },
                        "GlossSee": "markup"
                    }
                }
            }
        }
    })";

    tools::gzip_wrapper gzip;

    std::vector<std::uint8_t> unpacked_buffer;
    std::vector<std::uint8_t> packed_buffer;

    // json 1

    unpacked_buffer.resize(json_str1.length() + 1U);
    std::memcpy(unpacked_buffer.data(), json_str1.c_str(), json_str1.length());
    unpacked_buffer[json_str1.length()] = '\0';

    std::printf("json file 1 of %zu bytes\n", unpacked_buffer.size());
    std::printf("packing json file 1\n");

    packed_buffer = gzip.pack(unpacked_buffer);

    std::printf("compressed to %zu bytes\n", packed_buffer.size());
    std::printf("unpacking gzip file 1\n");

    unpacked_buffer = gzip.unpack(packed_buffer);

    std::printf("unpacked to %zu bytes\n", unpacked_buffer.size());

    // json 2

    unpacked_buffer.resize(json_str2.length() + 1U);
    std::memcpy(unpacked_buffer.data(), json_str2.c_str(), json_str2.length());
    unpacked_buffer[json_str2.length()] = '\0';

    std::printf("json file 2 of %zu bytes\n", unpacked_buffer.size());
    std::printf("packing json file 2\n");

    packed_buffer = gzip.pack(unpacked_buffer);

    std::printf("compressed to %zu bytes\n", packed_buffer.size());
    std::printf("unpacking gzip file 2\n");

    unpacked_buffer = gzip.unpack(packed_buffer);

    std::printf("unpacked to %zu bytes\n", unpacked_buffer.size());
}

//--------------------------------------------------------------------------------------------------------------------------------

// clang-format off

// https://www.cppstories.com/2023/finite-state-machines-variant-cpp/

// possible states
namespace traffic_light_state
{
    struct off {};
    struct operable_initializing {};
    struct operable_red
    {
        int count = 0;
    };
    struct operable_orange
    {
        int count = 0;
    };
    struct operable_green
    {
        int count = 0;
    };
}

// possible events
namespace traffic_light_event
{
    struct power_on {};
    struct power_off {};
    struct init_done {};
    struct next_state {};
}

using traffic_light_state_v = std::variant<traffic_light_state::off,
                                           traffic_light_state::operable_initializing,
                                           traffic_light_state::operable_red,
                                           traffic_light_state::operable_orange,
                                           traffic_light_state::operable_green>;

using traffic_light_event_v = std::variant<traffic_light_event::power_on,
                                           traffic_light_event::power_off,
                                           traffic_light_event::init_done,
                                           traffic_light_event::next_state>;
// clang-format on

class traffic_light_fsm : tools::non_copyable // NOLINT inherits from non copyable and non movable class
{
public:
    traffic_light_fsm() = default;
    ~traffic_light_fsm() = default;

    void start();

    void handle_event(const traffic_light_event_v& event);
    void update();

private:
    traffic_light_state_v m_state = traffic_light_state::off {}; // init state
    bool m_entering_state = false;

    // states transitions methods
    // define callbacks for [state, event]

    traffic_light_state_v on_event(const traffic_light_state::off& state, const traffic_light_event::power_on& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_initializing& state, const traffic_light_event::init_done& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_red& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_orange& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_green& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_initializing& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_red& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_orange& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_green& state, const traffic_light_event::power_off& event);

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    // fallback for undefined transitions (C++20 abbreviated templates)
    traffic_light_state_v on_event(const auto& state, const auto& event);
#else
    template <typename State, typename Event>
    traffic_light_state_v dispatch_event(const State& state, const Event& event)
    {
        return dispatch_event_impl(state, event, 0);
    }

    template <typename State, typename Event>
    auto dispatch_event_impl(const State& state, const Event& event, int priority)
        -> decltype(this->on_event(state, event))
    {
        (void)priority;
        return this->on_event(state, event);
    }

    template <typename State, typename Event>
    traffic_light_state_v dispatch_event_impl(const State& state, const Event& event, long priority)
    {
        (void)priority;
        (void)event;
        LOG_ERROR("Unsupported state transition");
        // don't switch to other state and do not execute entering state callback
        m_entering_state = false;
        return state;
    }
#endif

    // defines callbacks for [state]
    void on_state(const traffic_light_state::off& state);
    void on_state(const traffic_light_state::operable_initializing& state);
    void on_state(const traffic_light_state::operable_red& state);
    void on_state(const traffic_light_state::operable_orange& state);
    void on_state(const traffic_light_state::operable_green& state);
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    // fallback for undefined state (C++20 abbreviated templates)
    void on_state(const auto& state);
#else
    template <typename State>
    void dispatch_state(const State& state)
    {
        dispatch_state_impl(state, 0);
    }

    template <typename State>
    auto dispatch_state_impl(const State& state, int priority) -> decltype(this->on_state(state), void())
    {
        (void)priority;
        this->on_state(state);
    }

    template <typename State>
    void dispatch_state_impl(const State& state, long priority)
    {
        (void)priority;
        (void)state;
        if (m_entering_state)
        {
            LOG_ERROR("Unsupported state");
            m_entering_state = false;
        }
    }
#endif
};

void traffic_light_fsm::start()
{
    m_state = traffic_light_state::off {}; // init state
    m_entering_state = true;
}

// event processing routines invoking states transitions routines through visitor
void traffic_light_fsm::handle_event(const traffic_light_event_v& event)
{
    // clang-format off
    m_state
        = std::visit(tools::detail::overload 
            { 
            [&](const auto& state, const auto& evt)
                { 
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
                return on_event(state, evt);
#else
                return dispatch_event(state, evt);
#endif
                }
            },
            m_state,
            event);
    // clang-format on
}

// update cycle based on current state
void traffic_light_fsm::update()
{
    // clang-format off
    std::visit(tools::detail::overload 
        { 
        [&](const auto& state)
            { 
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
                on_state(state);
#else
                dispatch_state(state);
#endif
            }
        },
        m_state);
    // clang-format on
}

constexpr const int traffic_light_wait_ms = 1000;

// define callbacks for [state, event]
traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::off& state, const traffic_light_event::power_on& event)
{
    (void)state;
    (void)event;
    std::printf("switch ON traffic light\n");    
    tools::sleep_for(traffic_light_wait_ms);
    m_entering_state = true;
    return traffic_light_state::operable_initializing {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::init_done& event)
{
    (void)state;
    (void)event;
    std::printf("init traffic light completed\n");
    m_entering_state = true;
    return traffic_light_state::operable_red { 0 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_red& state, const traffic_light_event::next_state& event)
{    
    (void)state;
    (void)event;
    tools::sleep_for(traffic_light_wait_ms);

    traffic_light_state_v next_state;
    constexpr const int max_cycles = 2;
    constexpr const int nb_light_states = 3;
    if (state.count < max_cycles * nb_light_states)
    {
        std::printf("traffic light RED --> ORANGE\n");
        next_state = traffic_light_state::operable_orange { state.count + 1 };
    }
    else
    {
        // finish the scenario
        std::printf("traffic light RED --> OFF\n");
        next_state = traffic_light_state::off {};
    }

    m_entering_state = true;

    return next_state;
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::next_state& event)
{    
    (void)state;
    (void)event;
    tools::sleep_for(traffic_light_wait_ms);
    std::printf("traffic light ORANGE --> GREEN\n");
    m_entering_state = true;
    return traffic_light_state::operable_green { state.count + 1 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_green& state, const traffic_light_event::next_state& event)
{
    (void)state;
    (void)event;
    tools::sleep_for(traffic_light_wait_ms);
    std::printf("traffic light GREEN --> RED\n");
    m_entering_state = true;
    return traffic_light_state::operable_red { state.count + 1 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_red& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_green& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
traffic_light_state_v traffic_light_fsm::on_event(const auto& state, const auto& event)
{
    (void)event;
    LOG_ERROR("Unsupported state transition");
    // don't switch to other state and do not execute entering state callback
    m_entering_state = false;
    return state;
}
#endif

// defines callbacks for [state]
void traffic_light_fsm::on_state(const traffic_light_state::off& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light off\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_initializing& state)
{
    (void)state;
    if (m_entering_state)
    {    
        std::printf("traffic light initializing\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_red& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light RED\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_orange& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light ORANGE\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_green& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light GREEN\n");
        m_entering_state = false;
    }
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
void traffic_light_fsm::on_state(const auto& state)
{
    (void)state;
    if (m_entering_state)
    {
        LOG_ERROR("Unsupported state");
        m_entering_state = false;
    }
}
#endif

void test_variant_fsm()
{
    LOG_INFO("-- finite state machine (std::variant) --");
    print_stats();

    traffic_light_fsm fsm;

    fsm.start();
    fsm.update();
    fsm.update();
    fsm.update();

    // callback is invoked on state transition

    fsm.handle_event(traffic_light_event::power_on {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::init_done {});
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    //fsm.handle_event(traffic_light_event::power_on {});
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    std::printf("end fsm test\n");     
}

//--------------------------------------------------------------------------------------------------------------------------------

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
void test_calendar_day()
{
    // https://www.modernescpp.com/index.php/c20-query-calendar-dates-and-ordinal-dates/

    LOG_INFO("calendar time and day");

    auto now = std::chrono::system_clock::now();   
    auto current_date = std::chrono::year_month_day(std::chrono::floor<std::chrono::days>(now));
    auto current_year = current_date.year();

    std::printf("The current year is %d\n", static_cast<int>(current_year));    
   
    auto moon_landing = std::chrono::year(1969)/std::chrono::month(7)/std::chrono::day(21); // NOLINT test value    

    //auto anniversary_week_day = std::chrono::year_month_weekday(moon_landing);  

    current_date = std::chrono::year_month_day(
        std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));  
    current_year = current_date.year();
    
    auto elapsed_years = static_cast<int>(current_date.year()) - static_cast<int>(moon_landing.year());
    std::printf("Elapsed years since moon landing: %d\n", elapsed_years);

}
#endif

//--------------------------------------------------------------------------------------------------------------------------------

void test_timer()
{
    LOG_INFO("timer");
    
    {
        tools::timer_scheduler timer_scheduler;

        constexpr const int period_200ms = 200;
        constexpr const int period_120ms = 120;
        constexpr const int period_100ms = 100;
        constexpr const int period_75ms = 75;
        constexpr const int period_50ms = 50;
        constexpr const int period_40ms = 40;
        constexpr const int period_25ms = 25;
        constexpr const int period_20ms = 20;

        // Test one shot timer - after completion
        constexpr const int test_value = 42;
        std::atomic<int> val(0);
        timer_scheduler.add("timer1", period_100ms, [&](tools::timer_handle) { val.store(test_value); }, tools::timer_type::one_shot);
        tools::sleep_for(period_120ms);
        std::printf("Expect %d is 42\n", val.load());

        // Test one shot timer - not started yet
        val.store(0);
        timer_scheduler.add("timer2", period_100ms, [&](tools::timer_handle) { val.store(test_value); }, tools::timer_type::one_shot);
        tools::sleep_for(period_50ms);
        std::printf("Expect %d is 0\n", val.load());

        // Test periodic timer (auto-reload) - check immediately started
        std::atomic<std::size_t> count(0U);
        auto timer_id = timer_scheduler.add("timer3", period_40ms, [&](tools::timer_handle) { ++count; }, tools::timer_type::periodic);
        tools::sleep_for(period_20ms);    
        timer_scheduler.remove(timer_id);
        std::printf("Expect count %zu is 1\n", count.load());

        // Test periodic timer (auto-reload) - check after 3 cycles
        count.store(0U);
        timer_id = timer_scheduler.add("timer4", period_40ms, [&](tools::timer_handle) { ++count; }, tools::timer_type::periodic);
        tools::sleep_for(period_100ms);    
        timer_scheduler.remove(timer_id);
        std::printf("Expect count %zu is 3\n", count.load());

        // Test delete periodic timer (auto-reload) in callback
        count.store(0U);
        timer_scheduler.add("timer5", period_25ms,
                [&](tools::timer_handle timer_id) {
                    ++count;
                    timer_scheduler.remove(timer_id);
                },
                tools::timer_type::periodic);
        tools::sleep_for(period_75ms);
        std::printf("Expect count %zu is 1\n", count.load());

        // Test periodic timer delays        
        auto start_point = std::chrono::high_resolution_clock::now();
        std::queue<std::chrono::high_resolution_clock::time_point> time_points;
        timer_id = timer_scheduler.add("timer6", period_40ms, [&](tools::timer_handle)
        {
            time_points.emplace(std::chrono::high_resolution_clock::now());
        }, tools::timer_type::periodic);
        tools::sleep_for(period_200ms);    
        timer_scheduler.remove(timer_id);

        auto prev_point = start_point;
        while(!time_points.empty())
        {
            const auto cur_point = time_points.front();
            time_points.pop();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(cur_point - prev_point);
            std::printf("timepoint (periodic): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
            prev_point = cur_point;
        }

        // Test one shot timer delay        
        start_point = std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point time_point;
        timer_scheduler.add("timer7", period_120ms, [&](tools::timer_handle)
        {
            time_point = std::chrono::high_resolution_clock::now();
        }, tools::timer_type::one_shot);
        tools::sleep_for(period_200ms);  

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time_point - start_point);
        std::printf("timepoint (one shot): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));

        // Test one shot timer delay (variant with std::chrono input)       
        start_point = std::chrono::high_resolution_clock::now();
        constexpr const int timer_timeout_us = 120250;
        timer_scheduler.add("timer7", std::chrono::duration<std::uint64_t, std::micro>(timer_timeout_us), [&](tools::timer_handle)
        {
            time_point = std::chrono::high_resolution_clock::now();
        }, tools::timer_type::one_shot);
        tools::sleep_for(period_200ms);  

        elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time_point - start_point);
        std::printf("timepoint (one shot): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

struct smp_task_context
{    
};

using periodic_task0 = tools::periodic_task<smp_task_context>;
using worker_task1 = tools::worker_task<smp_task_context>;

void test_smp_tasks_cpu_affinity()
{
    LOG_INFO("-- smp tasks with cpu affinity --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_task_context>();

    constexpr const std::size_t task1_stack_size = 2048U;
    constexpr const int core1 = 1;
    worker_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        static int count = 0;
        std::printf("%s (core 0): count %d\n", task_name.c_str(), count);
        ++count;

        // delegate work on core 1
        task1.delegate([](auto context, const auto& task_name)
        {
            (void)context;
            std::printf("%s (core 1): work\n", task_name.c_str());
        });
    };

    constexpr const auto period_100ms = std::chrono::duration<std::uint64_t, std::milli>(100);
    constexpr const std::size_t task0_stack_size = 4096U;
    constexpr const int core0 = 0; 

    periodic_task0 task0(startup, periodic_lambda, context, "periodic_task0", period_100ms, task0_stack_size, core0, tools::base_task::default_priority); 

    // sleep 2 sec
    constexpr const int wait_task_ms = 2000;
    tools::sleep_for(wait_task_ms);
}

//--------------------------------------------------------------------------------------------------------------------------------

constexpr const std::size_t worker_pipe_depth = 5U;
struct smp_ring_task_context
{    
    tools::lock_free_ring_buffer<std::uint8_t, worker_pipe_depth> m_to_worker_pipe;
    tools::lock_free_ring_buffer<std::uint8_t, worker_pipe_depth> m_from_worker_pipe;    
};

using periodic_ring_task0 = tools::periodic_task<smp_ring_task_context>;
using worker_ring_task1 = tools::worker_task<smp_ring_task_context>;

void test_smp_tasks_lock_free_ring_buffer()
{
    LOG_INFO("-- smp tasks with lock free ring buffer --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_ring_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_ring_task_context>();

    constexpr const std::size_t task1_stack_size = 2048U;
    constexpr const int core1 = 1;
    worker_ring_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_ring_task_context>& context, const std::string& task_name)
    {        
        static int count = 0;
        std::printf("%s (core 0): count %d\n", task_name.c_str(), count);
        ++count;

        constexpr const int mask = 0xff;
        (void)context->m_to_worker_pipe.push(static_cast<std::uint8_t>(count & mask));

        std::uint8_t val = 0U; 
        if (context->m_from_worker_pipe.pop(val))
        {
            std::printf("(core 0): received computed (from core 1) %" PRIu8 "\n", val);
        }

        // delegate work on core 1
        task1.delegate([](const auto& context, const auto& task_name)
        {
            (void)task_name;
            std::uint8_t value = 0U; 
            if (context->m_to_worker_pipe.pop(value))
            {
                (void)context->m_from_worker_pipe.push(static_cast<std::uint8_t>(value * value));
            }
        });
    };

    constexpr const auto period_100ms = std::chrono::duration<std::uint64_t, std::milli>(100); 
    constexpr const std::size_t task0_stack_size = 4096U;
    constexpr const int core0 = 0;
    periodic_ring_task0 task0(startup, periodic_lambda, context, "periodic_task0", period_100ms, task0_stack_size, core0, tools::base_task::default_priority); 

    constexpr const int wait_tasks_ms = 2000;
    tools::sleep_for(wait_tasks_ms);
}

//--------------------------------------------------------------------------------------------------------------------------------

// The variable used to hold the message buffer structure.
static tools::memory_pipe::static_buffer_holder static_buf_holder = {}; // NOLINT this is the purpose to have a statically allocated holder
// Used to dimension the array used to hold the messages.  The available space
// will actually be one less than this, so 999.
constexpr const std::size_t static_storage_size = 1000U;
static std::array<std::uint8_t, static_storage_size> static_storage; // NOLINT this is the purpose to have a statically allocated buffer

struct smp_mem_task_context
{
    tools::memory_pipe m_to_worker_pipe;    
    
    //smp_mem_task_context(std::size_t to_size) : m_to_worker_pipe(to_size)
    smp_mem_task_context(std::size_t to_size) : m_to_worker_pipe(to_size, static_storage.data(), &static_buf_holder)
    {
    }
};

using periodic_mem_task0 = tools::periodic_task<smp_mem_task_context>;
using worker_mem_task1 = tools::worker_task<smp_mem_task_context>;

void test_smp_tasks_memory_pipe()
{
    LOG_INFO("-- smp tasks with memory pipe --");
    print_stats();    

    auto startup = [](const std::shared_ptr<smp_mem_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    std::memset(&static_buf_holder, 0, sizeof(static_buf_holder));
    auto context = std::make_shared<smp_mem_task_context>(static_storage.size());

    {
        constexpr const std::size_t task1_stack_size = 2048U;
        constexpr const int core1 = 1;
        worker_mem_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

        static const std::string label = "this\nis\na\ntest\nto\ntransmit\nseveral\nmessages\nbetween\ntwo\ncores\n";

        std::atomic_bool stop(false);

        // delegate work on core 1
        task1.delegate([&stop](const auto& context, const auto& task_name)
        {
            std::printf("%s (core 1)\n", task_name.c_str());

            const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(20);        

            while (!stop.load())
            {
                constexpr const std::size_t bytes_to_receive = 128U;
                std::array<std::uint8_t, bytes_to_receive> received = {};
                const auto received_bytes = context->m_to_worker_pipe.receive_range(
                    received.begin(), received.end(), timeout);
                if (received_bytes > 0U)
                {
                    auto *iter = std::data(received);
                    for (std::size_t idx = 0U; idx < received_bytes; ++idx)
                    {
                        std::printf("%c", *iter++);
                    }
                }
            }

            std::printf("\n");
        });

        std::size_t offset = 0U;
        auto periodic_lambda = [&offset](const std::shared_ptr<smp_mem_task_context>& context, const std::string& task_name)
        { 
            std::printf(" / %s (core 0)\n", task_name.c_str());

            constexpr const std::size_t chunk_size = 16U; 
            // Parenthesized std::min avoids Windows min macro expansion if NOMINMAX is missing in a TU.
            std::size_t to_send = (std::min)(chunk_size, label.size() - offset);
            
            if (offset < label.size())
            {            
                const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(10);
                const std::string_view chunk(label.data() + offset, to_send);
                const auto sent = context->m_to_worker_pipe.send_range(chunk, timeout);
                offset += sent; // try again on characters left 
            }
        };

        // 50 ms period
        constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(50); 
        constexpr const std::size_t task0_stack_size = 4096U;
        constexpr const int core0 = 0;
        periodic_mem_task0 task0(startup, periodic_lambda, context, "periodic_task0", period, task0_stack_size, core0, tools::base_task::default_priority); 

        // sleep 2 sec
        constexpr const int wait_processing_ms = 2000;
        constexpr const int wait_join_ms = 250;
        tools::sleep_for(wait_processing_ms);
        stop.store(true);
        tools::sleep_for(wait_join_ms);
    }
}

void test_memory_pipe_perfect_forwarding()
{
    LOG_INFO("-- memory pipe perfect forwarding --");
    print_stats();

    constexpr const std::size_t capacity = 64U;
    tools::memory_pipe pipe(capacity);
    constexpr const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(100);
    constexpr const std::array<std::uint8_t, 4> lvalue_payload = { 1U, 2U, 3U, 4U };
    constexpr const std::array<std::uint8_t, 3> rvalue_payload = { 5U, 6U, 7U };
    constexpr const std::array<std::uint8_t, 4> conversion_payload = { 9U, 8U, 7U, 6U };

    std::vector<std::uint8_t> lvalue_data(lvalue_payload.begin(), lvalue_payload.end());
    std::vector<std::uint8_t> received;

    const auto lvalue_sent = pipe.send(lvalue_data, timeout);
    const auto lvalue_received = pipe.receive(received, lvalue_data.size(), timeout);
    std::printf("lvalue bytes: sent=%zu received=%zu\n", lvalue_sent, lvalue_received);

    const auto rvalue_sent = pipe.send(std::vector<std::uint8_t>(rvalue_payload.begin(), rvalue_payload.end()), timeout);
    const auto rvalue_received = pipe.receive(received, rvalue_payload.size(), timeout);
    std::printf("rvalue bytes: sent=%zu received=%zu\n", rvalue_sent, rvalue_received);

    const std::vector<std::uint8_t> range_values = { 10U, 11U, 12U, 13U };
    const auto range_sent = pipe.send_range(range_values, timeout);
    constexpr const std::size_t range_batch_size = 4U;
    std::array<std::uint8_t, range_batch_size> range_received = { 0U, 0U, 0U, 0U };
    const auto range_received_count = pipe.receive_range(range_received.begin(), range_received.end(), timeout);
    std::printf("range bytes: sent=%zu received=%zu\n", range_sent, range_received_count);

    struct vector_convertible_data
    {
        std::vector<std::uint8_t> payload;

        operator std::vector<std::uint8_t>() &&
        {
            return std::move(payload);
        }
    };

    auto converted = vector_convertible_data {
        std::vector<std::uint8_t>(conversion_payload.begin(), conversion_payload.end())
    };
    const auto conversion_sent = pipe.send(std::move(converted), timeout);
    const auto conversion_received = pipe.receive(received, conversion_payload.size(), timeout);
    std::printf("conversion bytes: sent=%zu received=%zu\n", conversion_sent, conversion_received);

    const auto isr_sent = pipe.isr_send_range({ 14U, 15U, 16U });
    std::array<std::uint8_t, 3> isr_received = { 0U, 0U, 0U };
    const auto isr_received_count = pipe.isr_receive_range(isr_received.begin(), isr_received.end());
    std::printf("isr range bytes: sent=%zu received=%zu\n", isr_sent, isr_received_count);
}

//--------------------------------------------------------------------------------------------------------------------------------


void test_tasks_priority()
{
    LOG_INFO("-- tasks with priority --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_task_context>();

    constexpr const auto task1_stack_size = 2048U;
    constexpr const auto task1_priority = 0; // lo prio 
    worker_task1 task1(startup, context, "worker_task1", task1_stack_size, tools::base_task::run_on_all_cores, task1_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        static int count = 0;
        std::printf("%s (hi prio): count %d\n", task_name.c_str(), count);
        ++count;

        // delegate work on lower priority task
        task1.delegate([](const auto& context, const auto& task_name)
        {
            (void)context;
            std::printf("%s (lo prio): work\n", task_name.c_str());
        });
    };

    constexpr const auto period_ms = 100;
    constexpr const auto task0_stack_size = 4096U;
    constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(period_ms); 
    periodic_task0 task0(startup, periodic_lambda, context, "periodic_task0", period, task0_stack_size, tools::base_task::run_on_all_cores, 3 /* prio + 3 */); 

    // sleep 2 sec
    constexpr const auto sleep_time_ms = 2000;
    tools::sleep_for(sleep_time_ms);
}

//--------------------------------------------------------------------------------------------------------------------------------

#if defined(ESP_PLATFORM)

constexpr std::size_t isr_queue_depth = 20;

struct isr_context
{
    tools::ring_buffer<std::pair<std::uint32_t, std::uint32_t>, 1024> storage;
    std::shared_ptr<tools::data_task<isr_context, std::uint32_t>> data_task;
    tools::ring_buffer<std::uint32_t, 1024> isr_queue;
};

using isr_data_task = tools::data_task<isr_context, std::uint32_t>;

static bool timer_isr_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx)
{
    (void)edata;
    static std::uint32_t counter = 0U;
    static auto prev_timepoint = std::chrono::high_resolution_clock::now();

    auto current_timepoint = std::chrono::high_resolution_clock::now();
    auto elapsed =  std::chrono::duration_cast<std::chrono::microseconds>(current_timepoint - prev_timepoint); 
    prev_timepoint = current_timepoint;

    isr_context* context = reinterpret_cast<isr_context*>(user_ctx);

    std::uint32_t value = counter++;

    context->data_task->isr_submit(value);

    if (counter > 1)
    {
        context->isr_queue.emplace(static_cast<std::uint32_t>(elapsed.count()));
    }    

    return false; /* YIELD_FROM_ISR already handled in submit data call */
}

static void task_isr_startup(const std::shared_ptr<isr_context>& context, const std::string& task_name)
{
    (void)context;
    std::printf("starting %s\n", task_name.c_str());
}

static void task_isr_processing(const std::shared_ptr<isr_context>& context, const std::uint32_t& data, const std::string& task_name)
{
    static auto prev_timepoint = std::chrono::high_resolution_clock::now();
    auto current_timepoint = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_timepoint - prev_timepoint);
    prev_timepoint = current_timepoint;

    context->storage.emplace(std::make_pair(data, static_cast<std::uint32_t>(elapsed.count())));
}

void test_hardware_timer_interrupt()
{
    LOG_INFO("-- hardware timer interrupt & data task --");
    print_stats();

    // https://phatiphatt.wordpress.com/esp32-sampling_mode/
    // https://www.electronicwings.com/esp32/esp32-timer-interrupts
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gptimer.html

    auto my_isr_context = std::make_shared<isr_context>();

    {
        auto my_isr_task = std::make_shared<isr_data_task>(task_isr_startup, task_isr_processing, my_isr_context, isr_queue_depth, "isr_task", 4096);
        my_isr_context->data_task = my_isr_task;

        std::printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

        tools::sleep_for(1000);

        gptimer_handle_t gptimer = nullptr;
        gptimer_config_t timer_config = {};

        timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
        timer_config.direction = GPTIMER_COUNT_UP;
        constexpr const int one_mhz_clock = 1 * 1000 * 1000; // 1MHz, 1 tick = 1us
        timer_config.resolution_hz = one_mhz_clock; 

        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

        gptimer_alarm_config_t alarm_config = {};

        constexpr const int counter_reload_val = 0; // counter will reload with 0 on alarm event
        constexpr const int period_1ms = 1000;      // period = 0.001s @resolution 1MHz
        alarm_config.reload_count = counter_reload_val;                  
        alarm_config.alarm_count = period_1ms;                
        alarm_config.flags.auto_reload_on_alarm = true; // enable auto-reload

        ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

        gptimer_event_callbacks_t cbs = {};
        cbs.on_alarm = timer_isr_handler; // register user callback

        ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, my_isr_context.get()));
        ESP_ERROR_CHECK(gptimer_enable(gptimer));
        ESP_ERROR_CHECK(gptimer_start(gptimer));

        tools::sleep_for(500);

        ESP_ERROR_CHECK(gptimer_stop(gptimer));
        ESP_ERROR_CHECK(gptimer_disable(gptimer));

        my_isr_context->data_task.reset();
    }

    while (!my_isr_context->storage.empty())
    {
        auto [counter, elapsed] = my_isr_context->storage.front();
        my_isr_context->storage.pop();

        if (!my_isr_context->isr_queue.empty())
        {
            auto isr_elapsed = my_isr_context->isr_queue.front();
            my_isr_context->isr_queue.pop();
            std::printf("ISR produce elapsed %04" PRIu32 " us - ", isr_elapsed);
        }

        std::printf("ISR counter %" PRIu32 " - consume elapsed %" PRIu32 " us\n", counter, elapsed);
    }

    tools::sleep_for(1000);
}

#endif

//--------------------------------------------------------------------------------------------------------------------------------

namespace
{
    constexpr std::size_t ALLOC_MAX_SIZE = 512;
    constexpr std::size_t ALLOC_ITERATIONS = 100000;

    struct alloc_data
    {
        std::vector<std::uint8_t> data;

        alloc_data(std::size_t size) : data(size)
        {
        } 
    };

    void alloc_dealloc_worker(int wid)
    {
        std::printf("-- worker %d\n", wid);

        for (std::size_t i = 0; i < ALLOC_ITERATIONS; ++i)
        {
            auto heap_block = std::make_unique<alloc_data>((i % ALLOC_MAX_SIZE) + 1);
        } // cleanup
    }
}

void test_allocator_stress()
{
    std::printf("-- allocator stress --\n");

    print_stats();

    auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_task_context>();

    constexpr const std::size_t task_stack_size = 2048U;
    constexpr const int core1 = 1;

    worker_task1 task1(startup, context, "worker_task1", task_stack_size, core1, tools::base_task::default_priority);

    const auto start = std::chrono::high_resolution_clock::now();

    // delegate work on core 0
    task1.delegate([](auto context, const auto& task_name)
    {
        (void)context;
        (void)task_name;
        alloc_dealloc_worker(2);
    });

    alloc_dealloc_worker(1);

    const auto end = std::chrono::high_resolution_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::printf("allocation/deallocation total time: %d ms\n", static_cast<int>(millis));
}

//--------------------------------------------------------------------------------------------------------------------------------

#if defined(USE_MEM_POOL_ALLOCATOR)
// https://www.rastergrid.com/blog/sw-eng/2021/03/custom-memory-allocators/
extern void init_mem_pool_allocator();
extern void destroy_mem_pool_allocator();
#endif

void runner()
{
    
#if defined(USE_MEM_POOL_ALLOCATOR)
    // prevent memory fragmentation with frequent heap allocations of
    // small events/messages
    init_mem_pool_allocator();
#endif

#if defined(ESP_PLATFORM)
    test_hardware_timer_interrupt();
#endif

    test_ring_buffer();
    test_ring_buffer_perfect_forwarding();
    test_ring_buffer_iteration();

    test_ring_vector();
    test_ring_vector_perfect_forwarding();
    test_ring_vector_resize();
    test_ring_vector_iteration();

    test_lock_free_ring_buffer();
    test_lock_free_ring_buffer_perfect_forwarding();
    test_lock_free_ring_buffer_range();
    test_lock_free_ring_buffer_task_stress();
    test_sync_ring_buffer();
    test_sync_ring_vector();
    test_sync_ring_vector_perfect_forwarding();
    test_sync_queue();
    test_sync_queue_perfect_forwarding();
    test_sync_dictionary();
    test_histogram_perfect_forwarding();
    test_sync_observer_perfect_forwarding();
    test_async_observer_perfect_forwarding();

    test_publish_subscribe();
    test_generic_task();
    test_generic_task_perfect_forwarding();
    test_periodic_task();
    test_periodic_task_perfect_forwarding();
    test_periodic_publish_subscribe();

    test_queued_commands();
    test_ring_buffer_commands();

    test_worker_tasks();
    test_worker_task_perfect_forwarding();
    test_data_task_perfect_forwarding();

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    test_queued_bytepack_data();
    test_aggregated_bytepack_data();
    test_bytepack_data_task();
#endif

    test_json();
    test_queued_json_data();

    test_packing_unpacking_json_data();

    test_variant_fsm();

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    test_calendar_day();
#endif

    test_timer();

    test_smp_tasks_cpu_affinity();
    test_smp_tasks_lock_free_ring_buffer();
    test_memory_pipe_perfect_forwarding();
    test_smp_tasks_memory_pipe();
    test_tasks_priority();
    
    test_allocator_stress();

#if defined(USE_MEM_POOL_ALLOCATOR)
    destroy_mem_pool_allocator();
#endif

    std::printf("This is The END\n");
}

//--------------------------------------------------------------------------------------------------------------------------------
#if defined(FREERTOS_PLATFORM)
// https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/freertos.html

constexpr const std::size_t stack_size = 8192;

// Structure that will hold the TCB of the task being created.
StaticTask_t x_task_buffer = {}; // NOLINT required for xTaskCreateStatic

// Buffer that the task being created will use as its stack.  Note this is
// an array of StackType_t variables.  The size of StackType_t is dependent on
// the RTOS port.
std::array<StackType_t, stack_size> x_stack = {}; // NOLINT required for xTaskCreateStatic

// Function that implements the task being created.
void v_task_code(void* pv_parameters)
{
    // The parameter value is expected to be 1 as 1 is passed in the
    // pvParameters value in the call to xTaskCreateStatic().
    configASSERT((std::uint32_t)pv_parameters == 1UL);

    runner();

    vTaskDelete(nullptr); // delete itself
}

// Function that creates a task.
void launch_runner() noexcept
{
    TaskHandle_t x_handle = nullptr;

    // Create the task without using any dynamic memory allocation.
    x_handle = xTaskCreateStatic(v_task_code, // Function that implements the task.
        "RUNNER",                             // Text name for the task.
        stack_size,                           // Stack size in bytes, not words.
        reinterpret_cast<void*>(1),           // Parameter passed into the task.
        tskIDLE_PRIORITY,                     // Priority at which the task is created.
        x_stack.data(),                       // Array to use as the task's stack.
        &x_task_buffer);                      // Variable to hold the task's data structure.

    (void)x_handle;

    vTaskSuspend(nullptr); // suspend main task
}
#endif
//--------------------------------------------------------------------------------------------------------------------------------

#if !defined(FREERTOS_PLATFORM)
#if defined(CPP_EXCEPTIONS_ENABLED)
void runner_except_catch()
{
    try
    {
        runner();
    }
    catch(std::exception& exc)
    {
        LOG_ERROR("Exception catched - %s", exc.what());
    }
}
#else
void runner_no_except()
{
    runner();
}
#endif
#endif

#if defined(FREERTOS_PLATFORM)
extern "C" void app_main() noexcept
#else
int main() noexcept
#endif
{
#if defined(FREERTOS_PLATFORM)
    launch_runner();
#elif defined(CPP_EXCEPTIONS_ENABLED)
    runner_except_catch();
    return 0;
#else
    runner_no_except();
    return 0;
#endif
}
