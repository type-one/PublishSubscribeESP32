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
#include <concepts>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <span>
#endif

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
 * 5. Checks that the size of the queue is still 1.
 * 6. Get front element (value is 2) and pops the front element from the queue.
 * 7. Verifies that the front element of the queue is 1 now
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
    EXPECT_EQ(this->queue->size(), 1);
    EXPECT_EQ(this->queue->front_pop(), static_cast<TypeParam>(2));
    EXPECT_EQ(this->queue->size(), 0);
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

namespace
{
    struct forwarding_probe
    {
        forwarding_probe() = default;
        explicit forwarding_probe(int v)
            : value(v)
        {
        }

        forwarding_probe(const forwarding_probe& other)
            : value(other.value)
        {
            ++copy_ctor_count;
        }

        forwarding_probe(forwarding_probe&& other) noexcept
            : value(other.value)
        {
            other.value = -1;
            ++move_ctor_count;
        }

        forwarding_probe& operator=(const forwarding_probe& other)
        {
            if (this != &other)
            {
                value = other.value;
            }
            ++copy_assign_count;
            return *this;
        }

        forwarding_probe& operator=(forwarding_probe&& other) noexcept
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
            copy_ctor_count = 0;
            move_ctor_count = 0;
            copy_assign_count = 0;
            move_assign_count = 0;
        }

        int value = 0;
        static int copy_ctor_count;
        static int move_ctor_count;
        static int copy_assign_count;
        static int move_assign_count;
    };

    int forwarding_probe::copy_ctor_count = 0;
    int forwarding_probe::move_ctor_count = 0;
    int forwarding_probe::copy_assign_count = 0;
    int forwarding_probe::move_assign_count = 0;
}

/**
 * @brief Verifies push/emplace perfect-forwarding behavior for exact, brace-init, and conversion calls.
 *
 * This test validates the primary perfect-forwarding insertion paths for `sync_queue<std::string>`.
 * It covers exact-T lvalue/rvalue overloads, conversion forwarding from `const char*`,
 * brace-initialization compatibility, and variadic constructor forwarding with `emplace`.
 *
 * @test
 * - Push exact-T lvalue and exact-T rvalue values.
 * - Push a convertible type (`const char*`) to exercise forwarding conversion.
 * - Push a brace-initialized value to verify overload compatibility.
 * - Emplace constructor arguments and verify in-place construction result.
 * - Pop all values and check ordering and pco::expected payloads.
 */
TEST(SyncQueuePerfectForwardingTest, PushAndEmplaceSupportExactBraceAndConversionCalls)
{
    tools::sync_queue<std::string> queue;

    std::string exact_lvalue = "exact-lvalue";
    queue.push(exact_lvalue);                     // exact-T lvalue overload
    queue.push(std::string("exact-rvalue"));      // exact-T rvalue overload
    queue.push("conversion-from-cstr");           // forwarding conversion path
    queue.push({ "brace-init" });                 // brace-init path
    queue.emplace(4U, 'q');                       // variadic forwarding path

    auto pop_first = queue.front_pop();
    auto pop_second = queue.front_pop();
    auto pop_third = queue.front_pop();
    auto pop_fourth = queue.front_pop();
    auto pop_fifth = queue.front_pop();

    ASSERT_TRUE(pop_first.has_value());
    ASSERT_TRUE(pop_second.has_value());
    ASSERT_TRUE(pop_third.has_value());
    ASSERT_TRUE(pop_fourth.has_value());
    ASSERT_TRUE(pop_fifth.has_value());

    EXPECT_EQ(*pop_first, "exact-lvalue");
    EXPECT_EQ(*pop_second, "exact-rvalue");
    EXPECT_EQ(*pop_third, "conversion-from-cstr");
    EXPECT_EQ(*pop_fourth, "brace-init");
    EXPECT_EQ(*pop_fifth, "qqqq");
    EXPECT_TRUE(queue.empty());
}

/**
 * @brief Verifies ISR insertion APIs preserve perfect-forwarding behavior.
 *
 * This test validates that ISR-safe insertion methods follow the same functional
 * forwarding semantics as regular insertion methods for exact-T, conversion, brace-init,
 * and variadic emplace argument paths.
 *
 * @note In GoogleTest context there is no real ISR; standard C++ fallback behavior is used.
 *
 * @test
 * - Insert exact-T lvalue/rvalue payloads through `isr_push`.
 * - Insert a convertible payload and a brace-initialized payload through `isr_push`.
 * - Insert constructor arguments through `isr_emplace`.
 * - Pop all values and verify FIFO ordering and payload correctness.
 */
TEST(SyncQueuePerfectForwardingTest, IsrPushAndEmplaceSupportForwardingPaths)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    tools::sync_queue<std::string> queue;

    std::string exact_lvalue = "isr-lvalue";
    queue.isr_push(exact_lvalue);                   // exact-T lvalue overload
    queue.isr_push(std::string("isr-rvalue"));     // exact-T rvalue overload
    queue.isr_push("isr-conversion");              // forwarding conversion path
    queue.isr_push({ "isr-brace-init" });           // brace-init path
    queue.isr_emplace(3U, 'i');                     // variadic forwarding path

    auto pop_first = queue.front_pop();
    auto pop_second = queue.front_pop();
    auto pop_third = queue.front_pop();
    auto pop_fourth = queue.front_pop();
    auto pop_fifth = queue.front_pop();

    ASSERT_TRUE(pop_first.has_value());
    ASSERT_TRUE(pop_second.has_value());
    ASSERT_TRUE(pop_third.has_value());
    ASSERT_TRUE(pop_fourth.has_value());
    ASSERT_TRUE(pop_fifth.has_value());

    EXPECT_EQ(*pop_first, "isr-lvalue");
    EXPECT_EQ(*pop_second, "isr-rvalue");
    EXPECT_EQ(*pop_third, "isr-conversion");
    EXPECT_EQ(*pop_fourth, "isr-brace-init");
    EXPECT_EQ(*pop_fifth, "iii");
    EXPECT_TRUE(queue.empty());
}

/**
 * @brief Verifies exact-T overload preference for lvalue/rvalue insertion paths.
 *
 * This test uses `forwarding_probe` counters to confirm insertion behavior for
 * lvalue and rvalue payloads and validates resulting value ordering after extraction.
 *
 * @test
 * - Push an lvalue probe and an rvalue probe, then emplace a probe value.
 * - Extract all values with move-pop and verify ordering.
 * - Check constructor counters to validate lvalue/rvalue insertion behavior.
 */
TEST(SyncQueuePerfectForwardingTest, PushLvalueAndRvaluePreferExactTOverloads)
{
    forwarding_probe::reset_counters();

    tools::sync_queue<forwarding_probe> queue;
    forwarding_probe lvalue(10);

    queue.push(lvalue);
    queue.push(forwarding_probe(20));
    queue.emplace(30);

    // Validate insertion-path behavior before extraction introduces additional moves.
    EXPECT_EQ(forwarding_probe::copy_ctor_count, 1);
    EXPECT_EQ(forwarding_probe::move_ctor_count, 1);

    auto first = queue.front_pop_move();
    auto second = queue.front_pop_move();
    auto third = queue.front_pop_move();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());

    EXPECT_EQ(first->value, 10);
    EXPECT_EQ(second->value, 20);
    EXPECT_EQ(third->value, 30);
}

/**
 * @brief Verifies perfect-forwarding support for move-only payload types.
 *
 * This test ensures `sync_queue` accepts `std::unique_ptr<int>` through push,
 * emplace, and ISR emplace paths and that extraction via `front_pop_move` preserves
 * pointed values in FIFO order.
 *
 * @test
 * - Push a moved `unique_ptr` and verify source pointer becomes null.
 * - Emplace additional move-only payloads through regular and ISR APIs.
 * - Extract with move-pop and validate each pointed integer value.
 * - Verify queue becomes empty afterward.
 */
TEST(SyncQueuePerfectForwardingTest, SupportsMoveOnlyType)
{
    tools::sync_queue<std::unique_ptr<int>> queue;

    auto ptr = std::make_unique<int>(5);
    queue.push(std::move(ptr));
    EXPECT_EQ(ptr, nullptr);

    queue.emplace(std::make_unique<int>(9));

    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()
    queue.isr_emplace(std::make_unique<int>(12));

    auto first = queue.front_pop_move();
    auto second = queue.front_pop_move();
    auto third = queue.front_pop_move();

    ASSERT_TRUE(first.has_value());
    ASSERT_NE(*first, nullptr);
    EXPECT_EQ(**first, 5);

    ASSERT_TRUE(second.has_value());
    ASSERT_NE(*second, nullptr);
    EXPECT_EQ(**second, 9);

    ASSERT_TRUE(third.has_value());
    ASSERT_NE(*third, nullptr);
    EXPECT_EQ(**third, 12);

    EXPECT_FALSE(queue.front_pop_move().has_value());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
namespace
{
    struct non_constructible_payload
    {
    };

    template <typename Q, typename U>
    concept has_push_call = requires(Q& queue_ref, U&& value)
    {
        queue_ref.push(std::forward<U>(value));
    };

    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    template <typename Q, typename U>
    concept has_isr_push_call = requires(Q& queue_ref, U&& value)
    {
        queue_ref.isr_push(std::forward<U>(value));
    };

    template <typename Q, typename... Args>
    concept has_emplace_call = requires(Q& queue_ref, Args&&... args)
    {
        queue_ref.emplace(std::forward<Args>(args)...);
    };

    template <typename Q, typename... Args>
    concept has_isr_emplace_call = requires(Q& queue_ref, Args&&... args)
    {
        queue_ref.isr_emplace(std::forward<Args>(args)...);
    };

    template <typename Q, typename TRange>
    concept has_push_range_call = requires(Q& queue_ref, TRange& range)
    {
        queue_ref.push_range(range);
    };

    template <typename Q, typename TRange>
    concept has_isr_push_range_call = requires(Q& queue_ref, TRange& range)
    {
        queue_ref.isr_push_range(range);
    };

    template <typename Q, typename TDest>
    concept has_pop_range_iter_call = requires(Q& queue_ref, TDest& destination)
    {
        { queue_ref.pop_range(destination.begin(), destination.end()) } -> std::same_as<std::size_t>;
    };

    template <typename Q, typename U>
    concept has_pop_range_span_call = requires(Q& queue_ref, std::span<U> destination)
    {
        { queue_ref.pop_range(destination) } -> std::same_as<std::size_t>;
    };

}

/**
 * @brief C++20-only compile-time validation of forwarding constraints.
 *
 * This test verifies that `requires`-constrained forwarding APIs in `sync_queue`
 * accept constructible argument sets and reject non-constructible payload types for
 * push/isr_push/emplace/isr_emplace call expressions.
 *
 * @test
 * - Assert constructibility expectations for valid and invalid payload types.
 * - Assert call-validity concepts for each constrained forwarding API.
 * - Ensure non-constructible payload calls are rejected at compile time.
 */
TEST(SyncQueuePerfectForwardingTest, Cpp20RequiresConstraints)
{
    using queue_t = tools::sync_queue<std::string>;

    static_assert(std::is_constructible_v<std::string, const char*>);
    static_assert(std::is_constructible_v<std::string, std::size_t, char>);
    static_assert(!std::is_constructible_v<std::string, non_constructible_payload>);
    static_assert(!std::is_constructible_v<std::string, non_constructible_payload&>);

    static_assert(has_push_call<queue_t, const char*>);
    static_assert(has_push_call<queue_t, std::string&&>);
    static_assert(!has_push_call<queue_t, non_constructible_payload>);
    static_assert(!has_push_call<queue_t, non_constructible_payload&>);

    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()/emplace() and size()

    static_assert(has_isr_push_call<queue_t, const char*>);
    static_assert(has_isr_push_call<queue_t, std::string&&>);
    static_assert(!has_isr_push_call<queue_t, non_constructible_payload>);
    static_assert(!has_isr_push_call<queue_t, non_constructible_payload&>);

    static_assert(has_emplace_call<queue_t, std::size_t, char>);
    static_assert(has_isr_emplace_call<queue_t, std::size_t, char>);
    static_assert(!has_emplace_call<queue_t, non_constructible_payload>);
    static_assert(!has_isr_emplace_call<queue_t, non_constructible_payload>);

    SUCCEED();
}

/**
 * @brief C++20-only compile-time validation of push_range and isr_push_range constraints.
 *
 * Verifies that push_range and isr_push_range accept ranges with compatible element types
 * and reject ranges whose element type cannot be used to construct T.
 *
 * @test
 * - Assert push_range/isr_push_range accept std::vector and std::array of compatible type.
 * - Assert push_range/isr_push_range reject ranges with non-constructible element type.
 */
TEST(SyncQueueRangeTest, Cpp20RangeConstraints)
{
    using queue_t = tools::sync_queue<std::string>;
    using destination_t = std::array<std::string, 4>;

    static_assert(std::ranges::input_range<std::vector<std::string>>);
    static_assert(std::ranges::input_range<std::array<std::string, 3>>);

    static_assert(has_push_range_call<queue_t, std::vector<std::string>>);
    static_assert(has_push_range_call<queue_t, std::array<std::string, 3>>);
    static_assert(!has_push_range_call<queue_t, std::vector<non_constructible_payload>>);

    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()
    static_assert(has_isr_push_range_call<queue_t, std::vector<std::string>>);
    static_assert(!has_isr_push_range_call<queue_t, std::vector<non_constructible_payload>>);

    static_assert(has_pop_range_iter_call<queue_t, destination_t>);
    static_assert(has_pop_range_span_call<queue_t, std::string>);

    SUCCEED();
}
#endif

#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
/**
 * @brief C++17 compile-time checks for iterator-based pop_range API.
 *
 * @test
 * - Ensure pop_range iterator overload returns std::size_t.
 */
TEST(SyncQueueRangeTest, Cpp17IteratorPopRangeCompileChecks)
{
    tools::sync_queue<int> queue;
    std::array<int, 3> destination = { 0, 0, 0 };

    using pop_return_t = decltype(queue.pop_range(destination.begin(), destination.end()));

     static_assert(std::is_same<pop_return_t, std::size_t>::value, "pop_range iterator overload must return std::size_t");

    SUCCEED();
}
#endif

/**
 * @brief Test case for push_range inserting elements from an std::vector.
 *
 * @test
 * - Push a vector of ints into the queue via push_range.
 * - Verify size, front, and back after insertion.
 */
TEST(SyncQueueRangeTest, PushRangeFromVector)
{
    tools::sync_queue<int> queue;
    const std::vector<int> source = { 1, 2, 3, 4, 5 };
    queue.push_range(source);
    EXPECT_EQ(queue.size(), 5U);
    EXPECT_EQ(queue.front().value_or(0), 1);
    EXPECT_EQ(queue.back().value_or(0), 5);
}

/**
 * @brief Test case for push_range inserting elements from an std::array.
 *
 * @test
 * - Push an array of ints into the queue via push_range.
 * - Verify size and FIFO order after extraction.
 */
TEST(SyncQueueRangeTest, PushRangeFromStdArray)
{
    tools::sync_queue<int> queue;
    const std::array<int, 3> source = { 10, 20, 30 };
    queue.push_range(source);
    EXPECT_EQ(queue.size(), 3U);
    EXPECT_EQ(queue.front_pop().value_or(0), 10);
    EXPECT_EQ(queue.front_pop().value_or(0), 20);
    EXPECT_EQ(queue.front_pop().value_or(0), 30);
    EXPECT_TRUE(queue.empty());
}

/**
 * @brief Test case for isr_push_range inserting elements from an std::vector.
 *
 * @note In GoogleTest context there is no real ISR; standard C++ fallback behavior is used.
 *
 * @test
 * - Push a vector of ints into the queue via isr_push_range.
 * - Verify size and front element after insertion.
 */
TEST(SyncQueueRangeTest, IsrPushRangeFromVector)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()
    tools::sync_queue<int> queue;
    const std::vector<int> source = { 7, 8, 9 };
    queue.isr_push_range(source);
    EXPECT_EQ(queue.isr_size(), 3U);
    EXPECT_EQ(queue.front().value_or(0), 7);
}

/**
 * @brief Test case for push_range accepting brace-init lists.
 *
 * @test
 * - Push a brace-init list through push_range.
 * - Verify FIFO ordering and resulting queue size.
 */
TEST(SyncQueueRangeTest, PushRangeSupportsInitializerList)
{
    tools::sync_queue<int> queue;
    queue.push_range({ 11, 22, 33 });

    EXPECT_EQ(queue.size(), 3U);
    EXPECT_EQ(queue.front_pop().value_or(0), 11);
    EXPECT_EQ(queue.front_pop().value_or(0), 22);
    EXPECT_EQ(queue.front_pop().value_or(0), 33);
    EXPECT_TRUE(queue.empty());
}

/**
 * @brief Test case for isr_push_range accepting brace-init lists.
 *
 * @note In GoogleTest context there is no real ISR; standard C++ fallback behavior is used.
 *
 * @test
 * - Push a brace-init list through isr_push_range.
 * - Verify FIFO ordering and resulting queue size.
 */
TEST(SyncQueueRangeTest, IsrPushRangeSupportsInitializerList)
{
    // note: they won't be any real ISR in GTests as standard C++ implementation fallback to push()
    tools::sync_queue<int> queue;
    queue.isr_push_range({ 44, 55, 66 });

    EXPECT_EQ(queue.isr_size(), 3U);
    EXPECT_EQ(queue.front_pop().value_or(0), 44);
    EXPECT_EQ(queue.front_pop().value_or(0), 55);
    EXPECT_EQ(queue.front_pop().value_or(0), 66);
    EXPECT_TRUE(queue.empty());
}

/**
 * @brief Test case for pop_range extracting batches in FIFO order.
 *
 * @test
 * - Fill queue with five elements.
 * - Pop a batch smaller than queue size.
 * - Verify extracted order and remaining queue state.
 */
TEST(SyncQueueRangeTest, PopRangeReturnsFifoBatch)
{
    tools::sync_queue<int> queue;
    queue.push_range({ 1, 2, 3, 4, 5 });

    std::array<int, 3> batch = { 0, 0, 0 };
    const std::size_t popped_count = queue.pop_range(batch.begin(), batch.end());

    ASSERT_EQ(popped_count, 3U);
    EXPECT_EQ(batch[0], 1);
    EXPECT_EQ(batch[1], 2);
    EXPECT_EQ(batch[2], 3);
    EXPECT_EQ(queue.size(), 2U);
    EXPECT_EQ(queue.front().value_or(0), 4);
}

/**
 * @brief Test case for pop_range when asked for more items than available.
 *
 * @test
 * - Fill queue with two elements.
 * - Request a larger batch than available.
 * - Verify all available elements are returned and queue becomes empty.
 */
TEST(SyncQueueRangeTest, PopRangeClampsToAvailableItems)
{
    tools::sync_queue<int> queue;
    queue.push_range({ 9, 10 });

    std::array<int, 10> batch = { 0 };
    const std::size_t popped_count = queue.pop_range(batch.begin(), batch.end());

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(batch[0], 9);
    EXPECT_EQ(batch[1], 10);
    EXPECT_TRUE(queue.empty());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/**
 * @brief Test case for span-based pop_range overload.
 *
 * @test
 * - Fill queue and extract into contiguous storage via span.
 * - Verify extracted count and values.
 */
TEST(SyncQueueRangeTest, PopRangeWithSpanReturnsCount)
{
    tools::sync_queue<int> queue;
    queue.push_range({ 6, 7, 8 });

    std::array<int, 5> batch = { 0, 0, 0, 0, 0 };
    const std::size_t popped_count = queue.pop_range(std::span<int>(batch));

    ASSERT_EQ(popped_count, 3U);
    EXPECT_EQ(batch[0], 6);
    EXPECT_EQ(batch[1], 7);
    EXPECT_EQ(batch[2], 8);
    EXPECT_TRUE(queue.empty());
}

#endif
