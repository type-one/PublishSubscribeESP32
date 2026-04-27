/**
 * @file test_continuation_result.cpp
 * @brief Second-batch unit tests for result-based continuation semantics.
 */

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "tools/platform_detection.hpp"

#include "portable_concurrency/functional.hpp"
#include "portable_concurrency/future.hpp"

namespace
{
    struct queued_executor
    {
        std::vector<pco::unique_function<void()>> tasks;

        void run_all()
        {
            auto pending = std::move(tasks);
            tasks.clear();
            for (auto& task : pending)
            {
                task();
            }
        }
    };

    template <typename Task>
    void post(queued_executor& exec, Task&& task)
    {
        exec.tasks.emplace_back(std::forward<Task>(task));
    }
} // namespace

namespace pco
{
    template <>
    struct is_executor<queued_executor> : std::true_type
    {
    };
}

namespace
{

    /**
     * @brief Verifies then_value executes on success and transforms the value.
     */
    TEST(ContinuationResultTest, then_value_runs_on_success)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(21);

        auto chained = std::move(source).then_value([](int value) { return value * 2; });

        auto result = chained.get_result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42);
    }

    /**
     * @brief Tests then value executor skips when downstream destroyed.
     */
    TEST(ContinuationResultTest, then_value_executor_skips_when_downstream_destroyed)
    {
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        queued_executor exec;
        bool called = false;

        promise.set_value(4);

        auto chained = std::move(source).then_value(exec,
            [&called](int value)
            {
                called = true;
                return value + 1;
            });

        chained = pco::future_result<int> {};
        exec.run_all();

        EXPECT_FALSE(called);
    }

    /**
     * @brief Tests then error executor skips when downstream destroyed.
     */
    TEST(ContinuationResultTest, then_error_executor_skips_when_downstream_destroyed)
    {
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        queued_executor exec;
        bool called = false;

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source).then_error(exec,
            [&called](pco::result_error)
            {
                called = true;
                return 7;
            });

        chained = pco::future_result<int> {};
        exec.run_all();

        EXPECT_FALSE(called);
    }

    /**
     * @brief Tests then result executor skips when downstream destroyed.
     */
    TEST(ContinuationResultTest, then_result_executor_skips_when_downstream_destroyed)
    {
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        queued_executor exec;
        bool called = false;

        promise.set_value(9);

        auto chained = std::move(source).then_result(exec,
            [&called](pco::expected<int, pco::result_error> result)
            {
                called = true;
                return result.value() * 2;
            });

        chained = pco::future_result<int> {};
        exec.run_all();

        EXPECT_FALSE(called);
    }

    /**
     * @brief Tests then value skips when downstream destroyed.
     */
    TEST(ContinuationResultTest, then_value_skips_when_downstream_destroyed)
    {
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        bool called = false;

        auto chained = std::move(source).then_value(
            [&called](int value)
            {
                called = true;
                return value + 1;
            });

        chained = pco::future_result<int> {};
        promise.set_value(4);

        EXPECT_FALSE(called);
    }

    /**
     * @brief Tests then error skips when downstream destroyed.
     */
    TEST(ContinuationResultTest, then_error_skips_when_downstream_destroyed)
    {
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        bool called = false;

        auto chained = std::move(source).then_error(
            [&called](pco::result_error)
            {
                called = true;
                return 7;
            });

        chained = pco::future_result<int> {};
        promise.set_error(pco::result_error::execution_failure);

        EXPECT_FALSE(called);
    }

    /**
     * @brief Tests then result skips when downstream destroyed.
     */
    TEST(ContinuationResultTest, then_result_skips_when_downstream_destroyed)
    {
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        bool called = false;

        auto chained = std::move(source).then_result(
            [&called](pco::expected<int, pco::result_error> result)
            {
                called = true;
                return result.has_value() ? result.value() * 2 : -1;
            });

        chained = pco::future_result<int> {};
        promise.set_value(9);

        EXPECT_FALSE(called);
    }

    /**
     * @brief Verifies then_value is skipped when the source already carries an error.
     */
    TEST(ContinuationResultTest, then_value_skips_on_error)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);
        bool called = false;

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source).then_value(
            [&called](int)
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
     * @brief Verifies then_error can recover from an error into a value.
     */
    TEST(ContinuationResultTest, then_error_recovers_value)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source).then_error(
            [](pco::result_error error)
            {
                EXPECT_EQ(error, pco::result_error::execution_failure);
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
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);
        bool called = false;

        promise.set_value(42);

        auto chained = std::move(source).then_error(
            [&called](pco::result_error)
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
     * @brief Verifies then_result receives successful pco::expected values.
     */
    TEST(ContinuationResultTest, then_result_receives_success_expected)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(42);

        auto chained = std::move(source).then_result(
            [](pco::expected<int, pco::result_error> result)
            {
                EXPECT_TRUE(result.has_value());
                return result.value() + 1;
            });

        auto out = chained.get_result();
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out.value(), 43);
    }

    /**
     * @brief Verifies then_result receives error pco::expected values.
     */
    TEST(ContinuationResultTest, then_result_receives_error_expected)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_error(pco::result_error::broken_promise);

        auto chained = std::move(source).then_result(
            [](pco::expected<int, pco::result_error> result)
            {
                EXPECT_FALSE(result.has_value());
                EXPECT_EQ(result.error(), pco::result_error::broken_promise);
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
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source)
                           .then_error([](pco::result_error) { return 5; })
                           .then_value([](int value) { return value * 3; });

        auto out = chained.get_result();
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out.value(), 15);
    }

    /**
     * @brief Tests then value unwraps nested future result.
     */
    TEST(ContinuationResultTest, then_value_unwraps_nested_future_result)
    {
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        promise.set_value(5);

        auto chained = std::move(source).then_value(
            [](int value)
            {
                auto nested_pair = pco::make_result_promise<int>();
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
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        promise.set_value(6);

        auto chained = std::move(source).then_value(
            [](int value)
            {
                auto nested_pair = pco::make_result_promise<int>();
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
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        promise.set_value(7);

        auto chained = std::move(source).then_result(
            [](pco::expected<int, pco::result_error> result)
            {
                auto nested_pair = pco::make_result_promise<int>();
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
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        promise.set_value(8);

        auto chained = std::move(source).then_value(pco::inplace_executor,
            [](int value)
            {
                auto nested_pair = pco::make_result_promise<int>();
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
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto source = std::move(pair.second);

        promise.set_value(5);

        auto chained = std::move(source).then_value(
            [](int)
            {
                auto nested_pair = pco::make_result_promise<int>();
                nested_pair.first.set_error(pco::result_error::execution_failure);
                return std::move(nested_pair.second);
            });

        auto result = chained.get_result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), pco::result_error::execution_failure);
    }

#if defined(CPP_EXCEPTIONS_ENABLED)
    /**
     * @brief Verifies a throw in then_value maps to continuation_failure.
     */
    TEST(ContinuationResultTest, then_value_throw_maps_to_continuation_failure)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(42);

        auto chained = std::move(source).then_value([](int) -> int { throw std::runtime_error("boom"); });

        auto result = chained.get_result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), pco::result_error::continuation_failure);
    }

    /**
     * @brief Verifies a throw in then_error maps to continuation_failure.
     */
    TEST(ContinuationResultTest, then_error_throw_maps_to_continuation_failure)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source).then_error([](pco::result_error) -> int { throw std::runtime_error("boom"); });

        auto result = chained.get_result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), pco::result_error::continuation_failure);
    }

    /**
     * @brief Verifies a throw in then_result maps to continuation_failure.
     */
    TEST(ContinuationResultTest, then_result_throw_maps_to_continuation_failure)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(42);

        auto chained = std::move(source).then_result(
            [](pco::expected<int, pco::result_error>) -> int { throw std::runtime_error("boom"); });

        auto result = chained.get_result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), pco::result_error::continuation_failure);
    }
#endif

    /**
     * @brief Verifies then_value with executor dispatches to the executor and transforms value.
     */
    TEST(ContinuationResultTest, then_value_executor_dispatches_and_transforms)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(21);

        // Use inplace_executor to ensure deterministic execution
        auto chained = std::move(source).then_value(pco::inplace_executor, [](int value) { return value * 2; });

        auto result = chained.get_result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42);
    }

    /**
     * @brief Verifies executor-based then_value propagates errors without calling the continuation.
     */
    TEST(ContinuationResultTest, then_value_executor_propagates_error)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);
        bool called = false;

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source).then_value(pco::inplace_executor,
            [&called](int)
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
     * @brief Verifies executor-based then_error dispatches error handler to executor.
     */
    TEST(ContinuationResultTest, then_error_executor_dispatches_and_recovers)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source).then_error(pco::inplace_executor,
            [](pco::result_error error)
            {
                EXPECT_EQ(error, pco::result_error::execution_failure);
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
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);
        bool called = false;

        promise.set_value(42);

        auto chained = std::move(source).then_error(pco::inplace_executor,
            [&called](pco::result_error)
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
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(42);

        auto chained = std::move(source).then_result(pco::inplace_executor,
            [](pco::expected<int, pco::result_error> result)
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
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_error(pco::result_error::broken_promise);

        auto chained = std::move(source).then_result(pco::inplace_executor,
            [](pco::expected<int, pco::result_error> result)
            {
                EXPECT_FALSE(result.has_value());
                EXPECT_EQ(result.error(), pco::result_error::broken_promise);
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
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(10);

        auto chained = std::move(source)
                           .then_value(pco::inplace_executor, [](int value) { return value + 5; })
                           .then_value(pco::inplace_executor, [](int value) { return value * 2; });

        auto result = chained.get_result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 30);
    }

#if defined(CPP_EXCEPTIONS_ENABLED)
    /**
     * @brief Verifies executor-based then_value throw maps to continuation_failure.
     */
    TEST(ContinuationResultTest, then_value_executor_throw_maps_to_continuation_failure)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(42);

        auto chained = std::move(source).then_value(
            pco::inplace_executor, [](int) -> int { throw std::runtime_error("executor continuation failed"); });

        auto result = chained.get_result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), pco::result_error::continuation_failure);
    }

    /**
     * @brief Verifies executor-based then_error throw maps to continuation_failure.
     */
    TEST(ContinuationResultTest, then_error_executor_throw_maps_to_continuation_failure)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_error(pco::result_error::execution_failure);

        auto chained = std::move(source).then_error(pco::inplace_executor,
            [](pco::result_error) -> int { throw std::runtime_error("executor error handler failed"); });

        auto result = chained.get_result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), pco::result_error::continuation_failure);
    }

    /**
     * @brief Verifies executor-based then_result throw maps to continuation_failure.
     */
    TEST(ContinuationResultTest, then_result_executor_throw_maps_to_continuation_failure)
    {
        auto promise_and_future = pco::make_result_promise<int>();
        auto promise = std::move(promise_and_future.first);
        auto source = std::move(promise_and_future.second);

        promise.set_value(42);

        auto chained
            = std::move(source).then_result(pco::inplace_executor, [](pco::expected<int, pco::result_error>) -> int
                { throw std::runtime_error("executor result handler failed"); });

        auto result = chained.get_result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), pco::result_error::continuation_failure);
    }
#endif

} // namespace
