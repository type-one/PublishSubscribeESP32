/**
 * @file test_when_all_result.cpp
 * @brief Unit tests for pco::when_all combinator.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <list>
#include <thread>
#include <atomic>
#include <vector>

#include "portable_concurrency/future.hpp"

namespace
{

/**
 * @brief Tests empty when all returns ready empty tuple.
 */
TEST(WhenAllResultTest, empty_when_all_returns_ready_empty_tuple)
{
    auto combined = pco::when_all();
    EXPECT_TRUE(combined.valid());
    EXPECT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((std::tuple_size<std::decay_t<decltype(result.value())>>::value), 0U);
}

/**
 * @brief Tests collects all success results.
 */
TEST(WhenAllResultTest, collects_all_success_results)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    auto combined = pco::when_all(std::move(future1), std::move(future2));

    promise1.set_value(10);
    promise2.set_value(20);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto all = std::move(result).value();
    ASSERT_TRUE(std::get<0>(all).has_value());
    ASSERT_TRUE(std::get<1>(all).has_value());
    EXPECT_EQ(std::get<0>(all).value(), 10);
    EXPECT_EQ(std::get<1>(all).value(), 20);
}

/**
 * @brief Tests keeps individual errors in result tuple.
 */
TEST(WhenAllResultTest, keeps_individual_errors_in_result_tuple)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    auto combined = pco::when_all(std::move(future1), std::move(future2));

    promise1.set_error(pco::result_error::execution_failure);
    promise2.set_value(7);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto all = std::move(result).value();
    ASSERT_FALSE(std::get<0>(all).has_value());
    EXPECT_EQ(std::get<0>(all).error(), pco::result_error::execution_failure);
    ASSERT_TRUE(std::get<1>(all).has_value());
    EXPECT_EQ(std::get<1>(all).value(), 7);
}

/**
 * @brief Tests becomes ready only after all inputs complete.
 */
TEST(WhenAllResultTest, becomes_ready_only_after_all_inputs_complete)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    auto combined = pco::when_all(std::move(future1), std::move(future2));

    promise1.set_value(1);
    EXPECT_FALSE(combined.is_ready());

    std::thread delayed_setter([p = std::move(promise2)]() mutable
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        p.set_value(2);
    });

    combined.wait();
    EXPECT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    auto all = std::move(result).value();
    ASSERT_TRUE(std::get<0>(all).has_value());
    ASSERT_TRUE(std::get<1>(all).has_value());

    delayed_setter.join();
}

/**
 * @brief Tests invalid input returns no state error.
 */
TEST(WhenAllResultTest, invalid_input_returns_no_state_error)
{
    pco::future_result<int> invalid_future;

    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto valid_future = std::move(pair.second);

    auto combined = pco::when_all(std::move(invalid_future), std::move(valid_future));

    auto result = combined.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::no_state);

    promise.set_value(1);
}

/**
 * @brief Tests broken promise input still completes combined future.
 */
TEST(WhenAllResultTest, broken_promise_input_still_completes_combined_future)
{
    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    pco::future_result<int> future2;
    {
        auto pair2 = pco::make_result_promise<int>();
        future2 = std::move(pair2.second);
    }

    auto combined = pco::when_all(std::move(future1), std::move(future2));

    promise1.set_value(42);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto all = std::move(result).value();
    ASSERT_TRUE(std::get<0>(all).has_value());
    EXPECT_EQ(std::get<0>(all).value(), 42);
    ASSERT_FALSE(std::get<1>(all).has_value());
    EXPECT_EQ(std::get<1>(all).error(), pco::result_error::broken_promise);
}

/**
 * @brief Tests iterator overload empty range returns ready empty vector.
 */
TEST(WhenAllResultTest, iterator_overload_empty_range_returns_ready_empty_vector)
{
    std::list<pco::future_result<int>> empty;

    auto combined = pco::when_all(empty.begin(), empty.end());

    ASSERT_TRUE(combined.valid());
    ASSERT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

/**
 * @brief Tests iterator overload collects results in order.
 */
TEST(WhenAllResultTest, iterator_overload_collects_results_in_order)
{
    std::vector<pco::future_result<int>> futures;

    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    futures.push_back(std::move(pair1.second));

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    futures.push_back(std::move(pair2.second));

    auto combined = pco::when_all(futures.begin(), futures.end());

    promise2.set_error(pco::result_error::execution_failure);
    promise1.set_value(5);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2U);
    ASSERT_TRUE(result.value()[0].has_value());
    EXPECT_EQ(result.value()[0].value(), 5);
    ASSERT_FALSE(result.value()[1].has_value());
    EXPECT_EQ(result.value()[1].error(), pco::result_error::execution_failure);
}

/**
 * @brief Tests vector overload collects results.
 */
TEST(WhenAllResultTest, vector_overload_collects_results)
{
    std::vector<pco::future_result<int>> futures;

    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    futures.push_back(std::move(pair1.second));

    auto pair2 = pco::make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    futures.push_back(std::move(pair2.second));

    auto combined = pco::when_all(std::move(futures));

    promise1.set_value(11);
    promise2.set_value(22);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2U);
    ASSERT_TRUE(result.value()[0].has_value());
    ASSERT_TRUE(result.value()[1].has_value());
    EXPECT_EQ(result.value()[0].value(), 11);
    EXPECT_EQ(result.value()[1].value(), 22);
}

/**
 * @brief Tests iterator overload invalid input returns no state error.
 */
TEST(WhenAllResultTest, iterator_overload_invalid_input_returns_no_state_error)
{
    std::vector<pco::future_result<int>> futures;
    futures.emplace_back();

    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    futures.push_back(std::move(pair.second));

    auto combined = pco::when_all(futures.begin(), futures.end());
    auto result = combined.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::no_state);

    promise.set_value(1);
}

/**
 * @brief Tests vector overload invalid input returns no state error.
 */
TEST(WhenAllResultTest, vector_overload_invalid_input_returns_no_state_error)
{
    std::vector<pco::future_result<int>> futures;
    futures.emplace_back();

    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    futures.push_back(std::move(pair.second));

    auto combined = pco::when_all(std::move(futures));
    auto result = combined.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::no_state);

    promise.set_value(1);
}

/**
 * @brief Tests vector overload preserves broken promise in results.
 */
TEST(WhenAllResultTest, vector_overload_preserves_broken_promise_in_results)
{
    std::vector<pco::future_result<int>> futures;

    auto pair1 = pco::make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    futures.push_back(std::move(pair1.second));

    pco::future_result<int> broken_future;
    {
        auto pair2 = pco::make_result_promise<int>();
        broken_future = std::move(pair2.second);
    }
    futures.push_back(std::move(broken_future));

    auto combined = pco::when_all(std::move(futures));

    promise1.set_value(33);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2U);
    ASSERT_TRUE(result.value()[0].has_value());
    EXPECT_EQ(result.value()[0].value(), 33);
    ASSERT_FALSE(result.value()[1].has_value());
    EXPECT_EQ(result.value()[1].error(), pco::result_error::broken_promise);
}

// ---------------------------------------------------------------------------
// P2.1 — pco::when_all hardening parity
// ---------------------------------------------------------------------------

TEST(WhenAllResultTest, some_inputs_initially_ready_combined_waits_for_pending)
{
    auto pair0 = pco::make_result_promise<int>();
    auto pair1 = pco::make_result_promise<int>();
    // pair0 ready before pco::when_all
    pair0.first.set_value(10);

    auto combined = pco::when_all(std::move(pair0.second), std::move(pair1.second));
    EXPECT_FALSE(combined.is_ready());

    pair1.first.set_value(20);
    EXPECT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<0>(result.value()).value(), 10);
    EXPECT_EQ(std::get<1>(result.value()).value(), 20);
}

TEST(WhenAllResultTest, all_inputs_initially_ready_combined_is_immediately_ready)
{
    auto pair0 = pco::make_result_promise<int>();
    auto pair1 = pco::make_result_promise<int>();
    pair0.first.set_value(1);
    pair1.first.set_value(2);

    auto combined = pco::when_all(std::move(pair0.second), std::move(pair1.second));
    EXPECT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<0>(result.value()).value(), 1);
    EXPECT_EQ(std::get<1>(result.value()).value(), 2);
}

TEST(WhenAllResultTest, concurrent_readiness_collects_all_results)
{
    std::vector<pco::future_result<int>> futures;
    std::vector<pco::promise_result<int>> promises;
    promises.reserve(3);
    futures.reserve(3);
    for (int i = 0; i < 3; ++i) {
        auto p = pco::make_result_promise<int>();
        promises.push_back(std::move(p.first));
        futures.push_back(std::move(p.second));
    }

    auto combined = pco::when_all(std::move(futures));
    EXPECT_FALSE(combined.is_ready());

    std::atomic<bool> go{false};
    auto worker = [&](int idx, int val) {
        while (!go.load(std::memory_order_acquire)) {}
        promises[idx].set_value(val);
    };

    std::thread t0(worker, 0, 100);
    std::thread t1(worker, 1, 200);
    std::thread t2(worker, 2, 300);
    go.store(true, std::memory_order_release);
    t0.join(); t1.join(); t2.join();

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 3U);
    EXPECT_EQ(result.value()[0].value(), 100);
    EXPECT_EQ(result.value()[1].value(), 200);
    EXPECT_EQ(result.value()[2].value(), 300);
}

TEST(WhenAllResultTest, iterator_overload_preserves_input_order_in_results)
{
    std::vector<pco::future_result<int>> futures;
    std::vector<pco::promise_result<int>> promises;
    const int N = 5;
    for (int i = 0; i < N; ++i) {
        auto p = pco::make_result_promise<int>();
        promises.push_back(std::move(p.first));
        futures.push_back(std::move(p.second));
    }

    auto combined = pco::when_all(futures.begin(), futures.end());

    // Complete in reverse order
    for (int i = N - 1; i >= 0; --i)
        promises[i].set_value(i * 10);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    for (int i = 0; i < N; ++i)
        EXPECT_EQ(result.value()[i].value(), i * 10);
}

} // namespace
