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
 * @file example_time_list.cpp
 * @brief Implements the time_list example.
 * @author Laurent Lardinois
 * @date 2026-05-10
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
    constexpr const int64_t event_offset_sec_a = 5;
    constexpr const int64_t event_offset_sec_b = 10;
    constexpr const int64_t event_offset_sec_c = 20;
    constexpr const int64_t event_offset_sec_d = 30;
    constexpr const int64_t event_offset_sec_e = 60;

    constexpr const int64_t reading_offset_ms_a = 100;
    constexpr const int64_t reading_offset_ms_b = 200;
    constexpr const int64_t reading_offset_ms_c = 400;
    constexpr const int64_t reading_offset_ms_d = 550;
    constexpr const int64_t reading_offset_ms_e = 700;

    constexpr const int sensor_id_temperature = 1;
    constexpr const int sensor_id_voltage = 2;
    constexpr const int sensor_id_humidity = 3;
    constexpr const int sensor_id_current = 4;
    constexpr const int sensor_id_pressure = 5;

    constexpr const float value_temperature = 23.1F;
    constexpr const float value_voltage = 45.0F;
    constexpr const float value_humidity = 18.5F;
    constexpr const float value_current = 3.3F;
    constexpr const float value_pressure = 1013.2F;

    constexpr const long tick_boot = 50L;
    constexpr const long tick_init = 100L;
    constexpr const long tick_poll = 250L;
    constexpr const long tick_flush = 500L;
    constexpr const long tick_watchdog = 750L;
    constexpr const long tick_shutdown = 900L;

    /** @brief Simple sensor reading carrying a device id and a floating-point measurement. */
    struct sensor_reading
    {
        int device_id;
        float value;
        std::string label;
    };

    /**
     * @brief Demonstrates chronological ordering, top/pop, and empty checks with
     *        @c std::chrono::system_clock::time_point timestamps and @c std::string values.
     *
     * Entries are pushed in non-chronological order; the list always exposes the
     * earliest entry first. All entries are drained via top_pop() and their
     * timestamps and values are printed in chronological order.
     */
    void test_system_clock_string_list()
    {
        LOG_INFO("-- time_list: system_clock / string --");
        print_stats();

        const auto base_time = std::chrono::system_clock::now();

        tools::time_list<std::chrono::system_clock::time_point, std::string> timed_list;

        // Push entries intentionally out of chronological order.
        timed_list.push(base_time + std::chrono::seconds(event_offset_sec_d), "Event D - 30s");
        timed_list.push(base_time + std::chrono::seconds(event_offset_sec_a), "Event A - 5s");
        timed_list.push(base_time + std::chrono::seconds(event_offset_sec_e), "Event E - 60s");
        timed_list.push(base_time + std::chrono::seconds(event_offset_sec_b), "Event B - 10s");
        timed_list.push(base_time + std::chrono::seconds(event_offset_sec_c), "Event C - 20s");

        std::printf("size after push: %zu (expect 5)\n", timed_list.size());

        // Peek at the earliest entry without removing it.
        const auto earliest = timed_list.top();
        if (earliest.has_value())
        {
            std::printf("earliest (top): %s\n", earliest->second.c_str());
        }

        // Drain in chronological order.
        std::printf("draining in chronological order:\n");
        while (!timed_list.empty())
        {
            const auto entry = timed_list.top_pop();
            if (entry.has_value())
            {
                std::printf("  %s\n", entry->second.c_str());
            }
        }

        std::printf("size after drain: %zu (expect 0)\n", timed_list.size());
    }

    /**
     * @brief Demonstrates @c time_list with @c std::chrono::steady_clock::time_point timestamps
     *        and a user-defined @c sensor_reading value type.
     *
     * Shows that chronological pop order is independent of insertion order and
     * that the struct members are accessible through the returned optional pair.
     */
    void test_steady_clock_struct_list()
    {
        LOG_INFO("-- time_list: steady_clock / sensor_reading struct --");
        print_stats();

        const auto base_time = std::chrono::steady_clock::now();

        tools::time_list<std::chrono::steady_clock::time_point, sensor_reading> timed_list;

        // Push several readings with mixed timestamps.
        timed_list.push(base_time + std::chrono::milliseconds(reading_offset_ms_c),
            sensor_reading { sensor_id_humidity, value_humidity, "humidity" });
        timed_list.push(base_time + std::chrono::milliseconds(reading_offset_ms_a),
            sensor_reading { sensor_id_temperature, value_temperature, "temperature" });
        timed_list.push(base_time + std::chrono::milliseconds(reading_offset_ms_e),
            sensor_reading { sensor_id_pressure, value_pressure, "pressure" });
        timed_list.push(base_time + std::chrono::milliseconds(reading_offset_ms_b),
            sensor_reading { sensor_id_voltage, value_voltage, "voltage" });
        timed_list.push(base_time + std::chrono::milliseconds(reading_offset_ms_d),
            sensor_reading { sensor_id_current, value_current, "current" });

        std::printf("size after push: %zu (expect 5)\n", timed_list.size());

        // Drain and print each reading in ascending timestamp order.
        std::printf("sensor readings in chronological order:\n");
        while (!timed_list.empty())
        {
            const auto entry = timed_list.top_pop();
            if (entry.has_value())
            {
                const auto& reading = entry->second;
                std::printf("  device=%d  value=%.2f  label=%s\n", reading.device_id,
                    static_cast<double>(reading.value), reading.label.c_str());
            }
        }

        std::printf("size after drain: %zu (expect 0)\n", timed_list.size());
    }

    /**
     * @brief Demonstrates @c time_list with integral timestamps and @c std::function<void()> payloads.
     *
     * Functions are scheduled at integral tick values, pushed in arbitrary order,
     * then invoked in chronological (ascending tick) order by draining via top_pop().
     */
    void test_integral_timestamp_function_list()
    {
        LOG_INFO("-- time_list: integral timestamp / std::function --");
        print_stats();

        tools::time_list<long, std::function<void()>> timed_list;

        // Schedule callables at arbitrary tick values (not in order).
        timed_list.push(tick_flush, []() { std::printf("  [tick 500] periodic telemetry flush\n"); });
        timed_list.push(tick_init, []() { std::printf("  [tick 100] system initialised\n"); });
        timed_list.push(tick_watchdog, []() { std::printf("  [tick 750] watchdog reset\n"); });
        timed_list.push(tick_poll, []() { std::printf("  [tick 250] first sensor poll\n"); });
        timed_list.push(tick_shutdown, []() { std::printf("  [tick 900] shutdown requested\n"); });
        timed_list.push(tick_boot, []() { std::printf("  [tick  50] boot sequence start\n"); });

        std::printf("size after push: %zu (expect 6)\n", timed_list.size());

        // Peek: confirm earliest tick without removing.
        const auto top_entry = timed_list.top();
        if (top_entry.has_value())
        {
            std::printf("earliest tick (top): %ld\n", top_entry->first);
        }

        // Invoke callables in chronological order.
        std::printf("invoking functions in chronological order:\n");
        while (!timed_list.empty())
        {
            const auto entry = timed_list.top_pop();
            if (entry.has_value() && entry->second)
            {
                entry->second();
            }
        }

        std::printf("size after drain: %zu (expect 0)\n", timed_list.size());
    }

} // namespace

void run_example_time_list()
{
    test_system_clock_string_list();
    test_steady_clock_struct_list();
    test_integral_timestamp_function_list();
}
