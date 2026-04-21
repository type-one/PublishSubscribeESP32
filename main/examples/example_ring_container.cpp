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
 * @file example_ring_container.cpp
 * @brief Implements the ring buffer and ring vector examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
constexpr const double sample_value_1 = 5.6;
constexpr const double sample_value_2 = 7.2;
constexpr const double sample_value_3 = 1.2;
constexpr const double sample_value_4 = 9.1;
constexpr const double sample_value_5 = 10.1;
constexpr const double sample_value_6 = 7.5;

/** @brief Demonstrates push_range, pop_range, front, and pop operations on a @c tools::ring_buffer. */
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

    // Batch-pop demonstrates efficient draining when consumers can process in chunks.
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

/** @brief Exercises push/emplace/pop_move overloads of @c tools::ring_buffer with string lvalue, rvalue, implicit-conversion, and move-only element types. */
void test_ring_buffer_perfect_forwarding()
{
    LOG_INFO("-- ring buffer perfect forwarding --");
    print_stats();

    {
        constexpr const std::size_t queue_size = 8U;
        constexpr const std::size_t repeated_char_count = 4U;
        tools::ring_buffer<std::string, queue_size> str_queue;

        std::string lvalue = "lvalue-string";
        str_queue.push(lvalue);
        str_queue.push(std::string("rvalue-string"));
        str_queue.emplace(repeated_char_count, 'x');

        while (!str_queue.empty())
        {
            std::printf("%s\n", str_queue.front().c_str());
            str_queue.pop();
        }
    }

    {
        constexpr const std::size_t queue_size = 4U;
        // Move-only payloads verify ownership transfer APIs for non-copyable objects.
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

/** @brief Demonstrates basic emplace, front, and pop operations on a @c tools::ring_vector. */
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

/** @brief Exercises push/emplace/push_range/pop_range overloads of @c tools::ring_vector with lvalue, rvalue, implicit-conversion, and move-only element types. */
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

/** @brief Demonstrates resizing a @c tools::ring_vector while it holds elements and verifies that capacity grows and shrinks correctly. */
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

/** @brief Demonstrates index-based iteration, front/back access, and statistical computations (avg, variance, min/max) over a @c tools::ring_buffer. */
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

        val_queue->emplace(sample_value_1);
        val_queue->emplace(sample_value_2);
        val_queue->emplace(sample_value_3);
        val_queue->emplace(sample_value_4);
        val_queue->emplace(sample_value_5);
        val_queue->emplace(sample_value_6);

        std::printf("content\n");

        for (std::size_t i = 0U; i < val_queue->size(); ++i)
        {
            std::printf("%f\n", (*val_queue)[i]);
        }

        std::size_t cnt = val_queue->size() - 1U;

        for (std::size_t i = 0U; i < cnt; ++i)
        {
            std::printf("compute\n");

            // Take a stable snapshot before computing statistics on a mutating ring.
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

/** @brief Demonstrates index-based iteration, front/back access, and statistical computations (avg, variance, min/max) over a @c tools::ring_vector. */
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

        val_queue->emplace(sample_value_1);
        val_queue->emplace(sample_value_2);
        val_queue->emplace(sample_value_3);
        val_queue->emplace(sample_value_4);
        val_queue->emplace(sample_value_5);
        val_queue->emplace(sample_value_6);

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
} // namespace

void run_example_ring_container()
{
    // Learn baseline ring_buffer semantics first (push/pop/front/back behaviour).
    test_ring_buffer();
    // Then review forwarding overloads to pass lvalues/rvalues efficiently.
    test_ring_buffer_perfect_forwarding();
    // Inspect traversal and statistics patterns over buffered values.
    test_ring_buffer_iteration();
    // Repeat the same progression for ring_vector to compare API and behaviour.
    test_ring_vector();
    test_ring_vector_perfect_forwarding();
    // Validate resize behaviour when capacity must change at runtime.
    test_ring_vector_resize();
    test_ring_vector_iteration();
}
