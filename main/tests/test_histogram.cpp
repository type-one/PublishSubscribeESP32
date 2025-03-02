/**
 * @file test_histogram.cpp
 * @brief Unit tests for the histogram class using Google Test framework.
 *
 * This file contains a series of test cases for the histogram class,
 * verifying its functionality including adding elements, calculating
 * statistical measures (average, variance, median), and Gaussian probability.
 *
 * The tests are implemented using Google Test framework and cover various
 * scenarios to ensure the correctness of the histogram class.
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

#include <cmath>
#include <memory>

#include "tests/test_helper.hpp"
#include "tools/histogram.hpp"

/**
 * @brief Test fixture class for histogram tests.
 *
 * This class is a template test fixture for testing the histogram class.
 * It inherits from ::testing::Test and provides setup and teardown functionality.
 *
 * @tparam T The type parameter for the histogram class.
 */
template <typename T>
class HistogramTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        hist = std::make_unique<tools::histogram<T>>();
    }

    void TearDown() override
    {
        hist.reset();
    }

    std::unique_ptr<tools::histogram<T>> hist;
};


using MyTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(HistogramTest, MyTypes);

/**
 * @brief Test case for adding elements to the histogram and checking the top element.
 *
 * This test adds three elements to the histogram and verifies that the top element
 * is correctly identified, the total count of elements is accurate, and the occurrence
 * count of the top element is correct.
 *
 * @tparam TypeParam The type of elements in the histogram.
 *
 * Test steps:
 * - Add 5 to the histogram.
 * - Add 3 to the histogram.
 * - Add another 5 to the histogram.
 * - Check that the top element is 5.
 * - Check that the total count of elements is 3.
 * - Check that the occurrence count of the top element (5) is 2.
 */
TYPED_TEST(HistogramTest, AddAndTop)
{
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(3));
    this->hist->add(static_cast<TypeParam>(5));
    EXPECT_FLOAT_EQ(this->hist->top(), static_cast<TypeParam>(5));
    EXPECT_EQ(this->hist->total_count(), 3);
    EXPECT_EQ(this->hist->top_occurence(), 2);
}

/**
 * @brief Test case for calculating the average value in a histogram.
 *
 * This test adds several values to the histogram and checks if the calculated
 * average is correct.
 *
 * @tparam TypeParam The data type of the histogram values.
 *
 * Test Steps:
 * 1. Add the value 5 to the histogram.
 * 2. Add the value 3 to the histogram.
 * 3. Add the value 5 to the histogram.
 * 4. Add the value 7 to the histogram.
 * 5. Add the value 7 to the histogram.
 * 6. Add the value 7 to the histogram.
 * 7. Check if the average of the histogram values is 5.6666666666667.
 */
TYPED_TEST(HistogramTest, Average)
{
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(3));
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    // https://www.calculator.net/average-calculator.html
    EXPECT_FLOAT_EQ(this->hist->average(), static_cast<TypeParam>(5.6666666666667));
}

/**
 * @brief Test case for calculating the variance of the histogram.
 *
 * This test adds a series of values to the histogram and then checks if the
 * calculated variance matches the expected value.
 *
 * @tparam TypeParam The data type used for the histogram values.
 *
 * @details
 * The test performs the following steps:
 * 1. Adds the values 5, 3, 5, 7, 7, and 7 to the histogram.
 * 2. Calculates the average of the histogram values.
 * 3. Asserts that the variance of the histogram, given the calculated average,
 *    is equal to 2.2222222222222.
 * 4. Asserts that the standard deviation of the histogram, given the calculated variance,
 *    is equal to 1.4907119849999.
 */
TYPED_TEST(HistogramTest, Variance)
{
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(3));
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    TypeParam avg = this->hist->average();
    TypeParam variance = this->hist->variance(avg);
    // https://www.calculator.net/standard-deviation-calculator.html
    EXPECT_FLOAT_EQ(variance, static_cast<TypeParam>(2.2222222222222));
    EXPECT_FLOAT_EQ(this->hist->standard_deviation(variance), static_cast<TypeParam>(1.4907119849999));
}

/**
 * @brief Test case for checking the median calculation in the Histogram class for an even number of items.
 *
 * This test adds a series of values (even number) to the histogram and verifies that the median
 * is calculated correctly.
 *
 * @tparam TypeParam The data type used for the histogram values.
 *
 * Test steps:
 * - Add the value 5 to the histogram.
 * - Add the value 3 to the histogram.
 * - Add the value 5 to the histogram.
 * - Add the value 7 to the histogram.
 * - Add the value 7 to the histogram.
 * - Add the value 7 to the histogram.
 * - Verify that the median of the histogram is 6.
 */
TYPED_TEST(HistogramTest, MedianEven)
{
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(3));
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    // https://www.calculator.net/mean-median-mode-range-calculator.html
    EXPECT_FLOAT_EQ(this->hist->median(), static_cast<TypeParam>(6));
}

/**
 * @brief Test case for checking the median calculation in the Histogram class for an odd number of items.
 *
 * This test adds a series of values (odd number) to the histogram and verifies that the median
 * is calculated correctly.
 *
 * @tparam TypeParam The data type used for the histogram values.
 *
 * Test steps:
 * - Add the value 5 to the histogram.
 * - Add the value 3 to the histogram.
 * - Add the value 5 to the histogram.
 * - Add the value 7 to the histogram.
 * - Add the value 7 to the histogram.
 * - Add the value 7 to the histogram.
 * - Add the value 8 to the histogram.
 * - Verify that the median of the histogram is 7.
 */
TYPED_TEST(HistogramTest, MedianOdd)
{
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(3));
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(8));
    // https://www.calculator.net/mean-median-mode-range-calculator.html
    EXPECT_FLOAT_EQ(this->hist->median(), static_cast<TypeParam>(7));
}


/**
 * @brief Test case for Gaussian probability calculation in Histogram.
 *
 * This test adds several values to the histogram and calculates the Gaussian
 * probability of a specific value given the average and variance of the histogram.
 * It ensures that the calculated probability is greater than zero.
 *
 * @tparam TypeParam The data type used in the histogram (e.g., int, float).
 */
TYPED_TEST(HistogramTest, GaussianProbability)
{
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(3));
    this->hist->add(static_cast<TypeParam>(5));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    this->hist->add(static_cast<TypeParam>(7));
    TypeParam avg = this->hist->average();
    TypeParam prob = this->hist->gaussian_probability(static_cast<TypeParam>(5), avg, this->hist->variance(avg));
    EXPECT_GT(prob, static_cast<TypeParam>(0.0));
}

/**
 * @brief Test case for an empty histogram.
 *
 * This test verifies the behavior of the histogram when it is empty.
 * It checks the following:
 * - The total count of elements in the histogram is zero.
 * - The top occurrence in the histogram is zero.
 * - The average value of the histogram is zero.
 * - The variance of the histogram with respect to zero is zero.
 * - The median value of the histogram is zero.
 *
 * @tparam TypeParam The data type used for the histogram.
 */
TYPED_TEST(HistogramTest, EmptyHistogram)
{
    EXPECT_EQ(this->hist->total_count(), 0);
    EXPECT_EQ(this->hist->top_occurence(), 0);
    EXPECT_FLOAT_EQ(this->hist->average(), static_cast<TypeParam>(0));
    EXPECT_FLOAT_EQ(this->hist->variance(static_cast<TypeParam>(0)), static_cast<TypeParam>(0));
    EXPECT_FLOAT_EQ(this->hist->median(), static_cast<TypeParam>(0));
}
