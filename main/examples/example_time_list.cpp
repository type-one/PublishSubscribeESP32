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

    constexpr const std::size_t worker_stack_size = 4096U;

    constexpr const int64_t worker_event_offset_sec_a = 3;
    constexpr const int64_t worker_event_offset_sec_b = 12;
    constexpr const int64_t worker_event_offset_sec_c = 24;
    constexpr const int64_t worker_event_offset_sec_d = 48;

    constexpr const int worker_struct_id_a = 10;
    constexpr const int worker_struct_id_b = 11;
    constexpr const int worker_struct_id_c = 12;
    constexpr const int worker_struct_id_d = 13;

    constexpr const float worker_struct_value_a = 12.7F;
    constexpr const float worker_struct_value_b = 9.4F;
    constexpr const float worker_struct_value_c = 31.6F;
    constexpr const float worker_struct_value_d = 42.9F;

    constexpr const int64_t worker_reading_offset_ms_a = 120;
    constexpr const int64_t worker_reading_offset_ms_b = 260;
    constexpr const int64_t worker_reading_offset_ms_c = 380;
    constexpr const int64_t worker_reading_offset_ms_d = 640;

    constexpr const long worker_tick_a = 130L;
    constexpr const long worker_tick_b = 270L;
    constexpr const long worker_tick_c = 610L;
    constexpr const long worker_tick_d = 820L;

    constexpr const int wait_for_worker_ms = 200;

    /** @brief Simple sensor reading carrying a device id and a floating-point measurement. */
    struct sensor_reading
    {
        int device_id;
        float value;
        std::string label;
    };

    struct sync_string_context
    {
        tools::sync_time_list<std::chrono::system_clock::time_point, std::string> shared_list;
        std::chrono::system_clock::time_point base_time;
    };

    struct sync_struct_context
    {
        tools::sync_time_list<std::chrono::steady_clock::time_point, sensor_reading> shared_list;
        std::chrono::steady_clock::time_point base_time;
    };

    struct sync_function_context
    {
        tools::sync_time_list<long, std::function<void()>> shared_list;
    };

    using sync_string_worker = tools::worker_task<sync_string_context>;
    using sync_struct_worker = tools::worker_task<sync_struct_context>;
    using sync_function_worker = tools::worker_task<sync_function_context>;

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

    /**
     * @brief Demonstrates @c sync_time_list with @c system_clock timestamps and string payloads,
     *        using main-thread + worker-task concurrent producers.
     */
    void test_sync_system_clock_string_list_worker_main()
    {
        LOG_INFO("-- sync_time_list: system_clock / string (main + worker) --");
        print_stats();

        auto context = std::make_shared<sync_string_context>();
        context->base_time = std::chrono::system_clock::now();

        sync_string_worker worker(
            [](const std::shared_ptr<sync_string_context>&, const std::string&) {},
            context, "sync_string_worker", worker_stack_size);

        worker.delegate([](const std::shared_ptr<sync_string_context>& local_context, const std::string&)
        {
            local_context->shared_list.push(
                local_context->base_time + std::chrono::seconds(worker_event_offset_sec_b), "Worker Event B - 12s");
            local_context->shared_list.push(
                local_context->base_time + std::chrono::seconds(worker_event_offset_sec_d), "Worker Event D - 48s");
        });

        context->shared_list.push(context->base_time + std::chrono::seconds(worker_event_offset_sec_c),
            "Main Event C - 24s");
        context->shared_list.push(context->base_time + std::chrono::seconds(worker_event_offset_sec_a),
            "Main Event A - 3s");

        tools::sleep_for(wait_for_worker_ms);

        std::printf("size after concurrent push: %zu\n", context->shared_list.size());
        while (!context->shared_list.empty())
        {
            const auto entry = context->shared_list.top_pop();
            if (entry.has_value())
            {
                std::printf("  %s\n", entry->second.c_str());
            }
        }
    }

    /**
     * @brief Demonstrates @c sync_time_list with @c steady_clock timestamps and struct payloads,
     *        using main-thread + worker-task concurrent producers.
     */
    void test_sync_steady_clock_struct_list_worker_main()
    {
        LOG_INFO("-- sync_time_list: steady_clock / struct (main + worker) --");
        print_stats();

        auto context = std::make_shared<sync_struct_context>();
        context->base_time = std::chrono::steady_clock::now();

        sync_struct_worker worker(
            [](const std::shared_ptr<sync_struct_context>&, const std::string&) {},
            context, "sync_struct_worker", worker_stack_size);

        worker.delegate([](const std::shared_ptr<sync_struct_context>& local_context, const std::string&)
        {
            local_context->shared_list.push(local_context->base_time + std::chrono::milliseconds(worker_reading_offset_ms_b),
                sensor_reading { worker_struct_id_b, worker_struct_value_b, "worker-beta" });
            local_context->shared_list.push(local_context->base_time + std::chrono::milliseconds(worker_reading_offset_ms_d),
                sensor_reading { worker_struct_id_d, worker_struct_value_d, "worker-delta" });
        });

        context->shared_list.push(context->base_time + std::chrono::milliseconds(worker_reading_offset_ms_c),
            sensor_reading { worker_struct_id_c, worker_struct_value_c, "main-gamma" });
        context->shared_list.push(context->base_time + std::chrono::milliseconds(worker_reading_offset_ms_a),
            sensor_reading { worker_struct_id_a, worker_struct_value_a, "main-alpha" });

        tools::sleep_for(wait_for_worker_ms);

        std::printf("size after concurrent push: %zu\n", context->shared_list.size());
        while (!context->shared_list.empty())
        {
            const auto entry = context->shared_list.top_pop();
            if (entry.has_value())
            {
                const auto& reading = entry->second;
                std::printf("  device=%d  value=%.2f  label=%s\n", reading.device_id,
                    static_cast<double>(reading.value), reading.label.c_str());
            }
        }
    }

    /**
     * @brief Demonstrates @c sync_time_list with integral timestamps and function payloads,
     *        using main-thread + worker-task concurrent producers.
     */
    void test_sync_integral_function_list_worker_main()
    {
        LOG_INFO("-- sync_time_list: integral / function (main + worker) --");
        print_stats();

        auto context = std::make_shared<sync_function_context>();

        sync_function_worker worker(
            [](const std::shared_ptr<sync_function_context>&, const std::string&) {},
            context, "sync_function_worker", worker_stack_size);

        worker.delegate([](const std::shared_ptr<sync_function_context>& local_context, const std::string&)
        {
            local_context->shared_list.push(worker_tick_b, []() { std::printf("  [tick 270] worker sync job B\n"); });
            local_context->shared_list.push(worker_tick_d, []() { std::printf("  [tick 820] worker sync job D\n"); });
        });

        context->shared_list.push(worker_tick_c, []() { std::printf("  [tick 610] main sync job C\n"); });
        context->shared_list.push(worker_tick_a, []() { std::printf("  [tick 130] main sync job A\n"); });

        tools::sleep_for(wait_for_worker_ms);

        std::printf("size after concurrent push: %zu\n", context->shared_list.size());
        while (!context->shared_list.empty())
        {
            const auto entry = context->shared_list.top_pop();
            if (entry.has_value() && entry->second)
            {
                entry->second();
            }
        }
    }

} // namespace

void run_example_time_list()
{
    test_system_clock_string_list();
    test_steady_clock_struct_list();
    test_integral_timestamp_function_list();
    test_sync_system_clock_string_list_worker_main();
    test_sync_steady_clock_struct_list_worker_main();
    test_sync_integral_function_list_worker_main();
}
