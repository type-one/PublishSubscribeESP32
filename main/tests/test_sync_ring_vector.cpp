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
 */
TYPED_TEST(SyncRingVectorTest, Pop)
{
    this->vec->push(static_cast<TypeParam>(1));
    this->vec->push(static_cast<TypeParam>(2));
    this->vec->push(static_cast<TypeParam>(3));
    this->vec->pop();
    EXPECT_EQ(this->vec->size(), 2);
    EXPECT_EQ(this->vec->front(), static_cast<TypeParam>(2));
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
    auto producer = [this](int start, int end)
    {
        for (int i = start; i < end; ++i)
        {
            this->vec->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this](int count)
    {
        for (int i = 0; i < count; ++i)
        {
            this->vec->pop();
        }
    };

    std::thread producer1(producer, 0, 5);
    std::thread producer2(producer, 5, 10);
    std::thread consumer1(consumer, 5);
    std::thread consumer2(consumer, 5);

    producer1.join();
    producer2.join();
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
    auto producer = [this]()
    {
        for (int i = 0; i < 10; ++i)
        {
            this->vec->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this](int count)
    {
        for (int i = 0; i < count; ++i)
        {
            this->vec->pop();
        }
    };

    std::thread producerThread(producer);
    std::thread consumer1(consumer, 5);
    std::thread consumer2(consumer, 5);

    producerThread.join();
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
    auto producer = [this](int start, int end)
    {
        for (int i = start; i < end; ++i)
        {
            this->vec->push(static_cast<TypeParam>(i));
        }
    };

    auto consumer = [this](int count)
    {
        for (int i = 0; i < count; ++i)
        {
            this->vec->pop();
        }
    };

    std::thread producer1(producer, 0, 5);
    std::thread producer2(producer, 5, 10);
    std::thread consumerThread(consumer, 10);

    producer1.join();
    producer2.join();
    consumerThread.join();

    EXPECT_TRUE(this->vec->empty());
}
