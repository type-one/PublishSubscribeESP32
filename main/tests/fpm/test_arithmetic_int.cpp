/**
 * @file test_arithmetic_int.cpp
 * @brief Tests arithmetic operators between `fpm::fixed` and integral types.
 * @author Mike Lankamp
 * @date February 2019
 */

#include "common.hpp"

/**
 * @brief Verifies addition of a fixed-point value with an integer.
 */
TEST(arithmethic_int, addition)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P(10.5), P(3.5) + 7);
}

/**
 * @brief Verifies subtraction of an integer from a fixed-point value.
 */
TEST(arithmethic_int, subtraction)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P(-3.5), P(3.5) - 7);
}

/**
 * @brief Verifies multiplication of a fixed-point value by an integer.
 */
TEST(arithmethic_int, multiplication)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P(-24.5), P(3.5) * -7);
}

/**
 * @brief Verifies division of a fixed-point value by an integer.
 */
TEST(arithmethic_int, division)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P(3.5 / 7), P(3.5) / 7);
    EXPECT_EQ(P(-3.5 / 7), P(-3.5) / 7);
    EXPECT_EQ(P(3.5 / -7), P(3.5) / -7);
    EXPECT_EQ(P(-3.5 / -7), P(-3.5) / -7);

#ifndef NDEBUG
    EXPECT_DEATH(P(1) / 0, "");
#endif
}

/**
 * @brief Verifies integer division across a range of values.
 */
TEST(arithmethic_int, division_range)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;

    // These calculation will overflow and produce
    // wrong results without the intermediate type.
    EXPECT_EQ(P(32), P(256) / 8);
}
