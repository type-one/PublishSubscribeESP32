//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

/**
 * @file test_portable_concurrency_scenarios.cpp
 * @brief Scenario-style examples and tests for portable_concurrency usage.
 *
 * The goal is to mirror the practical style from the reference main.cpp examples:
 * - computation chains
 * - fan-out/fan-in gather with when_all
 * - coroutine scheduling on worker threads
 * - cancellation behavior with canceler_arg
 * - periodic task polling readiness of a longer worker computation
 * - chain of execution across two executors (two worker tasks)
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "portable_concurrency/p_future.hpp"
#include "portable_concurrency/p_future_policy.hpp"
#include "tools/periodic_task.hpp"
#include "tools/worker_task.hpp"

#if defined(PC_HAS_COROUTINES) || defined(__cpp_impl_coroutine) || defined(__cpp_coroutines) || (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#define PS_PC_HAS_COROUTINES
#endif

namespace {

struct scenario_worker_context {
    std::atomic<int> loop_counter { 0 };
};

using scenario_worker_task = tools::worker_task<scenario_worker_context>;

std::unique_ptr<scenario_worker_task> make_worker_task(
    const std::shared_ptr<scenario_worker_context>& context, const std::string& name)
{
    auto startup = [](const std::shared_ptr<scenario_worker_context>&, const std::string&) {};
    constexpr std::size_t stack_size = 4096U;
    return std::make_unique<scenario_worker_task>(std::move(startup), context, name, stack_size);
}

#if defined(PS_PC_HAS_COROUTINES) && defined(WORKER_TASK_HAS_PC_ASYNC)
portable_concurrency::future<int> coroutine_worker_job(
    scenario_worker_task& task, const std::shared_ptr<scenario_worker_context>& context, int value)
{
    co_await task.schedule();
    context->loop_counter.fetch_add(1);
    co_return value * 3;
}

portable_concurrency::future<int> coroutine_mixed_job(scenario_worker_task& task,
    const std::shared_ptr<scenario_worker_context>& context, portable_concurrency::future<int> async_value)
{
    co_await task.schedule();
    const auto previous = co_await async_value;
    context->loop_counter.fetch_add(1);
    co_return previous + 7;
}
#endif

struct polling_context {
    std::atomic<int> ready_checks { 0 };
    std::atomic<int> not_ready_checks { 0 };
};

using polling_periodic_task = tools::periodic_task<polling_context>;

} // namespace

#if defined(WORKER_TASK_HAS_PC_ASYNC)
/**
 * @brief Tests a multi-step async computation chain on a worker executor.
 */
TEST(PortableConcurrencyScenarios, ComputationChainOnWorkerTask)
{
    auto context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(context, "chain_worker");

    auto result_future = worker
                             ->delegate_async(
                                 [](const std::shared_ptr<scenario_worker_context>& ctx,
                                     const std::string&, int value)
                                 {
                                     ctx->loop_counter.fetch_add(1);
                                     return value * 2;
                                 },
                                 21)
                             .then([](portable_concurrency::future<int> previous)
                             {
                                 return previous.get() + 1;
                             })
                             .then([](portable_concurrency::future<int> previous)
                             {
                                 return previous.get() * 2;
                             });

    EXPECT_EQ(result_future.get(), 86);
    EXPECT_EQ(context->loop_counter.load(), 1);
}

/**
 * @brief Tests fan-out/fan-in composition using when_all over worker futures.
 */
TEST(PortableConcurrencyScenarios, GatherSeveralComputationsWithWhenAll)
{
    auto context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(context, "fanout_worker");

    std::vector<portable_concurrency::future<int>> jobs;
    jobs.reserve(5);

    for (int value = 1; value <= 5; ++value)
    {
        jobs.emplace_back(
            worker
                ->delegate_async(
                    [](const std::shared_ptr<scenario_worker_context>& ctx, const std::string&, int v)
                    {
                        ctx->loop_counter.fetch_add(1);
                        return v * v;
                    },
                    value)
                .then(worker->as_executor(),
                    [](portable_concurrency::future<int> previous)
                    {
                        return previous.get() + 10;
                    }));
    }

    auto total_future = portable_concurrency::when_all(std::move(jobs)).next(
        [](std::vector<portable_concurrency::future<int>> results)
        {
            int total = 0;
            for (auto& item : results)
            {
                total += item.get();
            }
            return total;
        });

    EXPECT_EQ(total_future.get(), 105);
    EXPECT_EQ(context->loop_counter.load(), 5);
}

/**
 * @brief Tests cancellation callback execution through canceler_arg promises.
 */
TEST(PortableConcurrencyScenarios, CancellationWithCancelerArg)
{
    std::atomic_bool cancel_called { false };

    {
        auto promise_and_future = portable_concurrency::make_promise<int>(
            portable_concurrency::canceler_arg,
            [&cancel_called]()
            {
                cancel_called.store(true);
            });

        auto promise_obj = std::move(promise_and_future.first);
        auto future_obj = std::move(promise_and_future.second);

        (void)promise_obj;

        // Drop the only strong owner of the state without fulfilling it.
        future_obj = portable_concurrency::future<int> {};
    }

    // Callback is expected to run synchronously on state destruction.
    EXPECT_TRUE(cancel_called.load());
}

/**
 * @brief Tests continuation chain hopping from one worker executor to another.
 */
TEST(PortableConcurrencyScenarios, ChainAcrossDifferentExecutors)
{
    auto context_a = std::make_shared<scenario_worker_context>();
    auto context_b = std::make_shared<scenario_worker_context>();

    auto worker_a = make_worker_task(context_a, "worker_a");
    auto worker_b = make_worker_task(context_b, "worker_b");

    auto chained = worker_a
                       ->delegate_async(
                           [](const std::shared_ptr<scenario_worker_context>& ctx, const std::string&, int value)
                           {
                               ctx->loop_counter.fetch_add(1);
                               return value * 2;
                           },
                           10)
                       .then(worker_b->as_executor(),
                           [ctx = context_b](portable_concurrency::future<int> previous)
                           {
                               ctx->loop_counter.fetch_add(1);
                               return previous.get() + 7;
                           });

    EXPECT_EQ(chained.get(), 27);
    EXPECT_EQ(context_a->loop_counter.load(), 1);
    EXPECT_EQ(context_b->loop_counter.load(), 1);
}

#if defined(PS_PC_HAS_COROUTINES)
/**
 * @brief Tests coroutine schedule() hop onto worker thread followed by continuation.
 */
TEST(PortableConcurrencyScenarios, CoroutineScheduleOnWorkerTask)
{
    auto context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(context, "coro_worker");

    auto result_future = coroutine_worker_job(*worker, context, 14)
                             .then(worker->as_executor(),
                                 [](portable_concurrency::future<int> previous)
                                 {
                                     return previous.get() + 2;
                                 });

    EXPECT_EQ(result_future.get(), 44);
    EXPECT_GE(context->loop_counter.load(), 1);
}

/**
 * @brief Tests mixed coroutine flow combining co_await and delegate_async result.
 */
TEST(PortableConcurrencyScenarios, CoroutineMixedWithAsyncValue)
{
    auto context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(context, "coro_mixed_worker");

    auto async_value = worker->delegate_async(
        [](const std::shared_ptr<scenario_worker_context>& ctx, const std::string&, int value)
        {
            ctx->loop_counter.fetch_add(1);
            return value * 5;
        },
        6);

    auto mixed_result = coroutine_mixed_job(*worker, context, std::move(async_value)).then(
        worker->as_executor(),
        [](portable_concurrency::future<int> previous)
        {
            return previous.get() + 3;
        });

    EXPECT_EQ(mixed_result.get(), 40);
    EXPECT_GE(context->loop_counter.load(), 2);
}
#endif // PS_PC_HAS_COROUTINES
#endif // WORKER_TASK_HAS_PC_ASYNC

/**
 * @brief Tests periodic polling of long-running worker future readiness.
 *
 * The periodic task repeatedly checks `is_ready()` while a worker computation
 * is in progress, and we verify both not-ready and ready states are observed.
 */
TEST(PortableConcurrencyScenarios, PeriodicTaskPollsLongWorkerComputationReadiness)
{
    auto worker_context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(worker_context, "long_compute_worker");

    auto long_future = worker->delegate_async_v2(
        [](const std::shared_ptr<scenario_worker_context>& ctx, const std::string&, int value)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(220));
            ctx->loop_counter.fetch_add(1);
            return value * 3;
        },
        7);

    auto periodic_context = std::make_shared<polling_context>();

    auto startup = [](const std::shared_ptr<polling_context>&, const std::string&) {};
    auto periodic = [&long_future](const std::shared_ptr<polling_context>& ctx, const std::string&)
    {
        if (long_future.is_ready())
        {
            ctx->ready_checks.fetch_add(1);
        }
        else
        {
            ctx->not_ready_checks.fetch_add(1);
        }
    };

    constexpr std::size_t periodic_stack_size = 4096U;
    constexpr auto period = std::chrono::duration<std::uint64_t, std::micro>(20000); // 20 ms

    {
        polling_periodic_task poller(
            std::move(startup), std::move(periodic), periodic_context, "future_polling_periodic", period, periodic_stack_size);

        std::this_thread::sleep_for(std::chrono::milliseconds(320));
    }

#if defined(PORTABLE_CONCURRENCY_V2_DEFAULT) || !defined(WORKER_TASK_HAS_PC_ASYNC)
    auto result = std::move(long_future)
                      .then_error([](portable_concurrency::v2::result_error)
                      {
                          return -1;
                      })
                      .then_value([](int value)
                      {
                          return value;
                      })
                      .get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 21);
#else
    EXPECT_EQ(long_future.get(), 21);
#endif

    EXPECT_GT(periodic_context->not_ready_checks.load(), 0);
    EXPECT_GT(periodic_context->ready_checks.load(), 0);
    EXPECT_EQ(worker_context->loop_counter.load(), 1);
}
