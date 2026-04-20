/**
 * @file test_detail.cpp
 * @brief Tests the `fpm::detail::find_highest_bit` helper used internally by math functions.
 * @author Mike Lankamp
 * @date May 2019
 */

#include "common.hpp"
#include "fpm/math.hpp"

/**
 * @brief Verifies find_highest_bit returns the position of the most significant set bit.
 */
TEST(detail, find_highest_bit)
{
    EXPECT_EQ(0, fpm::detail::find_highest_bit(1));
    EXPECT_EQ(1, fpm::detail::find_highest_bit(2));
    EXPECT_EQ(12, fpm::detail::find_highest_bit(0x1000));
    EXPECT_EQ(23, fpm::detail::find_highest_bit(0x00FFFFFF));
    EXPECT_EQ(23, fpm::detail::find_highest_bit(0x0087BD54));
    EXPECT_EQ(28, fpm::detail::find_highest_bit(0x10000000));
    EXPECT_EQ(31, fpm::detail::find_highest_bit(0x80000000));
}
