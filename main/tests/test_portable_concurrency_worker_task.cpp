//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

/**
 * @file test_portable_concurrency_worker_task.cpp
 * @brief Tests mirroring result-based worker_task examples from main.cpp.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "portable_concurrency/bits/coro.hpp"
#include "portable_concurrency/future.hpp"
#include "tools/periodic_task.hpp"
#include "tools/worker_task.hpp"

#if defined(PC_HAS_COROUTINES) || defined(__cpp_impl_coroutine) || defined(__cpp_coroutines) || (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#define PS_PC_HAS_COROUTINES
#endif

namespace {

struct worker_result_context {
    std::atomic<int> loop_counter { 0 };
};

struct periodic_stress_context {
    std::atomic<int> submitted_jobs { 0 };
    std::atomic<int> completed_jobs { 0 };
    std::atomic<int> failed_jobs { 0 };
    std::atomic<std::int64_t> processed_sum { 0 };
    std::vector<pco::future_result<int>> scheduled_results;
};

class stress_worker_processor {
public:
    int process_sample(int input_value) const
    {
        constexpr int sample_multiplier = 2;
        constexpr int sample_offset = 1;
        return (input_value * sample_multiplier) + sample_offset;
    }

    int process_chain_stage(int input_value, int stage_index) const
    {
        constexpr int stage_weight = 3;
        return input_value + ((stage_index + 1) * stage_weight);
    }
};

using worker_result_task = tools::worker_task<worker_result_context>;
using periodic_stress_task = tools::periodic_task<periodic_stress_context>;

std::unique_ptr<worker_result_task> make_worker_result_task(
    const std::shared_ptr<worker_result_context>& context, const std::string& name)
{
    auto startup = [](const std::shared_ptr<worker_result_context>&, const std::string&) {};
    constexpr std::size_t stack_size = 4096U;
    return std::make_unique<worker_result_task>(std::move(startup), context, name, stack_size);
}

#if defined(PS_PC_HAS_COROUTINES)
pco::future_result<int> coroutine_schedule_job(
    worker_result_task& worker, const std::shared_ptr<worker_result_context>& context, int value)
{
    co_await worker.schedule();
    context->loop_counter.fetch_add(1);
    co_return value * 3;
}

pco::future_result<int> coroutine_await_future_job(
    worker_result_task& worker,
    const std::shared_ptr<worker_result_context>& context,
    pco::future_result<int> upstream)
{
    co_await worker.schedule();
    const auto upstream_result = co_await upstream;
    if (!upstream_result.has_value())
    {
        co_return -1;
    }

    const int upstream_value = upstream_result.value();
    context->loop_counter.fetch_add(1);
    co_return upstream_value + 5;
}

pco::future_result<int> coroutine_process_sample_on_worker(
    worker_result_task& worker,
    const std::shared_ptr<worker_result_context>& context,
    const std::shared_ptr<stress_worker_processor>& processor,
    int input_value)
{
    co_await worker.schedule();
    context->loop_counter.fetch_add(1);
    co_return processor->process_sample(input_value);
}

pco::future_result<int> coroutine_alternating_chain_on_two_workers(worker_result_task& worker_first,
    worker_result_task& worker_second,
    const std::shared_ptr<worker_result_context>& context_first,
    const std::shared_ptr<worker_result_context>& context_second,
    const std::shared_ptr<stress_worker_processor>& processor,
    int input_value,
    int stage_count)
{
    int chain_value = input_value;
    for (int stage_index = 0; stage_index < stage_count; ++stage_index)
    {
        if ((stage_index % 2) == 0)
        {
            co_await worker_first.schedule();
            context_first->loop_counter.fetch_add(1);
        }
        else
        {
            co_await worker_second.schedule();
            context_second->loop_counter.fetch_add(1);
        }

        chain_value = processor->process_chain_stage(chain_value, stage_index);
    }

    co_return chain_value;
}
#endif

} // namespace

TEST(PortableConcurrencyWorkerTaskResultTest, AsyncResultChainOnWorkerExecutor)
{
    auto context = std::make_shared<worker_result_context>();
    auto worker = make_worker_result_task(context, "chain_worker_test");

    auto future = worker->delegate_async(
        [](const std::shared_ptr<worker_result_context>& ctx, const std::string&, int value)
        {
            ctx->loop_counter.fetch_add(1);
            return value * 7;
        },
        6);

    auto chained = std::move(future)
                       .then_value([](int value)
                       {
                           return value + 2;
                       })
                       .then_error([](pco::result_error)
                       {
                           return -1;
                       });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 44);
    EXPECT_EQ(context->loop_counter.load(), 1);
}

TEST(PortableConcurrencyWorkerTaskResultTest, GatherComputationsWithWorkerExecutor)
{
    auto context = std::make_shared<worker_result_context>();
    auto worker = make_worker_result_task(context, "gather_worker_test");

    std::vector<pco::future_result<int>> jobs;
    jobs.reserve(5);

    for (int value = 1; value <= 5; ++value)
    {
        jobs.emplace_back(worker->delegate_async(
            [](const std::shared_ptr<worker_result_context>& ctx, const std::string&, int input)
            {
                ctx->loop_counter.fetch_add(1);
                return input * input + 10;
            },
            value));
    }

    int total = 0;
    for (auto& job : jobs)
    {
        auto result = job.get_result();
        ASSERT_TRUE(result.has_value());
        total += result.value();
    }

    EXPECT_EQ(total, 105);
    EXPECT_EQ(context->loop_counter.load(), 5);
}

TEST(PortableConcurrencyWorkerTaskResultTest, PromiseResultManualFulfillment)
{
    auto context = std::make_shared<worker_result_context>();
    auto worker = make_worker_result_task(context, "promise_worker_test");

    auto pair = pco::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    auto shared_promise = std::make_shared<decltype(promise)>(std::move(promise));
    worker->delegate(
        [shared_promise](const std::shared_ptr<worker_result_context>& ctx, const std::string&)
        {
            ctx->loop_counter.fetch_add(1);
            shared_promise->set_value(99);
        });

    auto chained = std::move(future).then_value([](int value)
    {
        return value * 2;
    });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 198);
    EXPECT_EQ(context->loop_counter.load(), 1);
}

TEST(PortableConcurrencyWorkerTaskResultTest, PeriodicTaskFeedsWorkerStressWithoutCoroutines)
{

    constexpr int target_job_count = 80;
    constexpr std::size_t periodic_stack_size = 4096U;
    constexpr auto periodic_interval = std::chrono::milliseconds(2);
    constexpr auto wait_poll_interval = std::chrono::milliseconds(5);
    constexpr auto completion_timeout = std::chrono::seconds(5);

    auto worker_context = std::make_shared<worker_result_context>();
    auto worker = make_worker_result_task(worker_context, "periodic_stress_worker");
    auto stress_context = std::make_shared<periodic_stress_context>();
    auto processor = std::make_shared<stress_worker_processor>();
    stress_context->scheduled_results.reserve(static_cast<std::size_t>(target_job_count));

    auto startup = [](const std::shared_ptr<periodic_stress_context>&, const std::string&) {};
    auto* worker_ptr = worker.get();

    auto periodic = [worker_ptr, worker_context, processor](
                        const std::shared_ptr<periodic_stress_context>& local_context,
                        const std::string&)
    {
        int submitted_index = local_context->submitted_jobs.load();
        const int target_value = target_job_count;
        while ((submitted_index < target_value)
            && !local_context->submitted_jobs.compare_exchange_weak(submitted_index, submitted_index + 1))
        {
        }

        if (submitted_index >= target_value)
        {
            return;
        }

        auto chained = worker_ptr
                           ->delegate_async(
                               [processor](const std::shared_ptr<worker_result_context>& worker_ctx,
                                   const std::string&,
                                   int input_value)
                               {
                                   worker_ctx->loop_counter.fetch_add(1);
                                   return processor->process_sample(input_value);
                               },
                               submitted_index)
                           .then_value([local_context](int processed_value)
                           {
                               local_context->completed_jobs.fetch_add(1);
                               local_context->processed_sum.fetch_add(processed_value);
                               return processed_value;
                           })
                           .then_error([local_context](pco::result_error)
                           {
                               local_context->failed_jobs.fetch_add(1);
                               return -1;
                           });

        local_context->scheduled_results.emplace_back(std::move(chained));
    };

    {
        periodic_stress_task producer(
            std::move(startup), std::move(periodic), stress_context, "periodic_stress_producer", periodic_interval, periodic_stack_size);

        const auto deadline = std::chrono::steady_clock::now() + completion_timeout;
        while ((std::chrono::steady_clock::now() < deadline)
            && ((stress_context->completed_jobs.load() < target_job_count) || (stress_context->failed_jobs.load() > 0)))
        {
            if (stress_context->failed_jobs.load() > 0)
            {
                break;
            }
            std::this_thread::sleep_for(wait_poll_interval);
        }
    }

    ASSERT_EQ(stress_context->submitted_jobs.load(), target_job_count);
    ASSERT_EQ(stress_context->failed_jobs.load(), 0);
    ASSERT_EQ(stress_context->completed_jobs.load(), target_job_count);
    ASSERT_EQ(static_cast<int>(stress_context->scheduled_results.size()), target_job_count);

    for (auto& scheduled_future : stress_context->scheduled_results)
    {
        const auto result = scheduled_future.get_result();
        ASSERT_TRUE(result.has_value());
        ASSERT_GT(result.value(), 0);
    }

    const std::int64_t expected_sum
        = static_cast<std::int64_t>(target_job_count) * static_cast<std::int64_t>(target_job_count);
    EXPECT_EQ(stress_context->processed_sum.load(), expected_sum);
    EXPECT_EQ(worker_context->loop_counter.load(), target_job_count);
}

TEST(PortableConcurrencyWorkerTaskResultTest, AlternatingChainAcrossTwoWorkersWithoutCoroutines)
{

    constexpr int initial_value = 10;
    constexpr int stage_count = 6;

    auto context_first = std::make_shared<worker_result_context>();
    auto context_second = std::make_shared<worker_result_context>();
    auto worker_first = make_worker_result_task(context_first, "alternating_chain_worker_a");
    auto worker_second = make_worker_result_task(context_second, "alternating_chain_worker_b");
    auto processor = std::make_shared<stress_worker_processor>();

    int chain_value = initial_value;
    for (int stage_index = 0; stage_index < stage_count; ++stage_index)
    {
        auto* active_worker = ((stage_index % 2) == 0) ? worker_first.get() : worker_second.get();
        auto active_context = ((stage_index % 2) == 0) ? context_first : context_second;

        auto stage_future = active_worker->delegate_async(
            [processor](const std::shared_ptr<worker_result_context>& local_context,
                const std::string&,
                int input_chain_value,
                int current_stage)
            {
                local_context->loop_counter.fetch_add(1);
                return processor->process_chain_stage(input_chain_value, current_stage);
            },
            chain_value,
            stage_index);

        const auto stage_result = stage_future.get_result();
        ASSERT_TRUE(stage_result.has_value());
        chain_value = stage_result.value();
        ASSERT_GT(active_context->loop_counter.load(), 0);
    }

    int expected_value = initial_value;
    for (int stage_index = 0; stage_index < stage_count; ++stage_index)
    {
        expected_value = processor->process_chain_stage(expected_value, stage_index);
    }

    constexpr int expected_calls_per_worker = stage_count / 2;
    EXPECT_EQ(chain_value, expected_value);
    EXPECT_EQ(context_first->loop_counter.load(), expected_calls_per_worker);
    EXPECT_EQ(context_second->loop_counter.load(), expected_calls_per_worker);
}

#if defined(PS_PC_HAS_COROUTINES)
TEST(PortableConcurrencyWorkerTaskResultTest, CoroutineScheduleFlow)
{
    auto context = std::make_shared<worker_result_context>();
    auto worker = make_worker_result_task(context, "coro_schedule_test");

    auto scheduled = coroutine_schedule_job(*worker, context, 5).then_value([](int value)
    {
        return value + 4;
    });

    auto result = scheduled.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 19);
    EXPECT_EQ(context->loop_counter.load(), 1);
}

TEST(PortableConcurrencyWorkerTaskResultTest, CoroutineAwaitsFutureResult)
{
    auto context = std::make_shared<worker_result_context>();
    auto worker = make_worker_result_task(context, "coro_await_test");

    auto upstream = worker->delegate_async(
        [](const std::shared_ptr<worker_result_context>& ctx, const std::string&, int value)
        {
            ctx->loop_counter.fetch_add(1);
            return value * 4;
        },
        5);

    auto awaited = coroutine_await_future_job(*worker, context, std::move(upstream));

    auto result = awaited.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 25);
    EXPECT_EQ(context->loop_counter.load(), 2);
}

TEST(PortableConcurrencyWorkerTaskResultTest, PeriodicTaskFeedsWorkerStressWithCoroutines)
{

    constexpr int target_job_count = 80;
    constexpr std::size_t periodic_stack_size = 4096U;
    constexpr auto periodic_interval = std::chrono::milliseconds(2);
    constexpr auto wait_poll_interval = std::chrono::milliseconds(5);
    constexpr auto completion_timeout = std::chrono::seconds(5);

    auto worker_context = std::make_shared<worker_result_context>();
    auto worker = make_worker_result_task(worker_context, "periodic_coro_stress_worker");
    auto stress_context = std::make_shared<periodic_stress_context>();
    auto processor = std::make_shared<stress_worker_processor>();
    stress_context->scheduled_results.reserve(static_cast<std::size_t>(target_job_count));

    auto startup = [](const std::shared_ptr<periodic_stress_context>&, const std::string&) {};
    auto* worker_ptr = worker.get();

    auto periodic = [worker_ptr, worker_context, processor](
                        const std::shared_ptr<periodic_stress_context>& local_context,
                        const std::string&)
    {
        int submitted_index = local_context->submitted_jobs.load();
        const int target_value = target_job_count;
        while ((submitted_index < target_value)
            && !local_context->submitted_jobs.compare_exchange_weak(submitted_index, submitted_index + 1))
        {
        }

        if (submitted_index >= target_value)
        {
            return;
        }

        auto chained = coroutine_process_sample_on_worker(*worker_ptr, worker_context, processor, submitted_index)
                           .then_value([local_context](int processed_value)
                           {
                               local_context->completed_jobs.fetch_add(1);
                               local_context->processed_sum.fetch_add(processed_value);
                               return processed_value;
                           })
                           .then_error([local_context](pco::result_error)
                           {
                               local_context->failed_jobs.fetch_add(1);
                               return -1;
                           });

        local_context->scheduled_results.emplace_back(std::move(chained));
    };

    {
        periodic_stress_task producer(std::move(startup),
            std::move(periodic),
            stress_context,
            "periodic_coro_stress_producer",
            periodic_interval,
            periodic_stack_size);

        const auto deadline = std::chrono::steady_clock::now() + completion_timeout;
        while ((std::chrono::steady_clock::now() < deadline)
            && ((stress_context->completed_jobs.load() < target_job_count) || (stress_context->failed_jobs.load() > 0)))
        {
            if (stress_context->failed_jobs.load() > 0)
            {
                break;
            }
            std::this_thread::sleep_for(wait_poll_interval);
        }
    }

    ASSERT_EQ(stress_context->submitted_jobs.load(), target_job_count);
    ASSERT_EQ(stress_context->failed_jobs.load(), 0);
    ASSERT_EQ(stress_context->completed_jobs.load(), target_job_count);
    ASSERT_EQ(static_cast<int>(stress_context->scheduled_results.size()), target_job_count);

    for (auto& scheduled_future : stress_context->scheduled_results)
    {
        const auto result = scheduled_future.get_result();
        ASSERT_TRUE(result.has_value());
        ASSERT_GT(result.value(), 0);
    }

    const std::int64_t expected_sum
        = static_cast<std::int64_t>(target_job_count) * static_cast<std::int64_t>(target_job_count);
    EXPECT_EQ(stress_context->processed_sum.load(), expected_sum);
    EXPECT_EQ(worker_context->loop_counter.load(), target_job_count);
}

TEST(PortableConcurrencyWorkerTaskResultTest, AlternatingChainAcrossTwoWorkersWithCoroutines)
{

    constexpr int initial_value = 10;
    constexpr int stage_count = 6;

    auto context_first = std::make_shared<worker_result_context>();
    auto context_second = std::make_shared<worker_result_context>();
    auto worker_first = make_worker_result_task(context_first, "alternating_coro_chain_worker_a");
    auto worker_second = make_worker_result_task(context_second, "alternating_coro_chain_worker_b");
    auto processor = std::make_shared<stress_worker_processor>();

    auto chained_future = coroutine_alternating_chain_on_two_workers(
        *worker_first, *worker_second, context_first, context_second, processor, initial_value, stage_count);
    const auto chained_result = chained_future.get_result();
    ASSERT_TRUE(chained_result.has_value());

    int expected_value = initial_value;
    for (int stage_index = 0; stage_index < stage_count; ++stage_index)
    {
        expected_value = processor->process_chain_stage(expected_value, stage_index);
    }

    constexpr int expected_calls_per_worker = stage_count / 2;
    EXPECT_EQ(chained_result.value(), expected_value);
    EXPECT_EQ(context_first->loop_counter.load(), expected_calls_per_worker);
    EXPECT_EQ(context_second->loop_counter.load(), expected_calls_per_worker);
}
#endif
