/**
 * @file test_continuation_result.cpp
 * @brief Second-batch unit tests for v2 continuation semantics.
 */

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "portable_concurrency/p_functional.hpp"
#include "portable_concurrency/p_future_v2.hpp"

namespace
{
using namespace portable_concurrency::v2;
struct queued_executor {
    std::vector<portable_concurrency::unique_function<void()>> tasks;

    void run_all()
    {
        auto pending = std::move(tasks);
        tasks.clear();
        for (auto &task : pending)
        {
            task();
        }
    }
};

template <typename Task>
void post(queued_executor &exec, Task &&task)
{
    exec.tasks.emplace_back(std::forward<Task>(task));
}
} // namespace

namespace portable_concurrency
{
template <>
struct is_executor<queued_executor> : std::true_type {};
}

namespace
{

/**
 * @brief Verifies then_value executes on success and transforms the value.
 */
TEST(ContinuationResultTest, then_value_runs_on_success)
{
    auto promise_and_future = make_result_promise<int>();
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
 * @brief Tests then value executor skips when downstream destroyed.
 */
TEST(ContinuationResultTest, then_value_executor_skips_when_downstream_destroyed)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    queued_executor exec;
    bool called = false;

    promise.set_value(4);

    auto chained = std::move(source).then_value(exec, [&called](int value)
    {
        called = true;
        return value + 1;
    });

    chained = future_result<int>{};
    exec.run_all();

    EXPECT_FALSE(called);
}

/**
 * @brief Tests then error executor skips when downstream destroyed.
 */
TEST(ContinuationResultTest, then_error_executor_skips_when_downstream_destroyed)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    queued_executor exec;
    bool called = false;

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source).then_error(exec, [&called](result_error)
    {
        called = true;
        return 7;
    });

    chained = future_result<int>{};
    exec.run_all();

    EXPECT_FALSE(called);
}

/**
 * @brief Tests then result executor skips when downstream destroyed.
 */
TEST(ContinuationResultTest, then_result_executor_skips_when_downstream_destroyed)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    queued_executor exec;
    bool called = false;

    promise.set_value(9);

    auto chained = std::move(source).then_result(exec,
        [&called](portable_concurrency::v2::expected<int, result_error> result)
        {
            called = true;
            return result.value() * 2;
        });

    chained = future_result<int>{};
    exec.run_all();

    EXPECT_FALSE(called);
}

/**
 * @brief Tests then value skips when downstream destroyed.
 */
TEST(ContinuationResultTest, then_value_skips_when_downstream_destroyed)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    bool called = false;

    auto chained = std::move(source).then_value([&called](int value)
    {
        called = true;
        return value + 1;
    });

    chained = future_result<int>{};
    promise.set_value(4);

    EXPECT_FALSE(called);
}

/**
 * @brief Tests then error skips when downstream destroyed.
 */
TEST(ContinuationResultTest, then_error_skips_when_downstream_destroyed)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    bool called = false;

    auto chained = std::move(source).then_error([&called](result_error)
    {
        called = true;
        return 7;
    });

    chained = future_result<int>{};
    promise.set_error(result_error::execution_failure);

    EXPECT_FALSE(called);
}

/**
 * @brief Tests then result skips when downstream destroyed.
 */
TEST(ContinuationResultTest, then_result_skips_when_downstream_destroyed)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    bool called = false;

    auto chained = std::move(source).then_result(
        [&called](portable_concurrency::v2::expected<int, result_error> result)
        {
            called = true;
            return result.has_value() ? result.value() * 2 : -1;
        });

    chained = future_result<int>{};
    promise.set_value(9);

    EXPECT_FALSE(called);
}

/**
 * @brief Verifies then_value is skipped when the source already carries an error.
 */
TEST(ContinuationResultTest, then_value_skips_on_error)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);
    bool called = false;

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source).then_value([&called](int)
    {
        called = true;
        return 0;
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
    EXPECT_FALSE(called);
}

/**
 * @brief Verifies then_error can recover from an error into a value.
 */
TEST(ContinuationResultTest, then_error_recovers_value)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source).then_error([](result_error error)
    {
        EXPECT_EQ(error, result_error::execution_failure);
        return 7;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Verifies then_error is bypassed when the source is already successful.
 */
TEST(ContinuationResultTest, then_error_passthrough_on_success)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);
    bool called = false;

    promise.set_value(42);

    auto chained = std::move(source).then_error([&called](result_error)
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
 * @brief Verifies then_result receives successful expected values.
 */
TEST(ContinuationResultTest, then_result_receives_success_expected)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(42);

    auto chained = std::move(source).then_result(
        [](portable_concurrency::v2::expected<int, result_error> result)
        {
            EXPECT_TRUE(result.has_value());
            return result.value() + 1;
        });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 43);
}

/**
 * @brief Verifies then_result receives error expected values.
 */
TEST(ContinuationResultTest, then_result_receives_error_expected)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(result_error::broken_promise);

    auto chained = std::move(source).then_result(
        [](portable_concurrency::v2::expected<int, result_error> result)
        {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), result_error::broken_promise);
            return 3;
        });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 3);
}

/**
 * @brief Verifies continuation chains can branch from error to value flow.
 */
TEST(ContinuationResultTest, chained_error_to_value_flow)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source)
                       .then_error([](result_error)
                       {
                           return 5;
                       })
                       .then_value([](int value)
                       {
                           return value * 3;
                       });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 15);
}

/**
 * @brief Tests then value unwraps nested future result.
 */
TEST(ContinuationResultTest, then_value_unwraps_nested_future_result)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(5);

    auto chained = std::move(source).then_value([](int value)
    {
        auto nested_pair = make_result_promise<int>();
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
TEST(ContinuationResultTest, then_value_unwraps_nested_shared_result)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(6);

    auto chained = std::move(source).then_value([](int value)
    {
        auto nested_pair = make_result_promise<int>();
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
TEST(ContinuationResultTest, then_result_unwraps_nested_future_result)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(7);

    auto chained = std::move(source).then_result([](portable_concurrency::v2::expected<int, result_error> result)
    {
        auto nested_pair = make_result_promise<int>();
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
TEST(ContinuationResultTest, then_value_executor_unwraps_nested_future_result)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(8);

    auto chained = std::move(source).then_value(
        portable_concurrency::inplace_executor,
        [](int value)
        {
            auto nested_pair = make_result_promise<int>();
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
TEST(ContinuationResultTest, then_value_unwraps_nested_error)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(5);

    auto chained = std::move(source).then_value([](int)
    {
        auto nested_pair = make_result_promise<int>();
        nested_pair.first.set_error(result_error::execution_failure);
        return std::move(nested_pair.second);
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
}

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
/**
 * @brief Verifies a throw in then_value maps to continuation_failure.
 */
TEST(ContinuationResultTest, then_value_throw_maps_to_continuation_failure)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(42);

    auto chained = std::move(source).then_value([](int) -> int
    {
        throw std::runtime_error("boom");
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::continuation_failure);
}

/**
 * @brief Verifies a throw in then_error maps to continuation_failure.
 */
TEST(ContinuationResultTest, then_error_throw_maps_to_continuation_failure)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source).then_error([](result_error) -> int
    {
        throw std::runtime_error("boom");
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::continuation_failure);
}

/**
 * @brief Verifies a throw in then_result maps to continuation_failure.
 */
TEST(ContinuationResultTest, then_result_throw_maps_to_continuation_failure)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(42);

    auto chained = std::move(source).then_result(
        [](portable_concurrency::v2::expected<int, result_error>) -> int
        {
            throw std::runtime_error("boom");
        });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::continuation_failure);
}
#endif

/**
 * @brief Verifies then_value with executor dispatches to the executor and transforms value.
 */
TEST(ContinuationResultTest, then_value_executor_dispatches_and_transforms)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(21);

    // Use inplace_executor to ensure deterministic execution
    auto chained = std::move(source).then_value(
        portable_concurrency::inplace_executor,
        [](int value)
        {
            return value * 2;
        });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Verifies executor-based then_value propagates errors without calling the continuation.
 */
TEST(ContinuationResultTest, then_value_executor_propagates_error)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);
    bool called = false;

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source).then_value(
        portable_concurrency::inplace_executor,
        [&called](int)
        {
            called = true;
            return 0;
        });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
    EXPECT_FALSE(called);
}

/**
 * @brief Verifies executor-based then_error dispatches error handler to executor.
 */
TEST(ContinuationResultTest, then_error_executor_dispatches_and_recovers)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source).then_error(
        portable_concurrency::inplace_executor,
        [](result_error error)
        {
            EXPECT_EQ(error, result_error::execution_failure);
            return 7;
        });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Verifies executor-based then_error passes through success without calling handler.
 */
TEST(ContinuationResultTest, then_error_executor_passthrough_on_success)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);
    bool called = false;

    promise.set_value(42);

    auto chained = std::move(source).then_error(
        portable_concurrency::inplace_executor,
        [&called](result_error)
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
 * @brief Verifies executor-based then_result dispatches full result handler to executor.
 */
TEST(ContinuationResultTest, then_result_executor_dispatches_on_success)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(42);

    auto chained = std::move(source).then_result(
        portable_concurrency::inplace_executor,
        [](portable_concurrency::v2::expected<int, result_error> result)
        {
            EXPECT_TRUE(result.has_value());
            return result.value() + 1;
        });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 43);
}

/**
 * @brief Verifies executor-based then_result dispatches full result handler to executor on error.
 */
TEST(ContinuationResultTest, then_result_executor_dispatches_on_error)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(result_error::broken_promise);

    auto chained = std::move(source).then_result(
        portable_concurrency::inplace_executor,
        [](portable_concurrency::v2::expected<int, result_error> result)
        {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), result_error::broken_promise);
            return 3;
        });

    auto out = chained.get_result();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out.value(), 3);
}

/**
 * @brief Verifies executor-based continuations can be chained together.
 */
TEST(ContinuationResultTest, chained_executor_continuations)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(10);

    auto chained = std::move(source)
                       .then_value(
                           portable_concurrency::inplace_executor,
                           [](int value)
                           {
                               return value + 5;
                           })
                       .then_value(
                           portable_concurrency::inplace_executor,
                           [](int value)
                           {
                               return value * 2;
                           });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 30);
}

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
/**
 * @brief Verifies executor-based then_value throw maps to continuation_failure.
 */
TEST(ContinuationResultTest, then_value_executor_throw_maps_to_continuation_failure)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(42);

    auto chained = std::move(source).then_value(
        portable_concurrency::inplace_executor,
        [](int) -> int
        {
            throw std::runtime_error("executor continuation failed");
        });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::continuation_failure);
}

/**
 * @brief Verifies executor-based then_error throw maps to continuation_failure.
 */
TEST(ContinuationResultTest, then_error_executor_throw_maps_to_continuation_failure)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto chained = std::move(source).then_error(
        portable_concurrency::inplace_executor,
        [](result_error) -> int
        {
            throw std::runtime_error("executor error handler failed");
        });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::continuation_failure);
}

/**
 * @brief Verifies executor-based then_result throw maps to continuation_failure.
 */
TEST(ContinuationResultTest, then_result_executor_throw_maps_to_continuation_failure)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    promise.set_value(42);

    auto chained = std::move(source).then_result(
        portable_concurrency::inplace_executor,
        [](portable_concurrency::v2::expected<int, result_error>) -> int
        {
            throw std::runtime_error("executor result handler failed");
        });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::continuation_failure);
}
#endif

} // namespace
