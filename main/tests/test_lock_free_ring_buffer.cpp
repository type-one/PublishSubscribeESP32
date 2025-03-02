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
// (c) 2025 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois      //
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

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

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
