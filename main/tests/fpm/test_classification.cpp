/**
 * @file test_classification.cpp
 * @brief Tests numeric classification functions for `fpm::fixed`: `fpclassify`, `isfinite`,
 *        `isinf`, `isnan`, `isnormal`, `signbit`, and ordered-comparison predicates.
 * @author Mike Lankamp
 * @date April 2019
 */

#include "common.hpp"
#include "fpm/math.hpp"

/**
 * @brief Verifies fpclassify returns FP_NORMAL for non-zero and FP_ZERO for zero.
 */
TEST(classification, fpclassify)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_EQ(FP_NORMAL, fpclassify(P(1.0)));
    EXPECT_EQ(FP_NORMAL, fpclassify(P(-1.0)));
    EXPECT_EQ(FP_NORMAL, fpclassify(P(0.5)));
    EXPECT_EQ(FP_ZERO, fpclassify(P(0)));
}

/**
 * @brief Verifies fixed-point values are always finite.
 */
TEST(classification, isfinite)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_TRUE(isfinite(P(1.0)));
    EXPECT_TRUE(isfinite(P(-1.0)));
    EXPECT_TRUE(isfinite(P(0.5)));
    EXPECT_TRUE(isfinite(P(0)));
}

/**
 * @brief Verifies fixed-point values never report infinity.
 */
TEST(classification, isinf)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_FALSE(isinf(P(1.0)));
    EXPECT_FALSE(isinf(P(-1.0)));
    EXPECT_FALSE(isinf(P(0.5)));
    EXPECT_FALSE(isinf(P(0)));
}

/**
 * @brief Verifies fixed-point values never report NaN.
 */
TEST(classification, isnan)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_FALSE(isnan(P(1.0)));
    EXPECT_FALSE(isnan(P(-1.0)));
    EXPECT_FALSE(isnan(P(0.5)));
    EXPECT_FALSE(isnan(P(0)));
}

/**
 * @brief Verifies zero is not normal and non-zero values are normal.
 */
TEST(classification, isnormal)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_TRUE(isnormal(P(1.0)));
    EXPECT_TRUE(isnormal(P(-1.0)));
    EXPECT_TRUE(isnormal(P(0.5)));
    EXPECT_FALSE(isnormal(P(0)));
}

/**
 * @brief Verifies sign-bit detection for positive, negative, and zero values.
 */
TEST(classification, signbit)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_FALSE(signbit(P(1.0)));
    EXPECT_TRUE(signbit(P(-1.0)));
    EXPECT_FALSE(signbit(P(0.5)));
    EXPECT_FALSE(signbit(P(0)));
}

/**
 * @brief Verifies isgreater comparisons across positive, negative, and fractional values.
 */
TEST(classification, isgreater)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_FALSE(isgreater(P(1.0), P(2.0)));
    EXPECT_FALSE(isgreater(P(1.0), P(1.0)));
    EXPECT_TRUE(isgreater(P(2.0), P(1.0)));
    EXPECT_FALSE(isgreater(P(-2.0), P(-1.0)));
    EXPECT_FALSE(isgreater(P(-2.0), P(-2.0)));
    EXPECT_TRUE(isgreater(P(-1.0), P(-2.0)));
    EXPECT_FALSE(isgreater(P(0.25), P(0.5)));
    EXPECT_FALSE(isgreater(P(0.25), P(0.25)));
    EXPECT_TRUE(isgreater(P(0.5), P(0.25)));
    EXPECT_FALSE(isgreater(P(-1), P(0)));
    EXPECT_FALSE(isgreater(P(-1), P(-1)));
    EXPECT_TRUE(isgreater(P(0), P(-1)));
    EXPECT_FALSE(isgreater(P(0), P(0)));
}

/**
 * @brief Verifies isgreaterequal comparisons.
 */
TEST(classification, isgreaterequal)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_FALSE(isgreaterequal(P(1.0), P(2.0)));
    EXPECT_TRUE(isgreaterequal(P(1.0), P(1.0)));
    EXPECT_TRUE(isgreaterequal(P(2.0), P(1.0)));
    EXPECT_FALSE(isgreaterequal(P(-2.0), P(-1.0)));
    EXPECT_TRUE(isgreaterequal(P(-2.0), P(-2.0)));
    EXPECT_TRUE(isgreaterequal(P(-1.0), P(-2.0)));
    EXPECT_FALSE(isgreaterequal(P(0.25), P(0.5)));
    EXPECT_TRUE(isgreaterequal(P(0.25), P(0.25)));
    EXPECT_TRUE(isgreaterequal(P(0.5), P(0.25)));
    EXPECT_FALSE(isgreaterequal(P(-1), P(0)));
    EXPECT_TRUE(isgreaterequal(P(-1), P(-1)));
    EXPECT_TRUE(isgreaterequal(P(0), P(-1)));
    EXPECT_TRUE(isgreaterequal(P(0), P(0)));
}

/**
 * @brief Verifies isless comparisons.
 */
TEST(classification, isless)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_TRUE(isless(P(1.0), P(2.0)));
    EXPECT_FALSE(isless(P(1.0), P(1.0)));
    EXPECT_FALSE(isless(P(2.0), P(1.0)));
    EXPECT_TRUE(isless(P(-2.0), P(-1.0)));
    EXPECT_FALSE(isless(P(-2.0), P(-2.0)));
    EXPECT_FALSE(isless(P(-1.0), P(-2.0)));
    EXPECT_TRUE(isless(P(0.25), P(0.5)));
    EXPECT_FALSE(isless(P(0.25), P(0.25)));
    EXPECT_FALSE(isless(P(0.5), P(0.25)));
    EXPECT_TRUE(isless(P(-1), P(0)));
    EXPECT_FALSE(isless(P(-1), P(-1)));
    EXPECT_FALSE(isless(P(0), P(-1)));
    EXPECT_FALSE(isless(P(0), P(0)));
}

/**
 * @brief Verifies islessequal comparisons.
 */
TEST(classification, islessequal)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_TRUE(islessequal(P(1.0), P(2.0)));
    EXPECT_TRUE(islessequal(P(1.0), P(1.0)));
    EXPECT_FALSE(islessequal(P(2.0), P(1.0)));
    EXPECT_TRUE(islessequal(P(-2.0), P(-1.0)));
    EXPECT_TRUE(islessequal(P(-2.0), P(-2.0)));
    EXPECT_FALSE(islessequal(P(-1.0), P(-2.0)));
    EXPECT_TRUE(islessequal(P(0.25), P(0.5)));
    EXPECT_TRUE(islessequal(P(0.25), P(0.25)));
    EXPECT_FALSE(islessequal(P(0.5), P(0.25)));
    EXPECT_TRUE(islessequal(P(-1), P(0)));
    EXPECT_TRUE(islessequal(P(-1), P(-1)));
    EXPECT_FALSE(islessequal(P(0), P(-1)));
    EXPECT_TRUE(islessequal(P(0), P(0)));
}

/**
 * @brief Verifies ordered-and-unequal comparisons via islessgreater.
 */
TEST(classification, islessgreater)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_TRUE(islessgreater(P(1.0), P(2.0)));
    EXPECT_FALSE(islessgreater(P(1.0), P(1.0)));
    EXPECT_TRUE(islessgreater(P(2.0), P(1.0)));
    EXPECT_TRUE(islessgreater(P(-2.0), P(-1.0)));
    EXPECT_FALSE(islessgreater(P(-2.0), P(-2.0)));
    EXPECT_TRUE(islessgreater(P(-1.0), P(-2.0)));
    EXPECT_TRUE(islessgreater(P(0.25), P(0.5)));
    EXPECT_FALSE(islessgreater(P(0.25), P(0.25)));
    EXPECT_TRUE(islessgreater(P(0.5), P(0.25)));
    EXPECT_TRUE(islessgreater(P(-1), P(0)));
    EXPECT_FALSE(islessgreater(P(-1), P(-1)));
    EXPECT_TRUE(islessgreater(P(0), P(-1)));
    EXPECT_FALSE(islessgreater(P(0), P(0)));
}

/**
 * @brief Verifies fixed-point comparisons are never unordered.
 */
TEST(classification, isunordered)
{
    using P = fpm::fixed<std::int32_t, std::int64_t, 12>;
    EXPECT_FALSE(isunordered(P(1.0), P(2.0)));
    EXPECT_FALSE(isunordered(P(1.0), P(1.0)));
    EXPECT_FALSE(isunordered(P(2.0), P(1.0)));
    EXPECT_FALSE(isunordered(P(-2.0), P(-1.0)));
    EXPECT_FALSE(isunordered(P(-2.0), P(-2.0)));
    EXPECT_FALSE(isunordered(P(-1.0), P(-2.0)));
    EXPECT_FALSE(isunordered(P(0.25), P(0.5)));
    EXPECT_FALSE(isunordered(P(0.25), P(0.25)));
    EXPECT_FALSE(isunordered(P(0.5), P(0.25)));
    EXPECT_FALSE(isunordered(P(-1), P(0)));
    EXPECT_FALSE(isunordered(P(-1), P(-1)));
    EXPECT_FALSE(isunordered(P(0), P(-1)));
    EXPECT_FALSE(isunordered(P(0), P(0)));
}
