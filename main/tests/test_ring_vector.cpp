/**
 * @file test_ring_vector.cpp
 * @brief Unit tests for the tools::ring_vector class template.
 *
 * This file contains a set of unit tests for the tools::ring_vector class template.
 * The tests are implemented using the Google Test framework and cover various
 * functionalities of the ring vector, including element access, push and pop operations,
 * checking empty and full states, clearing the vector, and resizing it.
 *
 * The tests are organized into a test fixture class template, RingVectorTest, which
 * provides setup and teardown functionality for the ring vector. The tests are
 * parameterized to run with different types of elements stored in the ring vector.
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

#include "tools/ring_vector.hpp"

/**
 * @brief Test fixture for testing the tools::ring_vector class template.
 *
 * This class template inherits from ::testing::Test and provides setup and teardown
 * functionality for testing the tools::ring_vector class template.
 *
 * @tparam T The type of elements stored in the ring vector.
 */
template <typename T>
class RingVectorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        rv = std::make_unique<tools::ring_vector<T>>(5);
    }

    void TearDown() override
    {
        rv.reset();
    }

    std::unique_ptr<tools::ring_vector<T>> rv;
};

using MyTypes = ::testing::Types<int, float, double, char, std::complex<double>>;
TYPED_TEST_SUITE(RingVectorTest, MyTypes);

/**
 * @brief Test the operator[] functionality of the RingVector.
 *
 * This test case verifies that the operator[] correctly accesses elements
 * in the RingVector after multiple push and pop operations.
 *
 * @tparam TypeParam The type of elements stored in the RingVector.
 *
 * The test performs the following steps:
 * 1. Push elements 1, 2, 3, 4, and 5 into the RingVector.
 * 2. Verify that the elements are correctly accessible using operator[].
 * 3. Pop one element from the RingVector and push element 6.
 * 4. Verify that the elements are correctly accessible using operator[] after the pop and push operations.
 */
TYPED_TEST(RingVectorTest, TestOperatorBrackets)
{
    this->rv->push(static_cast<TypeParam>(1));
    this->rv->push(static_cast<TypeParam>(2));
    this->rv->push(static_cast<TypeParam>(3));
    this->rv->push(static_cast<TypeParam>(4));
    this->rv->push(static_cast<TypeParam>(5));

    EXPECT_EQ((*this->rv)[0], static_cast<TypeParam>(1));
    EXPECT_EQ((*this->rv)[1], static_cast<TypeParam>(2));
    EXPECT_EQ((*this->rv)[2], static_cast<TypeParam>(3));
    EXPECT_EQ((*this->rv)[3], static_cast<TypeParam>(4));
    EXPECT_EQ((*this->rv)[4], static_cast<TypeParam>(5));

    this->rv->pop();
    this->rv->push(static_cast<TypeParam>(6));

    EXPECT_EQ((*this->rv)[0], static_cast<TypeParam>(2));
    EXPECT_EQ((*this->rv)[1], static_cast<TypeParam>(3));
    EXPECT_EQ((*this->rv)[2], static_cast<TypeParam>(4));
    EXPECT_EQ((*this->rv)[3], static_cast<TypeParam>(5));
    EXPECT_EQ((*this->rv)[4], static_cast<TypeParam>(6));
}

/**
 * @brief Test case for pushing and popping elements in the RingVector.
 *
 * This test case verifies the functionality of pushing elements into the RingVector
 * and popping elements from it. It checks the front and back elements after each operation.
 *
 * @tparam TypeParam The type of elements stored in the RingVector.
 *
 * @test
 * - Push elements 10, 20, and 30 into the RingVector.
 * - Verify that the front element is 10 and the back element is 30.
 * - Pop the front element and verify that the new front element is 20.
 * - Push element 40 into the RingVector and verify that the back element is 40.
 */
TYPED_TEST(RingVectorTest, TestPushAndPop)
{
    this->rv->push(static_cast<TypeParam>(10));
    this->rv->push(static_cast<TypeParam>(20));
    this->rv->push(static_cast<TypeParam>(30));

    EXPECT_EQ(this->rv->front(), static_cast<TypeParam>(10));
    EXPECT_EQ(this->rv->back(), static_cast<TypeParam>(30));

    this->rv->pop();
    EXPECT_EQ(this->rv->front(), static_cast<TypeParam>(20));

    this->rv->push(static_cast<TypeParam>(40));
    EXPECT_EQ(this->rv->back(), static_cast<TypeParam>(40));
}

/**
 * @brief Test case to verify the behavior of the RingVector when it is empty and full.
 *
 * This test checks the following scenarios:
 * - The RingVector is initially empty and not full.
 * - After pushing elements into the RingVector, it becomes full.
 * - After popping an element from the RingVector, it is no longer full.
 *
 * @tparam TypeParam The type of elements stored in the RingVector.
 */
TYPED_TEST(RingVectorTest, TestEmptyAndFull)
{
    EXPECT_TRUE(this->rv->empty());
    EXPECT_FALSE(this->rv->full());

    this->rv->push(static_cast<TypeParam>(1));
    this->rv->push(static_cast<TypeParam>(2));
    this->rv->push(static_cast<TypeParam>(3));
    this->rv->push(static_cast<TypeParam>(4));
    this->rv->push(static_cast<TypeParam>(5));

    EXPECT_FALSE(this->rv->empty());
    EXPECT_TRUE(this->rv->full());

    this->rv->pop();
    EXPECT_FALSE(this->rv->full());
}

/**
 * @brief Test case to verify the clear functionality of the RingVector.
 *
 * This test case verifies that the clear method correctly empties the RingVector
 * and resets its size to zero.
 *
 * @tparam TypeParam The type of elements stored in the RingVector.
 *
 * @test
 * - Push elements 1, 2, and 3 into the RingVector.
 * - Clear the RingVector and verify that it is empty and its size is zero.
 */
TYPED_TEST(RingVectorTest, TestClear)
{
    this->rv->push(static_cast<TypeParam>(1));
    this->rv->push(static_cast<TypeParam>(2));
    this->rv->push(static_cast<TypeParam>(3));

    this->rv->clear();
    EXPECT_TRUE(this->rv->empty());
    EXPECT_EQ(this->rv->size(), 0);
}

/**
 * @brief Test case for resizing the RingVector.
 *
 * This test case verifies the behavior of the RingVector when it is resized.
 * It performs the following steps:
 * 1. Pushes five elements into the RingVector.
 * 2. Resizes the RingVector to a smaller size (3) and checks the size and capacity.
 * 3. Verifies that the remaining elements are the last three elements pushed.
 * 4. Resizes the RingVector to a larger size (6) and checks the capacity.
 * 5. Pushes two more elements and verifies the size.
 *
 * @tparam TypeParam The type of elements stored in the RingVector.
 */
TYPED_TEST(RingVectorTest, TestResize)
{
    this->rv->push(static_cast<TypeParam>(1));
    this->rv->push(static_cast<TypeParam>(2));
    this->rv->push(static_cast<TypeParam>(3));
    this->rv->push(static_cast<TypeParam>(4));
    this->rv->push(static_cast<TypeParam>(5));

    this->rv->resize(3);
    EXPECT_EQ(this->rv->size(), 3);
    EXPECT_EQ(this->rv->capacity(), 3);

    EXPECT_EQ((*this->rv)[0], static_cast<TypeParam>(3));
    EXPECT_EQ((*this->rv)[1], static_cast<TypeParam>(4));
    EXPECT_EQ((*this->rv)[2], static_cast<TypeParam>(5));

    this->rv->resize(6);
    EXPECT_EQ(this->rv->capacity(), 6);
    this->rv->push(static_cast<TypeParam>(6));
    this->rv->push(static_cast<TypeParam>(7));
    EXPECT_EQ(this->rv->size(), 5);
}
