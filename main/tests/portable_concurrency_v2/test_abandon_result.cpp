/**
 * @file test_abandon_result.cpp
 * @brief v2 parity tests for task abandonment and cancellation semantics.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "portable_concurrency/p_execution.hpp"
#include "portable_concurrency/p_future.hpp"

namespace
{
using namespace portable_concurrency::v2;

class null_executor_t {
private:
    template <typename F>
    friend void post(null_executor_t, F &&)
    {
        // Intentionally drop the task to simulate abandoned work.
    }
};

constexpr null_executor_t null_executor{};

} // namespace

namespace portable_concurrency {

template <>
struct is_executor<null_executor_t> : std::true_type {};

} // namespace portable_concurrency

namespace {


TEST(AbandonResultAsyncTest, dropped_async_task_yields_broken_promise)
{
    auto future = async_result(null_executor, []
    {
    });

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultAsyncTest, dropped_async_task_destroys_stored_callable)
{
    auto owned = std::make_shared<int>(42);
    std::weak_ptr<int> weak = owned;

    auto future = async_result(null_executor, [held = std::exchange(owned, nullptr)]
    {
        (void)held;
    });

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
    EXPECT_TRUE(weak.expired());
}

TEST(AbandonResultContinuationTest, dropped_future_next_task_yields_broken_promise)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    auto chained = std::move(source).next(null_executor, [](int value)
    {
        return std::to_string(value);
    });

    promise.set_value(42);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultContinuationTest, dropped_future_then_result_task_yields_broken_promise)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    auto chained = std::move(source).then_result(
        null_executor,
        [](portable_concurrency::v2::expected<int, result_error> current)
        {
            if (current.has_value()) {
                return std::to_string(current.value());
            }
            return std::string{"error"};
        });

    promise.set_value(42);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultContinuationTest, dropped_shared_next_task_yields_broken_promise)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.next(null_executor, [](int value)
    {
        return std::to_string(value);
    });

    promise.set_value(42);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultContinuationTest, dropped_shared_then_result_task_yields_broken_promise)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_result(
        null_executor,
        [](const portable_concurrency::v2::expected<int, result_error> &current)
        {
            if (current.has_value()) {
                return std::to_string(current.value());
            }
            return std::string{"error"};
        });

    promise.set_value(42);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultPackagedTaskTest, destroying_uninvoked_task_yields_broken_promise)
{
    future_result<size_t> future;
    {
        packaged_task_result<size_t(size_t, const std::string &)> task(
            [](size_t left, const std::string &right)
            {
                return left + right.size();
            });
        future = task.get_future();
    }

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultPackagedTaskTest, resetting_uninvoked_task_yields_broken_promise)
{
    packaged_task_result<size_t(size_t, const std::string &)> task(
        [](size_t left, const std::string &right)
        {
            return left + right.size();
        });

    auto future = task.get_future();
    task = {};

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultPackagedTaskTest, destroying_uninvoked_task_releases_callable_capture)
{
    auto owned = std::make_shared<int>(7);
    std::weak_ptr<int> weak = owned;
    future_result<void> future;

    {
        packaged_task_result<void()> task([held = std::exchange(owned, nullptr)]
        {
            (void)held;
        });
        future = task.get_future();
    }

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
    EXPECT_TRUE(weak.expired());
}

TEST(AbandonResultPromiseTest, destroying_promise_before_set_yields_broken_promise)
{
    future_result<size_t> future;
    {
        auto pair = make_result_promise<size_t>();
        future = std::move(pair.second);
    }

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultPromiseTest, resetting_unset_promise_yields_broken_promise)
{
    auto pair = make_result_promise<void>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    promise = {};

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultContinuationTest, invalid_nested_future_from_then_result_maps_to_broken_promise)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    auto chained = std::move(source).then_result(
        [](portable_concurrency::v2::expected<int, result_error>)
        {
            return future_result<std::string>{};
        });

    promise.set_value(42);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultContinuationTest, invalid_nested_future_from_next_maps_to_broken_promise)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    auto chained = std::move(source).next([](int)
    {
        return future_result<std::string>{};
    });

    promise.set_value(42);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

TEST(AbandonResultContinuationTest, invalid_nested_future_from_shared_next_maps_to_broken_promise)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second).share();

    auto chained = source.next([](int)
    {
        return future_result<std::string>{};
    });

    promise.set_value(42);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

} // namespace
