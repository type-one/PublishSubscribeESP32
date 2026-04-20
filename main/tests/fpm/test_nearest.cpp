/**
 * @file test_nearest.cpp
 * @brief Tests rounding functions for `fpm::fixed`: `round`, `ceil`, `floor`, `trunc`,
 *        `nearbyint`, and `rint`.
 * @author Mike Lankamp
 * @date May 2019
 */

#include "common.hpp"
#include "fpm/math.hpp"

/**
 * @brief Verifies round-half-away-from-zero behavior.
 */
TEST(nearest, round)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P( 2), round(P( 2.3)));
    EXPECT_EQ(P( 3), round(P( 2.5)));
    EXPECT_EQ(P( 3), round(P( 2.7)));
    EXPECT_EQ(P(-2), round(P(-2.3)));
    EXPECT_EQ(P(-3), round(P(-2.5)));
    EXPECT_EQ(P(-3), round(P(-2.7)));
    EXPECT_EQ(P( 0), round(P( 0)));
}

/**
 * @brief Verifies rounding toward positive infinity.
 */
TEST(nearest, ceil)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P( 1), ceil(P( 1)));
    EXPECT_EQ(P(-1), ceil(P(-1)));

    EXPECT_EQ(P( 3), ceil(P( 2.4)));
    EXPECT_EQ(P(-2), ceil(P(-2.4)));
    EXPECT_EQ(P( 0), ceil(P( 0)));
}

/**
 * @brief Verifies rounding toward negative infinity.
 */
TEST(nearest, floor)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P( 1), floor(P( 1)));
    EXPECT_EQ(P(-1), floor(P(-1)));

    EXPECT_EQ(P( 2), floor(P( 2.7)));
    EXPECT_EQ(P(-3), floor(P(-2.7)));
    EXPECT_EQ(P( 0), floor(P( 0)));
}

/**
 * @brief Verifies truncation toward zero.
 */
TEST(nearest, trunc)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P( 1), trunc(P( 1)));
    EXPECT_EQ(P(-1), trunc(P(-1)));

    EXPECT_EQ(P( 2), trunc(P( 2.7)));
    EXPECT_EQ(P(-2), trunc(P(-2.9)));
    EXPECT_EQ(P( 0), trunc(P( 0)));
}

/**
 * @brief Verifies tie-to-even rounding via nearbyint.
 */
TEST(nearest, nearbyint)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P( 2), nearbyint(P( 2.3)));
    EXPECT_EQ(P( 2), nearbyint(P( 2.5)));
    EXPECT_EQ(P( 4), nearbyint(P( 3.5)));
    EXPECT_EQ(P(-2), nearbyint(P(-2.3)));
    EXPECT_EQ(P(-2), nearbyint(P(-2.5)));
    EXPECT_EQ(P(-4), nearbyint(P(-3.5)));
    EXPECT_EQ(P( 0), nearbyint(P( 0)));
}

/**
 * @brief Verifies rint follows the same tie-to-even rounding as nearbyint.
 */
TEST(nearest, rint)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P( 2), rint(P( 2.3)));
    EXPECT_EQ(P( 2), rint(P( 2.5)));
    EXPECT_EQ(P( 4), rint(P( 3.5)));
    EXPECT_EQ(P(-2), rint(P(-2.3)));
    EXPECT_EQ(P(-2), rint(P(-2.5)));
    EXPECT_EQ(P(-4), rint(P(-3.5)));
    EXPECT_EQ(P( 0), rint(P( 0)));
}
