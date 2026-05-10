/**
 * @file test_time_list.cpp
 * @brief Unit tests for the tools::time_list class template.
 *
 * This file contains a comprehensive set of test cases for tools::time_list,
 * verifying chronological ordering, push/pop/emplace semantics, top/top_pop,
 * empty/size/clear, snapshot_sorted, and behaviour with various timestamp and
 * value types (integral timestamps, chrono time_points, and callable payloads).
 *
 * @author Laurent Lardinois and Copilot
 * @date 2026-05-10
 */

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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/time_list.hpp"

// ---------------------------------------------------------------------------
// Fixture – integral timestamp (long) with std::string value
// ---------------------------------------------------------------------------

/**
 * @brief Test fixture for tools::time_list with an integral timestamp type.
 */
class TimeListIntTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tl = std::make_unique<tools::time_list<long, std::string>>();
    }

    void TearDown() override
    {
        tl.reset();
    }

    std::unique_ptr<tools::time_list<long, std::string>> tl;
};

// ---------------------------------------------------------------------------
// Empty / size / clear
// ---------------------------------------------------------------------------

/** @brief A freshly constructed time_list must report empty and size zero. */
TEST_F(TimeListIntTest, StartsEmpty)
{
    EXPECT_TRUE(tl->empty());
    EXPECT_EQ(tl->size(), 0U);
}

/** @brief top() and top_pop() return nullopt on an empty list. */
TEST_F(TimeListIntTest, TopOnEmptyReturnsNullopt)
{
    EXPECT_FALSE(tl->top().has_value());
    EXPECT_FALSE(tl->top_pop().has_value());
}

/** @brief pop() on an empty list does not crash. */
TEST_F(TimeListIntTest, PopOnEmptyIsSafe)
{
    EXPECT_NO_THROW(tl->pop());
    EXPECT_TRUE(tl->empty());
}

/** @brief After pushing one entry the size must be 1 and empty() must be false. */
TEST_F(TimeListIntTest, SizeAfterOnePush)
{
    tl->push(10L, "alpha");
    EXPECT_FALSE(tl->empty());
    EXPECT_EQ(tl->size(), 1U);
}

/** @brief clear() removes all entries and resets size to zero. */
TEST_F(TimeListIntTest, ClearEmptiesTheList)
{
    tl->push(10L, "alpha");
    tl->push(20L, "beta");
    tl->push(30L, "gamma");
    ASSERT_EQ(tl->size(), 3U);

    tl->clear();

    EXPECT_TRUE(tl->empty());
    EXPECT_EQ(tl->size(), 0U);
    EXPECT_FALSE(tl->top().has_value());
}

// ---------------------------------------------------------------------------
// Chronological ordering with integral timestamps
// ---------------------------------------------------------------------------

/** @brief Entries pushed out of order are always returned in ascending timestamp order. */
TEST_F(TimeListIntTest, ChronologicalOrderIntTimestamp)
{
    tl->push(300L, "three hundred");
    tl->push(100L, "one hundred");
    tl->push(200L, "two hundred");
    tl->push(50L,  "fifty");
    tl->push(400L, "four hundred");

    constexpr const std::size_t expected_count = 5U;
    ASSERT_EQ(tl->size(), expected_count);

    const std::vector<long> expected_order = {50L, 100L, 200L, 300L, 400L};
    std::vector<long> actual_order;

    while (!tl->empty())
    {
        const auto entry = tl->top_pop();
        ASSERT_TRUE(entry.has_value());
        actual_order.push_back(entry->first);
    }

    EXPECT_EQ(actual_order, expected_order);
}

/** @brief top() peeks the earliest timestamp without removing it. */
TEST_F(TimeListIntTest, TopDoesNotRemoveEntry)
{
    tl->push(20L, "b");
    tl->push(10L, "a");

    const auto peeked = tl->top();
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(peeked->first, 10L);
    EXPECT_EQ(peeked->second, "a");

    // Size must remain unchanged after top().
    EXPECT_EQ(tl->size(), 2U);
}

/** @brief pop() removes the earliest entry; the next top() returns the successor. */
TEST_F(TimeListIntTest, PopRemovesEarliestEntry)
{
    tl->push(20L, "b");
    tl->push(10L, "a");

    tl->pop();

    ASSERT_EQ(tl->size(), 1U);
    const auto next_top = tl->top();
    ASSERT_TRUE(next_top.has_value());
    EXPECT_EQ(next_top->first, 20L);
    EXPECT_EQ(next_top->second, "b");
}

/** @brief top_pop() returns the entry AND reduces the size. */
TEST_F(TimeListIntTest, TopPopReturnsThenRemoves)
{
    tl->push(5L, "five");
    tl->push(1L, "one");

    const auto entry = tl->top_pop();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->first, 1L);
    EXPECT_EQ(entry->second, "one");
    EXPECT_EQ(tl->size(), 1U);
}

// ---------------------------------------------------------------------------
// snapshot_sorted
// ---------------------------------------------------------------------------

/** @brief snapshot_sorted() returns all entries in ascending timestamp order without modifying the list. */
TEST_F(TimeListIntTest, SnapshotSortedPreservesListAndReturnsOrder)
{
    tl->push(30L, "c");
    tl->push(10L, "a");
    tl->push(20L, "b");

    const auto snap = tl->snapshot_sorted();

    // Original list must still contain all three entries.
    EXPECT_EQ(tl->size(), 3U);

    // Snapshot must be sorted ascending.
    ASSERT_EQ(snap.size(), 3U);
    EXPECT_EQ(snap[0].first, 10L);
    EXPECT_EQ(snap[1].first, 20L);
    EXPECT_EQ(snap[2].first, 30L);
    EXPECT_EQ(snap[0].second, "a");
    EXPECT_EQ(snap[1].second, "b");
    EXPECT_EQ(snap[2].second, "c");
}

/** @brief snapshot_sorted() on an empty list returns an empty vector. */
TEST_F(TimeListIntTest, SnapshotSortedOnEmptyReturnsEmpty)
{
    EXPECT_TRUE(tl->snapshot_sorted().empty());
}

// ---------------------------------------------------------------------------
// Emplace
// ---------------------------------------------------------------------------

/** @brief emplace() constructs the value in-place and respects chronological order. */
TEST_F(TimeListIntTest, EmplaceInsertsInOrder)
{
    tl->emplace(50L, "fifty");
    tl->emplace(10L, "ten");
    tl->emplace(30L, "thirty");

    const auto first = tl->top_pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->first, 10L);

    const auto second = tl->top_pop();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->first, 30L);

    const auto third = tl->top_pop();
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(third->first, 50L);
}

// ---------------------------------------------------------------------------
// Fixture – chrono timestamp (steady_clock) with int value
// ---------------------------------------------------------------------------

/**
 * @brief Test fixture for tools::time_list with a chrono time_point timestamp.
 */
class TimeListChronoTest : public ::testing::Test
{
protected:
    using timestamp_type = std::chrono::steady_clock::time_point;
    using tl_type = tools::time_list<timestamp_type, int>;

    void SetUp() override
    {
        base = std::chrono::steady_clock::now();
        tl = std::make_unique<tl_type>();
    }

    void TearDown() override
    {
        tl.reset();
    }

    timestamp_type base;
    std::unique_ptr<tl_type> tl;
};

/** @brief Entries with chrono time_point timestamps are returned in ascending order. */
TEST_F(TimeListChronoTest, ChronologicalOrderChronoTimestamp)
{
    tl->push(base + std::chrono::milliseconds(300), 3);
    tl->push(base + std::chrono::milliseconds(100), 1);
    tl->push(base + std::chrono::milliseconds(500), 5);
    tl->push(base + std::chrono::milliseconds(200), 2);
    tl->push(base + std::chrono::milliseconds(400), 4);

    constexpr const std::size_t expected_count = 5U;
    ASSERT_EQ(tl->size(), expected_count);

    const std::vector<int> expected_values = {1, 2, 3, 4, 5};
    std::vector<int> actual_values;

    while (!tl->empty())
    {
        const auto entry = tl->top_pop();
        ASSERT_TRUE(entry.has_value());
        actual_values.push_back(entry->second);
    }

    EXPECT_EQ(actual_values, expected_values);
}

/** @brief top() returns nullopt when a chrono time_list is empty. */
TEST_F(TimeListChronoTest, TopOnEmptyChronoListReturnsNullopt)
{
    EXPECT_FALSE(tl->top().has_value());
}

/** @brief pop() on an empty chrono time_list does not crash. */
TEST_F(TimeListChronoTest, PopOnEmptyChronoListIsSafe)
{
    EXPECT_NO_THROW(tl->pop());
}

/** @brief snapshot_sorted() on a chrono time_list returns entries in ascending timestamp order. */
TEST_F(TimeListChronoTest, SnapshotSortedChronoTimestamp)
{
    tl->push(base + std::chrono::seconds(2), 200);
    tl->push(base + std::chrono::seconds(1), 100);
    tl->push(base + std::chrono::seconds(3), 300);

    const auto snap = tl->snapshot_sorted();

    ASSERT_EQ(snap.size(), 3U);
    EXPECT_EQ(snap[0].second, 100);
    EXPECT_EQ(snap[1].second, 200);
    EXPECT_EQ(snap[2].second, 300);

    // List must still be intact.
    EXPECT_EQ(tl->size(), 3U);
}

// ---------------------------------------------------------------------------
// Callable payload – std::function<void()>
// ---------------------------------------------------------------------------

/**
 * @brief Test fixture for tools::time_list with integral timestamps and callable payloads.
 */
class TimeListCallableTest : public ::testing::Test
{
protected:
    using tl_type = tools::time_list<long, std::function<void()>>;

    void SetUp() override
    {
        tl = std::make_unique<tl_type>();
    }

    void TearDown() override
    {
        tl.reset();
    }

    std::unique_ptr<tl_type> tl;
};

/** @brief Callables are invoked in chronological (ascending tick) order when drained via top_pop(). */
TEST_F(TimeListCallableTest, CallablesInvokedInChronologicalOrder)
{
    std::vector<int> invocation_order;

    tl->push(300L, [&invocation_order]() { invocation_order.push_back(3); });
    tl->push(100L, [&invocation_order]() { invocation_order.push_back(1); });
    tl->push(400L, [&invocation_order]() { invocation_order.push_back(4); });
    tl->push(200L, [&invocation_order]() { invocation_order.push_back(2); });

    while (!tl->empty())
    {
        const auto entry = tl->top_pop();
        ASSERT_TRUE(entry.has_value());
        ASSERT_TRUE(static_cast<bool>(entry->second));
        entry->second();
    }

    const std::vector<int> expected = {1, 2, 3, 4};
    EXPECT_EQ(invocation_order, expected);
}

/** @brief A callable at the earliest tick fires before others regardless of push order. */
TEST_F(TimeListCallableTest, EarliestCallableFiresFirst)
{
    std::atomic<int> first_fired {0};

    tl->push(500L, [&first_fired]()
    {
        int expected_zero = 0;
        first_fired.compare_exchange_strong(expected_zero, 500);
    });
    tl->push(100L, [&first_fired]()
    {
        int expected_zero = 0;
        first_fired.compare_exchange_strong(expected_zero, 100);
    });
    tl->push(300L, [&first_fired]()
    {
        int expected_zero = 0;
        first_fired.compare_exchange_strong(expected_zero, 300);
    });

    // Only fire the very first entry (earliest tick = 100).
    const auto entry = tl->top_pop();
    ASSERT_TRUE(entry.has_value());
    ASSERT_TRUE(static_cast<bool>(entry->second));
    entry->second();

    EXPECT_EQ(first_fired.load(), 100);
}

/** @brief snapshot_sorted() on a callable time_list returns entries ordered by tick. */
TEST_F(TimeListCallableTest, SnapshotSortedCallableList)
{
    tl->push(20L, []() {});
    tl->push(10L, []() {});
    tl->push(30L, []() {});

    const auto snap = tl->snapshot_sorted();

    ASSERT_EQ(snap.size(), 3U);
    EXPECT_EQ(snap[0].first, 10L);
    EXPECT_EQ(snap[1].first, 20L);
    EXPECT_EQ(snap[2].first, 30L);
}
