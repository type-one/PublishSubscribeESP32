/**
 * @file test_next_result.cpp
 * @brief Parity tests for next API and non-blocking executor continuations.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <vector>

#include "portable_concurrency/functional.hpp"
#include "portable_concurrency/future.hpp"

namespace
{
struct queued_executor {
    std::vector<pco::unique_function<void()>> tasks;

    void run_all()
    {
        auto pending = std::move(tasks);
        tasks.clear();
        for (auto &task : pending)
        {
            const auto invocation_result = task.invoke();
            ASSERT_TRUE(invocation_result.has_value());
        }
    }
};

template <typename Task>
void post(queued_executor &exec, Task &&task)
{
    exec.tasks.emplace_back(std::forward<Task>(task));
}
} // namespace

namespace pco
{
template <>
struct is_executor<queued_executor> : std::true_type {};
}

namespace
{

/**
 * @brief Verifies pco::future_result::next mirrors then_value behavior on success.
 */
TEST(NextResultTest, future_next_runs_on_success)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_value(10);

    auto chained = std::move(source).next([](int value)
    {
        return value + 5;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 15);
}

/**
 * @brief Verifies pco::future_result::next propagates source error without calling continuation.
 */
TEST(NextResultTest, future_next_skips_on_error)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);
    bool called = false;

    promise.set_error(pco::result_error::execution_failure);

    auto chained = std::move(source).next([&called](int)
    {
        called = true;
        return 0;
    });

    auto result = chained.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::execution_failure);
    EXPECT_FALSE(called);
}

/**
 * @brief Verifies pco::future_result::next(exec, fn) dispatches via the supplied executor.
 */
TEST(NextResultTest, future_next_executor_dispatches)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);
    queued_executor exec;

    auto chained = std::move(source).next(exec, [](int value)
    {
        return value * 3;
    });

    promise.set_value(4);
    exec.run_all();

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 12);
}

/**
 * @brief Verifies pco::shared_result::next mirrors then_value behavior on success.
 */
TEST(NextResultTest, shared_next_runs_on_success)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.next([](const int &value)
    {
        return value + 4;
    });

    promise.set_value(8);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 12);
}

/**
 * @brief Verifies pco::shared_result::next(exec, fn) dispatches via the supplied executor.
 */
TEST(NextResultTest, shared_next_executor_dispatches)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto shared = std::move(pair.second).share();

    auto chained = shared.next(pco::inplace_executor, [](const int &value)
    {
        return value * 2;
    });

    promise.set_value(9);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 18);
}

/**
 * @brief Verifies executor continuation registration on pco::future_result is non-blocking.
 */
TEST(NextResultTest, then_value_executor_registration_is_non_blocking)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);
    queued_executor exec;

    auto registration = std::async(std::launch::async, [&source, &exec]() mutable
    {
        return std::move(source).then_value(exec, [](int value)
        {
            return value + 1;
        });
    });

    EXPECT_EQ(registration.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);

    auto chained = registration.get();
    promise.set_value(7);
    exec.run_all();

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 8);
}

/**
 * @brief Verifies then_error(exec, fn) registration returns before the promise is set.
 */
TEST(NextResultTest, then_error_executor_registration_is_non_blocking)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);
    queued_executor exec;

    // fn receives error value E, returns T (recovery value)
    auto registration = std::async(std::launch::async, [&source, &exec]() mutable
    {
        return std::move(source).then_error(exec, [](pco::result_error /*err*/)
        {
            return -1;
        });
    });

    EXPECT_EQ(registration.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);

    auto chained = registration.get();
    promise.set_error(pco::result_error::execution_failure);
    exec.run_all();

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), -1);
}

/**
 * @brief Verifies then_result(exec, fn) registration returns before the promise is set.
 */
TEST(NextResultTest, then_result_executor_registration_is_non_blocking)
{
    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);
    queued_executor exec;
    bool fn_called = false;

    // fn receives pco::expected<T,E>, mirrors it back; use auto to stay type-agnostic
    auto registration = std::async(std::launch::async, [&source, &exec, &fn_called]() mutable
    {
        return std::move(source).then_result(exec, [&fn_called](auto r)
        {
            fn_called = true;
            return r;
        });
    });

    EXPECT_EQ(registration.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
    EXPECT_FALSE(fn_called); // fn must not run during registration

    auto chained = registration.get();
    promise.set_value(42);
    exec.run_all();

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
    EXPECT_EQ(result.value().value(), 42);
    EXPECT_TRUE(fn_called);
}

} // namespace
