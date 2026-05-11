/**
 * @file test_sync_time_list.cpp
 * @brief Unit tests for tools::sync_time_list.
 *
 * This file validates thread-safe chronological behavior of tools::sync_time_list,
 * including ordering, top/top_pop semantics, clear/snapshot behavior, and
 * concurrent producer inserts.
 *
 * @author Laurent Lardinois and Copilot
 * @date May 2026
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

#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "tools/sync_time_list.hpp"

/**
 * @brief Fixture for tools::sync_time_list tests.
 */
class SyncTimeListTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        sync_list = std::make_unique<tools::sync_time_list<long, int>>();
    }

    void TearDown() override
    {
        sync_list.reset();
    }

    std::unique_ptr<tools::sync_time_list<long, int>> sync_list;
};

TEST_F(SyncTimeListTest, StartsEmpty)
{
    EXPECT_TRUE(sync_list->empty());
    EXPECT_EQ(sync_list->size(), 0U);
    EXPECT_FALSE(sync_list->top().has_value());
    EXPECT_FALSE(sync_list->top_pop().has_value());
}

TEST_F(SyncTimeListTest, ChronologicalOrderWithTopPop)
{
    sync_list->push(30L, 30);
    sync_list->push(10L, 10);
    sync_list->push(40L, 40);
    sync_list->push(20L, 20);

    std::vector<long> timestamps;
    while (!sync_list->empty())
    {
        const auto entry = sync_list->top_pop();
        ASSERT_TRUE(entry.has_value());
        timestamps.push_back(entry->first);
    }

    const std::vector<long> expected { 10L, 20L, 30L, 40L };
    EXPECT_EQ(timestamps, expected);
}

TEST_F(SyncTimeListTest, SnapshotSortedKeepsContainerIntact)
{
    sync_list->push(300L, 3);
    sync_list->push(100L, 1);
    sync_list->push(200L, 2);

    const auto snapshot_values = sync_list->snapshot_sorted();

    ASSERT_EQ(snapshot_values.size(), 3U);
    EXPECT_EQ(snapshot_values[0].first, 100L);
    EXPECT_EQ(snapshot_values[1].first, 200L);
    EXPECT_EQ(snapshot_values[2].first, 300L);

    EXPECT_EQ(sync_list->size(), 3U);
}

TEST_F(SyncTimeListTest, ClearRemovesAllEntries)
{
    sync_list->push(10L, 10);
    sync_list->push(20L, 20);
    sync_list->push(30L, 30);

    sync_list->clear();

    EXPECT_TRUE(sync_list->empty());
    EXPECT_EQ(sync_list->size(), 0U);
    EXPECT_FALSE(sync_list->top().has_value());
}

TEST_F(SyncTimeListTest, ConcurrentProducersMaintainTotalCountAndOrder)
{
    constexpr const long producer_a_start = 1000L;
    constexpr const long producer_b_start = 2000L;
    constexpr const long timestamp_step = 1L;
    constexpr const int inserts_per_producer = 128;

    std::thread producer_a(
        [this]()
        {
            constexpr const long producer_start = 1000L;
            constexpr const int count_limit = 128;
            for (int index = 0; index < count_limit; ++index)
            {
                const long timestamp_value = producer_start + static_cast<long>(index);
                sync_list->push(timestamp_value, index);
            }
        });

    std::thread producer_b(
        [this]()
        {
            constexpr const long producer_start = 2000L;
            constexpr const int count_limit = 128;
            for (int index = 0; index < count_limit; ++index)
            {
                const long timestamp_value = producer_start + static_cast<long>(index);
                sync_list->push(timestamp_value, index);
            }
        });

    producer_a.join();
    producer_b.join();

    constexpr const std::size_t expected_total = static_cast<std::size_t>(inserts_per_producer * 2);
    EXPECT_EQ(sync_list->size(), expected_total);

    const auto snapshot_values = sync_list->snapshot_sorted();
    ASSERT_EQ(snapshot_values.size(), expected_total);

    for (std::size_t index = 1U; index < snapshot_values.size(); ++index)
    {
        EXPECT_LE(snapshot_values[index - 1U].first, snapshot_values[index].first);
    }

    EXPECT_EQ(snapshot_values.front().first, producer_a_start);
    EXPECT_EQ(snapshot_values[inserts_per_producer - 1].first,
        producer_a_start + static_cast<long>(inserts_per_producer) - timestamp_step);
    EXPECT_EQ(snapshot_values[inserts_per_producer].first, producer_b_start);
    EXPECT_EQ(
        snapshot_values.back().first, producer_b_start + static_cast<long>(inserts_per_producer) - timestamp_step);
}
