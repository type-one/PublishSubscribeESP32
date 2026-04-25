/**
 * @file test_future_result.cpp
 * @brief Imported-style unit tests for portable_concurrency v2 future_result.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "portable_concurrency/p_future.hpp"

namespace
{
using namespace pco::v2;

/**
 * @brief Verifies a default-constructed future_result is invalid.
 */
TEST(FutureResultTest, default_constructed_is_invalid)
{
    future_result<void> future;
    EXPECT_FALSE(future.valid());
}

/**
 * @brief Verifies a future obtained from a promise_result is valid.
 */
TEST(FutureResultTest, obtained_from_promise_is_valid)
{
    auto promise_and_future = make_result_promise<int>();
    auto future = std::move(promise_and_future.second);

    EXPECT_TRUE(future.valid());
}

/**
 * @brief Verifies is_ready reports false before the promise_result is fulfilled.
 */
TEST(FutureResultTest, is_ready_returns_false_on_nonready_state)
{
    auto promise_and_future = make_result_promise<int>();
    auto future = std::move(promise_and_future.second);

    EXPECT_FALSE(future.is_ready());
}

/**
 * @brief Verifies is_ready reports true after a value is stored.
 */
TEST(FutureResultTest, is_ready_returns_true_after_value_is_set)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_value(42);

    EXPECT_TRUE(future.is_ready());
}

/**
 * @brief Verifies get_result returns the stored value and invalidates the future.
 */
TEST(FutureResultTest, get_result_returns_value_and_invalidates_future)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_value(42);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
    EXPECT_FALSE(future.valid());
}

/**
 * @brief Verifies get_result on an invalid future reports no_state.
 */
TEST(FutureResultTest, get_result_on_invalid_future_reports_no_state)
{
    future_result<int> future;

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::no_state);
}

/**
 * @brief Verifies a stored error is returned unchanged.
 */
TEST(FutureResultTest, get_result_returns_stored_error)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_error(result_error::execution_failure);

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
}

/**
 * @brief Verifies void futures complete successfully.
 */
TEST(FutureResultTest, void_future_reports_success_when_value_is_set)
{
    auto promise_and_future = make_result_promise<void>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_value();

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
}

/**
 * @brief Verifies wait unblocks when promise is fulfilled asynchronously.
 */
TEST(FutureResultTest, wait_unblocks_on_async_value)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::thread producer([p = std::move(promise)]() mutable
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        p.set_value(42);
    });

    future.wait();
    EXPECT_TRUE(future.is_ready());

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);

    producer.join();
}

/**
 * @brief Verifies moved-from future becomes invalid and destination keeps state.
 */
TEST(FutureResultTest, move_construction_transfers_validity)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto source = std::move(promise_and_future.second);

    future_result<int> destination = std::move(source);

    EXPECT_FALSE(source.valid());
    EXPECT_TRUE(destination.valid());

    promise.set_value(5);
    auto result = destination.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 5);
}

/**
 * @brief Verifies wait on invalid future is harmless.
 */
TEST(FutureResultTest, wait_on_invalid_future_is_harmless)
{
    future_result<int> future;
    future.wait();
    SUCCEED();
}

/**
 * @brief Tests make ready result produces ready value.
 */
TEST(FutureResultTest, make_ready_result_produces_ready_value)
{
    auto future = make_ready_result(42);

    EXPECT_TRUE(future.valid());
    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)), pco::future_status::ready);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Tests make ready result void produces ready success.
 */
TEST(FutureResultTest, make_ready_result_void_produces_ready_success)
{
    auto future = make_ready_result<>();

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)), pco::future_status::ready);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
}

/**
 * @brief Tests make error result produces ready error.
 */
TEST(FutureResultTest, make_error_result_produces_ready_error)
{
    auto future = make_error_result<int>(result_error::execution_failure);

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)), pco::future_status::ready);

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
}

/**
 * @brief Tests wait for times out when not ready.
 */
TEST(FutureResultTest, wait_for_times_out_when_not_ready)
{
    auto promise_and_future = make_result_promise<int>();
    auto future = std::move(promise_and_future.second);

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(1)), pco::future_status::timeout);
}

/**
 * @brief Tests wait for returns ready after completion.
 */
TEST(FutureResultTest, wait_for_returns_ready_after_completion)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::thread producer([p = std::move(promise)]() mutable
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        p.set_value(42);
    });

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(100)), pco::future_status::ready);
    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);

    producer.join();
}

/**
 * @brief Tests wait until returns ready for ready future.
 */
TEST(FutureResultTest, wait_until_returns_ready_for_ready_future)
{
    auto future = make_ready_result(7);

    EXPECT_EQ(future.wait_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(1)),
              pco::future_status::ready);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Tests wait for invalid future returns timeout.
 */
TEST(FutureResultTest, wait_for_invalid_future_returns_timeout)
{
    future_result<int> future;

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)), pco::future_status::timeout);
}

/**
 * @brief Tests notify runs when future becomes ready.
 */
TEST(FutureResultTest, notify_runs_when_future_becomes_ready)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::atomic<int> notifications{0};
    future.notify([&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_value(10);
    future.wait();
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify runs immediately if already ready.
 */
TEST(FutureResultTest, notify_runs_immediately_if_already_ready)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);
    promise.set_value(10);

    std::atomic<int> notifications{0};
    future.notify([&]()
    {
        notifications.fetch_add(1);
    });

    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify executor dispatches callback.
 */
TEST(FutureResultTest, notify_executor_dispatches_callback)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::atomic<int> notifications{0};
    future.notify(pco::inplace_executor, [&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_value(10);
    future.wait();
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify runs once even if multiple completions attempted.
 */
TEST(FutureResultTest, notify_runs_once_even_if_multiple_completions_attempted)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::atomic<int> notifications{0};
    future.notify([&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_value(10);
    promise.set_error(result_error::execution_failure);

    future.wait();
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify runs when future becomes ready with error.
 */
TEST(FutureResultTest, notify_runs_when_future_becomes_ready_with_error)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::atomic<int> notifications{0};
    future.notify([&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_error(result_error::execution_failure);
    future.wait();
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify runs when the source promise is abandoned.
 */
TEST(FutureResultTest, notify_runs_when_promise_is_broken)
{
    future_result<int> future;
    {
        auto pair = make_result_promise<int>();
        future = std::move(pair.second);
    }

    std::atomic<int> notifications{0};
    future.notify([&]()
    {
        notifications.fetch_add(1);
    });

    future.wait();
    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify does not run if future is destroyed before completion.
 */
TEST(FutureResultTest, notify_not_called_if_future_destroyed_before_completion)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::atomic<int> notifications{0};
    future.notify([&]()
    {
        notifications.fetch_add(1);
    });

    future = {};
    promise.set_value(100);

    EXPECT_EQ(notifications.load(), 0);
}

/**
 * @brief Tests notify runs when completion happens asynchronously.
 */
TEST(FutureResultTest, notify_runs_for_async_completion)
{
    auto promise_and_future = make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    std::atomic<int> notifications{0};
    future.notify([&]()
    {
        notifications.fetch_add(1);
    });

    std::thread producer([p = std::move(promise)]() mutable
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        p.set_value(42);
    });

    future.wait();
    producer.join();
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests detach from pending invalidates source and keeps result path.
 */
TEST(FutureResultTest, detach_from_pending_invalidates_source_and_keeps_result_path)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    auto detached = future.detach();
    EXPECT_FALSE(future.valid());
    EXPECT_TRUE(detached.valid());

    promise.set_value(42);

    const auto &result = detached.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Tests detach from ready preserves ready result.
 */
TEST(FutureResultTest, detach_from_ready_preserves_ready_result)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    promise.set_value(77);

    auto detached = future.detach();
    EXPECT_FALSE(future.valid());
    EXPECT_TRUE(detached.valid());

    const auto &result = detached.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 77);
}

/**
 * @brief Tests detach keeps state alive after returned handle is dropped.
 */
TEST(FutureResultTest, detach_keeps_state_alive_after_returned_handle_is_dropped)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    // Drop the returned handle immediately; the keep-alive callback must
    // prevent the shared state from being destroyed so the promise can still
    // complete without crashing.
    { auto _ = future.detach(); }

    // If the state outlives the handle, setting the value must not crash.
    EXPECT_NO_FATAL_FAILURE(promise.set_value(99));
}

/**
 * @brief Tests move then detach keeps valid destination.
 */
TEST(FutureResultTest, move_then_detach_keeps_valid_destination)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    future_result<int> moved = std::move(future);
    EXPECT_FALSE(future.valid());
    EXPECT_TRUE(moved.valid());

    auto detached = moved.detach();
    EXPECT_FALSE(moved.valid());
    EXPECT_TRUE(detached.valid());

    promise.set_value(9);
    const auto &result = detached.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 9);
}

} // namespace
