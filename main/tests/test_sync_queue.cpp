/**
 * @file test_sync_queue.cpp
 * @brief Unit tests for the SyncQueue class template.
 *
 * This file contains a series of unit tests for the SyncQueue class template,
 * which provides a thread-safe queue implementation. The tests cover various
 * operations such as pushing, popping, checking the front and back elements,
 * and handling multiple producers and consumers.
 *
 * The tests are implemented using the Google Test framework and are organized
 * into a test fixture class template, SyncQueueTest, which is parameterized
 * by the type of elements stored in the queue.
 *
 * The test cases verify the correct behavior of the SyncQueue class template
 * for different types of elements, including int, float, double, char, and
 * std::complex.
 *
 * The tests also include scenarios for handling operations from an ISR context
 * and concurrent access by multiple threads.
 *
 * @author Laurent Lardinois and Copilot GPT-4o
 *
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

#include <complex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "tools/sync_queue.hpp"

/**
 * @brief Test fixture for SyncQueue tests.
 *
 * @tparam T The type of elements in the queue.
 */
template <typename T>
class SyncQueueTest : public ::testing::Test
{
protected:
    /**
     * @brief Set up the test environment.
     */
    void SetUp() override
    {
        // Code here will be called immediately after the constructor (right before each test).
        queue = std::make_unique<tools::sync_queue<T>>();
    }

    /**
     * @brief Tear down the test environment.
     */
    void TearDown() override
    {
        // Code here will be called immediately after each test (right before the destructor).
        queue.reset();
    }

    std::unique_ptr<tools::sync_queue<T>> queue;
};

using TestTypes = ::testing::Types<int, float, double, char, std::complex<double>>;
TYPED_TEST_SUITE(SyncQueueTest, TestTypes);

/**
 * @brief Test case for pushing elements into the queue and checking its size.
 *
 * This test case verifies that elements can be pushed into the queue and that
 * the size of the queue is updated correctly.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * @test
 * - Push an element with value 1 into the queue.
 * - Push an element with value 2 into the queue.
 * - Check that the size of the queue is 2.
 */
TYPED_TEST(SyncQueueTest, PushAndSize)
{
    this->queue->push(static_cast<TypeParam>(1));
    this->queue->push(static_cast<TypeParam>(2));
    EXPECT_EQ(this->queue->size(), 2);
}

/**
 * @brief Test case for checking the front and back elements of the queue.
 *
 * This test case verifies that the front and back elements of the queue
 * are correctly returned after pushing elements into the queue.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * Test steps:
 * 1. Push an element with value 1 into the queue.
 * 2. Push an element with value 2 into the queue.
 * 3. Check that the front element of the queue is 1.
 * 4. Check that the back element of the queue is 2.
 */
TYPED_TEST(SyncQueueTest, FrontAndBack)
{
    this->queue->push(static_cast<TypeParam>(1));
    this->queue->push(static_cast<TypeParam>(2));
    EXPECT_EQ(this->queue->front(), static_cast<TypeParam>(1));
    EXPECT_EQ(this->queue->back(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for popping elements from the SyncQueue.
 *
 * This test case verifies the behavior of the SyncQueue when elements are popped.
 * It performs the following steps:
 * 1. Pushes two elements (1 and 2) into the queue.
 * 2. Pops one element from the queue.
 * 3. Checks that the size of the queue is 1.
 * 4. Verifies that the front element of the queue is 2.
 *
 * @tparam TypeParam The type of elements stored in the SyncQueue.
 */
TYPED_TEST(SyncQueueTest, Pop)
{
    this->queue->push(static_cast<TypeParam>(1));
    this->queue->push(static_cast<TypeParam>(2));
    this->queue->pop();
    EXPECT_EQ(this->queue->size(), 1);
    EXPECT_EQ(this->queue->front(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for checking if the queue is empty after popping all elements.
 *
 * This test case verifies that the queue is empty after pushing and popping an element.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * Test steps:
 * 1. Push an element with value 1 into the queue.
 * 2. Pop the element from the queue.
 * 3. Check that the queue is empty.
 */
TYPED_TEST(SyncQueueTest, Empty)
{
    this->queue->push(static_cast<TypeParam>(1));
    this->queue->pop();
    EXPECT_TRUE(this->queue->empty());
}

/**
 * @brief Test case for emplacing an element into the queue.
 *
 * This test case verifies that an element can be emplaced into the queue
 * and that the size and front element of the queue are updated correctly.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * Test steps:
 * 1. Emplace an element with value 3 into the queue.
 * 2. Check that the size of the queue is 1.
 * 3. Check that the front element of the queue is 3.
 */
TYPED_TEST(SyncQueueTest, Emplace)
{
    this->queue->emplace(static_cast<TypeParam>(3));
    EXPECT_EQ(this->queue->size(), 1);
    EXPECT_EQ(this->queue->front(), static_cast<TypeParam>(3));
}

/**
 * @brief Test case for ISR push and size functionality in SyncQueue.
 *
 * This test verifies that the isr_push method correctly adds elements to the queue
 * and that the isr_size method accurately reflects the number of elements in the queue.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * @test
 * - Push an element with value 4 into the queue using isr_push and check if the size is 1.
 * - Push another element with value 5 into the queue using isr_push and check if the size is 2.
 */
TYPED_TEST(SyncQueueTest, IsrPushAndSize)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push() and size()
    this->queue->isr_push(static_cast<TypeParam>(4));
    EXPECT_EQ(this->queue->isr_size(), 1);
    this->queue->isr_push(static_cast<TypeParam>(5));
    EXPECT_EQ(this->queue->isr_size(), 2);
}

/**
 * @brief Test case for ISR emplace operation in SyncQueue.
 *
 * This test verifies that the `isr_emplace` method correctly inserts an element
 * into the queue from an ISR context and that the size and back element of the
 * queue are updated accordingly.
 *
 * @tparam TypeParam The type parameter for the test case.
 *
 * @test
 * - Emplaces an element with value 6 into the queue using `isr_emplace`.
 * - Checks that the size of the queue is 1 using `isr_size`.
 * - Verifies that the back element of the queue is 6.
 */
TYPED_TEST(SyncQueueTest, IsrEmplace)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to emplace() and size()
    this->queue->isr_emplace(static_cast<TypeParam>(6));
    EXPECT_EQ(this->queue->isr_size(), 1);
    EXPECT_EQ(this->queue->back(), static_cast<TypeParam>(6));
}

/**
 * @brief Test case for multiple operations on SyncQueue.
 *
 * This test performs a series of operations on the SyncQueue including push, emplace,
 * isr_push, and isr_emplace. It verifies the size, front, and back elements of the queue
 * after these operations. It also checks the queue's behavior after popping all elements.
 *
 * @tparam TypeParam The type of elements stored in the SyncQueue.
 *
 * Operations performed:
 * - Push elements 1 and 2.
 * - Emplace element 3.
 * - ISR push element 4.
 * - ISR emplace element 5.
 * - Verify the size of the queue is 5.
 * - Verify the front element is 1.
 * - Verify the back element is 5.
 * - Pop elements and verify the front element after each pop.
 * - Verify the queue is empty after all pops.
 */
TYPED_TEST(SyncQueueTest, MultipleOperations)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    this->queue->push(static_cast<TypeParam>(1));
    this->queue->push(static_cast<TypeParam>(2));
    this->queue->emplace(static_cast<TypeParam>(3));
    this->queue->isr_push(static_cast<TypeParam>(4));
    this->queue->isr_emplace(static_cast<TypeParam>(5));
    EXPECT_EQ(this->queue->size(), 5);
    EXPECT_EQ(this->queue->front(), static_cast<TypeParam>(1));
    EXPECT_EQ(this->queue->back(), static_cast<TypeParam>(5));
    this->queue->pop();
    EXPECT_EQ(this->queue->front(), static_cast<TypeParam>(2));
    this->queue->pop();
    this->queue->pop();
    this->queue->pop();
    this->queue->pop();
    EXPECT_TRUE(this->queue->empty());
}

/**
 * @brief Test case for multiple producers and multiple consumers using a synchronized queue.
 *
 * This test creates multiple producer and consumer threads. Each producer thread pushes a
 * specified number of elements into the queue, and each consumer thread pops elements from
 * the queue. The test ensures that all elements are processed and the queue is empty at the end.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * @test
 * - Creates 4 producer threads, each pushing 100 elements into the queue.
 * - Creates 4 consumer threads, each popping 100 elements from the queue.
 * - Ensures that the queue is empty after all producers and consumers have finished.
 */
TYPED_TEST(SyncQueueTest, MultipleProducersMultipleConsumers)
{
    constexpr const int num_producers = 4;
    constexpr const int num_consumers = 4;
    constexpr const int num_elements = 100;

    auto producer = [this]()
    {
        for (int i = 0; i < num_elements; ++i)
        {
            this->queue->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this]()
    {
        for (int i = 0; i < num_elements; ++i)
        {
            while (this->queue->empty())
            {
                std::this_thread::yield();
            }
            this->queue->pop();
        }
    };

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < num_producers; ++i)
    {
        producers.emplace_back(producer);
    }

    for (int i = 0; i < num_consumers; ++i)
    {
        consumers.emplace_back(consumer);
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    for (auto& consumer : consumers)
    {
        consumer.join();
    }

    EXPECT_TRUE(this->queue->empty());
}

/**
 * @brief Test case for single producer and multiple consumers scenario.
 *
 * This test case verifies the behavior of the queue when a single producer
 * pushes elements into the queue and multiple consumers pop elements from
 * the queue concurrently.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * The test creates one producer thread and multiple consumer threads.
 * The producer thread pushes a specified number of elements into the queue.
 * Each consumer thread pops a portion of the elements from the queue.
 *
 * The test ensures that all elements are consumed and the queue is empty
 * at the end.
 */
TYPED_TEST(SyncQueueTest, SingleProducerMultipleConsumers)
{
    constexpr const int num_consumers = 4;
    constexpr const int num_elements = 100;

    auto producer = [this]()
    {
        for (int i = 0; i < num_elements; ++i)
        {
            this->queue->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this]()
    {
        for (int i = 0; i < num_elements / num_consumers; ++i)
        {
            while (this->queue->empty())
            {
                std::this_thread::yield();
            }
            this->queue->pop();
        }
    };

    std::thread producer_thread(producer);
    std::vector<std::thread> consumers;

    for (int i = 0; i < num_consumers; ++i)
    {
        consumers.emplace_back(consumer);
    }

    producer_thread.join();

    for (auto& consumer : consumers)
    {
        consumer.join();
    }

    EXPECT_TRUE(this->queue->empty());
}

/**
 * @brief Test case for multiple producers and a single consumer using a synchronized queue.
 *
 * This test spawns multiple producer threads and a single consumer thread. Each producer
 * pushes a predefined number of elements into the queue, and the consumer pops all elements
 * from the queue. The test verifies that the queue is empty after all producers have finished
 * and the consumer has consumed all elements.
 *
 * @tparam TypeParam The type of elements in the queue.
 *
 * @test
 * - Spawns `num_producers` producer threads, each pushing `num_elements` elements into the queue.
 * - Spawns a single consumer thread that pops `num_producers * num_elements` elements from the queue.
 * - Verifies that the queue is empty after all elements have been consumed.
 */
TYPED_TEST(SyncQueueTest, MultipleProducersSingleConsumer)
{
    constexpr const int num_producers = 4;
    constexpr const int num_elements = 100;

    auto producer = [this]()
    {
        for (int i = 0; i < num_elements; ++i)
        {
            this->queue->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this]()
    {
        for (int i = 0; i < num_producers * num_elements; ++i)
        {
            while (this->queue->empty())
            {
                std::this_thread::yield();
            }
            this->queue->pop();
        }
    };

    std::vector<std::thread> producers;
    std::thread consumer_thread(consumer);

    for (int i = 0; i < num_producers; ++i)
    {
        producers.emplace_back(producer);
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    consumer_thread.join();

    EXPECT_TRUE(this->queue->empty());
}
