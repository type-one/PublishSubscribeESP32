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
 * @file example_timer_and_date.cpp
 * @brief Implements the calendar and timer examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/** @brief Demonstrates C++20 calendar utilities: computes the current year and years elapsed since the moon landing. */
void test_calendar_day()
{
    LOG_INFO("calendar time and day");

    constexpr const int moon_landing_year = 1969;
    constexpr const unsigned moon_landing_month = 7U;
    constexpr const unsigned moon_landing_day = 21U;

    auto now = std::chrono::system_clock::now();
    auto current_date = std::chrono::year_month_day(std::chrono::floor<std::chrono::days>(now));
    auto current_year = current_date.year();

    std::printf("The current year is %d\n", static_cast<int>(current_year));

    auto moon_landing = std::chrono::year(moon_landing_year) / std::chrono::month(moon_landing_month)
        / std::chrono::day(moon_landing_day);

    current_date = std::chrono::year_month_day(
        std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
    current_year = current_date.year();

    auto elapsed_years = static_cast<int>(current_date.year()) - static_cast<int>(moon_landing.year());
    std::printf("Elapsed years since moon landing: %d\n", elapsed_years);
}
#endif

/** @brief Exercises one-shot and periodic timer_scheduler callbacks, verifying timing with measured time points. */
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

        constexpr const int test_value = 42;
        std::atomic<int> val(0);
        // One-shot timer: value should flip exactly once after timeout.
        timer_scheduler.add("timer1", period_100ms, [&](tools::timer_handle) { val.store(test_value); }, tools::timer_type::one_shot);
        tools::sleep_for(period_120ms);
        std::printf("Expect %d is 42\n", val.load());

        val.store(0);
        timer_scheduler.add("timer2", period_100ms, [&](tools::timer_handle) { val.store(test_value); }, tools::timer_type::one_shot);
        tools::sleep_for(period_50ms);
        std::printf("Expect %d is 0\n", val.load());

        std::atomic<std::size_t> count(0U);
        // Periodic timer: count increments every period until removed.
        auto timer_id = timer_scheduler.add("timer3", period_40ms, [&](tools::timer_handle) { ++count; }, tools::timer_type::periodic);
        tools::sleep_for(period_20ms);
        timer_scheduler.remove(timer_id);
        std::printf("Expect count %zu is 1\n", count.load());

        count.store(0U);
        timer_id = timer_scheduler.add("timer4", period_40ms, [&](tools::timer_handle) { ++count; }, tools::timer_type::periodic);
        tools::sleep_for(period_100ms);
        timer_scheduler.remove(timer_id);
        std::printf("Expect count %zu is 3\n", count.load());

        count.store(0U);
        timer_scheduler.add("timer5", period_25ms,
            [&](tools::timer_handle local_timer_id)
            {
                ++count;
                timer_scheduler.remove(local_timer_id);
            },
            tools::timer_type::periodic);
        tools::sleep_for(period_75ms);
        std::printf("Expect count %zu is 1\n", count.load());

        auto start_point = std::chrono::high_resolution_clock::now();
        std::queue<std::chrono::high_resolution_clock::time_point> time_points;
        timer_id = timer_scheduler.add("timer6", period_40ms, [&](tools::timer_handle)
        {
            time_points.emplace(std::chrono::high_resolution_clock::now());
        }, tools::timer_type::periodic);
        tools::sleep_for(period_200ms);
        timer_scheduler.remove(timer_id);

        auto prev_point = start_point;
        // Log interval jitter between periodic callbacks.
        while (!time_points.empty())
        {
            const auto cur_point = time_points.front();
            time_points.pop();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(cur_point - prev_point);
            std::printf("timepoint (periodic): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
            prev_point = cur_point;
        }

        start_point = std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point time_point;
        timer_scheduler.add("timer7", period_120ms, [&](tools::timer_handle)
        {
            time_point = std::chrono::high_resolution_clock::now();
        }, tools::timer_type::one_shot);
        tools::sleep_for(period_200ms);

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time_point - start_point);
        std::printf("timepoint (one shot): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));

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
} // namespace

/** @brief Example entry point: runs the calendar date and timer scheduler examples. */
void run_example_timer_and_date()
{
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    // Start with calendar utilities to see how date arithmetic and formatting are exposed by the framework.
    test_calendar_day();
#endif
    // Continue with timer scheduling patterns (periodic and one-shot) that tasks can reuse in production code.
    test_timer();
}
