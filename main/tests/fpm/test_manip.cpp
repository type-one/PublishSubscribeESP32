/**
 * @file test_manip.cpp
 * @brief Tests number manipulation functions for `fpm::fixed`: `copysign`, `nextafter`,
 *        `nexttoward`, and `modf`.
 * @author Mike Lankamp
 * @date May 2019
 */

#include "common.hpp"
#include "fpm/math.hpp"

/**
 * @brief Verifies copysign transfers sign from one value to another.
 */
TEST(manipulation, copysign)
{
    using P = fpm::fixed_24_8;

    EXPECT_EQ(P(-13.125), copysign(P(-13.125), P(-7.25)));
    EXPECT_EQ(P(-13.125), copysign(P( 13.125), P(-7.25)));
    EXPECT_EQ(P( 13.125), copysign(P(-13.125), P( 7.25)));
    EXPECT_EQ(P( 13.125), copysign(P( 13.125), P( 7.25)));

    EXPECT_EQ(P(-13), copysign(P(-13), P(-7)));
    EXPECT_EQ(P(-13), copysign(P( 13), P(-7)));
    EXPECT_EQ(P( 13), copysign(P(-13), P( 7)));
    EXPECT_EQ(P( 13), copysign(P( 13), P( 7)));
}

/**
 * @brief Verifies nextafter returns the adjacent representable value toward a target.
 */
TEST(manipulation, nextafter)
{
    using P = fpm::fixed_16_16;

    EXPECT_EQ(P(2.5), nextafter(P(2.5), P(2.5)));
    EXPECT_EQ(P(-2.5), nextafter(P(-2.5), P(-2.5)));

    EXPECT_EQ(P::from_raw_value(1), nextafter(P(0), std::numeric_limits<P>::max()));
    EXPECT_EQ(P::from_raw_value(0x10001), nextafter(P(1), P(10)));
    EXPECT_EQ(P::from_raw_value(-0x0ffff), nextafter(P(-1), P(10)));

    EXPECT_EQ(P::from_raw_value(-1), nextafter(P(0), std::numeric_limits<P>::min()));
    EXPECT_EQ(P::from_raw_value(0x0ffff), nextafter(P(1), P(-10)));
    EXPECT_EQ(P::from_raw_value(-0x10001), nextafter(P(-1), P(-10)));
}

/**
 * @brief Verifies nexttoward behaves identically to nextafter for fixed-point.
 */
TEST(manipulation, nexttoward)
{
    using P = fpm::fixed_16_16;

    EXPECT_EQ(P(2.5), nexttoward(P(2.5), P(2.5)));
    EXPECT_EQ(P(-2.5), nexttoward(P(-2.5), P(-2.5)));

    EXPECT_EQ(P::from_raw_value(1), nexttoward(P(0), std::numeric_limits<P>::max()));
    EXPECT_EQ(P::from_raw_value(0x10001), nexttoward(P(1), P(10)));
    EXPECT_EQ(P::from_raw_value(-0x0ffff), nexttoward(P(-1), P(10)));

    EXPECT_EQ(P::from_raw_value(-1), nexttoward(P(0), std::numeric_limits<P>::min()));
    EXPECT_EQ(P::from_raw_value(0x0ffff), nexttoward(P(1), P(-10)));
    EXPECT_EQ(P::from_raw_value(-0x10001), nexttoward(P(-1), P(-10)));
}

/**
 * @brief Verifies modf splits a value into integral and fractional parts.
 */
TEST(manipulation, modf)
{
    using P = fpm::fixed_16_16;

    P integral;
    EXPECT_EQ(P(0), modf(P(0), &integral));
    EXPECT_EQ(P(0), integral);

    EXPECT_EQ(P(0.25), modf(P(12.25), &integral));
    EXPECT_EQ(P(12), integral);

    EXPECT_EQ(P(-0.25), modf(P(-12.25), &integral));
    EXPECT_EQ(P(-12), integral);
}
