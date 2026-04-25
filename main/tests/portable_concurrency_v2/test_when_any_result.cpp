/**
 * @file test_when_any_result.cpp
 * @brief Unit tests for v2 when_any tuple combinator.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <list>
#include <thread>
#include <atomic>
#include <vector>

#include "portable_concurrency/p_future.hpp"

namespace
{
using namespace pco::v2;

/**
 * @brief Tests empty when any returns ready with npos index.
 */
TEST(WhenAnyResultTest, empty_when_any_returns_ready_with_npos_index)
{
    auto combined = when_any();
    EXPECT_TRUE(combined.valid());
    EXPECT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto any = std::move(result).value();
    EXPECT_EQ(any.index, static_cast<std::size_t>(-1));
    EXPECT_EQ(std::tuple_size<decltype(any.futures)>::value, 0U);
}

/**
 * @brief Tests returns index of first ready future.
 */
TEST(WhenAnyResultTest, returns_index_of_first_ready_future)
{
    auto pair1 = make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    auto pair2 = make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    auto combined = when_any(std::move(future1), std::move(future2));

    promise1.set_value(111);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto any = std::move(result).value();
    EXPECT_EQ(any.index, 0U);

    auto winner_result = std::get<0>(any.futures).get_result();
    ASSERT_TRUE(winner_result.has_value());
    EXPECT_EQ(winner_result.value(), 111);

    promise2.set_value(222);
    auto loser_result = std::get<1>(any.futures).get_result();
    ASSERT_TRUE(loser_result.has_value());
    EXPECT_EQ(loser_result.value(), 222);
}

/**
 * @brief Tests returns second index when second completes first.
 */
TEST(WhenAnyResultTest, returns_second_index_when_second_completes_first)
{
    auto pair1 = make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    auto pair2 = make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    auto combined = when_any(std::move(future1), std::move(future2));

    promise2.set_error(result_error::execution_failure);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto any = std::move(result).value();
    EXPECT_EQ(any.index, 1U);

    auto winner_result = std::get<1>(any.futures).get_result();
    ASSERT_FALSE(winner_result.has_value());
    EXPECT_EQ(winner_result.error(), result_error::execution_failure);

    promise1.set_value(7);
    auto other_result = std::get<0>(any.futures).get_result();
    ASSERT_TRUE(other_result.has_value());
    EXPECT_EQ(other_result.value(), 7);
}

/**
 * @brief Tests already ready input completes without delay.
 */
TEST(WhenAnyResultTest, already_ready_input_completes_without_delay)
{
    auto pair1 = make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    auto pair2 = make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    promise2.set_value(42);

    auto combined = when_any(std::move(future1), std::move(future2));
    combined.wait();

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    auto any = std::move(result).value();
    EXPECT_EQ(any.index, 1U);

    auto winner_result = std::get<1>(any.futures).get_result();
    ASSERT_TRUE(winner_result.has_value());
    EXPECT_EQ(winner_result.value(), 42);

    promise1.set_value(9);
    auto other_result = std::get<0>(any.futures).get_result();
    ASSERT_TRUE(other_result.has_value());
    EXPECT_EQ(other_result.value(), 9);
}

/**
 * @brief Tests invalid input returns no state error.
 */
TEST(WhenAnyResultTest, invalid_input_returns_no_state_error)
{
    future_result<int> invalid_future;

    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto valid_future = std::move(pair.second);

    auto combined = when_any(std::move(invalid_future), std::move(valid_future));

    auto result = combined.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::no_state);

    promise.set_value(1);
}

/**
 * @brief Tests iterator overload empty range returns npos and empty vector.
 */
TEST(WhenAnyResultTest, iterator_overload_empty_range_returns_npos_and_empty_vector)
{
    std::list<future_result<int>> empty;

    auto combined = when_any(empty.begin(), empty.end());

    ASSERT_TRUE(combined.valid());
    ASSERT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().index, static_cast<std::size_t>(-1));
    EXPECT_TRUE(result.value().futures.empty());
}

/**
 * @brief Tests iterator overload returns index of first ready future.
 */
TEST(WhenAnyResultTest, iterator_overload_returns_index_of_first_ready_future)
{
    std::vector<future_result<int>> futures;

    auto pair1 = make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    futures.push_back(std::move(pair1.second));

    auto pair2 = make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    futures.push_back(std::move(pair2.second));

    auto combined = when_any(futures.begin(), futures.end());

    promise2.set_value(9);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().index, 1U);
    ASSERT_EQ(result.value().futures.size(), 2U);
    ASSERT_TRUE(result.value().futures[1].is_ready());
    EXPECT_EQ(result.value().futures[1].get_result().value(), 9);

    promise1.set_value(4);
}

/**
 * @brief Tests vector overload reuses passed vector storage.
 */
TEST(WhenAnyResultTest, vector_overload_reuses_passed_vector_storage)
{
    std::vector<future_result<int>> futures;
    futures.reserve(2);

    auto pair1 = make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    futures.push_back(std::move(pair1.second));

    auto pair2 = make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    futures.push_back(std::move(pair2.second));

    const auto *buffer = futures.data();
    auto combined = when_any(std::move(futures));

    promise1.set_value(17);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().index, 0U);
    EXPECT_EQ(result.value().futures.data(), buffer);

    promise2.set_value(23);
}

/**
 * @brief Tests broken promise input can win and is preserved.
 */
TEST(WhenAnyResultTest, broken_promise_input_can_win_and_is_preserved)
{
    future_result<int> broken_future;
    {
        auto pair = make_result_promise<int>();
        broken_future = std::move(pair.second);
    }

    auto pair2 = make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    auto combined = when_any(std::move(broken_future), std::move(future2));

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());

    auto any = std::move(result).value();
    EXPECT_EQ(any.index, 0U);

    auto winner_result = std::get<0>(any.futures).get_result();
    ASSERT_FALSE(winner_result.has_value());
    EXPECT_EQ(winner_result.error(), result_error::broken_promise);

    promise2.set_value(55);
    auto other_result = std::get<1>(any.futures).get_result();
    ASSERT_TRUE(other_result.has_value());
    EXPECT_EQ(other_result.value(), 55);
}

/**
 * @brief Tests iterator overload invalid input returns no state error.
 */
TEST(WhenAnyResultTest, iterator_overload_invalid_input_returns_no_state_error)
{
    std::vector<future_result<int>> futures;
    futures.emplace_back();

    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    futures.push_back(std::move(pair.second));

    auto combined = when_any(futures.begin(), futures.end());

    auto result = combined.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::no_state);

    promise.set_value(1);
}

/**
 * @brief Tests vector overload invalid input returns no state error.
 */
TEST(WhenAnyResultTest, vector_overload_invalid_input_returns_no_state_error)
{
    std::vector<future_result<int>> futures;
    futures.emplace_back();

    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    futures.push_back(std::move(pair.second));

    auto combined = when_any(std::move(futures));

    auto result = combined.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::no_state);

    promise.set_value(1);
}

// ---------------------------------------------------------------------------
// P2.1 — when_any hardening parity
// ---------------------------------------------------------------------------

TEST(WhenAnyResultTest, later_readiness_does_not_change_winner_index)
{
    auto pair0 = make_result_promise<int>();
    auto pair1 = make_result_promise<int>();
    auto pair2 = make_result_promise<int>();

    auto combined = when_any(std::move(pair0.second),
                             std::move(pair1.second),
                             std::move(pair2.second));
    EXPECT_FALSE(combined.is_ready());

    pair1.first.set_value(42);
    EXPECT_TRUE(combined.is_ready());

    // further completions must not change winner
    pair0.first.set_value(1);
    pair2.first.set_value(3);

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().index, 1U);
}

TEST(WhenAnyResultTest, concurrent_readiness_resolves_with_valid_winner_index)
{
    std::vector<future_result<int>> futures;
    std::vector<promise_result<int>> promises;
    promises.reserve(2);
    futures.reserve(2);
    for (int i = 0; i < 2; ++i) {
        auto p = make_result_promise<int>();
        promises.push_back(std::move(p.first));
        futures.push_back(std::move(p.second));
    }

    auto combined = when_any(std::move(futures));

    std::atomic<bool> go{false};
    std::thread t0([&] { while (!go.load(std::memory_order_acquire)) {} promises[0].set_value(10); });
    std::thread t1([&] { while (!go.load(std::memory_order_acquire)) {} promises[1].set_value(20); });
    go.store(true, std::memory_order_release);
    t0.join(); t1.join();

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().index == 0U || result.value().index == 1U);
}

TEST(WhenAnyResultTest, iterator_overload_winner_index_stable_after_more_complete)
{
    std::vector<future_result<int>> futures;
    std::vector<promise_result<int>> promises;
    const int N = 4;
    for (int i = 0; i < N; ++i) {
        auto p = make_result_promise<int>();
        promises.push_back(std::move(p.first));
        futures.push_back(std::move(p.second));
    }

    auto combined = when_any(futures.begin(), futures.end());
    promises[2].set_value(99);
    EXPECT_TRUE(combined.is_ready());

    // complete remaining after get
    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    const auto winner = result.value().index;
    EXPECT_EQ(winner, 2U);

    promises[0].set_value(0);
    promises[1].set_value(1);
    promises[3].set_value(3);

    // winner must not change
    EXPECT_EQ(result.value().index, winner);
}

TEST(WhenAnyResultTest, already_ready_input_wins_with_correct_index)
{
    auto pair0 = make_result_promise<int>();
    auto pair1 = make_result_promise<int>();
    pair1.first.set_value(55); // index 1 is already ready

    auto combined = when_any(std::move(pair0.second), std::move(pair1.second));
    EXPECT_TRUE(combined.is_ready());

    auto result = combined.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().index, 1U);

    pair0.first.set_value(0); // completing the other doesn't change it
}

} // namespace
