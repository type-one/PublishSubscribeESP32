/**
 * @file test_portable_concurrency.cpp
 * @brief Unit tests for result-based portable_concurrency v2 APIs.
 */

#include <gtest/gtest.h>

#include <stdexcept>

#include "portable_concurrency/p_execution.hpp"
#include "portable_concurrency/p_future.hpp"

namespace
{
using namespace pco::v2;

/**
 * @brief Verifies a successful async_result chain using then_value.
 *
 * This checks the common happy path: async computation succeeds and
 * continuation transforms the produced value.
 */
TEST(PortableConcurrencyV2Test, AsyncResultThenValueSuccess)
{
    auto future = async_result(pco::inplace_executor,
        [](int value)
        {
            return value * 2;
        },
        21);

    auto chained = std::move(future).then_value([](int value)
    {
        return value + 1;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 43);
}

/**
 * @brief Verifies that a default-constructed future_result reports no_state.
 */
TEST(PortableConcurrencyV2Test, DefaultConstructedFutureReturnsNoState)
{
    future_result<int> future;

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::no_state);
}

/**
 * @brief Verifies broken_promise when promise_result is destroyed unsatisfied.
 */
TEST(PortableConcurrencyV2Test, PromiseDestructionWithoutValueReturnsBrokenPromise)
{
    future_result<int> future;
    {
        promise_result<int> promise;
        future = promise.get_future();
    }

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

/**
 * @brief Verifies then_value does not run on error and propagates the same error.
 */
TEST(PortableConcurrencyV2Test, ThenValuePropagatesExistingError)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(future).then_value([](int value)
    {
        return value + 1;
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
}

/**
 * @brief Verifies then_error can recover from an error and produce a value.
 */
TEST(PortableConcurrencyV2Test, ThenErrorRecoversFromError)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto recovered = std::move(future).then_error([](result_error error)
    {
        EXPECT_EQ(error, result_error::execution_failure);
        return 7;
    });

    auto result = recovered.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Demonstrates ergonomic branching with then_error + then_value.
 *
 * The test intentionally starts in an error state, recovers with then_error,
 * then continues as a normal value flow with then_value.
 */
TEST(PortableConcurrencyV2Test, ThenErrorThenValueSimplifiesBranching)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto simplified = std::move(future)
                          .then_error([](result_error)
                          {
                              return 5;
                          })
                          .then_value([](int value)
                          {
                              return value * 3;
                          });

    auto result = simplified.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 15);
}

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
/**
 * @brief Verifies thrown continuation exceptions map to continuation_failure.
 */
TEST(PortableConcurrencyV2Test, ThenResultMapsThrownExceptionToContinuationFailure)
{
    auto future = async_result(pco::inplace_executor,
        []()
        {
            return 1;
        });

    auto chained = std::move(future).then_result(
        [](pco::v2::expected<int, result_error>) -> int
        {
            throw std::runtime_error("boom");
        });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::continuation_failure);
}
#endif

} // namespace
