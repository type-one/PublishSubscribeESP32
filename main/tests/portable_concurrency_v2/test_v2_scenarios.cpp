/**
 * @file test_v2_scenarios.cpp
 * @brief Black-box scenario-style tests for portable_concurrency v2.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "portable_concurrency/p_future_policy.hpp"
#include "portable_concurrency/p_future.hpp"
#include "tools/periodic_task.hpp"
#include "tools/worker_task.hpp"

namespace
{
using namespace portable_concurrency::v2;
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

struct polling_context {
    std::atomic<int> ready_checks { 0 };
    std::atomic<int> not_ready_checks { 0 };
};

using polling_periodic_task = tools::periodic_task<polling_context>;

} // namespace

/**
 * @brief Tests PromiseChainRecoveryAndTransform.
 */
TEST(V2Scenarios, PromiseChainRecoveryAndTransform)
{
    auto pair = make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto source = std::move(pair.second);

    promise.set_error(result_error::execution_failure);

    auto result = std::move(source)
                      .then_error([](result_error)
                      {
                          return 10;
                      })
                      .then_value([](int value)
                      {
                          return value * 3;
                      })
                      .get_result();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 30);
}

/**
 * @brief Tests WhenAllBrokenPromiseRecoveryScenario.
 */
TEST(V2Scenarios, WhenAllBrokenPromiseRecoveryScenario)
{
    auto pair1 = make_result_promise<int>();
    auto promise1 = std::move(pair1.first);
    auto future1 = std::move(pair1.second);

    future_result<int> broken_future;
    {
        auto pair2 = make_result_promise<int>();
        broken_future = std::move(pair2.second);
    }

    auto combined = when_all(std::move(future1), std::move(broken_future));

    promise1.set_value(42);

    auto chained = std::move(combined)
                       .then_value([](std::tuple<
                                      portable_concurrency::v2::expected<int, result_error>,
                                      portable_concurrency::v2::expected<int, result_error>> all)
                       {
                           const auto& first = std::get<0>(all);
                           const auto& second = std::get<1>(all);

                           if (first.has_value() && !second.has_value() &&
                               second.error() == result_error::broken_promise)
                           {
                               return first.value() + 100;
                           }
                           return -1;
                       });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 142);
}

/**
 * @brief Tests WhenAnyBrokenPromiseWinnerFallbackScenario.
 */
TEST(V2Scenarios, WhenAnyBrokenPromiseWinnerFallbackScenario)
{
    future_result<int> broken_future;
    {
        auto pair = make_result_promise<int>();
        broken_future = std::move(pair.second);
    }

    auto pair2 = make_result_promise<int>();
    auto promise2 = std::move(pair2.first);
    auto future2 = std::move(pair2.second);

    auto chained = when_any(std::move(broken_future), std::move(future2))
                       .then_value([](portable_concurrency::v2::when_any_result<
                                      std::tuple<future_result<int>,
                                                 future_result<int>>> any)
                       {
                           auto winner = std::get<0>(any.futures).get_result();
                           if (!winner.has_value() && winner.error() == result_error::broken_promise)
                           {
                               return 77;
                           }
                           return -1;
                       });

    promise2.set_value(11);

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 77);
}

/**
 * @brief Tests ComputationChainOnWorkerTask.
 */
TEST(V2Scenarios, ComputationChainOnWorkerTask)
{
    auto context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(context, "v2_chain_worker");

    auto result = worker
                      ->delegate_async_v2(
                          [](const std::shared_ptr<scenario_worker_context>& ctx, const std::string&, int value)
                          {
                              ctx->loop_counter.fetch_add(1);
                              return value * 2;
                          },
                          21)
                      .then_value([](int value)
                      {
                          return value + 1;
                      })
                      .then_value([](int value)
                      {
                          return value * 2;
                      })
                      .get_result();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 86);
    EXPECT_EQ(context->loop_counter.load(), 1);
}

/**
 * @brief Tests GatherSeveralComputationsWithWhenAll.
 */
TEST(V2Scenarios, GatherSeveralComputationsWithWhenAll)
{
    auto context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(context, "v2_fanout_worker");

    std::vector<future_result<int>> jobs;
    jobs.reserve(5);

    for (int value = 1; value <= 5; ++value)
    {
        jobs.emplace_back(
            worker
                ->delegate_async_v2(
                    [](const std::shared_ptr<scenario_worker_context>& ctx, const std::string&, int v)
                    {
                        ctx->loop_counter.fetch_add(1);
                        return v * v;
                    },
                    value)
                .then_value(worker->as_executor(),
                    [](int previous)
                    {
                        return previous + 10;
                    }));
    }

    auto total = when_all(std::move(jobs))
                     .then_value([](std::vector<portable_concurrency::v2::expected<int, result_error>> results)
                     {
                         int sum = 0;
                         for (const auto& item : results)
                         {
                             if (item.has_value())
                             {
                                 sum += item.value();
                             }
                         }
                         return sum;
                     })
                     .get_result();

    ASSERT_TRUE(total.has_value());
    EXPECT_EQ(total.value(), 105);
    EXPECT_EQ(context->loop_counter.load(), 5);
}

/**
 * @brief Tests ChainAcrossDifferentExecutors.
 */
TEST(V2Scenarios, ChainAcrossDifferentExecutors)
{
    auto context_a = std::make_shared<scenario_worker_context>();
    auto context_b = std::make_shared<scenario_worker_context>();

    auto worker_a = make_worker_task(context_a, "v2_worker_a");
    auto worker_b = make_worker_task(context_b, "v2_worker_b");

    auto result = worker_a
                      ->delegate_async_v2(
                          [](const std::shared_ptr<scenario_worker_context>& ctx, const std::string&, int value)
                          {
                              ctx->loop_counter.fetch_add(1);
                              return value * 2;
                          },
                          10)
                      .then_value(worker_b->as_executor(),
                          [ctx = context_b](int previous)
                          {
                              ctx->loop_counter.fetch_add(1);
                              return previous + 7;
                          })
                      .get_result();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 27);
    EXPECT_EQ(context_a->loop_counter.load(), 1);
    EXPECT_EQ(context_b->loop_counter.load(), 1);
}

/**
 * @brief Tests PeriodicTaskPollsLongWorkerComputationReadiness.
 */
TEST(V2Scenarios, PeriodicTaskPollsLongWorkerComputationReadiness)
{
    auto worker_context = std::make_shared<scenario_worker_context>();
    auto worker = make_worker_task(worker_context, "v2_long_compute_worker");

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
    constexpr auto period = std::chrono::duration<std::uint64_t, std::micro>(20000);

    {
        polling_periodic_task poller(
            std::move(startup), std::move(periodic), periodic_context, "v2_future_polling_periodic", period, periodic_stack_size);

        std::this_thread::sleep_for(std::chrono::milliseconds(320));
    }

    auto result = std::move(long_future)
                      .then_error([](result_error)
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

    EXPECT_GT(periodic_context->not_ready_checks.load(), 0);
    EXPECT_GT(periodic_context->ready_checks.load(), 0);
    EXPECT_EQ(worker_context->loop_counter.load(), 1);
}
