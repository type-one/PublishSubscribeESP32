/**
 * @file test_continuation_result_no_exceptions.cpp
 * @brief No-exception-mode focused tests for result-based continuation semantics.
 *
 * These tests are designed to run when exceptions are disabled (-fno-exceptions).
 * They verify that continuation chains work correctly without exception handling overhead,
 * focusing on the direct execution paths that are used in embedded/no-exception builds.
 */

#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)

#include <gtest/gtest.h>

#include "portable_concurrency/future.hpp"

namespace
{

/**
 * @brief Verifies then_value executes directly without exception handling in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, then_value_direct_execution)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(21);

    auto chained = std::move(source).then_value([](int value)
    {
        return value * 2;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Verifies then_value skips and propagates error in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, then_value_error_propagation)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);
    bool called = false;

    promise.set_error(pco::result_error::execution_failure);

    auto chained = std::move(source).then_value([&called](int)
    {
        called = true;
        return 0;
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::execution_failure);
    EXPECT_FALSE(called);
}

/**
 * @brief Verifies then_error directly executes recovery in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, then_error_direct_recovery)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(pco::result_error::execution_failure);

    auto chained = std::move(source).then_error([](pco::result_error error)
    {
        EXPECT_EQ(error, pco::result_error::execution_failure);
        return 7;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Verifies then_error is bypassed on success in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, then_error_success_bypass)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);
    bool called = false;

    promise.set_value(42);

    auto chained = std::move(source).then_error([&called](pco::result_error)
    {
        called = true;
        return -1;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
    EXPECT_FALSE(called);
}

/**
 * @brief Verifies then_result directly invokes callback with value in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, then_result_direct_value_invocation)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(42);

    auto chained = std::move(source).then_result(
        [](pco::expected<int, pco::result_error> result)
        {
            EXPECT_TRUE(result.has_value());
            return result.value() + 1;
        });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 43);
}

/**
 * @brief Verifies then_result directly invokes callback with error in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, then_result_direct_error_invocation)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(pco::result_error::broken_promise);

    auto chained = std::move(source).then_result(
        [](pco::expected<int, pco::result_error> result)
        {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), pco::result_error::broken_promise);
            return 3;
        });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 3);
}

/**
 * @brief Verifies multiple chained continuations execute directly in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, multiple_chained_continuations)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(5);

    auto chained = std::move(source)
                       .then_value([](int x)
                       {
                           return x * 2;
                       })
                       .then_value([](int x)
                       {
                           return x + 3;
                       })
                       .then_value([](int x)
                       {
                           return x * 4;
                       });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    // (5 * 2 + 3) * 4 = 52
    EXPECT_EQ(result.value(), 52);
}

/**
 * @brief Verifies complex error recovery chain in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, complex_error_recovery_chain)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(pco::result_error::broken_promise);

    auto chained = std::move(source)
                       .then_error([](pco::result_error error)
                       {
                           // Recover from broken_promise to a default value
                           if (error == pco::result_error::broken_promise) {
                               return 100;
                           }
                           return 0;
                       })
                       .then_value([](int x)
                       {
                           return x / 2;
                       })
                       .then_result(
                           [](pco::expected<int, pco::result_error> result)
                           {
                               EXPECT_TRUE(result.has_value());
                               return result.value() + 10;
                           });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    // Start from broken_promise -> recover to 100 -> divide by 2 -> 50 + 10 = 60
    EXPECT_EQ(out.value(), 60);
}

/**
 * @brief Verifies no overhead from exception handling in no-exception mode.
 *
 * This test verifies that the continuation machinery is present and functional
 * without any exception-related overhead (no try-catch blocks, just direct calls).
 */
TEST(ContinuationResultNoExceptionsTest, lightweight_continuation_execution)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    int callback_count = 0;
    promise.set_value(1);

    auto chained = std::move(source).then_value([&callback_count](int x)
    {
        ++callback_count;
        return x;
    });

    EXPECT_EQ(callback_count, 1);
    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
}

/**
 * @brief Verifies void promise error recovery through then_result in no-exception mode.
 */
TEST(ContinuationResultNoExceptionsTest, void_promise_error_recovery_via_then_result)
{
    auto promise_and_future = pco::make_result_promise<void>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(pco::result_error::execution_failure);

    auto chained = std::move(source).then_result(
        [](pco::expected<void, pco::result_error> result)
        {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), pco::result_error::execution_failure);
            return 99;  // Transition from void error to int value
        });

    auto final_result = chained.get_result();
    ASSERT_TRUE(final_result.has_value());
    EXPECT_EQ(final_result.value(), 99);
}

/**
 * @brief Tests then value unwraps nested future result.
 */
TEST(ContinuationResultNoExceptionsTest, then_value_unwraps_nested_future_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(5);

    auto chained = std::move(source).then_value([](int value)
    {
        auto nested_pair = pco::make_result_promise<int>();
        nested_pair.first.set_value(value * 4);
        return std::move(nested_pair.second);
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 20);
}

/**
 * @brief Tests then value unwraps nested shared result.
 */
TEST(ContinuationResultNoExceptionsTest, then_value_unwraps_nested_shared_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(6);

    auto chained = std::move(source).then_value([](int value)
    {
        auto nested_pair = pco::make_result_promise<int>();
        nested_pair.first.set_value(value + 3);
        return std::move(nested_pair.second).share();
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 9);
}

/**
 * @brief Tests then result unwraps nested future result.
 */
TEST(ContinuationResultNoExceptionsTest, then_result_unwraps_nested_future_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(7);

    auto chained = std::move(source).then_result([](pco::expected<int, pco::result_error> result)
    {
        auto nested_pair = pco::make_result_promise<int>();
        nested_pair.first.set_value(result.value() * 2);
        return std::move(nested_pair.second);
    });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 14);
}

/**
 * @brief Tests then value executor unwraps nested future result.
 */
TEST(ContinuationResultNoExceptionsTest, then_value_executor_unwraps_nested_future_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(8);

    auto chained = std::move(source).then_value(
        pco::inplace_executor,
        [](int value)
        {
            auto nested_pair = pco::make_result_promise<int>();
            nested_pair.first.set_value(value - 1);
            return std::move(nested_pair.second);
        });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Tests then value unwraps nested error.
 */
TEST(ContinuationResultNoExceptionsTest, then_value_unwraps_nested_error)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(5);

    auto chained = std::move(source).then_value([](int)
    {
        auto nested_pair = pco::make_result_promise<int>();
        nested_pair.first.set_error(pco::result_error::execution_failure);
        return std::move(nested_pair.second);
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::execution_failure);
}

} // namespace

#else

// Placeholder for when exceptions ARE enabled: skip this test file
int placeholder_no_exceptions = 0;

#endif
