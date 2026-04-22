//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

/**
 * @file test_portable_concurrency_policy.cpp
 * @brief Unit tests for the p_future_policy build-time policy selector.
 *
 * These tests validate that:
 *  1. The compile-time policy tag matches the active build configuration.
 *  2. The future_t / promise_t aliases resolve to the expected concrete types.
 *  3. make_async_default dispatches correctly using the inplace_executor.
 *  4. The full round-trip (post → wait → retrieve result) works as expected
 *     for both the v2 (result-based) and v1 (exception-based) code paths.
 *
 * Build with PORTABLE_CONCURRENCY_V2_DEFAULT defined to exercise the v2 path;
 * build without it (the default) to exercise the v1 path.
 */

#include <gtest/gtest.h>

#include <stdexcept>

#include "portable_concurrency/p_execution.hpp"
#include "portable_concurrency/p_future_policy.hpp"

// ---------------------------------------------------------------------------
// Compile-time checks
// ---------------------------------------------------------------------------

/**
 * @brief Confirms compile-time policy tag and boolean reflect selected mode.
 */
TEST(AsyncPolicyTest, CompileTimePolicyTagIsCorrect)
{
#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    static_assert(pco::uses_v2_policy, "Expected v2 policy when PORTABLE_CONCURRENCY_V2_DEFAULT is defined");
    static_assert(std::is_same_v<pco::active_async_policy, pco::async_policy_v2_tag>, "Expected v2 tag type");
#else
    static_assert(!pco::uses_v2_policy, "Expected v1 policy when PORTABLE_CONCURRENCY_V2_DEFAULT is not defined");
    static_assert(std::is_same_v<pco::active_async_policy, pco::async_policy_v1_tag>, "Expected v1 tag type");
#endif
    SUCCEED();
}

/**
 * @brief Confirms future_t<T> aliases the expected concrete future type.
 */
TEST(AsyncPolicyTest, FutureTAliasResolvesCorrectlyForInt)
{
#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    static_assert(
        std::is_same_v<pco::future_t<int>, pco::v2::future_result<int, pco::v2::result_error>>,
        "future_t<int> must alias future_result<int, result_error> in v2 mode");
#else
    static_assert(
        std::is_same_v<pco::future_t<int>, pco::future<int>>,
        "future_t<int> must alias future<int> in v1 mode");
#endif
    SUCCEED();
}

/**
 * @brief Confirms promise_t<T> aliases the expected concrete promise type.
 */
TEST(AsyncPolicyTest, PromiseTAliasResolvesCorrectlyForInt)
{
#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    static_assert(
        std::is_same_v<pco::promise_t<int>, pco::v2::promise_result<int, pco::v2::result_error>>,
        "promise_t<int> must alias promise_result<int, result_error> in v2 mode");
#else
    static_assert(
        std::is_same_v<pco::promise_t<int>, pco::promise<int>>,
        "promise_t<int> must alias promise<int> in v1 mode");
#endif
    SUCCEED();
}

/**
 * @brief Confirms shared_future_t<T> aliases the expected concrete shared type.
 */
TEST(AsyncPolicyTest, SharedFutureTAliasResolvesCorrectlyForInt)
{
#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    static_assert(
        std::is_same_v<pco::shared_future_t<int>, pco::v2::shared_result<int, pco::v2::result_error>>,
        "shared_future_t<int> must alias shared_result<int, result_error> in v2 mode");
#else
    static_assert(
        std::is_same_v<pco::shared_future_t<int>, pco::shared_future<int>>,
        "shared_future_t<int> must alias shared_future<int> in v1 mode");
#endif
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Runtime: make_async_default with inplace_executor
// ---------------------------------------------------------------------------

/**
 * @brief Verifies make_async_default returns and propagates an integer result.
 */
TEST(AsyncPolicyTest, MakeAsyncDefaultIntReturnsCorrectValue)
{
    auto fut = pco::make_async_default(pco::inplace_executor, []() { return 42; });

#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    auto result = std::move(fut)
                      .then_error([](pco::v2::result_error)
                      {
                          return -1;
                      })
                      .then_value([](int value)
                      {
                          return value;
                      })
                      .get_result();
    ASSERT_TRUE(result.has_value()) << "Expected successful result";
    EXPECT_EQ(result.value(), 42);
#else
    EXPECT_EQ(fut.get(), 42);
#endif
}

/**
 * @brief Verifies void computation completes and executes in active policy mode.
 */
TEST(AsyncPolicyTest, MakeAsyncDefaultVoidCompletesWithoutError)
{
    bool executed = false;
    auto fut = pco::make_async_default(pco::inplace_executor, [&executed]() { executed = true; });

#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    auto result = fut.get_result();
    ASSERT_TRUE(result.has_value()) << "Expected successful result for void lambda";
#else
    fut.get(); // throws on error in v1; void futures complete normally
#endif

    EXPECT_TRUE(executed);
}

/**
 * @brief Verifies argument forwarding through make_async_default.
 */
TEST(AsyncPolicyTest, MakeAsyncDefaultForwardsArgument)
{
    auto fut = pco::make_async_default(pco::inplace_executor,
        [](int x) { return x * 2; }, 21);

#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    auto result = fut.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
#else
    EXPECT_EQ(fut.get(), 42);
#endif
}

// ---------------------------------------------------------------------------
// Runtime: make_result_promise / promise_t round-trip
// ---------------------------------------------------------------------------

/**
 * @brief Verifies promise/future round-trip delivers the expected value.
 */
TEST(AsyncPolicyTest, PromiseTRoundTripDeliversValue)
{
#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    // v2: promise_result uses get_future() (not deprecated in v2 API)
    pco::promise_t<int> p;
    auto fut = p.get_future();
    p.set_value(99);

    auto result = fut.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 99);
#else
    // v1: use make_promise to avoid the deprecated promise::get_future() overload
    auto [p, fut] = pco::make_promise<int>();
    p.set_value(99);

    EXPECT_EQ(fut.get(), 99);
#endif
}

/**
 * @brief Verifies make_ready_default value factory in both modes.
 */
TEST(AsyncPolicyTest, MakeReadyDefaultValueFactoryReturnsReadyResult)
{
    auto fut = pco::make_ready_default(123);

#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    auto result = fut.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 123);
#else
    EXPECT_EQ(fut.get(), 123);
#endif
}

/**
 * @brief Verifies make_ready_default void factory in both modes.
 */
TEST(AsyncPolicyTest, MakeReadyDefaultVoidFactoryCompletes)
{
    auto fut = pco::make_ready_default();

#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    auto result = fut.get_result();
    ASSERT_TRUE(result.has_value());
#else
    EXPECT_NO_THROW(fut.get());
#endif
}

/**
 * @brief Verifies make_error_default error factory in both modes.
 */
TEST(AsyncPolicyTest, MakeErrorDefaultFactoryPropagatesFailure)
{
#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT
    auto fut = pco::make_error_default<int>(pco::v2::result_error::broken_promise);
    auto result = fut.get_result();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::v2::result_error::broken_promise);
#else
    auto fut = pco::make_error_default<int>(std::runtime_error("policy bridge error"));
    EXPECT_THROW(fut.get(), std::runtime_error);
#endif
}
