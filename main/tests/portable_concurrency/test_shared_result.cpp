/**
 * @file test_shared_result.cpp
 * @brief Unit tests for pco::shared_result and shared-input combinators.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "portable_concurrency/p_future.hpp"

namespace
{

/**
 * @brief Tests default constructed is invalid.
 */
TEST(SharedResultTest, default_constructed_is_invalid)
{
    pco::shared_result<int> shared;
    EXPECT_FALSE(shared.valid());
}

/**
 * @brief Tests share transfers validity from future.
 */
TEST(SharedResultTest, share_transfers_validity_from_future)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto future = std::move(promise_and_future.second);

    auto shared = std::move(future).share();

    EXPECT_FALSE(future.valid());
    EXPECT_TRUE(shared.valid());
}

/**
 * @brief Tests copy constructed from valid is valid.
 */
TEST(SharedResultTest, copy_constructed_from_valid_is_valid)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto shared1 = std::move(promise_and_future.second).share();
    auto shared2 = shared1;

    EXPECT_TRUE(shared1.valid());
    EXPECT_TRUE(shared2.valid());
}

/**
 * @brief Tests get result can be called twice.
 */
TEST(SharedResultTest, get_result_can_be_called_twice)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto shared = std::move(promise_and_future.second).share();

    promise.set_value(42);

    const auto &result1 = shared.get_result();
    const auto &result2 = shared.get_result();
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1.value(), 42);
    EXPECT_EQ(result2.value(), 42);
    EXPECT_TRUE(shared.valid());
}

/**
 * @brief Tests get result returns stable reference for unique ptr value.
 */
TEST(SharedResultTest, get_result_returns_stable_reference_for_unique_ptr_value)
{
    auto promise_and_future = pco::make_result_promise<std::unique_ptr<int>>();
    auto promise = std::move(promise_and_future.first);
    auto shared = std::move(promise_and_future.second).share();

    promise.set_value(std::make_unique<int>(7));

    const auto &result1 = shared.get_result();
    const auto &result2 = shared.get_result();
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1.value().get(), result2.value().get());
    EXPECT_EQ(*result1.value(), 7);
}

/**
 * @brief Tests broken promise is observable from multiple copies.
 */
TEST(SharedResultTest, broken_promise_is_observable_from_multiple_copies)
{
    pco::shared_result<int> shared1;
    {
        auto promise_and_future = pco::make_result_promise<int>();
        shared1 = std::move(promise_and_future.second).share();
    }
    auto shared2 = shared1;

    const auto &result1 = shared1.get_result();
    const auto &result2 = shared2.get_result();
    ASSERT_FALSE(result1.has_value());
    ASSERT_FALSE(result2.has_value());
    EXPECT_EQ(result1.error(), pco::result_error::broken_promise);
    EXPECT_EQ(result2.error(), pco::result_error::broken_promise);
}

/**
 * @brief Tests wait for times out when not ready.
 */
TEST(SharedResultTest, wait_for_times_out_when_not_ready)
{
    auto pair = pco::make_result_promise<int>();
    auto shared = std::move(pair.second).share();

    EXPECT_EQ(shared.wait_for(std::chrono::milliseconds(1)), pco::future_status::timeout);
}

/**
 * @brief Tests wait until returns ready after completion.
 */
TEST(SharedResultTest, wait_until_returns_ready_after_completion)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    std::thread producer([p = std::move(promise)]() mutable
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        p.set_value(33);
    });

    EXPECT_EQ(shared.wait_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(100)),
              pco::future_status::ready);
    EXPECT_EQ(shared.get_result().value(), 33);

    producer.join();
}

/**
 * @brief Tests wait for invalid shared returns timeout.
 */
TEST(SharedResultTest, wait_for_invalid_shared_returns_timeout)
{
    pco::shared_result<int> shared;

    EXPECT_EQ(shared.wait_for(std::chrono::milliseconds(0)), pco::future_status::timeout);
}

/**
 * @brief Tests detach from pending invalidates source and keeps result path.
 */
TEST(SharedResultTest, detach_from_pending_invalidates_source_and_keeps_result_path)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto detached = shared.detach();
    EXPECT_FALSE(shared.valid());
    EXPECT_TRUE(detached.valid());

    promise.set_value(42);

    const auto &result = detached.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

/**
 * @brief Tests detach from ready preserves ready result.
 */
TEST(SharedResultTest, detach_from_ready_preserves_ready_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    promise.set_value(77);

    auto detached = shared.detach();
    EXPECT_FALSE(shared.valid());
    EXPECT_TRUE(detached.valid());

    const auto &result = detached.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 77);
}

/**
 * @brief Tests detached handle does not cancel completion.
 */
TEST(SharedResultTest, detached_handle_does_not_cancel_completion)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto detached = shared.detach();
    EXPECT_FALSE(shared.valid());

    promise.set_value(5);

    const auto &result = detached.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 5);
}

/**
 * @brief Tests move then detach keeps valid destination.
 */
TEST(SharedResultTest, move_then_detach_keeps_valid_destination)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    pco::shared_result<int> moved = std::move(shared);
    EXPECT_FALSE(shared.valid());
    EXPECT_TRUE(moved.valid());

    auto detached = moved.detach();
    EXPECT_FALSE(moved.valid());
    EXPECT_TRUE(detached.valid());

    promise.set_value(9);
    const auto &result = detached.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 9);
}

/**
 * @brief Tests when any accepts shared results.
 */
TEST(SharedResultTest, when_any_accepts_shared_results)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto shared1 = std::move(pair1.second).share();

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto shared2 = std::move(pair2.second).share();

    auto combined = pco::when_any(shared1, shared2);

    promise2.set_value(9);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().index, 1U);
    EXPECT_EQ(std::get<1>(result.value().futures).get_result().value(), 9);

    promise1.set_value(4);
    EXPECT_EQ(shared1.get_result().value(), 4);
}

/**
 * @brief Tests when all accepts shared results.
 */
TEST(SharedResultTest, when_all_accepts_shared_results)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto shared1 = std::move(pair1.second).share();

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto shared2 = std::move(pair2.second).share();

    auto combined = pco::when_all(shared1, shared2);

    promise1.set_value(1);
    promise2.set_value(2);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<0>(result.value()).get_result().value(), 1);
    EXPECT_EQ(std::get<1>(result.value()).get_result().value(), 2);
    EXPECT_EQ(shared1.get_result().value(), 1);
    EXPECT_EQ(shared2.get_result().value(), 2);
}

/**
 * @brief Tests when all accepts mixed future and shared.
 */
TEST(SharedResultTest, when_all_accepts_mixed_future_and_shared)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto shared2 = std::move(pair2.second).share();

    auto combined = pco::when_all(std::move(future1), shared2);

    promise2.set_value(20);
    promise1.set_value(10);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto mixed = std::move(result).value();
    auto future_result_value = std::get<0>(mixed).get_result();
    ASSERT_TRUE(future_result_value.has_value());
    EXPECT_EQ(future_result_value.value(), 10);

    const auto &shared_result_value = std::get<1>(mixed).get_result();
    ASSERT_TRUE(shared_result_value.has_value());
    EXPECT_EQ(shared_result_value.value(), 20);
}

/**
 * @brief Tests when all vector accepts shared results.
 */
TEST(SharedResultTest, when_all_vector_accepts_shared_results)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto shared1 = std::move(pair1.second).share();

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto shared2 = std::move(pair2.second).share();

    std::vector<pco::shared_result<int>> shareds;
    shareds.push_back(shared1);
    shareds.push_back(shared2);

    auto combined = pco::when_all(std::move(shareds));

    promise2.set_value(20);
    promise1.set_value(10);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2U);
    EXPECT_EQ(result.value()[0].get_result().value(), 10);
    EXPECT_EQ(result.value()[1].get_result().value(), 20);
}

/**
 * @brief Tests when any vector accepts shared results and reuses storage.
 */
TEST(SharedResultTest, when_any_vector_accepts_shared_results_and_reuses_storage)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto shared1 = std::move(pair1.second).share();

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto shared2 = std::move(pair2.second).share();

    std::vector<pco::shared_result<int>> shareds;
    shareds.reserve(2);
    shareds.push_back(shared1);
    shareds.push_back(shared2);
    const auto *buffer = shareds.data();

    auto combined = pco::when_any(std::move(shareds));

    promise1.set_value(33);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().index, 0U);
    EXPECT_EQ(result.value().futures.data(), buffer);

    promise2.set_value(44);
    EXPECT_EQ(shared2.get_result().value(), 44);
}

/**
 * @brief Tests then value transforms success.
 */
TEST(SharedResultTest, then_value_transforms_success)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_value([](const int &value)
    {
        return value * 3;
    });

    promise.set_value(7);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 21);
}

/**
 * @brief Tests then value propagates error.
 */
TEST(SharedResultTest, then_value_propagates_error)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_value([](const int &value)
    {
        return value * 2;
    });

    promise.set_error(pco::result_error::execution_failure);

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::execution_failure);
}

/**
 * @brief Tests then error recovers error.
 */
TEST(SharedResultTest, then_error_recovers_error)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_error([](pco::result_error)
    {
        return 123;
    });

    promise.set_error(pco::result_error::execution_failure);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 123);
}

/**
 * @brief Tests then result receives full pco::expected.
 */
TEST(SharedResultTest, then_result_receives_full_expected)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_result([](const pco::shared_result<int>::result_type &res)
    {
        return res.has_value() ? res.value() + 1 : -1;
    });

    promise.set_value(4);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 5);
}

/**
 * @brief Tests then value executor dispatches.
 */
TEST(SharedResultTest, then_value_executor_dispatches)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_value(pco::inplace_executor, [](const int &value)
    {
        return value + 8;
    });

    promise.set_value(2);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 10);
}

/**
 * @brief Tests then result executor dispatches.
 */
TEST(SharedResultTest, then_result_executor_dispatches)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_result(pco::inplace_executor,
                                      [](const pco::shared_result<int>::result_type &res)
    {
        return res.has_value() ? res.value() * 2 : 0;
    });

    promise.set_value(6);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 12);
}

/**
 * @brief Tests then value supports move only value by const reference.
 */
TEST(SharedResultTest, then_value_supports_move_only_value_by_const_reference)
{
    auto pair = pco::make_result_promise<std::unique_ptr<int>>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_value([](const std::unique_ptr<int> &ptr)
    {
        return *ptr + 5;
    });

    promise.set_value(std::make_unique<int>(10));

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 15);
}

/**
 * @brief Tests then value unwraps nested future result.
 */
TEST(SharedResultTest, then_value_unwraps_nested_future_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_value([](const int &value)
    {
        auto nested_pair = pco::make_result_promise<int>();
        nested_pair.first.set_value(value * 5);
        return std::move(nested_pair.second);
    });

    promise.set_value(3);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 15);
}

/**
 * @brief Tests then result unwraps nested shared result.
 */
TEST(SharedResultTest, then_result_unwraps_nested_shared_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_result([](const pco::shared_result<int>::result_type &res)
    {
        auto nested_pair = pco::make_result_promise<int>();
        nested_pair.first.set_value(res.value() + 11);
        return std::move(nested_pair.second).share();
    });

    promise.set_value(4);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 15);
}

/**
 * @brief Tests void then value transforms success.
 */
TEST(SharedResultTest, void_then_value_transforms_success)
{
    auto pair = pco::make_result_promise<void>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_value([]()
    {
        return 77;
    });

    promise.set_value();

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 77);
}

/**
 * @brief Tests void then error recovers error.
 */
TEST(SharedResultTest, void_then_error_recovers_error)
{
    auto pair = pco::make_result_promise<void>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    bool invoked = false;
    auto chained = shared.then_error([&](pco::result_error)
    {
        invoked = true;
    });

    promise.set_error(pco::result_error::execution_failure);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(invoked);
}

/**
 * @brief Tests void then result executor dispatches.
 */
TEST(SharedResultTest, void_then_result_executor_dispatches)
{
    auto pair = pco::make_result_promise<void>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.then_result(pco::inplace_executor,
                                      [](const pco::shared_result<void>::result_type &res)
    {
        return res.has_value() ? 1 : 0;
    });

    promise.set_value();

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1);
}

/**
 * @brief Tests notify runs when shared becomes ready.
 */
TEST(SharedResultTest, notify_runs_when_shared_becomes_ready)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    std::atomic<int> notifications{0};
    shared.notify([&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_value(1);
    shared.wait();
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify runs immediately if already ready.
 */
TEST(SharedResultTest, notify_runs_immediately_if_already_ready)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();
    promise.set_value(1);

    std::atomic<int> notifications{0};
    shared.notify([&]()
    {
        notifications.fetch_add(1);
    });

    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify executor dispatches callback.
 */
TEST(SharedResultTest, notify_executor_dispatches_callback)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    std::atomic<int> notifications{0};
    shared.notify(pco::inplace_executor, [&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_value(1);
    shared.wait();
    EXPECT_EQ(notifications.load(), 1);
}

/**
 * @brief Tests notify runs once even if multiple completions attempted.
 */
TEST(SharedResultTest, notify_runs_once_even_if_multiple_completions_attempted)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    std::atomic<int> notifications{0};
    shared.notify([&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_value(1);
    promise.set_error(pco::result_error::execution_failure);

    shared.wait();
    EXPECT_EQ(notifications.load(), 1);
}

TEST(SharedResultTest, notify_runs_when_shared_becomes_ready_with_error)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    std::atomic<int> notifications{0};
    shared.notify([&]()
    {
        notifications.fetch_add(1);
    });

    promise.set_error(pco::result_error::execution_failure);
    shared.wait();
    const auto &result = shared.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::execution_failure);
    EXPECT_EQ(notifications.load(), 1);
}

TEST(SharedResultTest, notify_runs_when_promise_is_broken)
{
    pco::shared_result<int> shared;
    {
        auto pair = pco::make_result_promise<int>();
        shared = std::move(pair.second).share();
    }

    std::atomic<int> notifications{0};
    shared.notify([&]()
    {
        notifications.fetch_add(1);
    });

    shared.wait();
    const auto &result = shared.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::broken_promise);
    EXPECT_EQ(notifications.load(), 1);
}

TEST(SharedResultTest, notify_not_called_if_shared_destroyed_before_completion)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    std::atomic<int> notifications{0};
    shared.notify([&]()
    {
        notifications.fetch_add(1);
    });

    shared = {};
    promise.set_value(77);

    EXPECT_EQ(notifications.load(), 0);
}

TEST(SharedResultTest, notify_runs_for_async_completion)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    std::atomic<int> notifications{0};
    shared.notify([&]()
    {
        notifications.fetch_add(1);
    });

    std::thread producer([p = std::move(promise)]() mutable
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        p.set_value(5);
    });

    shared.wait();
    producer.join();
    EXPECT_EQ(notifications.load(), 1);
}

// ---------------------------------------------------------------------------
// P1.3 — Shared continuation multi-subscriber parity
// Target v1: test_shared_future_then.cpp :: all_of_multiple_continuations_are_invoked
// ---------------------------------------------------------------------------

TEST(SharedResultTest, all_then_value_continuations_are_invoked)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    bool executed1 = false, executed2 = false;
    auto cnt1 = shared.then_value([&executed1](const int &) { executed1 = true; return 0; });
    auto cnt2 = shared.then_value([&executed2](const int &) { executed2 = true; return 0; });

    promise.set_value(123);

    EXPECT_TRUE(executed1);
    EXPECT_TRUE(executed2);
}

TEST(SharedResultTest, all_continuations_observe_consistent_value)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto cnt1 = shared.then_value([](const int &v) { return v; });
    auto cnt2 = shared.then_value([](const int &v) { return v * 2; });

    promise.set_value(7);

    auto r1 = cnt1.get_result();
    auto r2 = cnt2.get_result();
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1.value(), 7);
    EXPECT_EQ(r2.value(), 14);
}

TEST(SharedResultTest, all_continuations_invoked_on_broken_promise)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto cnt1 = shared.then_value([](const int &v) { return v; });
    auto cnt2 = shared.then_value([](const int &v) { return v; });

    promise = {}; // abandon — triggers broken_promise

    auto r1 = cnt1.get_result();
    auto r2 = cnt2.get_result();
    ASSERT_FALSE(r1.has_value());
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r1.error(), pco::result_error::broken_promise);
    EXPECT_EQ(r2.error(), pco::result_error::broken_promise);
}

TEST(SharedResultTest, continuation_attached_after_readiness_fires_inline)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    promise.set_value(42); // ready before any continuation is attached

    bool executed = false;
    auto cnt = shared.then_value([&executed](const int &v) { executed = true; return v; });

    EXPECT_TRUE(executed);
    auto r = cnt.get_result();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 42);
}

TEST(SharedResultTest, multiple_notify_callbacks_all_invoked)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    bool notified1 = false, notified2 = false, notified3 = false;
    shared.notify([&]() { notified1 = true; });
    shared.notify([&]() { notified2 = true; });
    shared.notify([&]() { notified3 = true; });

    promise.set_value(1);

    EXPECT_TRUE(notified1);
    EXPECT_TRUE(notified2);
    EXPECT_TRUE(notified3);
}

} // namespace
