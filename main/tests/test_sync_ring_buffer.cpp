/**
 * @file test_sync_ring_buffer.cpp
 * @brief Unit tests for the SyncRingBuffer class using Google Test framework.
 *
 * This file contains a series of test cases for the SyncRingBuffer class template.
 * The tests cover various functionalities including pushing, popping, emplacing,
 * and checking the state of the buffer (empty, full, size, capacity).
 * Additionally, tests for ISR (Interrupt Service Routine) operations and
 * multi-threaded scenarios with multiple producers and consumers are included.
 *
 * @date February 2025
 *
 * @author Laurent Lardinois and Copilot GPT-4o
 *
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
#include <array>
#include <chrono>
#include <complex>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

#include "tools/sync_ring_buffer.hpp"

/**
 * @brief Test fixture for SyncRingBuffer tests.
 *
 * This class template is used to set up and tear down the test environment
 * for testing the SyncRingBuffer class. It inherits from the Google Test
 * framework's ::testing::Test class.
 *
 * @tparam T The type of elements stored in the SyncRingBuffer.
 */
template <typename T>
class SyncRingBufferTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        buffer = std::make_unique<tools::sync_ring_buffer<T, 5>>();
    }

    void TearDown() override
    {
        buffer.reset();
    }

    std::unique_ptr<tools::sync_ring_buffer<T, 5>> buffer;
};

using TestTypes = ::testing::Types<int, float, double, char, std::complex<double>>;
TYPED_TEST_SUITE(SyncRingBufferTest, TestTypes);

/**
 * @brief Test case to verify the behavior of an empty SyncRingBuffer.
 *
 * This test checks the following conditions for an empty buffer:
 * - The buffer should be empty.
 * - The buffer should not be full.
 * - The size of the buffer should be 0.
 */
TYPED_TEST(SyncRingBufferTest, EmptyBuffer)
{
    ASSERT_TRUE(this->buffer->empty());
    ASSERT_FALSE(this->buffer->full());
    ASSERT_EQ(this->buffer->size(), 0);
}

/**
 * @brief Test case for pushing an element into the SyncRingBuffer and checking the front element.
 *
 * This test case verifies the following:
 * - Pushing an element into the buffer.
 * - Ensuring the buffer is not empty after the push operation.
 * - Checking that the front element of the buffer is the pushed element.
 * - Verifying the size of the buffer after the push operation.
 *
 * @tparam TypeParam The type of the elements in the SyncRingBuffer.
 */
TYPED_TEST(SyncRingBufferTest, PushAndFront)
{
    this->buffer->push(static_cast<TypeParam>(1));
    ASSERT_FALSE(this->buffer->empty());
    ASSERT_EQ(this->buffer->front(), static_cast<TypeParam>(1));
    ASSERT_EQ(this->buffer->size(), 1);
}

/**
 * @brief Test case for emplacing an element into the SyncRingBuffer and verifying the back element.
 *
 * This test case verifies that an element can be emplaced into the SyncRingBuffer and that the back
 * element of the buffer is correctly updated. It also checks that the size of the buffer is updated
 * accordingly.
 *
 * @tparam TypeParam The type of the elements stored in the SyncRingBuffer.
 *
 * @test
 * - Emplace an element with value 2 into the buffer.
 * - Verify that the back element of the buffer is 2.
 * - Verify that the size of the buffer is 1.
 */
TYPED_TEST(SyncRingBufferTest, EmplaceAndBack)
{
    this->buffer->emplace(static_cast<TypeParam>(2));
    ASSERT_EQ(this->buffer->back(), static_cast<TypeParam>(2));
    ASSERT_EQ(this->buffer->size(), 1);
}

/**
 * @brief Test case for popping an element from the SyncRingBuffer.
 *
 * This test case verifies the behavior of the SyncRingBuffer when an element is popped.
 * It performs the following steps:
 * 1. Pushes the value 1 into the buffer.
 * 2. Pushes the value 2 into the buffer.
 * 3. Pops the front element from the buffer.
 * 4. Asserts that the front element of the buffer is now 2.
 * 5. Asserts that the size of the buffer is 1.
 * 6. Pops and asserts that the front element of the buffer is now 2.
 * 7. Asserts that the size of the buffer is 0.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingBuffer.
 */
TYPED_TEST(SyncRingBufferTest, Pop)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->pop();
    ASSERT_EQ(this->buffer->front(), static_cast<TypeParam>(2));
    ASSERT_EQ(this->buffer->size(), 1);
    ASSERT_EQ(this->buffer->front_pop(), static_cast<TypeParam>(2));
    ASSERT_EQ(this->buffer->size(), 0);
}

/**
 * @brief Test case for filling the SyncRingBuffer to its full capacity.
 *
 * This test case verifies the behavior of the SyncRingBuffer when it is filled to its maximum capacity.
 * It performs the following steps:
 * 1. Pushes the value 1 into the buffer.
 * 2. Pushes the value 2 into the buffer.
 * 3. Pushes the value 3 into the buffer.
 * 4. Pushes the value 4 into the buffer.
 * 5. Pushes the value 5 into the buffer.
 * 6. Asserts that the buffer is full.
 * 7. Asserts that the size of the buffer is 5.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingBuffer.
 */
TYPED_TEST(SyncRingBufferTest, FullBuffer)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));
    this->buffer->push(static_cast<TypeParam>(4));
    this->buffer->push(static_cast<TypeParam>(5));
    ASSERT_TRUE(this->buffer->full());
    ASSERT_EQ(this->buffer->size(), 5);
}

/**
 * @brief Test case for ISR push and ISR full functionality in SyncRingBuffer.
 *
 * This test verifies that the ISR push operation works correctly and that the buffer
 * correctly reports being full after a series of push operations.
 *
 * @tparam TypeParam The type parameter for the SyncRingBuffer.
 *
 * The test performs the following steps:
 * 1. Pushes five elements into the buffer using the ISR push method.
 * 2. Asserts that the buffer is full after the pushes.
 * 3. Asserts that the size of the buffer is 5.
 */
TYPED_TEST(SyncRingBufferTest, IsrPushAndIsrFull)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    this->buffer->isr_push(static_cast<TypeParam>(1));
    this->buffer->isr_push(static_cast<TypeParam>(2));
    this->buffer->isr_push(static_cast<TypeParam>(3));
    this->buffer->isr_push(static_cast<TypeParam>(4));
    this->buffer->isr_push(static_cast<TypeParam>(5));
    ASSERT_TRUE(this->buffer->isr_full());
    ASSERT_EQ(this->buffer->isr_size(), 5);
}

/**
 * @brief Test case for ISR emplace operation in SyncRingBuffer.
 *
 * This test case verifies that the `isr_emplace` method correctly inserts elements
 * into the buffer from an ISR context and that the buffer size is updated accordingly.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingBuffer.
 *
 * The test performs the following steps:
 * 1. Emplaces five elements (1, 2, 3, 4, 5) into the buffer using `isr_emplace`.
 * 2. Asserts that the buffer size is 5 after the insertions.
 */
TYPED_TEST(SyncRingBufferTest, IsrEmplace)
{
    this->buffer->isr_emplace(static_cast<TypeParam>(1));
    this->buffer->isr_emplace(static_cast<TypeParam>(2));
    this->buffer->isr_emplace(static_cast<TypeParam>(3));
    this->buffer->isr_emplace(static_cast<TypeParam>(4));
    this->buffer->isr_emplace(static_cast<TypeParam>(5));
    ASSERT_EQ(this->buffer->isr_size(), 5);
}

/**
 * @brief Test to verify the capacity of the SyncRingBuffer.
 *
 * This test checks if the capacity of the buffer is correctly set to 5.
 */
TYPED_TEST(SyncRingBufferTest, Capacity)
{
    ASSERT_EQ(this->buffer->capacity(), 5);
}

/**
 * @brief Test case for multiple producers and multiple consumers using a synchronized ring buffer.
 *
 * This test creates two producer threads and two consumer threads. Each producer thread pushes
 * 10 elements into the buffer, and each consumer thread pops 10 elements from the buffer.
 * The test ensures that the buffer is empty at the end.
 *
 * @tparam TypeParam The type of elements stored in the buffer.
 */
TYPED_TEST(SyncRingBufferTest, MultipleProducersMultipleConsumers)
{
    std::atomic_bool producers_done { false };

    auto producer = [this](int id)
    {
        for (int i = 0; i < 10; ++i)
        {
            this->buffer->push(static_cast<TypeParam>(id * 10 + i));
        }
    };

    auto consumer = [this, &producers_done]()
    {
        for (int i = 0; i < 10; ++i)
        {
            while (this->buffer->empty() && !producers_done.load())
            {
                std::this_thread::yield();
            }

            if (!this->buffer->empty())
            {
                this->buffer->pop();
            }
        }
    };

    std::thread producer1(producer, 1);
    std::thread producer2(producer, 2);
    std::thread consumer1(consumer);
    std::thread consumer2(consumer);

    producer1.join();
    producer2.join();

    producers_done.store(true);

    consumer1.join();
    consumer2.join();

    ASSERT_TRUE(this->buffer->empty());
}

/**
 * @brief Test case for single producer and multiple consumers scenario.
 *
 * This test case verifies the behavior of the SyncRingBuffer when there is a single producer
 * and multiple consumers. The producer pushes 20 elements into the buffer, while each of the
 * two consumers attempts to pop 10 elements from the buffer.
 *
 * @tparam TypeParam The type of elements stored in the SyncRingBuffer.
 *
 * The test creates three threads:
 * - One producer thread that pushes 20 elements into the buffer.
 * - Two consumer threads that each attempt to pop 10 elements from the buffer.
 *
 * The test asserts that the buffer is empty at the end of the test.
 */
TYPED_TEST(SyncRingBufferTest, SingleProducerMultipleConsumers)
{
    std::atomic_bool producer_done { false };

    auto producer = [this]()
    {
        for (int i = 0; i < 20; ++i)
        {
            this->buffer->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this, &producer_done]()
    {
        for (int i = 0; i < 10; ++i)
        {
            while (this->buffer->empty() && !producer_done.load())
            {
                std::this_thread::yield();
            }

            if (!this->buffer->empty())
            {
                this->buffer->pop();
            }
        }
    };

    std::thread producerThread(producer);
    std::thread consumer1(consumer);
    std::thread consumer2(consumer);

    producerThread.join();

    producer_done.store(true);

    consumer1.join();
    consumer2.join();

    ASSERT_TRUE(this->buffer->empty());
}

/**
 * @brief Test case for multiple producers and a single consumer using a synchronized ring buffer.
 *
 * This test spawns two producer threads and one consumer thread. Each producer thread pushes
 * 10 elements into the buffer, while the consumer thread pops 20 elements from the buffer.
 * The test verifies that the buffer is empty after all threads have completed their operations.
 *
 * @tparam TypeParam The type of elements stored in the synchronized ring buffer.
 */
TYPED_TEST(SyncRingBufferTest, MultipleProducersSingleConsumer)
{
    std::atomic_bool producers_done { false };

    auto producer = [this](int id)
    {
        for (int i = 0; i < 10; ++i)
        {
            this->buffer->push(static_cast<TypeParam>(id * 10 + i));
        }
    };

    auto consumer = [this, &producers_done]()
    {
        for (int i = 0; i < 20; ++i)
        {
            while (this->buffer->empty() && !producers_done.load())
            {
                std::this_thread::yield();
            }

            if (!this->buffer->empty())
            {
                this->buffer->pop();
            }
        }
    };

    std::thread producer1(producer, 1);
    std::thread producer2(producer, 2);
    std::thread consumerThread(consumer);

    producer1.join();
    producer2.join();

    producers_done.store(true);

    consumerThread.join();

    ASSERT_TRUE(this->buffer->empty());
}

namespace
{
    /**
     * @brief Probe type used to observe copy/move assignment behavior.
     *
     * This helper tracks assignment counters to validate that forwarding in
     * sync_ring_buffer selects copy assignment for lvalues and move assignment
     * for rvalues.
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
 *
 * This test validates that lvalue push uses copy assignment, rvalue push uses
 * move assignment, and emplace forwards constructor arguments correctly.
 */
TEST(SyncRingBufferPerfectForwardingTest, PushAndEmplaceForwarding)
{
    sync_forwarding_probe::reset_counters();

    tools::sync_ring_buffer<sync_forwarding_probe, 4> buffer;

    sync_forwarding_probe lvalue(10);
    buffer.push(lvalue);
    buffer.push(sync_forwarding_probe(20));
    buffer.emplace(30);

    ASSERT_EQ(buffer.size(), 3);
    EXPECT_EQ(sync_forwarding_probe::copy_assign_count, 1);
    EXPECT_EQ(sync_forwarding_probe::move_assign_count, 2);

    const auto first = buffer.front_pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->value, 10);

    const auto second = buffer.front_pop();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->value, 20);

    const auto third = buffer.front_pop();
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(third->value, 30);
}

/**
 * @brief Verifies perfect forwarding paths for ISR insertion APIs.
 *
 * This test validates that ISR-safe push/emplace variants preserve forwarding
 * semantics and insert expected values.
 */
TEST(SyncRingBufferPerfectForwardingTest, IsrPushAndEmplaceForwarding)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    sync_forwarding_probe::reset_counters();

    tools::sync_ring_buffer<sync_forwarding_probe, 4> buffer;

    sync_forwarding_probe lvalue(1);
    buffer.isr_push(lvalue);
    buffer.isr_push(sync_forwarding_probe(2));
    buffer.isr_emplace(3);

    ASSERT_EQ(buffer.isr_size(), 3);
    EXPECT_EQ(sync_forwarding_probe::copy_assign_count, 1);
    EXPECT_EQ(sync_forwarding_probe::move_assign_count, 2);
}

/**
 * @brief Verifies forwarding support with move-only payloads.
 *
 * This test ensures sync_ring_buffer accepts std::unique_ptr values through
 * push/emplace and can extract them through the move-based front_pop_move API.
 */
TEST(SyncRingBufferPerfectForwardingTest, SupportsMoveOnlyType)
{
    tools::sync_ring_buffer<std::unique_ptr<int>, 4> buffer;

    auto ptr = std::make_unique<int>(5);
    buffer.push(std::move(ptr));
    EXPECT_EQ(ptr, nullptr);

    buffer.emplace(std::make_unique<int>(9));

    auto first = buffer.front_pop_move();
    ASSERT_TRUE(first.has_value());
    ASSERT_NE(*first, nullptr);
    EXPECT_EQ(**first, 5);

    auto second = buffer.front_pop_move();
    ASSERT_TRUE(second.has_value());
    ASSERT_NE(*second, nullptr);
    EXPECT_EQ(**second, 9);

    EXPECT_FALSE(buffer.front_pop_move().has_value());
}

/**
 * @brief Verifies push_range supports initializer-list and generic ranges.
 *
 * @test
 * - Push initializer-list values.
 * - Push std::vector range values.
 * - Verify resulting front/back and size.
 */
TEST(SyncRingBufferRangeTest, PushRangeSupportsInitializerAndRange)
{
    tools::sync_ring_buffer<int, 8> buffer;

    buffer.push_range({ 1, 2, 3 });
    const std::vector<int> extra_values = { 4, 5 };
    buffer.push_range(extra_values);

    EXPECT_EQ(buffer.size(), 5U);
    EXPECT_EQ(buffer.front().value_or(0), 1);
    EXPECT_EQ(buffer.back().value_or(0), 5);
}

/**
 * @brief Verifies ISR push_range supports initializer-list and generic ranges.
 *
 * @note In GoogleTest context there is no real ISR; standard C++ fallback behavior is used.
 *
 * @test
 * - Push range values using ISR-safe APIs.
 * - Verify resulting front/back and size.
 */
TEST(SyncRingBufferRangeTest, IsrPushRangeSupportsInitializerAndRange)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()
    tools::sync_ring_buffer<int, 8> buffer;

    buffer.isr_push_range({ 6, 7 });
    const std::vector<int> more_values = { 8, 9 };
    buffer.isr_push_range(more_values);

    EXPECT_EQ(buffer.isr_size(), 4U);
    EXPECT_EQ(buffer.front().value_or(0), 6);
    EXPECT_EQ(buffer.back().value_or(0), 9);
}

/**
 * @brief Verifies iterator-pair pop_range extracts effective count in FIFO order.
 *
 * @test
 * - Fill buffer.
 * - Pop into fixed destination storage via iterators.
 * - Validate returned count and remaining queue content.
 */
TEST(SyncRingBufferRangeTest, PopRangeIteratorReturnsEffectiveCount)
{
    tools::sync_ring_buffer<int, 8> buffer;
    buffer.push_range({ 10, 20, 30, 40 });

    std::array<int, 3> destination = { 0, 0, 0 };
    const std::size_t popped_count = buffer.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 3U);
    EXPECT_EQ(destination[0], 10);
    EXPECT_EQ(destination[1], 20);
    EXPECT_EQ(destination[2], 30);
    EXPECT_EQ(buffer.size(), 1U);
    EXPECT_EQ(buffer.front().value_or(0), 40);
}

/**
 * @brief Verifies pop_range clamps to available data.
 *
 * @test
 * - Push fewer elements than destination capacity.
 * - Validate returned count and empty buffer state.
 */
TEST(SyncRingBufferRangeTest, PopRangeClampsToAvailable)
{
    tools::sync_ring_buffer<int, 8> buffer;
    buffer.push_range({ 50, 60 });

    std::array<int, 5> destination = { 0, 0, 0, 0, 0 };
    const std::size_t popped_count = buffer.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 50);
    EXPECT_EQ(destination[1], 60);
    EXPECT_TRUE(buffer.empty());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/**
 * @brief Verifies C++20 span-based pop_range overload for sync_ring_buffer.
 *
 * @test
 * - Fill buffer.
 * - Pop through std::span destination.
 * - Validate returned count and remaining data.
 */
TEST(SyncRingBufferRangeTest, PopRangeSpanReturnsEffectiveCount)
{
    tools::sync_ring_buffer<int, 8> buffer;
    buffer.push_range({ 70, 80, 90 });

    std::array<int, 2> destination = { 0, 0 };
    const std::size_t popped_count = buffer.pop_range(std::span<int>(destination));

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 70);
    EXPECT_EQ(destination[1], 80);
    EXPECT_EQ(buffer.size(), 1U);
    EXPECT_EQ(buffer.front().value_or(0), 90);
}
#endif

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
namespace
{
    /**
     * @brief Marker type intentionally not constructible as int.
     *
     * Used by C++20-only checks validating requires-based filtering intent.
     */
    struct sync_non_constructible_payload
    {
    };

    template <typename Buf, typename Range>
    concept has_sync_rb_push_range_call = requires(Buf& buffer, Range&& range)
    {
        buffer.push_range(std::forward<Range>(range));
    };

    template <typename Buf, typename Range>
    concept has_sync_rb_isr_push_range_call = requires(Buf& buffer, Range&& range)
    {
        buffer.isr_push_range(std::forward<Range>(range));
    };

    template <typename Buf, typename OutputIt>
    concept has_sync_rb_pop_range_iter_call = requires(Buf& buffer, OutputIt first, OutputIt last)
    {
        buffer.pop_range(first, last);
    };

    template <typename Buf>
    concept has_sync_rb_pop_range_span_call = requires(Buf& buffer, std::span<int> destination)
    {
        buffer.pop_range(destination);
    };
}

/**
 * @brief C++20-only compile-time checks for range API constraints.
 */
TEST(SyncRingBufferRangeTest, Cpp20RangeConstraints)
{
    using buffer_t = tools::sync_ring_buffer<int, 8>;

    static_assert(has_sync_rb_push_range_call<buffer_t, std::vector<int>&>);
    static_assert(has_sync_rb_push_range_call<buffer_t, std::initializer_list<int>>);
    static_assert(has_sync_rb_isr_push_range_call<buffer_t, std::vector<int>&>);
    static_assert(has_sync_rb_isr_push_range_call<buffer_t, std::initializer_list<int>>);
    static_assert(has_sync_rb_pop_range_iter_call<buffer_t, int*>);
    static_assert(has_sync_rb_pop_range_span_call<buffer_t>);

    const auto transformed = std::views::iota(0, 3)
        | std::views::transform([](const int value)
          {
              return value + 100;
          });
    static_assert(has_sync_rb_push_range_call<buffer_t, decltype(transformed)>);
    static_assert(has_sync_rb_isr_push_range_call<buffer_t, decltype(transformed)>);

    SUCCEED();
}

/**
 * @brief C++20-only compile-time checks for forwarding constraints.
 *
 * This test validates constructibility assumptions used by C++20 requires
 * clauses in sync_ring_buffer::push/emplace and ISR variants.
 */
TEST(SyncRingBufferPerfectForwardingTest, Cpp20RequiresConstraints)
{
    static_assert(std::is_constructible_v<int, int>);
    static_assert(!std::is_constructible_v<int, sync_non_constructible_payload>);
    static_assert(!std::is_constructible_v<int, sync_non_constructible_payload&>);

    SUCCEED();
}
#endif
