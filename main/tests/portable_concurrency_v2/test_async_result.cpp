/**
 * @file test_async_result.cpp
 * @brief Imported-style unit tests for portable_concurrency v2 async_result.
 */

#include <gtest/gtest.h>

#include "portable_concurrency/p_execution.hpp"
#include "portable_concurrency/p_future_v2.hpp"
#include "portable_concurrency/p_thread_pool.hpp"

namespace
{
using namespace portable_concurrency::v2;

/**
 * @brief Verifies async_result returns a valid future_result.
 */
TEST(AsyncResultTest, returns_valid_future)
{
    auto future = async_result(portable_concurrency::inplace_executor, []
    {
        return 42;
    });

    EXPECT_TRUE(future.valid());
}

/**
 * @brief Verifies async_result delivers the function return value.
 */
TEST(AsyncResultTest, delivers_function_result)
{
    auto future = async_result(portable_concurrency::inplace_executor, []
    {
        return 42;
    });

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Verifies async_result forwards call arguments to the callable.
 */
TEST(AsyncResultTest, forwards_parameters_to_callable)
{
    auto future = async_result(portable_concurrency::inplace_executor,
        [](int left, int right)
        {
            return left + right;
        },
        20,
        22);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Verifies async_result supports void callables.
 */
TEST(AsyncResultTest, supports_void_callable)
{
    bool executed = false;
    auto future = async_result(portable_concurrency::inplace_executor, [&executed]
    {
        executed = true;
    });

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(executed);
}

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
/**
 * @brief Verifies thrown callable exceptions map to execution_failure.
 */
TEST(AsyncResultTest, thrown_callable_exception_maps_to_execution_failure)
{
    auto future = async_result(portable_concurrency::inplace_executor, []() -> int
    {
        throw 42;
    });

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
}
#endif

/**
 * @brief Verifies then_value can transform an async_result success value.
 */
TEST(AsyncResultTest, then_value_transforms_async_result_value)
{
    auto future = async_result(portable_concurrency::inplace_executor, []
    {
        return 21;
    });

    auto chained = std::move(future).then_value([](int value)
    {
        return value * 2;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Verifies void async_result can chain through then_result.
 */
TEST(AsyncResultTest, void_async_result_then_result_transitions_to_value)
{
    auto future = async_result(portable_concurrency::inplace_executor, []
    {
    });

    auto chained = std::move(future).then_result([](portable_concurrency::v2::expected<void, result_error> result)
    {
        EXPECT_TRUE(result.has_value());
        return 11;
    });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 11);
}


// ---------------------------------------------------------------------------
// P2.2 — async parity depth
// ---------------------------------------------------------------------------

TEST(AsyncResultTest, executes_callable_on_specified_executor_thread)
{
    portable_concurrency::static_thread_pool pool{1};
    auto exec = pool.executor();

    auto future = async_result(exec, [] {
        return std::this_thread::get_id();
    });

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), std::this_thread::get_id());
}

TEST(AsyncResultTest, unwraps_nested_future_result)
{

    portable_concurrency::static_thread_pool pool{1};
    auto exec = pool.executor();

    auto future = async_result(exec, [exec] {
        return async_result(exec, [] { return 100500; });
    });

    // result type must be future_result<int>, not future_result<future_result<int>>
    static_assert(
        std::is_same<decltype(future),
                     future_result<int>>::value,
        "async_result must unwrap nested future_result");

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 100500);
}

TEST(AsyncResultTest, unwraps_nested_shared_result)
{
    portable_concurrency::static_thread_pool pool{1};
    auto exec = pool.executor();

    auto future = async_result(exec, [exec] {
        return async_result(exec, [] { return 42; }).share();
    });

    static_assert(
        std::is_same<decltype(future),
                     future_result<int>>::value,
        "async_result must unwrap nested shared_result");

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(AsyncResultTest, destroys_callable_after_invocation)
{
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;

    portable_concurrency::static_thread_pool pool{1};
    auto exec = pool.executor();

    auto future = async_result(exec, [sp = std::exchange(sp, nullptr)] {
        return *sp;
    });

    auto result = future.get_result();   // blocks until callable has run
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
    EXPECT_TRUE(wp.expired());           // callable must have been released
}

} // namespace
