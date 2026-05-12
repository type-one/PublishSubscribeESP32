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
 * @file test_sync_priority_queue.cpp
 * @brief Unit tests for the sync_priority_queue container.
 * @author Laurent Lardinois
 * @date January 2025
 */

#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <vector>

#include "tools/async_observer.hpp"
#include "tools/sync_priority_queue.hpp"

namespace
{
    /**
     * @brief Basic test fixture for sync_priority_queue.
     */
    class SyncPriorityQueueTest : public ::testing::Test
    {
    protected:
        tools::sync_priority_queue<int> min_queue;
        tools::sync_max_priority_queue<int> max_queue;
    };

    /**
     * @brief Test thread_safe marker struct.
     */
    TEST_F(SyncPriorityQueueTest, ThreadSafeMarker)
    {
        EXPECT_TRUE((tools::sync_priority_queue<int>::thread_safe::value));
        EXPECT_TRUE((tools::sync_max_priority_queue<int>::thread_safe::value));
    }

    /**
     * @brief Test basic push and top operations.
     */
    TEST_F(SyncPriorityQueueTest, PushAndTop)
    {
        min_queue.push(5);
        min_queue.push(2);
        min_queue.push(8);

        auto top = min_queue.top();
        EXPECT_TRUE(top.has_value());
        EXPECT_EQ(top.value(), 2); // Min element should be on top
    }

    /**
     * @brief Test empty queue behavior.
     */
    TEST_F(SyncPriorityQueueTest, EmptyQueue)
    {
        EXPECT_TRUE(min_queue.empty());
        EXPECT_EQ(min_queue.size(), 0U);

        auto top = min_queue.top();
        EXPECT_FALSE(top.has_value());
    }

    /**
     * @brief Test top_pop operation.
     */
    TEST_F(SyncPriorityQueueTest, TopPopOrder)
    {
        min_queue.push(5);
        min_queue.push(2);
        min_queue.push(8);
        min_queue.push(1);
        min_queue.push(9);

        std::vector<int> result;
        while (!min_queue.empty())
        {
            auto elem = min_queue.top_pop();
            EXPECT_TRUE(elem.has_value());
            result.push_back(elem.value());
        }

        std::vector<int> expected = { 1, 2, 5, 8, 9 };
        EXPECT_EQ(result, expected);
    }

    /**
     * @brief Test top_pop on empty queue.
     */
    TEST_F(SyncPriorityQueueTest, TopPopEmpty)
    {
        auto elem = min_queue.top_pop();
        EXPECT_FALSE(elem.has_value());
    }

    /**
     * @brief Test size tracking.
     */
    TEST_F(SyncPriorityQueueTest, SizeTracking)
    {
        EXPECT_EQ(min_queue.size(), 0U);

        min_queue.push(1);
        EXPECT_EQ(min_queue.size(), 1U);

        min_queue.push(2);
        EXPECT_EQ(min_queue.size(), 2U);

        min_queue.pop();
        EXPECT_EQ(min_queue.size(), 1U);

        const auto result = min_queue.top_pop();
        EXPECT_EQ(min_queue.size(), 0U);
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 2);
    }

    /**
     * @brief Test max-heap ordering.
     */
    TEST_F(SyncPriorityQueueTest, MaxHeapOrder)
    {
        max_queue.push(5);
        max_queue.push(2);
        max_queue.push(8);
        max_queue.push(1);
        max_queue.push(9);

        std::vector<int> result;
        while (!max_queue.empty())
        {
            auto elem = max_queue.top_pop();
            EXPECT_TRUE(elem.has_value());
            result.push_back(elem.value());
        }

        std::vector<int> expected = { 9, 8, 5, 2, 1 };
        EXPECT_EQ(result, expected);
    }

    /**
     * @brief Test emplace operation.
     */
    TEST_F(SyncPriorityQueueTest, Emplace)
    {
        min_queue.emplace(3);
        min_queue.emplace(1);
        min_queue.emplace(2);

        auto first = min_queue.top_pop();
        EXPECT_TRUE(first.has_value());
        EXPECT_EQ(first.value(), 1);
    }

    /**
     * @brief Test push_range with iterator pair.
     */
    TEST_F(SyncPriorityQueueTest, PushRangeIterators)
    {
        std::vector<int> values = { 5, 2, 8, 1, 9 };
        min_queue.push_range(values.begin(), values.end());

        EXPECT_EQ(min_queue.size(), 5U);

        auto first = min_queue.top_pop();
        EXPECT_TRUE(first.has_value());
        EXPECT_EQ(first.value(), 1);
    }

    /**
     * @brief Test push_range with initializer_list.
     */
    TEST_F(SyncPriorityQueueTest, PushRangeInitializerList)
    {
        min_queue.push_range({ 5, 2, 8, 1, 9 });

        EXPECT_EQ(min_queue.size(), 5U);

        auto first = min_queue.top_pop();
        EXPECT_TRUE(first.has_value());
        EXPECT_EQ(first.value(), 1);
    }

    /**
     * @brief Test pop_range operation.
     */
    TEST_F(SyncPriorityQueueTest, PopRange)
    {
        min_queue.push_range({ 5, 2, 8, 1, 9 });

        std::vector<int> destination(3);
        std::size_t popped = min_queue.pop_range(destination.begin(), destination.end());

        EXPECT_EQ(popped, 3U);
        EXPECT_EQ(destination[0], 1);
        EXPECT_EQ(destination[1], 2);
        EXPECT_EQ(destination[2], 5);
    }

    /**
     * @brief Test pop_range on empty queue.
     */
    TEST_F(SyncPriorityQueueTest, PopRangeEmpty)
    {
        std::vector<int> destination(3);
        std::size_t popped = min_queue.pop_range(destination.begin(), destination.end());

        EXPECT_EQ(popped, 0U);
    }

    /**
     * @brief Test custom comparator with struct type.
     */
    struct priority_item
    {
        int priority;
        std::string data;

        priority_item() = default;
        priority_item(int priority_val, const std::string& data_val)
            : priority(priority_val)
            , data(data_val)
        {
        }

        bool operator<(const priority_item& other) const
        {
            return priority < other.priority;
        }

        bool operator>(const priority_item& other) const
        {
            return priority > other.priority;
        }
    };

    /**
     * @brief Test with custom struct and default comparator.
     */
    TEST(SyncPriorityQueueCustomType, DefaultComparator)
    {
        tools::sync_priority_queue<priority_item> queue;

        queue.push(priority_item(3, "low"));
        queue.push(priority_item(1, "high"));
        queue.push(priority_item(2, "medium"));

        auto first = queue.top_pop();
        EXPECT_TRUE(first.has_value());
        EXPECT_EQ(first.value().priority, 1);
        EXPECT_EQ(first.value().data, "high");
    }

    /**
     * @brief Test with custom struct and less comparator (max-heap).
     */
    TEST(SyncPriorityQueueCustomType, GreaterComparator)
    {
        tools::sync_priority_queue<priority_item, std::less<priority_item>> queue;

        queue.push(priority_item(3, "low"));
        queue.push(priority_item(1, "high"));
        queue.push(priority_item(2, "medium"));

        auto first = queue.top_pop();
        EXPECT_TRUE(first.has_value());
        EXPECT_EQ(first.value().priority, 3);
        EXPECT_EQ(first.value().data, "low");
    }

    /**
     * @brief Test top_pop_move with move-only type semantics.
     */
    TEST_F(SyncPriorityQueueTest, TopPopMove)
    {
        tools::sync_priority_queue<std::string> str_queue;

        str_queue.push("hello");
        str_queue.push("world");

        auto elem = str_queue.top_pop_move();
        EXPECT_TRUE(elem.has_value());
        // String order depends on comparison (lexicographic)
        EXPECT_TRUE(elem.value() == "hello" || elem.value() == "world");
    }

    /**
     * @brief Test async_observer with sync_priority_queue (transparent integration).
     */
    TEST(AsyncObserverWithPriorityQueue, TransparentIntegration)
    {
        enum class topic
        {
            event_a,
            event_b
        };

        struct event_data
        {
            int priority;
            std::string msg;

            event_data() = default;
            event_data(int p, const std::string& m)
                : priority(p)
                , msg(m)
            {
            }

            bool operator<(const event_data& other) const
            {
                return priority < other.priority;
            }
        };

        // Create async observer with sync_priority_queue
        // No API changes needed!
        tools::async_observer<topic, event_data, tools::sync_priority_queue> observer;

        observer.inform(topic::event_a, event_data(3, "low priority"), "test");
        observer.inform(topic::event_a, event_data(1, "high priority"), "test");
        observer.inform(topic::event_a, event_data(2, "medium priority"), "test");

        auto events = observer.pop_all_events();
        EXPECT_EQ(events.size(), 3U);

        // Events should be in priority order (1, 2, 3)
        EXPECT_EQ(std::get<1>(events[0]).priority, 1);
        EXPECT_EQ(std::get<1>(events[1]).priority, 2);
        EXPECT_EQ(std::get<1>(events[2]).priority, 3);
    }

    /**
     * @brief Test with tuple (as used in async_observer).
     */
    TEST(SyncPriorityQueueTuple, BasicOperation)
    {
        using tuple_type = std::tuple<int, int, std::string>;

        tools::sync_priority_queue<tuple_type> queue;

        queue.push(std::make_tuple(3, 100, "third"));
        queue.push(std::make_tuple(1, 50, "first"));
        queue.push(std::make_tuple(2, 75, "second"));

        auto first = queue.top_pop();
        EXPECT_TRUE(first.has_value());
        // Tuple comparison is lexicographic, so (1,50,"first") < (2,75,"second") < (3,100,"third")
        EXPECT_EQ(std::get<0>(first.value()), 1);
        EXPECT_EQ(std::get<2>(first.value()), "first");
    }

    /**
     * @brief Stress test with many elements.
     */
    TEST_F(SyncPriorityQueueTest, StressMany)
    {
        constexpr int num_elements = 1000;

        for (int i = num_elements; i > 0; --i)
        {
            min_queue.push(i);
        }

        EXPECT_EQ(min_queue.size(), static_cast<std::size_t>(num_elements));

        int expected = 1;
        while (!min_queue.empty())
        {
            auto elem = min_queue.top_pop();
            EXPECT_TRUE(elem.has_value());
            EXPECT_EQ(elem.value(), expected);
            ++expected;
        }

        EXPECT_EQ(expected, num_elements + 1);
    }

} // namespace
