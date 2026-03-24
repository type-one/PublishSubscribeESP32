/**
 * @file test_sync_ring_vector.cpp
 * @brief Unit tests for the SyncRingVector class template.
 *
 * This file contains a series of unit tests for the SyncRingVector class template.
 * The tests cover various functionalities such as pushing, popping, resizing,
 * and checking the state of the SyncRingVector. The tests are implemented using
 * the Google Test framework.
 *
 * @author Laurent Lardinois and Copilot GPT-4o
 * @date February 2025
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
#include <complex>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

#include "tools/sync_ring_vector.hpp"

/**
 * @brief Test fixture for SyncRingVector.
 *
 * @tparam T The type of elements in the SyncRingVector.
 */
template <typename T>
class SyncRingVectorTest : public ::testing::Test
{
protected:
    /**
     * @brief Set up the test fixture.
     */
    void SetUp() override
    {
        vec = std::make_unique<tools::sync_ring_vector<T>>(5);
    }

    /**
     * @brief Tear down the test fixture.
     */
    void TearDown() override
    {
        vec.reset();
    }

    std::unique_ptr<tools::sync_ring_vector<T>> vec;
};

using TestTypes = ::testing::Types<int, float, double, char, std::complex<double>>;
TYPED_TEST_SUITE(SyncRingVectorTest, TestTypes);

/**
 * @brief Test case for pushing elements into the SyncRingVector and checking its size.
 *
 * This test case verifies that elements can be pushed into the SyncRingVector
 * and that the size of the vector is updated correctly.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 */
TYPED_TEST(SyncRingVectorTest, PushAndSize)
{
    this->vec->push(static_cast<TypeParam>(1));
    this->vec->push(static_cast<TypeParam>(2));
    this->vec->push(static_cast<TypeParam>(3));
    EXPECT_EQ(this->vec->size(), 3);
}

/**
 * @brief Test case to verify the front and back elements of the SyncRingVector.
 *
 * This test case pushes three elements into the SyncRingVector and checks if
 * the front and back elements are as expected.
 *
 * @tparam TypeParam The type parameter for the test case.
 *
 * @test
 * - Pushes elements 1, 2, and 3 into the SyncRingVector.
 * - Expects the front element to be 1.
 * - Expects the back element to be 3.
 */
TYPED_TEST(SyncRingVectorTest, FrontAndBack)
{
    this->vec->push(static_cast<TypeParam>(1));
    this->vec->push(static_cast<TypeParam>(2));
    this->vec->push(static_cast<TypeParam>(3));
    EXPECT_EQ(this->vec->front(), static_cast<TypeParam>(1));
    EXPECT_EQ(this->vec->back(), static_cast<TypeParam>(3));
}

/**
 * @brief Test case for popping elements from SyncRingVector.
 *
 * This test case verifies the behavior of the pop operation in the SyncRingVector.
 * It pushes three elements into the vector, pops one element, and then checks
 * the size and the front element of the vector.
 *
 * @tparam TypeParam The type parameter for the test case.
 *
 * @test
 * - Pushes elements 1, 2, and 3 into the vector.
 * - Pops one element from the vector.
 * - Expects the size of the vector to be 2.
 * - Expects the front element of the vector to be 2.
 * - Expects the size of the vector to be still 2.
 * - Pops and expects the front element of the vector to be 2.
 * - Expects the size of the vector to be now 1.
 */
TYPED_TEST(SyncRingVectorTest, Pop)
{
    this->vec->push(static_cast<TypeParam>(1));
    this->vec->push(static_cast<TypeParam>(2));
    this->vec->push(static_cast<TypeParam>(3));
    this->vec->pop();
    EXPECT_EQ(this->vec->size(), 2);
    EXPECT_EQ(this->vec->front(), static_cast<TypeParam>(2));
    EXPECT_EQ(this->vec->size(), 2);
    EXPECT_EQ(this->vec->front_pop(), static_cast<TypeParam>(2));
    EXPECT_EQ(this->vec->size(), 1);

}

/**
 * @brief Test case to check the behavior of SyncRingVector when it is empty and full.
 *
 * This test case performs the following steps:
 * 1. Pushes elements into the SyncRingVector.
 * 2. Checks if the vector is not empty after pushing elements.
 * 3. Continues to push elements until the vector is full.
 * 4. Verifies that the vector is full after pushing the maximum number of elements.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 */
TYPED_TEST(SyncRingVectorTest, EmptyAndFull)
{
    this->vec->push(static_cast<TypeParam>(1));
    this->vec->push(static_cast<TypeParam>(2));
    EXPECT_FALSE(this->vec->empty());
    this->vec->push(static_cast<TypeParam>(3));
    this->vec->push(static_cast<TypeParam>(4));
    this->vec->push(static_cast<TypeParam>(5));
    EXPECT_TRUE(this->vec->full());
}

/**
 * @brief Test case for resizing the SyncRingVector.
 *
 * This test case verifies that the capacity of the SyncRingVector can be resized.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 *
 * @test
 * - Resizes the vector to a capacity of 10.
 * - Expects the capacity of the vector to be 10.
 */
TYPED_TEST(SyncRingVectorTest, Resize)
{
    this->vec->resize(10);
    EXPECT_EQ(this->vec->capacity(), 10);
}

/**
 * @brief Test case for ISR push and ISR size operations in SyncRingVector.
 *
 * This test case verifies that elements can be pushed into the SyncRingVector
 * using the ISR push method and that the ISR size method returns the correct size.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 *
 * @test
 * - Pushes elements 1, 2, and 3 using the ISR push method.
 * - Expects the ISR size of the vector to be 3.
 */
TYPED_TEST(SyncRingVectorTest, IsrPushAndIsrSize)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    this->vec->isr_push(static_cast<TypeParam>(1));
    this->vec->isr_push(static_cast<TypeParam>(2));
    this->vec->isr_push(static_cast<TypeParam>(3));
    EXPECT_EQ(this->vec->isr_size(), 3);
}

/**
 * @brief Test to check if the ISR (Interrupt Service Routine) buffer is full.
 *
 * This test pushes five elements into the ISR buffer and then checks if the buffer is full.
 * The elements pushed are of the type specified by the test parameter.
 *
 * @tparam TypeParam The type of elements to be pushed into the ISR buffer.
 */
TYPED_TEST(SyncRingVectorTest, IsrFull)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    this->vec->isr_push(static_cast<TypeParam>(1));
    this->vec->isr_push(static_cast<TypeParam>(2));
    this->vec->isr_push(static_cast<TypeParam>(3));
    this->vec->isr_push(static_cast<TypeParam>(4));
    this->vec->isr_push(static_cast<TypeParam>(5));
    EXPECT_TRUE(this->vec->isr_full());
}

/**
 * @brief Test case for ISR resize operation in SyncRingVector.
 *
 * This test case verifies that the capacity of the SyncRingVector can be resized
 * using the ISR resize method.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 *
 * @test
 * - Resizes the vector to a capacity of 15 using the ISR resize method.
 * - Expects the capacity of the vector to be 15.
 */
TYPED_TEST(SyncRingVectorTest, IsrResize)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    this->vec->isr_resize(15);
    EXPECT_EQ(this->vec->capacity(), 15);
}

/**
 * @brief Test case for multiple producers and multiple consumers using SyncRingVector.
 *
 * This test spawns two producer threads and two consumer threads. Each producer
 * thread pushes a range of integers into the SyncRingVector, while each consumer
 * thread pops a specified number of elements from the SyncRingVector.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 *
 * The test verifies that after all producer and consumer threads have completed
 * their operations, the SyncRingVector is empty.
 */
TYPED_TEST(SyncRingVectorTest, MultipleProducersMultipleConsumers)
{
    std::atomic_bool producers_done { false };

    auto producer = [this](int start, int end)
    {
        for (int i = start; i < end; ++i)
        {
            this->vec->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this, &producers_done](int count)
    {
        for (int i = 0; i < count; ++i)
        {
            while (this->vec->empty() && !producers_done.load())
            {
                std::this_thread::yield();
            }

            if (!this->vec->empty())
            {
                this->vec->pop();
            }
        }
    };

    std::thread producer1(producer, 0, 5);
    std::thread producer2(producer, 5, 10);
    std::thread consumer1(consumer, 5);
    std::thread consumer2(consumer, 5);

    producer1.join();
    producer2.join();

    producers_done.store(true);

    consumer1.join();
    consumer2.join();

    EXPECT_TRUE(this->vec->empty());
}

/**
 * @brief Test case for single producer and multiple consumers using SyncRingVector.
 *
 * This test case verifies the behavior of the SyncRingVector when a single producer
 * pushes elements into the vector and multiple consumers pop elements from the vector.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 *
 * The test creates one producer thread that pushes 10 elements into the vector.
 * It also creates two consumer threads, each popping 5 elements from the vector.
 * After all threads have completed their operations, the test checks if the vector is empty.
 */
TYPED_TEST(SyncRingVectorTest, SingleProducerMultipleConsumers)
{
    std::atomic_bool producers_done { false };

    auto producer = [this]()
    {
        for (int i = 0; i < 10; ++i)
        {
            this->vec->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this, &producers_done](int count)
    {
        for (int i = 0; i < count; ++i)
        {
            while (this->vec->empty() && !producers_done.load())
            {
                std::this_thread::yield();
            }

            if (!this->vec->empty())
            {
                this->vec->pop();
            }
        }
    };

    std::thread producerThread(producer);
    std::thread consumer1(consumer, 5);
    std::thread consumer2(consumer, 5);

    producerThread.join();

    producers_done.store(true);

    consumer1.join();
    consumer2.join();

    EXPECT_TRUE(this->vec->empty());
}

/**
 * @brief Test case for multiple producers and a single consumer using SyncRingVector.
 *
 * This test spawns two producer threads and one consumer thread. Each producer
 * thread pushes a range of integers into the SyncRingVector, while the consumer
 * thread pops elements from the SyncRingVector. The test verifies that the
 * SyncRingVector is empty after all producers and the consumer have finished
 * their operations.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingVector.
 */
TYPED_TEST(SyncRingVectorTest, MultipleProducersSingleConsumer)
{
    std::atomic_bool producers_done { false };

    auto producer = [this](int start, int end)
    {
        for (int i = start; i < end; ++i)
        {
            this->vec->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this, &producers_done](int count)
    {
        for (int i = 0; i < count; ++i)
        {
            while (this->vec->empty() && !producers_done.load())
            {
                std::this_thread::yield();
            }

            if (!this->vec->empty())
            {
                this->vec->pop();
            }
        }
    };

    std::thread producer1(producer, 0, 5);
    std::thread producer2(producer, 5, 10);
    std::thread consumerThread(consumer, 10);

    producer1.join();
    producer2.join();

    producers_done.store(true);

    consumerThread.join();

    EXPECT_TRUE(this->vec->empty());
}

namespace
{
    /**
     * @brief Probe type used to observe copy/move assignment behavior.
     */
    struct sync_forwarding_probe
    {
        sync_forwarding_probe() = default;
        explicit sync_forwarding_probe(int input)
            : value(input)
        {
        }

        sync_forwarding_probe(const sync_forwarding_probe&) = default;
        sync_forwarding_probe(sync_forwarding_probe&&) noexcept = default;

        sync_forwarding_probe& operator=(const sync_forwarding_probe& other)
        {
            if (this != &other)
            {
                value = other.value;
            }
            ++copy_assign_count;
            return *this;
        }

        sync_forwarding_probe& operator=(sync_forwarding_probe&& other) noexcept
        {
            if (this != &other)
            {
                value = other.value;
                other.value = -1;
            }
            ++move_assign_count;
            return *this;
        }

        static void reset_counters()
        {
            copy_assign_count = 0;
            move_assign_count = 0;
        }

        int value = 0;
        static int copy_assign_count;
        static int move_assign_count;
    };

    int sync_forwarding_probe::copy_assign_count = 0;
    int sync_forwarding_probe::move_assign_count = 0;
}

/**
 * @brief Verifies perfect forwarding paths for push and emplace.
 */
TEST(SyncRingVectorPerfectForwardingTest, PushAndEmplaceForwarding)
{
    sync_forwarding_probe::reset_counters();

    tools::sync_ring_vector<sync_forwarding_probe> vec(4);

    sync_forwarding_probe lvalue(10);
    vec.push(lvalue);
    vec.push(sync_forwarding_probe(20));
    vec.emplace(30);

    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(sync_forwarding_probe::copy_assign_count, 1);
    EXPECT_EQ(sync_forwarding_probe::move_assign_count, 2);
}

/**
 * @brief Verifies perfect forwarding paths for ISR insertion APIs.
 */
TEST(SyncRingVectorPerfectForwardingTest, IsrPushAndEmplaceForwarding)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()


    sync_forwarding_probe::reset_counters();

    tools::sync_ring_vector<sync_forwarding_probe> vec(4);

    sync_forwarding_probe lvalue(1);
    vec.isr_push(lvalue);
    vec.isr_push(sync_forwarding_probe(2));
    vec.isr_emplace(3);

    ASSERT_EQ(vec.isr_size(), 3);
    EXPECT_EQ(sync_forwarding_probe::copy_assign_count, 1);
    EXPECT_EQ(sync_forwarding_probe::move_assign_count, 2);
}

/**
 * @brief Verifies move-only insertion and extraction using front_pop_move.
 */
TEST(SyncRingVectorPerfectForwardingTest, SupportsMoveOnlyType)
{
    tools::sync_ring_vector<std::unique_ptr<int>> vec(4);

    auto ptr = std::make_unique<int>(5);
    vec.push(std::move(ptr));
    EXPECT_EQ(ptr, nullptr);

    vec.emplace(std::make_unique<int>(9));

    auto first = vec.front_pop_move();
    ASSERT_TRUE(first.has_value());
    ASSERT_NE(*first, nullptr);
    EXPECT_EQ(**first, 5);

    auto second = vec.front_pop_move();
    ASSERT_TRUE(second.has_value());
    ASSERT_NE(*second, nullptr);
    EXPECT_EQ(**second, 9);

    EXPECT_FALSE(vec.front_pop_move().has_value());
}

TEST(SyncRingVectorOverwriteApiTest, ScalarOverwriteAndRejectReturns)
{
    tools::sync_ring_vector<int> vec(3);

    EXPECT_TRUE(vec.push(1));
    EXPECT_TRUE(vec.push(2));
    EXPECT_TRUE(vec.emplace(3));
    EXPECT_TRUE(vec.full());

    EXPECT_FALSE(vec.push(4));
    EXPECT_FALSE(vec.emplace(5));
    EXPECT_EQ(vec.size(), 3U);
    EXPECT_EQ(vec.front().value_or(-1), 1);
    EXPECT_EQ(vec.back().value_or(-1), 3);

    EXPECT_TRUE(vec.push_overwrite(4));
    EXPECT_TRUE(vec.emplace_overwrite(5));
    EXPECT_EQ(vec.size(), 3U);
    EXPECT_EQ(vec.front().value_or(-1), 3);
    EXPECT_EQ(vec.back().value_or(-1), 5);
}

TEST(SyncRingVectorOverwriteApiTest, IsrScalarOverwriteAndRejectReturns)
{
    tools::sync_ring_vector<int> vec(3);

    EXPECT_EQ(vec.isr_push_range({ 1, 2, 3 }), 3U);
    EXPECT_TRUE(vec.isr_full());

    EXPECT_EQ(vec.isr_push_range({ 4, 5 }), 0U);
    EXPECT_EQ(vec.isr_size(), 3U);
    EXPECT_EQ(vec.front().value_or(-1), 1);
    EXPECT_EQ(vec.back().value_or(-1), 3);

    EXPECT_TRUE(vec.isr_push_overwrite(4));
    EXPECT_TRUE(vec.isr_emplace_overwrite(5));
    EXPECT_EQ(vec.isr_size(), 3U);
    EXPECT_EQ(vec.front().value_or(-1), 3);
    EXPECT_EQ(vec.back().value_or(-1), 5);
}

TEST(SyncRingVectorOverwriteApiTest, RangeOverwriteReportsInsertedVsOverwritten)
{
    tools::sync_ring_vector<int> vec(3);

    auto result = vec.push_range_overwrite({ 10, 20 });
    EXPECT_EQ(result.inserted, 2U);
    EXPECT_EQ(result.overwritten, 0U);

    result = vec.push_range_overwrite({ 30, 40, 50 });
    EXPECT_EQ(result.inserted, 1U);
    EXPECT_EQ(result.overwritten, 2U);
    EXPECT_EQ(vec.size(), 3U);
    EXPECT_EQ(vec.front().value_or(-1), 30);
    EXPECT_EQ(vec.back().value_or(-1), 50);

    const std::size_t accepted = vec.push_range({ 60, 70 });
    EXPECT_EQ(accepted, 0U);
}

TEST(SyncRingVectorOverwriteApiTest, IsrRangeOverwriteReportsInsertedVsOverwritten)
{
    tools::sync_ring_vector<int> vec(3);

    auto result = vec.isr_push_range_overwrite({ 1, 2 });
    EXPECT_EQ(result.inserted, 2U);
    EXPECT_EQ(result.overwritten, 0U);

    result = vec.isr_push_range_overwrite({ 3, 4, 5 });
    EXPECT_EQ(result.inserted, 1U);
    EXPECT_EQ(result.overwritten, 2U);
    EXPECT_EQ(vec.isr_size(), 3U);
    EXPECT_EQ(vec.front().value_or(-1), 3);
    EXPECT_EQ(vec.back().value_or(-1), 5);

    const std::size_t accepted = vec.isr_push_range({ 6, 7 });
    EXPECT_EQ(accepted, 0U);
}

/**
 * @brief Verifies push_range supports initializer-list and generic range insertion.
 */
TEST(SyncRingVectorRangeTest, PushRangeSupportsInitializerAndRange)
{
    tools::sync_ring_vector<int> vec(8);

    vec.push_range({ 1, 2, 3 });
    const std::vector<int> extra_values = { 4, 5 };
    vec.push_range(extra_values);

    EXPECT_EQ(vec.size(), 5U);
    EXPECT_EQ(vec.front().value_or(0), 1);
    EXPECT_EQ(vec.back().value_or(0), 5);
}

/**
 * @brief Verifies ISR push_range supports initializer-list and generic ranges.
 */
TEST(SyncRingVectorRangeTest, IsrPushRangeSupportsInitializerAndRange)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()
    tools::sync_ring_vector<int> vec(8);

    vec.isr_push_range({ 6, 7 });
    const std::vector<int> extra_values = { 8, 9 };
    vec.isr_push_range(extra_values);

    EXPECT_EQ(vec.isr_size(), 4U);
    EXPECT_EQ(vec.front().value_or(0), 6);
    EXPECT_EQ(vec.back().value_or(0), 9);
}

/**
 * @brief Verifies iterator-based pop_range returns effective extracted count.
 */
TEST(SyncRingVectorRangeTest, PopRangeIteratorReturnsEffectiveCount)
{
    tools::sync_ring_vector<int> vec(8);
    vec.push_range({ 10, 20, 30, 40 });

    std::array<int, 3> destination = { 0, 0, 0 };
    const std::size_t popped_count = vec.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 3U);
    EXPECT_EQ(destination[0], 10);
    EXPECT_EQ(destination[1], 20);
    EXPECT_EQ(destination[2], 30);
    EXPECT_EQ(vec.size(), 1U);
    EXPECT_EQ(vec.front().value_or(0), 40);
}

/**
 * @brief Verifies pop_range clamps to available elements.
 */
TEST(SyncRingVectorRangeTest, PopRangeClampsToAvailable)
{
    tools::sync_ring_vector<int> vec(8);
    vec.push_range({ 50, 60 });

    std::array<int, 5> destination = { 0, 0, 0, 0, 0 };
    const std::size_t popped_count = vec.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 50);
    EXPECT_EQ(destination[1], 60);
    EXPECT_TRUE(vec.empty());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/**
 * @brief Verifies C++20 span-based pop_range overload.
 */
TEST(SyncRingVectorRangeTest, PopRangeSpanReturnsEffectiveCount)
{
    tools::sync_ring_vector<int> vec(8);
    vec.push_range({ 70, 80, 90 });

    std::array<int, 2> destination = { 0, 0 };
    const std::size_t popped_count = vec.pop_range(std::span<int>(destination));

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 70);
    EXPECT_EQ(destination[1], 80);
    EXPECT_EQ(vec.size(), 1U);
    EXPECT_EQ(vec.front().value_or(0), 90);
}
#endif

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
namespace
{
    /**
     * @brief Marker type intentionally not constructible as int.
     */
    struct non_constructible_payload
    {
    };

    template <typename Vec, typename Range>
    concept has_sync_rv_push_range_call = requires(Vec& vec, Range&& range)
    {
        vec.push_range(std::forward<Range>(range));
    };

    template <typename Vec, typename Range>
    concept has_sync_rv_isr_push_range_call = requires(Vec& vec, Range&& range)
    {
        vec.isr_push_range(std::forward<Range>(range));
    };

    template <typename Vec, typename OutputIt>
    concept has_sync_rv_pop_range_iter_call = requires(Vec& vec, OutputIt first, OutputIt last)
    {
        vec.pop_range(first, last);
    };

    template <typename Vec>
    concept has_sync_rv_pop_range_span_call = requires(Vec& vec, std::span<int> destination)
    {
        vec.pop_range(destination);
    };
}

/**
 * @brief C++20-only compile-time checks for range API constraints.
 */
TEST(SyncRingVectorRangeTest, Cpp20RangeConstraints)
{
    using vector_t = tools::sync_ring_vector<int>;

    static_assert(has_sync_rv_push_range_call<vector_t, std::vector<int>&>);
    static_assert(has_sync_rv_push_range_call<vector_t, std::initializer_list<int>>);
    static_assert(has_sync_rv_isr_push_range_call<vector_t, std::vector<int>&>);
    static_assert(has_sync_rv_isr_push_range_call<vector_t, std::initializer_list<int>>);
    static_assert(has_sync_rv_pop_range_iter_call<vector_t, int*>);
    static_assert(has_sync_rv_pop_range_span_call<vector_t>);

    const auto transformed = std::views::iota(0, 3)
        | std::views::transform([](const int value)
          {
              return value + 200;
          });
    static_assert(has_sync_rv_push_range_call<vector_t, decltype(transformed)>);
    static_assert(has_sync_rv_isr_push_range_call<vector_t, decltype(transformed)>);

    SUCCEED();
}

/**
 * @brief C++20-only compile-time checks for forwarding constraints.
 */
TEST(SyncRingVectorPerfectForwardingTest, Cpp20RequiresConstraints)
{
    static_assert(std::is_constructible_v<int, int>);
    static_assert(!std::is_constructible_v<int, non_constructible_payload>);
    static_assert(!std::is_constructible_v<int, non_constructible_payload&>);

    SUCCEED();
}
#endif
