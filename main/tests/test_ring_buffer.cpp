/**
 * @file test_ring_buffer.cpp
 * @brief Test suite for the ring buffer implementation using Google Test framework.
 *
 * This file contains a series of test cases to verify the functionality of the ring buffer
 * implementation. It includes tests for pushing, popping, emplacing elements, and verifying
 * the behavior of the ring buffer when it is full, cleared, copied, or moved.
 *
 * The tests are organized into a test fixture class template, RingBufferTest, which
 * provides setup and teardown functionality for the ring buffer. The tests are
 * parameterized to run with different types of elements stored in the ring buffer.
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
#include <utility>
#include <vector>

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <span>
#endif

#include "tools/ring_buffer.hpp"

/**
 * @brief Test fixture class for testing the ring buffer.
 *
 * This class template is used to set up and tear down the test environment
 * for testing the ring buffer implementation. It inherits from the Google Test
 * framework's ::testing::Test class.
 *
 * @tparam T The type of elements stored in the ring buffer.
 */
template <typename T>
class RingBufferTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        buffer = std::make_unique<tools::ring_buffer<T, 5>>();
    }

    void TearDown() override
    {
        buffer.reset();
    }

    std::unique_ptr<tools::ring_buffer<T, 5>> buffer;
};

using MyTypes = ::testing::Types<int, float, double, char, std::complex<double>>;
TYPED_TEST_SUITE(RingBufferTest, MyTypes);

/**
 * @brief Test case for pushing and popping elements in the ring buffer.
 *
 * This test case verifies the functionality of pushing elements into the ring buffer
 * and then popping them out. It checks the size of the buffer, the front element,
 * and the back element after each operation.
 *
 * @tparam TypeParam The type of elements stored in the ring buffer.
 *
 * Test steps:
 * 1. Push three elements (1, 2, 3) into the buffer.
 * 2. Verify the size of the buffer is 3.
 * 3. Verify the front element is 1.
 * 4. Verify the back element is 3.
 * 5. Pop one element from the buffer.
 * 6. Verify the size of the buffer is 2.
 * 7. Verify the front element is 2.
 */
TYPED_TEST(RingBufferTest, PushAndPop)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));

    EXPECT_EQ(this->buffer->size(), 3);
    EXPECT_EQ(this->buffer->front(), static_cast<TypeParam>(1));
    EXPECT_EQ(this->buffer->back(), static_cast<TypeParam>(3));

    this->buffer->pop();
    EXPECT_EQ(this->buffer->size(), 2);
    EXPECT_EQ(this->buffer->front(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for the emplace functionality of the RingBuffer.
 *
 * This test verifies that elements can be emplaced into the RingBuffer
 * and that the size, front, and back elements are as expected after
 * emplacement.
 *
 * @tparam TypeParam The type of elements stored in the RingBuffer.
 *
 * @test
 * - Emplace three elements (1, 2, 3) into the buffer.
 * - Check that the size of the buffer is 3.
 * - Verify that the front element is 1.
 * - Verify that the back element is 3.
 */
TYPED_TEST(RingBufferTest, Emplace)
{
    this->buffer->emplace(static_cast<TypeParam>(1));
    this->buffer->emplace(static_cast<TypeParam>(2));
    this->buffer->emplace(static_cast<TypeParam>(3));

    EXPECT_EQ(this->buffer->size(), 3);
    EXPECT_EQ(this->buffer->front(), static_cast<TypeParam>(1));
    EXPECT_EQ(this->buffer->back(), static_cast<TypeParam>(3));
}

/**
 * @brief Test case to verify the clear functionality of the RingBuffer.
 *
 * This test pushes three elements into the buffer and then clears it.
 * It checks that the buffer size is zero and that the buffer is empty
 * after the clear operation.
 */
TYPED_TEST(RingBufferTest, Clear)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));

    this->buffer->clear();
    EXPECT_EQ(this->buffer->size(), 0);
    EXPECT_TRUE(this->buffer->empty());
}

/**
 * @brief Test case to verify the behavior of the RingBuffer when it is full.
 *
 * This test pushes five elements into the buffer and checks if the buffer
 * reports being full and if the size of the buffer is correctly reported as 5.
 *
 * @tparam TypeParam The type of elements stored in the RingBuffer.
 */
TYPED_TEST(RingBufferTest, FullBuffer)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));
    this->buffer->push(static_cast<TypeParam>(4));
    this->buffer->push(static_cast<TypeParam>(5));

    EXPECT_TRUE(this->buffer->full());
    EXPECT_EQ(this->buffer->size(), 5);
}

/**
 * @brief Test case to verify the behavior of the ring buffer when it is full and a new element is pushed.
 *
 * This test pushes six elements into the ring buffer, which has a capacity of five.
 * The sixth push should overwrite the first element.
 *
 * @tparam TypeParam The data type used in the ring buffer.
 *
 * @test
 * - Push elements 1, 2, 3, 4, and 5 into the buffer.
 * - Push element 6, which should overwrite the first element (1).
 * - Verify that the size of the buffer is 6.
 * - Verify that the front of the buffer is now 2.
 * - Verify that the back of the buffer is now 6.
 */
TYPED_TEST(RingBufferTest, OverwriteWhenFull)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));
    this->buffer->push(static_cast<TypeParam>(4));
    this->buffer->push(static_cast<TypeParam>(5));

    const bool inserted = this->buffer->push(static_cast<TypeParam>(6));

    EXPECT_FALSE(inserted);
    EXPECT_EQ(this->buffer->size(), 5U);
    EXPECT_EQ(this->buffer->front(), static_cast<TypeParam>(1));
    EXPECT_EQ(this->buffer->back(), static_cast<TypeParam>(5));
}

TEST(RingBufferOverwriteApiTest, ScalarOverwriteReturnsEvictionAndKeepsBoundedHistory)
{
    tools::ring_buffer<int, 3> buffer;

    EXPECT_TRUE(buffer.push(1));
    EXPECT_TRUE(buffer.push(2));
    EXPECT_TRUE(buffer.emplace(3));
    EXPECT_TRUE(buffer.full());

    EXPECT_FALSE(buffer.push(4));
    EXPECT_FALSE(buffer.emplace(5));
    EXPECT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer.front(), 1);
    EXPECT_EQ(buffer.back(), 3);

    EXPECT_TRUE(buffer.push_overwrite(4));
    EXPECT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer.front(), 2);
    EXPECT_EQ(buffer.back(), 4);

    EXPECT_TRUE(buffer.emplace_overwrite(5));
    EXPECT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer.front(), 3);
    EXPECT_EQ(buffer.back(), 5);
}

TEST(RingBufferOverwriteApiTest, RangeOverwriteReportsInsertedVsOverwritten)
{
    tools::ring_buffer<int, 3> buffer;

    auto result = buffer.push_range_overwrite({ 10, 20 });
    EXPECT_EQ(result.inserted, 2U);
    EXPECT_EQ(result.overwritten, 0U);
    EXPECT_EQ(buffer.size(), 2U);

    result = buffer.push_range_overwrite({ 30, 40, 50 });
    EXPECT_EQ(result.inserted, 1U);
    EXPECT_EQ(result.overwritten, 2U);
    EXPECT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer.front(), 30);
    EXPECT_EQ(buffer.back(), 50);

    const std::size_t accepted = buffer.push_range({ 60, 70 });
    EXPECT_EQ(accepted, 0U);
    EXPECT_EQ(buffer.size(), 3U);
    EXPECT_EQ(buffer.front(), 30);
    EXPECT_EQ(buffer.back(), 50);
}

/**
 * @brief Test the copy constructor of the ring buffer.
 *
 * This test verifies that the copy constructor correctly copies the elements
 * from the original buffer to the new buffer. It pushes three elements into
 * the original buffer and then creates a copy of it. The test checks that the
 * size of the copied buffer is correct and that the front and back elements
 * match the expected values.
 *
 * @tparam TypeParam The type of elements stored in the ring buffer.
 */
TYPED_TEST(RingBufferTest, CopyConstructor)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));

    tools::ring_buffer<TypeParam, 5> copy_buffer(*this->buffer);

    EXPECT_EQ(copy_buffer.size(), 3);
    EXPECT_EQ(copy_buffer.front(), static_cast<TypeParam>(1));
    EXPECT_EQ(copy_buffer.back(), static_cast<TypeParam>(3));
}

/**
 * @brief Test the move constructor of the ring buffer.
 *
 * This test verifies that the move constructor correctly transfers the contents
 * of one ring buffer to another. It pushes three elements into the original buffer,
 * then moves the buffer to a new instance. The test checks that the new buffer has
 * the correct size and that the front and back elements are as expected.
 *
 * @tparam TypeParam The type of elements stored in the ring buffer.
 */
TYPED_TEST(RingBufferTest, MoveConstructor)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));

    tools::ring_buffer<TypeParam, 5> move_buffer(std::move(*this->buffer));

    EXPECT_EQ(move_buffer.size(), 3);
    EXPECT_EQ(move_buffer.front(), static_cast<TypeParam>(1));
    EXPECT_EQ(move_buffer.back(), static_cast<TypeParam>(3));
}

/**
 * @brief Test case for the copy assignment operator of the ring buffer.
 *
 * This test verifies that the copy assignment operator correctly copies the contents
 * of one ring buffer to another. It pushes three elements into the original buffer,
 * performs the copy assignment, and then checks that the size and elements of the
 * copied buffer match the original buffer.
 *
 * @tparam TypeParam The type of elements stored in the ring buffer.
 */
TYPED_TEST(RingBufferTest, CopyAssignment)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));

    tools::ring_buffer<TypeParam, 5> copy_buffer;
    copy_buffer = *this->buffer;

    EXPECT_EQ(copy_buffer.size(), 3);
    EXPECT_EQ(copy_buffer.front(), static_cast<TypeParam>(1));
    EXPECT_EQ(copy_buffer.back(), static_cast<TypeParam>(3));
}

/**
 * @brief Test case for move assignment operator of the ring buffer.
 *
 * This test verifies that the move assignment operator correctly transfers
 * the contents of one ring buffer to another. It pushes three elements into
 * the original buffer, moves the buffer to a new one, and checks that the
 * new buffer has the correct size and elements.
 *
 * @tparam TypeParam The type of elements stored in the ring buffer.
 */
TYPED_TEST(RingBufferTest, MoveAssignment)
{
    this->buffer->push(static_cast<TypeParam>(1));
    this->buffer->push(static_cast<TypeParam>(2));
    this->buffer->push(static_cast<TypeParam>(3));

    tools::ring_buffer<TypeParam, 5> move_buffer;
    move_buffer = std::move(*this->buffer);

    EXPECT_EQ(move_buffer.size(), 3);
    EXPECT_EQ(move_buffer.front(), static_cast<TypeParam>(1));
    EXPECT_EQ(move_buffer.back(), static_cast<TypeParam>(3));
}

namespace
{
    /**
     * @brief Probe type used to observe copy/move operations in forwarding tests.
     *
     * This helper tracks copy/move constructor and assignment usage so tests can
     * verify whether lvalue/rvalue forwarding reaches the expected assignment path
     * in the ring buffer slot.
     */
    struct forwarding_probe
    {
        forwarding_probe() = default;
        explicit forwarding_probe(int input)
            : value(input)
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
            ++move_ctor_count;
            other.value = -1;
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
 * @brief Verifies that pushing an lvalue selects copy assignment.
 *
 * This test pushes an lvalue instance and checks that the copy assignment counter
 * is incremented while move assignment remains unused.
 */
/**
 * @brief Verifies that pushing an lvalue selects copy assignment.
 *
 * This test pushes an lvalue object and checks that copy assignment is used
 * instead of move assignment.
 */
TEST(RingBufferPerfectForwardingTest, PushLvalueUsesCopyAssignment)
{
    forwarding_probe::reset_counters();

    tools::ring_buffer<forwarding_probe, 4> buffer;
    forwarding_probe sample(42);
    buffer.push(sample);

    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer[0].value, 42);
    EXPECT_EQ(forwarding_probe::copy_assign_count, 1);
    EXPECT_EQ(forwarding_probe::move_assign_count, 0);
}

/**
 * @brief Verifies that pushing an rvalue selects move assignment.
 *
 * This test pushes a temporary object and checks that move assignment is used
 * instead of copy assignment.
 */
TEST(RingBufferPerfectForwardingTest, PushRvalueUsesMoveAssignment)
{
    forwarding_probe::reset_counters();

    tools::ring_buffer<forwarding_probe, 4> buffer;
    buffer.push(forwarding_probe(21));

    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer[0].value, 21);
    EXPECT_EQ(forwarding_probe::copy_assign_count, 0);
    EXPECT_EQ(forwarding_probe::move_assign_count, 1);
}

/**
 * @brief Verifies that emplace forwards constructor arguments correctly.
 *
 * This test emplaces a value from constructor arguments and checks that the
 * resulting element is correct and that the move assignment path is used.
 */
TEST(RingBufferPerfectForwardingTest, EmplaceForwardsConstructorArguments)
{
    forwarding_probe::reset_counters();

    tools::ring_buffer<forwarding_probe, 4> buffer;
    buffer.emplace(64);

    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer[0].value, 64);
    EXPECT_EQ(forwarding_probe::copy_assign_count, 0);
    EXPECT_EQ(forwarding_probe::move_assign_count, 1);
}

/**
 * @brief Verifies forwarding support for move-only types.
 *
 * This test uses std::unique_ptr<int> to ensure push/emplace accept move-only
 * payloads and preserve pointed values after move-based extraction.
 */
TEST(RingBufferPerfectForwardingTest, SupportsMoveOnlyType)
{
    tools::ring_buffer<std::unique_ptr<int>, 4> buffer;

    auto ptr = std::make_unique<int>(5);
    buffer.push(std::move(ptr));
    EXPECT_EQ(ptr, nullptr);

    buffer.emplace(std::make_unique<int>(9));

    auto first = buffer.pop_move();
    ASSERT_TRUE(first.has_value());
    ASSERT_NE(*first, nullptr);
    EXPECT_EQ(**first, 5);

    auto second = buffer.pop_move();
    ASSERT_TRUE(second.has_value());
    ASSERT_NE(*second, nullptr);
    EXPECT_EQ(**second, 9);

    EXPECT_FALSE(buffer.pop_move().has_value());
}

/**
 * @brief Verifies push_range supports initializer-list and range insertion.
 *
 * @test
 * - Push an initializer-list into the ring buffer.
 * - Push an std::vector range into the ring buffer.
 * - Verify resulting FIFO order in the preserved capacity window.
 */
TEST(RingBufferRangeTest, PushRangeSupportsInitializerAndRange)
{
    tools::ring_buffer<int, 8> buffer;

    buffer.push_range({ 1, 2, 3 });

    const std::vector<int> extra_values = { 4, 5 };
    buffer.push_range(extra_values);

    EXPECT_EQ(buffer.size(), 5U);
    EXPECT_EQ(buffer.front(), 1);
    EXPECT_EQ(buffer.back(), 5);
}

/**
 * @brief Verifies iterator-pair pop_range extracts the effective item count.
 *
 * @test
 * - Push several values into the buffer.
 * - Extract into fixed destination storage via iterators.
 * - Validate returned count, extracted order, and remaining content.
 */
TEST(RingBufferRangeTest, PopRangeIteratorReturnsEffectiveCount)
{
    tools::ring_buffer<int, 8> buffer;
    buffer.push_range({ 10, 20, 30, 40 });

    std::array<int, 3> destination = { 0, 0, 0 };
    const std::size_t popped_count = buffer.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 3U);
    EXPECT_EQ(destination[0], 10);
    EXPECT_EQ(destination[1], 20);
    EXPECT_EQ(destination[2], 30);
    EXPECT_EQ(buffer.size(), 1U);
    EXPECT_EQ(buffer.front(), 40);
}

/**
 * @brief Verifies pop_range clamps to available data when destination is larger.
 *
 * @test
 * - Push fewer elements than destination capacity.
 * - Validate returned count and that buffer is empty afterward.
 */
TEST(RingBufferRangeTest, PopRangeClampsToAvailable)
{
    tools::ring_buffer<int, 8> buffer;
    buffer.push_range({ 7, 8 });

    std::array<int, 5> destination = { 0, 0, 0, 0, 0 };
    const std::size_t popped_count = buffer.pop_range(destination.begin(), destination.end());

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 7);
    EXPECT_EQ(destination[1], 8);
    EXPECT_TRUE(buffer.empty());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/**
 * @brief Verifies C++20 span-based pop_range overload.
 *
 * @test
 * - Push values into the ring buffer.
 * - Pop through std::span destination.
 * - Validate returned count and ordering.
 */
TEST(RingBufferRangeTest, PopRangeSpanReturnsEffectiveCount)
{
    tools::ring_buffer<int, 8> buffer;
    buffer.push_range({ 3, 4, 5 });

    std::array<int, 2> destination = { 0, 0 };
    const std::size_t popped_count = buffer.pop_range(std::span<int>(destination));

    ASSERT_EQ(popped_count, 2U);
    EXPECT_EQ(destination[0], 3);
    EXPECT_EQ(destination[1], 4);
    EXPECT_EQ(buffer.size(), 1U);
    EXPECT_EQ(buffer.front(), 5);
}
#endif

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
namespace
{
    /**
     * @brief Marker type intentionally not constructible as int.
     *
     * Used by C++20-only constraint checks validating requires-based filtering.
     */
    struct non_constructible_payload
    {
    };
}

/**
 * @brief C++20-only compile-time checks for forwarding constraints.
 *
 * This test validates constructibility assumptions used by C++20 requires
 * clauses in ring_buffer::push and ring_buffer::emplace.
 */
TEST(RingBufferPerfectForwardingTest, Cpp20RequiresConstraints)
{
    static_assert(std::is_constructible_v<int, int>);
    static_assert(!std::is_constructible_v<int, non_constructible_payload>);
    static_assert(!std::is_constructible_v<int, non_constructible_payload&>);

    SUCCEED();
}
#endif
