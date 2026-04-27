/**
 * @file test_promise_result.cpp
 * @brief Imported-style unit tests for portable_concurrency pco::promise_result.
 */

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "portable_concurrency/future.hpp"

namespace
{
using pco::canceler_arg;

/**
 * @brief Verifies get_future returns a valid pco::future_result the first time.
 */
TEST(PromiseResultTest, get_future_returns_valid_future)
{
    pco::promise_result<int> promise;
    auto future = promise.get_future();

    EXPECT_TRUE(future.valid());
}

/**
 *  Verifies a value set before get_future can still be retrieved.
 */
TEST(PromiseResultTest, can_retrieve_value_set_before_get_future)
{
    pco::promise_result<int> promise;
    promise.set_value(123);

    auto future = promise.get_future();
    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 123);
}

/**
 * @brief Verifies retrieving the future twice returns an invalid second future.
 */
TEST(PromiseResultTest, get_future_twice_returns_invalid_second_future)
{
    pco::promise_result<int> promise;
    auto first_future = promise.get_future();
    auto second_future = promise.get_future();

    EXPECT_TRUE(first_future.valid());
    EXPECT_FALSE(second_future.valid());
}

/**
 * @brief Verifies set_value delivers the stored result to the paired future.
 */
TEST(PromiseResultTest, set_value_delivers_result_to_future)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_value(99);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 99);
}

/**
 * @brief Verifies set_error delivers the stored error to the paired future.
 */
TEST(PromiseResultTest, set_error_delivers_error_to_future)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_error(pco::result_error::execution_failure);

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::execution_failure);
}

/**
 * @brief Verifies set_value on a promise without an attached future remains harmless.
 */
TEST(PromiseResultTest, set_value_without_retrieved_future_is_harmless)
{
    pco::promise_result<int> promise;

    promise.set_value(5);

    SUCCEED();
}

/**
 * @brief Verifies destroying an unsatisfied pco::promise_result yields broken_promise.
 */
TEST(PromiseResultTest, unsatisfied_promise_destruction_yields_broken_promise)
{
    pco::future_result<int> future;
    {
        pco::promise_result<int> promise;
        future = promise.get_future();
    }

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::broken_promise);
}

/**
 * @brief Verifies void pco::promise_result can satisfy its future without payload.
 */
TEST(PromiseResultTest, void_promise_set_value_completes_future)
{
    auto promise_and_future = pco::make_result_promise<void>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_value();

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
}

/**
 * @brief Verifies first completion wins when set_value is followed by set_error.
 */
TEST(PromiseResultTest, first_completion_wins_value_then_error)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_value(7);
    promise.set_error(pco::result_error::execution_failure);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Verifies first completion wins when set_error is followed by set_value.
 */
TEST(PromiseResultTest, first_completion_wins_error_then_value)
{
    auto promise_and_future = pco::make_result_promise<int>();
    auto promise = std::move(promise_and_future.first);
    auto future = std::move(promise_and_future.second);

    promise.set_error(pco::result_error::execution_failure);
    promise.set_value(7);

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::execution_failure);
}

/**
 *  Verifies repeated completion without a retrieved future is harmless.
 */
TEST(PromiseResultTest, repeated_completion_without_future_is_harmless)
{
    pco::promise_result<int> promise;

    EXPECT_NO_FATAL_FAILURE(promise.set_value(1));
    EXPECT_NO_FATAL_FAILURE(promise.set_value(2));
    EXPECT_NO_FATAL_FAILURE(promise.set_error(pco::result_error::execution_failure));
}

/**
 *  Verifies repeated completion with a future preserves the first result.
 */
TEST(PromiseResultTest, repeated_completion_with_future_preserves_first_result)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    promise.set_value(7);
    promise.set_value(11);
    promise.set_error(pco::result_error::execution_failure);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

/**
 * @brief Verifies get_future after moving promise keeps single-retrieval semantics.
 */
TEST(PromiseResultTest, moved_promise_preserves_single_get_future_semantics)
{
    pco::promise_result<int> original;
    pco::promise_result<int> moved = std::move(original);

    auto first_future = moved.get_future();
    auto second_future = moved.get_future();

    EXPECT_TRUE(first_future.valid());
    EXPECT_FALSE(second_future.valid());
}

/**
 *  Verifies moved-from promise get_future behaves like no_state (invalid future).
 */
TEST(PromiseResultTest, moved_from_get_future_returns_invalid_future)
{
    pco::promise_result<int> promise;
    pco::promise_result<int> moved = std::move(promise);

    auto moved_from_future = promise.get_future();
    auto moved_future = moved.get_future();

    EXPECT_FALSE(moved_from_future.valid());
    EXPECT_TRUE(moved_future.valid());
}

/**
 *  Verifies moved-from set_value is a no-op and does not affect moved-to state.
 */
TEST(PromiseResultTest, moved_from_set_value_is_noop)
{
    pco::promise_result<int> promise;
    pco::promise_result<int> moved = std::move(promise);
    auto future = moved.get_future();

    promise.set_value(99);
    moved.set_value(5);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 5);
}

/**
 *  Verifies moved-from set_error is a no-op and does not affect moved-to state.
 */
TEST(PromiseResultTest, moved_from_set_error_is_noop)
{
    pco::promise_result<int> promise;
    pco::promise_result<int> moved = std::move(promise);
    auto future = moved.get_future();

    promise.set_error(pco::result_error::execution_failure);
    moved.set_value(8);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 8);
}

/**
 *  Verifies allocator-tag constructor is intentionally unsupported in the result-based API.
 */
TEST(PromiseResultTest, allocator_constructor_is_not_supported)
{
    EXPECT_FALSE((std::is_constructible_v<pco::promise_result<int>, std::allocator_arg_t, std::allocator<int>>));
}

/**
 * @brief Tests is awaiten returns true before future retrieval.
 */
TEST(PromiseResultTest, is_awaiten_returns_true_before_future_retrieval)
{
    pco::promise_result<int> promise;

    EXPECT_TRUE(promise.is_awaiten());
}

/**
 * @brief Tests is awaiten returns true while future exists.
 */
TEST(PromiseResultTest, is_awaiten_returns_true_while_future_exists)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    EXPECT_TRUE(promise.is_awaiten());
    EXPECT_TRUE(future.valid());
}

/**
 * @brief Tests is awaiten returns false after future destruction.
 */
TEST(PromiseResultTest, is_awaiten_returns_false_after_future_destruction)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    future = pco::future_result<int>{};

    EXPECT_FALSE(promise.is_awaiten());
}

/**
 * @brief Tests is awaiten returns true after future detach.
 */
TEST(PromiseResultTest, is_awaiten_returns_true_after_future_detach)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);

    {
        auto detached = std::move(pair.second).detach();
        detached = pco::future_result<int>{};
    }

    EXPECT_TRUE(promise.is_awaiten());
}

/**
 * @brief Tests set value is noop after future destruction.
 */
TEST(PromiseResultTest, set_value_is_noop_after_future_destruction)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    future = pco::future_result<int>{};

    EXPECT_NO_FATAL_FAILURE(promise.set_value(42));
    EXPECT_FALSE(promise.is_awaiten());
}

/**
 * @brief Tests set error is noop after future destruction.
 */
TEST(PromiseResultTest, set_error_is_noop_after_future_destruction)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    future = pco::future_result<int>{};

    EXPECT_NO_FATAL_FAILURE(promise.set_error(pco::result_error::execution_failure));
    EXPECT_FALSE(promise.is_awaiten());
}

/**
 * @brief Tests canceler is not called by constructor.
 */
TEST(PromiseResultTest, canceler_is_not_called_by_constructor)
{
    size_t call_count = 0U;
    pco::promise_result<int> promise(canceler_arg, [&]() { ++call_count; });

    EXPECT_EQ(call_count, 0U);
    EXPECT_TRUE(promise.is_awaiten());
}

/**
 * @brief Tests canceler is called once if future destroyed before set.
 */
TEST(PromiseResultTest, canceler_is_called_once_if_future_destroyed_before_set)
{
    size_t call_count = 0U;
    {
        auto pair = pco::make_result_promise<int>(canceler_arg, [&]() { ++call_count; });
        auto promise = std::move(pair.first);
        auto future = std::move(pair.second);

        future = pco::future_result<int>{};
        EXPECT_FALSE(promise.is_awaiten());
    }

    EXPECT_EQ(call_count, 1U);
}

/**
 * @brief Tests canceler is not called if value delivered before future destruction.
 */
TEST(PromiseResultTest, canceler_is_not_called_if_value_delivered_before_future_destruction)
{
    size_t call_count = 0U;
    {
        auto pair = pco::make_result_promise<int>(canceler_arg, [&]() { ++call_count; });
        auto promise = std::move(pair.first);
        auto future = std::move(pair.second);

        promise.set_value(42);
        future = pco::future_result<int>{};
    }

    EXPECT_EQ(call_count, 0U);
}

/**
 * @brief Tests canceler supports move only cleanup.
 */
TEST(PromiseResultTest, canceler_supports_move_only_cleanup)
{
    bool called = false;
    auto deleter = [&called](int *value)
    {
        called = true;
        delete value;
    };

    std::unique_ptr<int, decltype(deleter)> resource(new int{42}, deleter);
    auto pair = pco::make_result_promise<int>(canceler_arg,
        [held = std::move(resource)]() mutable
        {
            held.reset();
        });
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    future = pco::future_result<int>{};

    EXPECT_TRUE(called);
    EXPECT_FALSE(promise.is_awaiten());
}

} // namespace
