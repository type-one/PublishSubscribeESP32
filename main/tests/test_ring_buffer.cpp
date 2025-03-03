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
#include <utility>

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

using MyTypes = ::testing::Types<int, float, double, char , std::complex<double>>;
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

    this->buffer->push(static_cast<TypeParam>(6)); // This should overwrite the first element (1)

    EXPECT_EQ(this->buffer->size(), 6); // Size should be 6 because we increment size even when overwriting
    EXPECT_EQ(this->buffer->front(), static_cast<TypeParam>(2)); // The new front should be 2
    EXPECT_EQ(this->buffer->back(), static_cast<TypeParam>(6));  // The new back should be 6
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
