//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

/**
 * @file test_portable_concurrency_v2_worker_task.cpp
 * @brief Tests mirroring v2 worker_task examples from main.cpp.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "portable_concurrency/bits/coro.h"
#include "portable_concurrency/p_future_v2.hpp"
#include "tools/worker_task.hpp"

#if defined(PC_HAS_COROUTINES) || defined(__cpp_impl_coroutine) || defined(__cpp_coroutines) || (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#define PS_PC_HAS_COROUTINES
#endif

namespace {

struct worker_v2_context {
    std::atomic<int> loop_counter { 0 };
};

using worker_v2_task = tools::worker_task<worker_v2_context>;

std::unique_ptr<worker_v2_task> make_worker_v2_task(
    const std::shared_ptr<worker_v2_context>& context, const std::string& name)
{
    auto startup = [](const std::shared_ptr<worker_v2_context>&, const std::string&) {};
    constexpr std::size_t stack_size = 4096U;
    return std::make_unique<worker_v2_task>(std::move(startup), context, name, stack_size);
}

#if defined(PS_PC_HAS_COROUTINES) && defined(WORKER_TASK_HAS_PC_ASYNC)
portable_concurrency::v2::future_result<int> coroutine_v2_schedule_job(
    worker_v2_task& worker, const std::shared_ptr<worker_v2_context>& context, int value)
{
    co_await worker.schedule();
    context->loop_counter.fetch_add(1);
    co_return value * 3;
}

portable_concurrency::v2::future_result<int> coroutine_v2_await_future_job(
    worker_v2_task& worker,
    const std::shared_ptr<worker_v2_context>& context,
    portable_concurrency::v2::future_result<int> upstream)
{
    co_await worker.schedule();
    const int upstream_value = co_await upstream;
    context->loop_counter.fetch_add(1);
    co_return upstream_value + 5;
}
#endif

} // namespace

TEST(PortableConcurrencyV2WorkerTaskTest, AsyncResultChainOnWorkerExecutor)
{
    auto context = std::make_shared<worker_v2_context>();
    auto worker = make_worker_v2_task(context, "v2_chain_worker_test");

    auto future = worker->delegate_async_v2(
        [](const std::shared_ptr<worker_v2_context>& ctx, const std::string&, int value)
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
                       .then_error([](portable_concurrency::v2::result_error)
                       {
                           return -1;
                       });

    auto result = chained.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 44);
    EXPECT_EQ(context->loop_counter.load(), 1);
}

TEST(PortableConcurrencyV2WorkerTaskTest, GatherComputationsWithWorkerExecutor)
{
    auto context = std::make_shared<worker_v2_context>();
    auto worker = make_worker_v2_task(context, "v2_gather_worker_test");

    std::vector<portable_concurrency::v2::future_result<int>> jobs;
    jobs.reserve(5);

    for (int value = 1; value <= 5; ++value)
    {
        jobs.emplace_back(worker->delegate_async_v2(
            [](const std::shared_ptr<worker_v2_context>& ctx, const std::string&, int input)
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

TEST(PortableConcurrencyV2WorkerTaskTest, PromiseResultManualFulfillment)
{
    auto context = std::make_shared<worker_v2_context>();
    auto worker = make_worker_v2_task(context, "v2_promise_worker_test");

    auto pair = portable_concurrency::v2::make_result_promise<int>();
    auto promise = std::move(pair.first);
    auto future = std::move(pair.second);

    auto shared_promise = std::make_shared<decltype(promise)>(std::move(promise));
    worker->delegate(
        [shared_promise](const std::shared_ptr<worker_v2_context>& ctx, const std::string&)
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

#if defined(PS_PC_HAS_COROUTINES) && defined(WORKER_TASK_HAS_PC_ASYNC)
TEST(PortableConcurrencyV2WorkerTaskTest, CoroutineScheduleFlow)
{
    auto context = std::make_shared<worker_v2_context>();
    auto worker = make_worker_v2_task(context, "v2_coro_schedule_test");

    auto scheduled = coroutine_v2_schedule_job(*worker, context, 5).then_value([](int value)
    {
        return value + 4;
    });

    auto result = scheduled.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 19);
    EXPECT_EQ(context->loop_counter.load(), 1);
}

TEST(PortableConcurrencyV2WorkerTaskTest, CoroutineAwaitsFutureResult)
{
    auto context = std::make_shared<worker_v2_context>();
    auto worker = make_worker_v2_task(context, "v2_coro_await_test");

    auto upstream = worker->delegate_async_v2(
        [](const std::shared_ptr<worker_v2_context>& ctx, const std::string&, int value)
        {
            ctx->loop_counter.fetch_add(1);
            return value * 4;
        },
        5);

    auto awaited = coroutine_v2_await_future_job(*worker, context, std::move(upstream));

    auto result = awaited.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 25);
    EXPECT_EQ(context->loop_counter.load(), 2);
}
#endif
