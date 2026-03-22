/**
 * @file test_lock_free_ring_buffer.cpp
 * @brief Unit tests for the LockFreeRingBuffer class.
 * 
 * This file contains a set of unit tests for the LockFreeRingBuffer class using the Google Test framework.
 * The tests cover various scenarios including capacity checks, push and pop operations, overflow and underflow conditions,
 * and producer-consumer scenarios with interleaved operations.
 * 
 * The tests are organized into a test fixture class template, LockFreeRingBufferTest, which
 * provides setup and teardown functionality for the lock-free ring buffer. The tests are
 * parameterized to run with different types of elements stored in the lock-free ring buffer.
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
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <span>
#endif

#include "tools/lock_free_ring_buffer.hpp"

/**
 * @brief Test fixture for LockFreeRingBuffer tests.
 *
 * @tparam T The type of elements in the ring buffer.
 */
template <typename T>
class LockFreeRingBufferTest : public ::testing::Test
{
protected:
    /**
     * @brief Set up the test fixture.
     *
     * This function is called before each test is run.
     */
    void SetUp() override
    {
        // Initialize buffer before each test
        buffer = std::make_unique<tools::lock_free_ring_buffer<T, 4>>();
    }

    /**
     * @brief Tear down the test fixture.
     *
     * This function is called after each test is run.
     */
    void TearDown() override
    {
        // Clean up buffer after each test
        buffer.reset();
    }

    /// The lock-free ring buffer used in the tests.
    std::unique_ptr<tools::lock_free_ring_buffer<T, 4>> buffer;
};

/// The types to be tested with the LockFreeRingBufferTest fixture.
using MyTypes = ::testing::Types<int, float, double, char>;
TYPED_TEST_SUITE(LockFreeRingBufferTest, MyTypes);

/**
 * @brief Test the capacity of the lock-free ring buffer.
 */
TYPED_TEST(LockFreeRingBufferTest, CapacityTest)
{
    ASSERT_EQ(this->buffer->capacity(), 16);
}

/**
 * @brief Test case for pushing and popping elements in a lock-free ring buffer.
 *
 * This test case verifies the functionality of pushing elements into the ring buffer
 * until it is full, and then popping elements from the ring buffer until it is empty.
 *
 * @tparam TypeParam The type of elements stored in the ring buffer.
 *
 * @test
 * - Push elements 1 to 15 into the buffer and verify each push operation succeeds.
 * - Attempt to push element 16 and verify the push operation fails (buffer should be full).
 * - Pop elements from the buffer and verify the values are in the correct order (1 to 15).
 * - Attempt to pop from the buffer when it is empty and verify the pop operation fails.
 */
TYPED_TEST(LockFreeRingBufferTest, PushPopTest)
{
    TypeParam value;
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(1)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(2)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(3)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(4)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(5)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(6)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(7)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(8)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(9)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(10)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(11)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(12)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(13)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(14)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(15)));
    ASSERT_FALSE(this->buffer->push(static_cast<TypeParam>(16))); // Buffer should be full

    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(1));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(2));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(3));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(4));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(5));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(6));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(7));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(8));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(9));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(10));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(11));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(12));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(13));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(14));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(15));
    ASSERT_FALSE(this->buffer->pop(value)); // Buffer should be empty
}

/**
 * @brief Test case for interleaved push and pop operations on a lock-free ring buffer.
 *
 * This test verifies the correct behavior of the lock-free ring buffer when push and pop operations
 * are interleaved. It ensures that the buffer maintains the correct order of elements and handles
 * empty buffer conditions appropriately.
 *
 * @tparam TypeParam The type of elements stored in the ring buffer.
 *
 * Test steps:
 * - Push elements 1 and 2 into the buffer.
 * - Pop an element and verify it is 1.
 * - Push element 3 into the buffer.
 * - Pop an element and verify it is 2.
 * - Pop an element and verify it is 3.
 * - Attempt to pop from an empty buffer and verify the operation fails.
 */
TYPED_TEST(LockFreeRingBufferTest, PushPopInterleavedTest)
{
    TypeParam value;
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(1)));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(2)));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(1));
    ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(3)));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(2));
    ASSERT_TRUE(this->buffer->pop(value));
    ASSERT_EQ(value, static_cast<TypeParam>(3));
    ASSERT_FALSE(this->buffer->pop(value)); // Buffer should be empty
}

/**
 * @brief Test case to verify the behavior of the LockFreeRingBuffer when it overflows.
 *
 * This test attempts to push 15 elements into the buffer, which should succeed.
 * It then attempts to push a 16th element, which should fail, indicating that the buffer is full.
 *
 * @tparam TypeParam The type of elements stored in the LockFreeRingBuffer.
 */
TYPED_TEST(LockFreeRingBufferTest, OverflowTest)
{
    for (int i = 0; i < 15; ++i)
    {
        ASSERT_TRUE(this->buffer->push(static_cast<TypeParam>(i)));
    }
    ASSERT_FALSE(this->buffer->push(static_cast<TypeParam>(16))); // Buffer should be full
}

/**
 * @brief Test case to verify the behavior of the LockFreeRingBuffer when it underflows.
 *
 * This test attempts to pop an element from an empty buffer, which should fail,
 * indicating that the buffer is empty.
 *
 * @tparam TypeParam The type of elements stored in the LockFreeRingBuffer.
 */
TYPED_TEST(LockFreeRingBufferTest, UnderflowTest)
{
    TypeParam value;
    ASSERT_FALSE(this->buffer->pop(value)); // Buffer should be empty
}

/**
 * @brief Test case for interleaved producer-consumer operations on a lock-free ring buffer.
 *
 * This test spawns two threads: one for producing and one for consuming elements in the ring buffer.
 * The producer thread pushes integers from 0 to 100000 into the buffer, while the consumer thread
 * pops these integers from the buffer and verifies that they are in the correct order.
 *
 * @tparam TypeParam The type of elements stored in the lock-free ring buffer.
 *
 * The test ensures that:
 * - The producer can successfully push elements into the buffer.
 * - The consumer can successfully pop elements from the buffer.
 * - The elements popped by the consumer are in the same order as they were pushed by the producer.
 */
TYPED_TEST(LockFreeRingBufferTest, ProducerConsumerInterleavedTest)
{
    std::thread producer(
        [this]()
        {
            for (int i = 0; i < 100000; ++i)
            {
                while (!this->buffer->push(static_cast<TypeParam>(i)))
                {
                    std::this_thread::yield();
                }
            }
        });

    std::thread consumer(
        [this]()
        {
            TypeParam value;
            for (int i = 0; i < 100000; ++i)
            {
                while (!this->buffer->pop(value))
                {
                    std::this_thread::yield();
                }
                ASSERT_EQ(value, static_cast<TypeParam>(i));
            }
        });

    producer.join();
    consumer.join();
}

/**
 * @brief Verifies exact-T lvalue and rvalue push overloads produce the same stored value.
 *
 * This test pushes matching lvalue and rvalue elements into a buffer and confirms
 * that extracted values are equal, verifying that both overloads route through the
 * same storage path for trivial types.
 *
 * @test
 * - Push an exact-T lvalue and an exact-T rvalue.
 * - Extract both values via pop_opt() and verify equality.
 */
TEST(LockFreeRingBufferPerfectForwardingTest, PushLvalueAndRvalueAreEquivalentForTrivialTypes)
{
    tools::lock_free_ring_buffer<int, 2> buffer;

    int lval = 42;
    ASSERT_TRUE(buffer.push(lval));
    ASSERT_TRUE(buffer.push(int(99)));  // NOLINT exact-T rvalue overload

    auto first_opt = buffer.pop_opt();
    auto second_opt = buffer.pop_opt();

    ASSERT_TRUE(first_opt.has_value());
    ASSERT_TRUE(second_opt.has_value());
    EXPECT_EQ(*first_opt, 42);
    EXPECT_EQ(*second_opt, 99);
    EXPECT_FALSE(buffer.pop_opt().has_value());
}

/**
 * @brief Verifies forwarding template conversion path for push.
 *
 * This test pushes convertible-type values (int literals) into a float buffer
 * and verifies that values are stored and extracted correctly via the forwarding
 * template conversion path.
 *
 * @test
 * - Push int literals into a float buffer using the conversion forwarding path.
 * - Extract values via pop_opt() and verify converted values.
 */
TEST(LockFreeRingBufferPerfectForwardingTest, PushConversionForwardingTemplate)
{
    tools::lock_free_ring_buffer<float, 2> buffer;

    ASSERT_TRUE(buffer.push(1));   // NOLINT int-to-float forwarding conversion
    ASSERT_TRUE(buffer.push(2));   // NOLINT int-to-float forwarding conversion

    auto first_opt = buffer.pop_opt();
    auto second_opt = buffer.pop_opt();

    ASSERT_TRUE(first_opt.has_value());
    ASSERT_TRUE(second_opt.has_value());
    EXPECT_FLOAT_EQ(*first_opt, 1.0F);
    EXPECT_FLOAT_EQ(*second_opt, 2.0F);
}

/**
 * @brief Verifies that pop_opt() returns nullopt when the buffer is empty.
 *
 * This test ensures pop_opt() correctly returns std::nullopt when no elements
 * are available and returns a valid optional after a successful push.
 *
 * @test
 * - Call pop_opt() on empty buffer and verify it returns nullopt.
 * - Push one element and call pop_opt() to verify it returns the value.
 * - Call pop_opt() again on empty buffer and verify nullopt.
 */
TEST(LockFreeRingBufferPerfectForwardingTest, PopOptReturnsNulloptWhenEmpty)
{
    tools::lock_free_ring_buffer<int, 2> buffer;

    EXPECT_FALSE(buffer.pop_opt().has_value());

    ASSERT_TRUE(buffer.push(7));

    auto item = buffer.pop_opt();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 7);

    EXPECT_FALSE(buffer.pop_opt().has_value());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
namespace
{
    /**
     * @brief Marker type intentionally not constructible as int.
     *
     * Used by C++20-only checks validating requires-based filtering.
     */
    struct non_constructible_payload
    {
    };

    template <typename Buf, typename U>
    concept has_push_call = requires(Buf& buf, U&& value)
    {
        buf.push(std::forward<U>(value));
    };
}

/**
 * @brief C++20-only compile-time validation of forwarding constraints.
 *
 * This test verifies that requires-constrained push in lock_free_ring_buffer
 * accepts constructible inputs and rejects non-constructible payload types.
 *
 * @test
 * - Assert constructibility expectations for valid and invalid payload types.
 * - Assert call-validity for the push forwarding template.
 * - Ensure non-constructible payload calls are rejected at compile time.
 */
TEST(LockFreeRingBufferPerfectForwardingTest, Cpp20RequiresConstraints)
{
    using buffer_t = tools::lock_free_ring_buffer<int, 2>;

    static_assert(std::is_constructible_v<int, int>);
    static_assert(std::is_constructible_v<int, float>);
    static_assert(!std::is_constructible_v<int, non_constructible_payload>);
    static_assert(!std::is_constructible_v<int, non_constructible_payload&>);

    static_assert(has_push_call<buffer_t, int>);
    static_assert(has_push_call<buffer_t, float>);
    static_assert(!has_push_call<buffer_t, non_constructible_payload>);
    static_assert(!has_push_call<buffer_t, non_constructible_payload&>);

    SUCCEED();
}
#endif

/**
 * @brief Verifies push_range supports initializer-list and generic range insertion.
 *
 * @test
 * - Push an initializer-list and verify the reported pushed count.
 * - Push a std::vector range and verify the reported pushed count.
 * - Drain buffer via pop_opt() and verify FIFO order.
 */
TEST(LockFreeRingBufferRangeTest, PushRangeSupportsInitializerAndRange)
{
    tools::lock_free_ring_buffer<int, 3> buffer; // capacity 8, 7 usable slots

    const std::size_t pushed_init = buffer.push_range({ 1, 2, 3 });
    ASSERT_EQ(pushed_init, 3U);

    const std::vector<int> extra_values = { 4, 5 };
    const std::size_t pushed_vec = buffer.push_range(extra_values);
    ASSERT_EQ(pushed_vec, 2U);

    for (int expected = 1; expected <= 5; ++expected)
    {
        auto item = buffer.pop_opt();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, expected);
    }
    EXPECT_FALSE(buffer.pop_opt().has_value());
}

/**
 * @brief Verifies iterator-pair pop_range extracts the effective item count.
 *
 * @test
 * - Push four values and extract three.
 * - Validate returned count and extracted order.
 * - Verify the fourth item remains and buffer is then empty.
 */
TEST(LockFreeRingBufferRangeTest, PopRangeIteratorReturnsEffectiveCount)
{
    tools::lock_free_ring_buffer<int, 3> buffer;
    const std::size_t pushed = buffer.push_range({ 10, 20, 30, 40 });
    ASSERT_EQ(pushed, 4U);

    std::array<int, 3> destination = { 0, 0, 0 };
    const std::size_t popped_count = buffer.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 3U);
    EXPECT_EQ(destination[0], 10);
    EXPECT_EQ(destination[1], 20);
    EXPECT_EQ(destination[2], 30);

    auto remaining = buffer.pop_opt();
    ASSERT_TRUE(remaining.has_value());
    EXPECT_EQ(*remaining, 40);
    EXPECT_FALSE(buffer.pop_opt().has_value());
}

/**
 * @brief Verifies pop_range clamps to available data when destination is larger.
 *
 * @test
 * - Push fewer elements than destination capacity.
 * - Validate returned count and that buffer is empty afterward.
 */
TEST(LockFreeRingBufferRangeTest, PopRangeClampsToAvailable)
{
    tools::lock_free_ring_buffer<int, 3> buffer;
    const std::size_t pushed = buffer.push_range({ 7, 8 });
    ASSERT_EQ(pushed, 2U);

    std::array<int, 5> destination = { 0, 0, 0, 0, 0 };
    const std::size_t popped_count = buffer.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 7);
    EXPECT_EQ(destination[1], 8);
    EXPECT_FALSE(buffer.pop_opt().has_value());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/**
 * @brief Verifies C++20 span-based pop_range overload.
 *
 * @test
 * - Push values into the buffer.
 * - Pop through std::span destination.
 * - Validate returned count, ordering, and remaining content.
 */
TEST(LockFreeRingBufferRangeTest, PopRangeSpanReturnsEffectiveCount)
{
    tools::lock_free_ring_buffer<int, 3> buffer;
    const std::size_t pushed = buffer.push_range({ 3, 4, 5 });
    ASSERT_EQ(pushed, 3U);

    std::array<int, 2> destination = { 0, 0 };
    const std::size_t popped_count = buffer.pop_range(std::span<int>(destination));

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 3);
    EXPECT_EQ(destination[1], 4);

    auto remaining = buffer.pop_opt();
    ASSERT_TRUE(remaining.has_value());
    EXPECT_EQ(*remaining, 5);
}

namespace
{
    template <typename Buf, typename Range>
    concept has_push_range_call = requires(Buf& buf, Range&& range)
    {
        buf.push_range(std::forward<Range>(range));
    };

    template <typename Buf, typename OutputIt>
    concept has_pop_range_iter_call = requires(Buf& buf, OutputIt first, OutputIt last)
    {
        buf.pop_range(first, last);
    };
}

/**
 * @brief Verifies C++20 range-call constraints for push_range and pop_range.
 */
TEST(LockFreeRingBufferRangeTest, Cpp20RangeConstraints)
{
    using buf_t = tools::lock_free_ring_buffer<int, 3>;

    static_assert(has_push_range_call<buf_t, std::vector<int>&>);
    static_assert(has_push_range_call<buf_t, std::initializer_list<int>>);
    static_assert(has_pop_range_iter_call<buf_t, int*>);

    SUCCEED();
}
#endif
