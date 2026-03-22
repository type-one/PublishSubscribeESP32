/**
 * @file test_memory_pipe.cpp
 * @brief Unit tests for the memory_pipe functionality using Google Test framework.
 * 
 * This file contains a series of test cases to verify the correct behavior of the memory_pipe class.
 * The tests cover basic send/receive operations, ISR send/receive operations, timeout scenarios,
 * and single producer/consumer scenarios.
 * 
 * The tests use the Google Test framework and are organized into a test fixture class MemoryPipeTest.
 * Each test case is implemented as a member function of this class.
 * 
 * @note The tests assume a buffer size of 10 for the memory_pipe instance.
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
#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

#include "tools/memory_pipe.hpp"

/**
 * @class MemoryPipeTest
 * @brief A test fixture class for testing the memory_pipe functionality.
 *
 * This class inherits from ::testing::Test and provides setup and teardown
 * methods for initializing and cleaning up a memory_pipe instance.
 *
 */
class MemoryPipeTest : public ::testing::Test
{
protected:
    /**
     * @brief Sets up the test environment by initializing the memory_pipe with a buffer size.
     *
     */
    void SetUp() override
    {
        buffer_size = 10;
        pipe = std::make_unique<tools::memory_pipe>(buffer_size);
    }

    /**
     * @brief Cleans up the test environment by resetting the memory_pipe instance.
     *
     */
    void TearDown() override
    {
        pipe.reset();
    }

    /**
     * @brief Size of the buffer used by the memory_pipe.
     */
    std::size_t buffer_size;

    /**
     * @brief Unique pointer to the memory_pipe instance used in the tests.
     */
    std::unique_ptr<tools::memory_pipe> pipe;
};

/**
 * @brief Test case for sending and receiving data through a memory pipe.
 *
 * This test verifies that data can be sent and received correctly through a memory pipe.
 * It sends a vector of bytes and checks if the sent data matches the received data.
 *
 * @test
 * - Sends a vector of bytes {1, 2, 3, 4, 5} through the memory pipe.
 * - Verifies that the number of sent bytes matches the size of the data to send.
 * - Receives the data from the memory pipe.
 * - Verifies that the number of received bytes matches the size of the data sent.
 * - Asserts that the received data matches the sent data.
 */
TEST_F(MemoryPipeTest, SendReceive)
{
    std::vector<std::uint8_t> data_to_send = { 1, 2, 3, 4, 5 };
    std::vector<std::uint8_t> data_received;

    auto timeout = std::chrono::milliseconds(100);

    std::size_t sent_bytes = pipe->send(data_to_send, timeout);
    ASSERT_EQ(sent_bytes, data_to_send.size());

    std::size_t received_bytes = pipe->receive(data_received, data_to_send.size(), timeout);
    ASSERT_EQ(received_bytes, data_to_send.size());
    ASSERT_EQ(data_received, data_to_send);
}

/**
 * @brief Test to verify ISR send and receive functionality of MemoryPipe.
 *
 * This test sends a vector of bytes using the ISR send function and verifies
 * that the correct number of bytes are sent. It then receives the bytes using
 * the ISR receive function and checks that the received bytes match the sent bytes.
 *
 * @test
 * - Sends a vector of bytes {6, 7, 8, 9, 10} using the ISR send function.
 * - Asserts that the number of sent bytes is equal to the size of the data to send.
 * - Receives the bytes using the ISR receive function.
 * - Asserts that the number of received bytes is equal to the size of the data to send.
 * - Asserts that the received data matches the sent data.
 */
TEST_F(MemoryPipeTest, IsrSendReceive)
{
    std::vector<std::uint8_t> data_to_send = { 6, 7, 8, 9, 10 };
    std::vector<std::uint8_t> data_received;

    std::size_t sent_bytes = pipe->isr_send(data_to_send);
    ASSERT_EQ(sent_bytes, data_to_send.size());

    std::size_t received_bytes = pipe->isr_receive(data_received, data_to_send.size());
    ASSERT_EQ(received_bytes, data_to_send.size());
    ASSERT_EQ(data_received, data_to_send);
}

/**
 * @brief Test case for sending and receiving data with a timeout using MemoryPipe.
 *
 * This test verifies that when attempting to send data that exceeds the buffer size,
 * the send operation does not send all the data. It also checks that the receive
 * operation correctly receives the amount of data that was sent within the specified timeout.
 *
 * @test
 * - Create a vector of data that exceeds the buffer size.
 * - Attempt to send the data with a timeout.
 * - Verify that not all data is sent.
 * - Attempt to receive the data with a timeout.
 * - Verify that the received data size matches the sent data size.
 */
TEST_F(MemoryPipeTest, SendReceiveTimeout)
{
    std::vector<std::uint8_t> data_to_send(buffer_size + 1, 1); // Exceed buffer size
    std::vector<std::uint8_t> data_received;

    auto timeout = std::chrono::milliseconds(100);

    std::size_t sent_bytes = pipe->send(data_to_send, timeout);
    ASSERT_LT(sent_bytes, data_to_send.size()); // Should not send all data

    std::size_t received_bytes = pipe->receive(data_received, sent_bytes, timeout);
    ASSERT_EQ(received_bytes, sent_bytes);
    ASSERT_EQ(data_received.size(), sent_bytes);
}

/**
 * @brief Test to verify ISR send and receive functionality with a timeout using MemoryPipe.
 *
 * This test sends a vector of bytes that exceeds the buffer size using the ISR send function
 * and verifies that not all data is sent. It then receives the bytes using the ISR receive
 * function and checks that the received bytes match the sent bytes.
 *
 * @test
 * - Create a vector of data that exceeds the buffer size.
 * - Attempt to send the data using the ISR send function.
 * - Verify that not all data is sent.
 * - Attempt to receive the data using the ISR receive function.
 * - Verify that the received data size matches the sent data size.
 */
TEST_F(MemoryPipeTest, IsrSendReceiveTimeout)
{
    std::vector<std::uint8_t> data_to_send(buffer_size + 1, 1); // Exceed buffer size
    std::vector<std::uint8_t> data_received;

    std::size_t sent_bytes = pipe->isr_send(data_to_send);
    ASSERT_LT(sent_bytes, data_to_send.size()); // Should not send all data

    std::size_t received_bytes = pipe->isr_receive(data_received, sent_bytes);
    ASSERT_EQ(received_bytes, sent_bytes);
    ASSERT_EQ(data_received.size(), sent_bytes);
}

/**
 * @brief Test case for single producer and single consumer using MemoryPipe.
 *
 * This test creates a producer thread that sends a vector of bytes through the pipe
 * and a consumer thread that receives the bytes from the pipe. The test verifies that
 * the number of bytes sent and received are equal and that the data received matches
 * the data sent.
 *
 * @note The test uses a timeout of 100 milliseconds for both sending and receiving operations.
 */
TEST_F(MemoryPipeTest, SingleProducerSingleConsumer)
{
    std::vector<std::uint8_t> data_to_send = { 11, 12, 13, 14, 15 };
    std::vector<std::uint8_t> data_received;
    auto timeout = std::chrono::milliseconds(100);

    std::thread producer(
        [&]()
        {
            std::size_t sent_bytes = pipe->send(data_to_send, timeout);
            ASSERT_EQ(sent_bytes, data_to_send.size());
        });

    std::thread consumer(
        [&]()
        {
            std::size_t received_bytes = pipe->receive(data_received, data_to_send.size(), timeout);
            ASSERT_EQ(received_bytes, data_to_send.size());
            ASSERT_EQ(data_received, data_to_send);
        });

    producer.join();
    consumer.join();
}

namespace
{
    struct vector_convertible_data
    {
        std::vector<std::uint8_t> payload;

        operator std::vector<std::uint8_t>() &&
        {
            return std::move(payload);
        }
    };
}

TEST_F(MemoryPipeTest, PerfectForwardingSendSupportsLvalueRvalueAndConversion)
{
    constexpr std::size_t local_capacity = 64U;
    tools::memory_pipe local_pipe(local_capacity);

    const auto timeout = std::chrono::milliseconds(100);

    // lvalue vector path
    std::vector<std::uint8_t> lvalue_data = { 1U, 2U, 3U, 4U };
    std::vector<std::uint8_t> received;
    ASSERT_EQ(local_pipe.send(lvalue_data, timeout), lvalue_data.size());
    ASSERT_EQ(local_pipe.receive(received, lvalue_data.size(), timeout), lvalue_data.size());
    ASSERT_EQ(received, lvalue_data);

    // rvalue vector path
    received.clear();
    std::vector<std::uint8_t> rvalue_expected = { 5U, 6U, 7U };
    ASSERT_EQ(local_pipe.send(std::vector<std::uint8_t> { 5U, 6U, 7U }, timeout), rvalue_expected.size());
    ASSERT_EQ(local_pipe.receive(received, rvalue_expected.size(), timeout), rvalue_expected.size());
    ASSERT_EQ(received, rvalue_expected);

    // conversion path
    received.clear();
    vector_convertible_data converted { std::vector<std::uint8_t> { 9U, 8U, 7U, 6U } };
    ASSERT_EQ(local_pipe.send(std::move(converted), timeout), 4U);
    ASSERT_EQ(local_pipe.receive(received, 4U, timeout), 4U);
    ASSERT_EQ(received, (std::vector<std::uint8_t> { 9U, 8U, 7U, 6U }));
}

TEST_F(MemoryPipeTest, SendRangeSupportsInitializerAndRange)
{
    tools::memory_pipe local_pipe(64U);
    const auto timeout = std::chrono::milliseconds(100);

    const std::vector<std::uint8_t> extra_values = { 4U, 5U };
    ASSERT_EQ(local_pipe.send_range({ 1U, 2U, 3U }, timeout), 3U);
    ASSERT_EQ(local_pipe.send_range(extra_values, timeout), extra_values.size());

    std::array<std::uint8_t, 5> received = { 0U, 0U, 0U, 0U, 0U };
    const std::size_t received_count = local_pipe.receive_range(received.begin(), received.end(), timeout);

    ASSERT_EQ(received_count, 5U);
    EXPECT_EQ(received[0], 1U);
    EXPECT_EQ(received[1], 2U);
    EXPECT_EQ(received[2], 3U);
    EXPECT_EQ(received[3], 4U);
    EXPECT_EQ(received[4], 5U);
}

TEST_F(MemoryPipeTest, IsrSendRangeSupportsInitializerAndRange)
{
    tools::memory_pipe local_pipe(64U);

    const std::vector<std::uint8_t> extra_values = { 8U, 9U };
    ASSERT_EQ(local_pipe.isr_send_range({ 6U, 7U }), 2U);
    ASSERT_EQ(local_pipe.isr_send_range(extra_values), extra_values.size());

    std::array<std::uint8_t, 4> received = { 0U, 0U, 0U, 0U };
    const std::size_t received_count = local_pipe.isr_receive_range(received.begin(), received.end());

    ASSERT_EQ(received_count, 4U);
    EXPECT_EQ(received[0], 6U);
    EXPECT_EQ(received[1], 7U);
    EXPECT_EQ(received[2], 8U);
    EXPECT_EQ(received[3], 9U);
}

TEST_F(MemoryPipeTest, ReceiveRangeClampsToAvailable)
{
    tools::memory_pipe local_pipe(64U);
    const auto timeout = std::chrono::milliseconds(100);

    ASSERT_EQ(local_pipe.send_range({ 11U, 12U }, timeout), 2U);

    std::array<std::uint8_t, 5> received = { 0U, 0U, 0U, 0U, 0U };
    const std::size_t received_count = local_pipe.receive_range(received.begin(), received.end(), timeout);

    ASSERT_EQ(received_count, 2U);
    EXPECT_EQ(received[0], 11U);
    EXPECT_EQ(received[1], 12U);
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
TEST_F(MemoryPipeTest, ReceiveRangeSpanReturnsEffectiveCount)
{
    tools::memory_pipe local_pipe(64U);
    const auto timeout = std::chrono::milliseconds(100);

    ASSERT_EQ(local_pipe.send_range({ 21U, 22U, 23U }, timeout), 3U);

    std::array<std::uint8_t, 2> received = { 0U, 0U };
    const std::size_t received_count = local_pipe.receive_range(std::span<std::uint8_t>(received), timeout);

    ASSERT_EQ(received_count, 2U);
    EXPECT_EQ(received[0], 21U);
    EXPECT_EQ(received[1], 22U);
}

TEST_F(MemoryPipeTest, IsrReceiveRangeSpanReturnsEffectiveCount)
{
    tools::memory_pipe local_pipe(64U);

    ASSERT_EQ(local_pipe.isr_send_range({ 31U, 32U, 33U }), 3U);

    std::array<std::uint8_t, 2> received = { 0U, 0U };
    const std::size_t received_count = local_pipe.isr_receive_range(std::span<std::uint8_t>(received));

    ASSERT_EQ(received_count, 2U);
    EXPECT_EQ(received[0], 31U);
    EXPECT_EQ(received[1], 32U);
}

namespace
{
    template <typename TData, typename = void>
    struct can_call_send_with_timeout : std::false_type
    {
    };

    template <typename TData>
    struct can_call_send_with_timeout<TData,
        std::void_t<decltype(std::declval<tools::memory_pipe&>().send(std::declval<TData>(),
            std::declval<const std::chrono::duration<std::uint64_t, std::milli>&>()))>> : std::true_type
    {
    };

    template <typename TData, typename = void>
    struct can_call_isr_send : std::false_type
    {
    };

    template <typename TData>
    struct can_call_isr_send<TData,
        std::void_t<decltype(std::declval<tools::memory_pipe&>().isr_send(std::declval<TData>()))>>
        : std::true_type
    {
    };

    template <typename TRange, typename = void>
    struct can_call_send_range_with_timeout : std::false_type
    {
    };

    template <typename TRange>
    struct can_call_send_range_with_timeout<TRange,
        std::void_t<decltype(std::declval<tools::memory_pipe&>().send_range(std::declval<TRange>(),
            std::declval<const std::chrono::duration<std::uint64_t, std::milli>&>()))>> : std::true_type
    {
    };

    template <typename TRange, typename = void>
    struct can_call_isr_send_range : std::false_type
    {
    };

    template <typename TRange>
    struct can_call_isr_send_range<TRange,
        std::void_t<decltype(std::declval<tools::memory_pipe&>().isr_send_range(std::declval<TRange>()))>>
        : std::true_type
    {
    };
}

TEST(MemoryPipeCompileTimeChecks, PerfectForwardingSendConstraints)
{
    static_assert(can_call_send_with_timeout<std::vector<std::uint8_t>>::value);
    static_assert(can_call_send_with_timeout<vector_convertible_data>::value);
    static_assert(!can_call_send_with_timeout<int>::value);

    static_assert(can_call_isr_send<std::vector<std::uint8_t>>::value);
    static_assert(can_call_isr_send<vector_convertible_data>::value);
    static_assert(!can_call_isr_send<int>::value);
}

TEST(MemoryPipeCompileTimeChecks, RangeConstraints)
{
    static_assert(can_call_send_range_with_timeout<std::vector<std::uint8_t>>::value);
    static_assert(can_call_send_range_with_timeout<std::initializer_list<std::uint8_t>>::value);
    static_assert(!can_call_send_range_with_timeout<int>::value);

    static_assert(can_call_isr_send_range<std::vector<std::uint8_t>>::value);
    static_assert(can_call_isr_send_range<std::initializer_list<std::uint8_t>>::value);
    static_assert(!can_call_isr_send_range<int>::value);

    const auto transformed = std::views::iota(0, 3)
        | std::views::transform([](const int value)
          {
              return static_cast<std::uint8_t>(value + 41);
          });

    static_assert(can_call_send_range_with_timeout<decltype(transformed)>::value);
    static_assert(can_call_isr_send_range<decltype(transformed)>::value);

    SUCCEED();
}
#endif
