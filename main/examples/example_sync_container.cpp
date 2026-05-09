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
 * @file example_sync_container.cpp
 * @brief Implements the lock-free and synchronized container examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
    /** @brief Demonstrates basic push/pop operations and boundary behaviour (full/empty) of @c
     * tools::lock_free_ring_buffer. */
    void test_lock_free_ring_buffer()
    {
        LOG_INFO("-- lock free ring buffer --");
        print_stats();

        constexpr const std::size_t queue_size_pow2 = 3U;
        constexpr const std::uint8_t val_overflow_probe_1 = 5U;
        constexpr const std::uint8_t val_overflow_probe_2 = 6U;
        constexpr const std::uint8_t val_overflow_probe_3 = 7U;
        constexpr const std::uint8_t val_overflow_probe_4 = 8U;

        auto str_queue = std::make_unique<tools::lock_free_ring_buffer<std::uint8_t, queue_size_pow2>>();
        bool result = false;

        result = str_queue->push(1U);
        result = result && str_queue->push(2U);
        result = result && str_queue->push(3U);

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

        result = str_queue->push(1U);
        result = result && str_queue->push(2U);
        result = result && str_queue->push(3U);
        result = result && str_queue->push(4U);

        std::printf("expect success - %s\n", result ? "success" : "failure");

        result = str_queue->push(val_overflow_probe_1);
        std::printf("expect success - %s\n", result ? "success" : "failure");

        result = str_queue->push(val_overflow_probe_2);
        std::printf("expect success - %s\n", result ? "success" : "failure");

        result = str_queue->push(val_overflow_probe_3);
        std::printf("expect success - %s\n", result ? "success" : "failure");

        result = str_queue->push(val_overflow_probe_4);
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

    /** @brief Exercises push/pop_opt overloads of @c tools::lock_free_ring_buffer with integer lvalue, rvalue, and
     * implicit-conversion inputs. */
    void test_lock_free_ring_buffer_perfect_forwarding()
    {
        LOG_INFO("-- lock free ring buffer perfect forwarding --");
        print_stats();

        {
            constexpr const std::size_t queue_size_pow2 = 2U;
            constexpr const std::uint16_t val_first = 10U;
            constexpr const std::uint16_t val_second = 20U;
            constexpr const std::uint16_t val_third = 30U;
            tools::lock_free_ring_buffer<std::uint16_t, queue_size_pow2> buf;

            std::uint16_t lvalue = val_first;
            (void)buf.push(lvalue);
            (void)buf.push(std::uint16_t(val_second));
            (void)buf.push(static_cast<std::uint16_t>(val_third));

            while (true)
            {
                auto item = buf.pop_opt();
                if (!item.has_value())
                {
                    break;
                }
                std::printf("%u\n", static_cast<unsigned>(item.value()));
            }
        }

        {
            constexpr const std::size_t queue_size_pow2 = 2U;
            tools::lock_free_ring_buffer<float, queue_size_pow2> float_buf;

            (void)float_buf.push(1);
            (void)float_buf.push(2);
            (void)float_buf.push(3);

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

    /** @brief Demonstrates push_range (initialiser list and vector) and iterator-pair pop_range on a @c
     * tools::lock_free_ring_buffer. */
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
            std::printf("  batch[%zu] = %d\n", idx, batch_ptr[idx]);
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

    /** @brief Stress-tests @c tools::lock_free_ring_buffer with concurrent producer and consumer generic tasks,
     * verifying checksum integrity and throughput. */
    void test_lock_free_ring_buffer_task_stress()
    {
        LOG_INFO("-- lock free ring buffer task stress --");
        print_stats();

        constexpr const std::size_t stress_buffer_pow2 = 10U;

        /** @brief Shared producer/consumer state used for the lock-free ring-buffer stress benchmark. */
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
                        // Busy-wait with yield to model non-blocking lock-free producer behaviour.
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
                        // Consumer mirrors producer strategy and validates ordering guarantees.
                        while (!shared_context->buffer.pop(popped_value))
                        {
                            tools::yield();
                        }

                        if (popped_value != expected)
                        {
                            std::printf(
                                "stress mismatch: expected=%" PRIu32 " got=%" PRIu32 "\n", expected, popped_value);
                        }

                        local_sum += popped_value;
                        shared_context->consumed_count.fetch_add(1U);
                    }
                    shared_context->consumer_sum.store(local_sum);
                },
                context, "lf_rb_stress_consumer", task_stack);
        }

        const auto end = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

        std::printf("stress consumed=%" PRIu32 "/%" PRIu32 " in %lld ms\n", context->consumed_count.load(), item_count,
            static_cast<long long>(elapsed_ms));
        std::printf("stress checksum producer=%" PRIu64 " consumer=%" PRIu64 "\n", context->producer_sum.load(),
            context->consumer_sum.load());
    }

    /** @brief Exercises push/emplace/push_range/pop_range overloads of @c tools::sync_ring_buffer with string lvalue,
     * rvalue, conversion, and move-only element types. */
    void test_sync_ring_buffer()
    {
        LOG_INFO("-- sync ring buffer --");
        print_stats();

        {
            constexpr const std::size_t queue_size = 64U;
            constexpr const std::size_t repeated_char_count = 5U;
            tools::sync_ring_buffer<std::string, queue_size> str_queue;

            std::string lvalue = "sync-lvalue";
            str_queue.push(lvalue);
            str_queue.push(std::string("sync-rvalue"));
            str_queue.emplace(repeated_char_count, 'z');
            str_queue.push_range({ "sync-range-1", "sync-range-2" });

            constexpr const std::array<std::string_view, 2U> ar_values = { "sync-ar-range-1", "sync-ar-range-2" };
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

    /** @brief Demonstrates emplace, push_range, pop_range, and front_pop operations on a @c tools::sync_ring_vector. */
    void test_sync_ring_vector()
    {
        LOG_INFO("-- sync ring vector --");
        print_stats();

        constexpr const std::size_t queue_size = 64U;
        tools::sync_ring_vector<std::string> str_queue(queue_size);

        str_queue.emplace("toto");
        str_queue.push_range({ "sync-ring-vector-basic-range-1", "sync-ring-vector-basic-range-2" });

        constexpr const std::array<std::string_view, 2U> ar_values
            = { "sync-ring-vector-basic-ar-1", "sync-ring-vector-basic-ar-2" };
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

    /** @brief Exercises push/emplace/push_range/pop_range overloads of @c tools::sync_ring_vector with string lvalue,
     * rvalue, conversion, and move-only element types. */
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

            constexpr const std::array<std::string_view, 2U> ar_values
                = { "sync-ring-vector-ar-range-1", "sync-ring-vector-ar-range-2" };
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

    /** @brief Demonstrates basic emplace and front_pop operations on a @c tools::sync_queue. */
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

    /** @brief Exercises push/emplace/push_range/pop_range overloads of @c tools::sync_queue with lvalue, rvalue,
     * implicit conversion, brace-init, range-view, and move-only inputs. */
    void test_sync_queue_perfect_forwarding()
    {
        LOG_INFO("-- sync queue perfect forwarding --");
        print_stats();

        {
            tools::sync_queue<std::string> str_queue;

            std::string lvalue = "sync-queue-lvalue";
            str_queue.push(lvalue);
            str_queue.push(std::string("sync-queue-rvalue"));
            str_queue.push("sync-queue-conversion");
            str_queue.push({ "sync-queue-brace-init" });
            str_queue.emplace(4U, 'q');

            constexpr const std::array<std::string_view, 2U> range_lvalue
                = { "sync-queue-range-lvalue-1", "sync-queue-range-lvalue-2" };
            constexpr const std::array<std::string_view, 2U> range_ar
                = { "sync-queue-range-ar-1", "sync-queue-range-ar-2" };

            str_queue.push_range(range_lvalue);
            str_queue.push_range(range_ar);
            str_queue.push_range({ "sync-queue-brace-range-1", "sync-queue-brace-range-2" });

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            constexpr const std::array<std::size_t, 2U> suffixes = { 1U, 2U };
            // C++20 ranges path: demonstrate direct view pipeline submission into queue APIs.
            const auto transformed = suffixes
                | std::views::transform([](const std::size_t suffix)
                    { return std::string("sync-queue-ranges-view-") + std::to_string(suffix); });
            str_queue.push_range(transformed);
#else
            const std::array<std::string, 2U> transformed
                = { "sync-queue-ranges-fallback-1", "sync-queue-ranges-fallback-2" };
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

    /** @brief Exercises add/add_range/find/contains/remove/remove_collection operations on a @c tools::sync_dictionary.
     */
    void test_sync_dictionary()
    {
        LOG_INFO("-- sync dictionary --");
        print_stats();

        tools::sync_dictionary<std::string, std::string> str_dict;

        std::string key_lvalue = "key-lvalue";
        std::string value_lvalue = "value-lvalue";

        str_dict.add(key_lvalue, value_lvalue);
        str_dict.add(std::string("key-rvalue"), std::string("value-rvalue"));
        str_dict.add("key-conversion", "value-conversion");
        str_dict.add("key-conversion", std::string("value-updated"));

        const std::vector<std::pair<std::string, std::string>> range_values
            = { { "key-range-1", "value-range-1" }, { "key-range-2", "value-range-2" } };
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
} // namespace

void run_example_sync_container()
{
    // First inspect lock-free single-producer/single-consumer style ring-buffer primitives.
    test_lock_free_ring_buffer();
    test_lock_free_ring_buffer_perfect_forwarding();
    test_lock_free_ring_buffer_range();
    // Stress the lock-free buffer under concurrent producer/consumer task load.
    test_lock_free_ring_buffer_task_stress();
    // Continue with synchronized container variants for multi-thread-safe usage patterns.
    test_sync_ring_buffer();
    test_sync_ring_vector();
    test_sync_ring_vector_perfect_forwarding();
    // Compare queue semantics and forwarding/range APIs.
    test_sync_queue();
    test_sync_queue_perfect_forwarding();
    // Finish with synchronized dictionary CRUD and collection operations.
    test_sync_dictionary();
}
