//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//                                                                             //
// This software is provided 'as-is', without any express or implied           //
// warranty.In no event will the authors be held liable for any damages        //
// arising from the use of this software.                                      //
//                                                                             //
// Permission is granted to anyone to use this software for any purpose,       //
// including commercial applications, and to alter itand redistribute it       //
// freely, subject to the following restrictions :                             //
//                                                                             //
// 1. The origin of this software must not be misrepresented; you must not     //
// claim that you wrote the original software.If you use this software         //
// in a product, an acknowledgment in the product documentation would be       //
// appreciated but is not required.                                            //
// 2. Altered source versions must be plainly marked as such, and must not be  //
// misrepresented as being the original software.                              //
// 3. This notice may not be removed or altered from any source distribution.  //
//-----------------------------------------------------------------------------//

/**
 * @file example_async_processing.cpp
 * @brief Demonstrates the portable_concurrency v2 result-based async API.
 *
 * Covers: future_result, promise_result, packaged_task_result, async_result,
 * then_value, then_error, then_result, when_all, make_result_promise,
 * delegate_async_v2, and — when coroutines are available — co_await on
 * future_result and worker_task::schedule().
 *
 * @author Laurent Lardinois
 * @date 2026-05-25
 */

#include "example_common.hpp"
#include "examples.hpp"

#include <functional>
#include <numeric>

#include "portable_concurrency/bits/coro.h"
#include "portable_concurrency/p_execution.hpp"
#include "portable_concurrency/p_future_v2.hpp"

#if defined(PC_HAS_COROUTINES) || defined(__cpp_impl_coroutine) || defined(__cpp_coroutines) \
    || (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#define ASYNC_EXAMPLE_HAS_COROUTINES
#endif

namespace
{
    // -----------------------------------------------------------------------
    // Shared worker context

    /** @brief Shared context carried by every worker task in this example. */
    struct async_context
    {
        std::atomic<int> call_count { 0 };
    };

    using async_worker = tools::worker_task<async_context>;

    /** @brief Factory that creates a named @c async_worker with a minimal stack. */
    std::unique_ptr<async_worker> make_async_worker(
        const std::shared_ptr<async_context>& context, const std::string& name)
    {
        constexpr std::size_t stack_size = 4096U;
        auto startup = [](const std::shared_ptr<async_context>&, const std::string&) {};
        return std::make_unique<async_worker>(std::move(startup), context, name, stack_size);
    }

    // -----------------------------------------------------------------------
    // v2 coroutine helpers — compiled only when coroutine support is present

#if defined(ASYNC_EXAMPLE_HAS_COROUTINES) && defined(WORKER_TASK_HAS_PC_ASYNC)
    /**
     * @brief Coroutine that hops onto @p worker, increments the call counter, and
     *        returns @p input scaled by three.
     */
    portable_concurrency::v2::future_result<int> schedule_and_compute(
        async_worker& worker, const std::shared_ptr<async_context>& context, int input)
    {
        co_await worker.schedule();
        context->call_count.fetch_add(1);
        co_return input * 3;
    }

    /**
     * @brief Coroutine that hops onto @p worker, awaits @p upstream, and
     *        adds an offset to the resolved value.
     */
    portable_concurrency::v2::future_result<int> schedule_and_await(
        async_worker& worker,
        const std::shared_ptr<async_context>& context,
        portable_concurrency::v2::future_result<int> upstream,
        int offset)
    {
        co_await worker.schedule();
        const int base = co_await upstream;
        context->call_count.fetch_add(1);
        co_return base + offset;
    }
#endif // ASYNC_EXAMPLE_HAS_COROUTINES && WORKER_TASK_HAS_PC_ASYNC

    // -----------------------------------------------------------------------
    // Example 1: async_result with inplace_executor + then_value / then_error

    /**
     * @brief Shows a basic v2 chain: inline computation, value transformation,
     *        and an error recovery fallback.
     */
    void test_async_result_chain()
    {
        LOG_INFO("-- v2: async_result + then_value + then_error --");
        print_stats();

        using namespace portable_concurrency::v2;

        constexpr int input_value = 7;

        auto future = async_result(
            portable_concurrency::inplace_executor,
            [](int squared_input)
            {
                return squared_input * squared_input;
            },
            input_value);

        const auto result = std::move(future)
                                .then_value([](int squared) { return squared + 1; })
                                .then_error([](result_error) { return -1; })
                                .get_result();

        if (result.has_value())
        {
            std::printf("async_result chain: 7^2 + 1 = %d\n", result.value());
        }
    }

    // -----------------------------------------------------------------------
    // Example 2: then_result to inspect the full expected<T,E>

    /**
     * @brief Demonstrates @c then_result, which receives the full
     *        @c expected<T,E> and can branch on value or error.
     */
    void test_then_result_inspection()
    {
        LOG_INFO("-- v2: then_result full expected inspection --");
        print_stats();

        using namespace portable_concurrency::v2;

        constexpr int offset_value = 10;
        constexpr int input_base = 5;

        auto future = async_result(
            portable_concurrency::inplace_executor,
            [](int input_val) { return input_val + offset_value; },
            input_base);

        const auto description = std::move(future)
                                     .then_result(
                                         [](const expected<int, result_error>& res) -> std::string
                                         {
                                             if (res.has_value())
                                             {
                                                 return "value=" + std::to_string(res.value());
                                             }
                                             return "error";
                                         })
                                     .get_result();

        if (description.has_value())
        {
            std::printf("then_result inspection: %s\n", description.value().c_str());
        }
    }

    // -----------------------------------------------------------------------
    // Example 3: error recovery — execution_failure caught by then_error

    /**
     * @brief Shows that @c then_error can intercept @c result_error::execution_failure
     *        and recover with a default value.
     */
    void test_error_recovery()
    {
        LOG_INFO("-- v2: error recovery via then_error --");
        print_stats();

        using namespace portable_concurrency::v2;

        // A broken_promise scenario: discard the promise without fulfilling it.
        auto pair = make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto future = std::move(pair.second);

        // Let the promise go out of scope — future sees broken_promise.
        promise = promise_result<int> {};

        const auto recovered = std::move(future)
                                   .then_error(
                                       [](result_error err) -> int
                                       {
                                           if (err == result_error::broken_promise)
                                           {
                                               std::printf("then_error caught: broken_promise — recovering with 0\n");
                                           }
                                           return 0;
                                       })
                                   .get_result();

        std::printf("error_recovery result: %d\n", recovered.has_value() ? recovered.value() : -1);
    }

    // -----------------------------------------------------------------------
    // Example 4: packaged_task_result — pre-packaged callable

    /**
     * @brief Demonstrates @c packaged_task_result: package a callable once,
     *        then invoke it to obtain a @c future_result.
     */
    void test_packaged_task()
    {
        LOG_INFO("-- v2: packaged_task_result --");
        print_stats();

        using namespace portable_concurrency::v2;

        constexpr int first_arg = 30;
        constexpr int second_arg = 12;

        packaged_task_result<int(int, int)> task { [](int arg_a, int arg_b) { return arg_a + arg_b; } };
        auto future = task.get_future();
        task(first_arg, second_arg);

        const auto result = future.get_result();
        if (result.has_value())
        {
            std::printf("packaged_task_result: 30 + 12 = %d\n", result.value());
        }
    }

    // -----------------------------------------------------------------------
    // Example 5: packaged_task_result with std::reference_wrapper return

    /**
     * @brief Shows the @c std::reference_wrapper workaround for returning a
     *        reference from @c packaged_task_result (direct reference types are
     *        prohibited by the v2 static_assert).
     */
    void test_packaged_task_reference_wrapper()
    {
        LOG_INFO("-- v2: packaged_task_result with reference_wrapper --");
        print_stats();

        using namespace portable_concurrency::v2;

        constexpr int initial_value = 42;
        constexpr int increment_amount = 8;
        constexpr int expected_result = 50;

        int value = initial_value;

        packaged_task_result<std::reference_wrapper<int>()> task { [&value]()
            {
                return std::ref(value);
            } };
        auto future = task.get_future();
        task();
        const auto result = future.get_result();

        if (result.has_value())
        {
            // Modifying through the returned reference_wrapper affects the original.
            result.value().get() += increment_amount;
            std::printf("reference_wrapper result: original value is now %d (expected %d)\n", value, expected_result);
        }
    }

    // -----------------------------------------------------------------------
    // Example 6: manual promise_result fulfilled by a worker

    /**
     * @brief Creates a @c promise_result / @c future_result pair, fulfils the
     *        promise from a worker thread, and chains a transformation.
     */
    void test_manual_promise_fulfillment()
    {
        LOG_INFO("-- v2: manual promise_result fulfilled from worker --");
        print_stats();

        using namespace portable_concurrency::v2;

        constexpr int promise_value = 77;
        constexpr int multiplication_factor = 2;

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "promise_fulfillment_worker");

        auto pair = make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto future = std::move(pair.second);

        // Wrap in shared_ptr so the worker lambda can capture it safely.
        auto shared_promise = std::make_shared<promise_result<int>>(std::move(promise));

        worker->delegate(
            [shared_promise](const std::shared_ptr<async_context>& ctx, const std::string&)
            {
                ctx->call_count.fetch_add(1);
                shared_promise->set_value(promise_value);
            });

        const auto result = std::move(future)
                                .then_value([](int val) { return val * multiplication_factor; })
                                .get_result();

        if (result.has_value())
        {
            std::printf("manual promise: 77 * 2 = %d\n", result.value());
        }
    }

    // -----------------------------------------------------------------------
    // Example 7: delegate_async_v2 chain on a worker

    /**
     * @brief Dispatches work to a worker via @c delegate_async_v2, then chains
     *        @c then_value and @c then_error on the returned @c future_result.
     */
    void test_delegate_async_v2_chain()
    {
        LOG_INFO("-- v2: delegate_async_v2 chain --");
        print_stats();

        using namespace portable_concurrency::v2;

        constexpr int multiplier = 6;
        constexpr int addition_offset = 2;
        constexpr int input_val = 7;

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "v2_chain_worker");

        auto future = worker->delegate_async_v2(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int value_input)
            {
                ctx->call_count.fetch_add(1);
                return value_input * multiplier;
            },
            input_val);

        const auto result = std::move(future)
                                .then_value([](int val) { return val + addition_offset; })
                                .then_error([](result_error) { return -1; })
                                .get_result();

        if (result.has_value())
        {
            std::printf("delegate_async_v2 chain: 7*6+2 = %d\n", result.value());
        }
        std::printf("worker call_count: %d\n", context->call_count.load());
    }

    // -----------------------------------------------------------------------
    // Example 8: fan-out / fan-in gather with a loop of delegate_async_v2

    /**
     * @brief Dispatches several independent jobs in a loop and accumulates
     *        their results after all have completed.
     */
    void test_fan_out_gather()
    {
        LOG_INFO("-- v2: fan-out / fan-in gather --");
        print_stats();

        using namespace portable_concurrency::v2;

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "v2_gather_worker");

        constexpr int job_count = 5;
        std::vector<future_result<int>> jobs;
        jobs.reserve(static_cast<std::size_t>(job_count));

        for (int i = 1; i <= job_count; ++i)
        {
            jobs.emplace_back(worker->delegate_async_v2(
                [](const std::shared_ptr<async_context>& ctx, const std::string&, int input)
                {
                    ctx->call_count.fetch_add(1);
                    return input * input; // i^2
                },
                i));
        }

        int total = 0;
        for (auto& job : jobs)
        {
            const auto result = job.get_result();
            if (result.has_value())
            {
                total += result.value();
            }
        }

        // 1^2 + 2^2 + 3^2 + 4^2 + 5^2 = 55
        std::printf("fan-out gather sum of squares 1..5 = %d (expected 55)\n", total);
        std::printf("worker call_count: %d\n", context->call_count.load());
    }

    // -----------------------------------------------------------------------
    // Example 9: when_all gather two parallel delegate_async_v2 results

    /**
     * @brief Demonstrates @c when_all on a vector of @c future_result values,
     *        collecting two parallel worker results into a single future.
     */
    void test_when_all_gather()
    {
        LOG_INFO("-- v2: when_all gather --");
        print_stats();

        using namespace portable_concurrency::v2;

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "v2_when_all_worker");

        constexpr int triple_multiplier = 3;
        constexpr int first_input = 10;
        constexpr int offset_val = 7;
        constexpr int second_input = 20;

        auto future_a = worker->delegate_async_v2(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int val_a)
            {
                ctx->call_count.fetch_add(1);
                return val_a * triple_multiplier;
            },
            first_input);

        auto future_b = worker->delegate_async_v2(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int val_b)
            {
                ctx->call_count.fetch_add(1);
                return val_b + offset_val;
            },
            second_input);

        std::vector<future_result<int>> futures;
        futures.reserve(2U);
        futures.push_back(std::move(future_a));
        futures.push_back(std::move(future_b));

        using expected_int = tools::expected<int, result_error>;

        auto combined = when_all(std::move(futures)).then_value(
            [](const std::vector<expected_int>& results)
            {
                int sum = 0;
                for (const auto& res : results)
                {
                    if (res.has_value())
                    {
                        sum += res.value();
                    }
                }
                return sum;
            });

        const auto result = combined.get_result();
        if (result.has_value())
        {
            // 10*3=30, 20+7=27, total=57
            std::printf("when_all gather: 10*3 + (20+7) = %d (expected 57)\n", result.value());
        }
        std::printf("worker call_count: %d\n", context->call_count.load());
    }

    // -----------------------------------------------------------------------
    // Example 10: coroutine schedule() + co_await future_result

#if defined(ASYNC_EXAMPLE_HAS_COROUTINES) && defined(WORKER_TASK_HAS_PC_ASYNC)
    /**
     * @brief Shows a coroutine that hops onto a worker via @c schedule(), performs
     *        a computation, then awaits an upstream @c future_result.
     */
    void test_coroutine_schedule_and_await()
    {
        LOG_INFO("-- v2: coroutine schedule() + co_await future_result --");
        print_stats();

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "v2_coro_worker");

        // Fire an upstream async job that the second coroutine will await.
        auto upstream = worker->delegate_async_v2(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int v)
            {
                ctx->call_count.fetch_add(1);
                return v * 4;
            },
            5);

        // schedule_and_compute hops onto the worker, increments the counter, returns input * 3.
        auto scheduled = schedule_and_compute(*worker, context, 8)
                             .then_value([](int v) { return v + 1; });

        // schedule_and_await hops onto the worker, awaits upstream (5*4=20), adds offset 3.
        auto awaited = schedule_and_await(*worker, context, std::move(upstream), 3);

        const auto result_a = scheduled.get_result();
        const auto result_b = awaited.get_result();

        if (result_a.has_value())
        {
            std::printf("coro schedule: 8*3+1 = %d (expected 25)\n", result_a.value());
        }
        if (result_b.has_value())
        {
            std::printf("coro await: 5*4+3 = %d (expected 23)\n", result_b.value());
        }
        std::printf("worker call_count: %d\n", context->call_count.load());
    }
#endif // ASYNC_EXAMPLE_HAS_COROUTINES && WORKER_TASK_HAS_PC_ASYNC

    // -----------------------------------------------------------------------
    // Example 11: timeout with wait_for()

    /**
     * @brief Demonstrates using @c wait_for() on a @c future_result to detect
     *        when an async operation exceeds its time budget.
     *
     * In this example, the slow task completes before the timeout, so we see
     * the result; if it didn't, @c is_ready() after @c wait_for() would return false.
     */
    void test_timeout_detection()
    {
        LOG_INFO("-- v2: timeout detection with wait_for() --");
        print_stats();

        using namespace portable_concurrency::v2;
        using namespace std::chrono_literals;

        constexpr int base_value = 50;
        constexpr auto timeout_duration = 100ms;

        auto future = async_result(
            portable_concurrency::inplace_executor,
            [](int input_base)
            {
                // Simulate a fast computation: just multiply.
                return input_base * 2;
            },
            base_value);

        // Wait up to 100 milliseconds for the result.
        const auto status = future.wait_for(timeout_duration);

        std::string status_str;
        if (status == portable_concurrency::future_status::ready)
        {
            status_str = "ready";
            const auto result = future.get_result();
            if (result.has_value())
            {
                std::printf("timeout_detection: computation ready in time, result=%d\n", result.value());
            }
        }
        else if (status == portable_concurrency::future_status::timeout)
        {
            status_str = "timeout";
            std::printf("timeout_detection: computation did NOT complete in 100ms\n");
        }
        else
        {
            status_str = "deferred";
        }

        std::printf("wait_for() returned: %s\n", status_str.c_str());
    }

    // -----------------------------------------------------------------------
    // Example 12: cancellation with cleanup action on timeout

    /**
     * @brief Demonstrates creating a promise with a custom cancellation action
     *        that fires when the promise is destroyed (simulating a timeout-triggered
     *        cleanup).
     *
     * The pattern: create a promise with a cancel action, spawn slow async work,
     * wait with timeout, and if timeout occurs, destroy the promise to trigger cleanup.
     */
    void test_timeout_and_cancellation()
    {
        LOG_INFO("-- v2: timeout and cancellation with cleanup action --");
        print_stats();

        using namespace portable_concurrency::v2;
        using namespace std::chrono_literals;

        constexpr auto cancel_timeout_duration = 10ms;
        constexpr auto cleanup_delay = 5ms;

        std::atomic<bool> cleanup_called { false };

        {
            // Create a promise with a custom cancellation/cleanup action.
            // The lambda captures cleanup_called by reference; when the promise
            // is destroyed, the lambda fires.
            auto pair = make_result_promise<int>(
                portable_concurrency::canceler_arg,
                [&cleanup_called]()
                {
                    std::printf("cancellation_action fired: cleanup logic triggered\n");
                    cleanup_called.store(true);
                });

            auto promise = std::move(pair.first);
            auto future = std::move(pair.second);

            // Simulate: we could spawn a slow task on a worker and give it the promise.
            // For this example, we just let the promise sit unfulfilled.

            // Wait a short time for completion.
            const auto status = future.wait_for(cancel_timeout_duration);

            if (status == portable_concurrency::future_status::timeout)
            {
                std::printf("timeout detected: destroying promise to trigger cancellation\n");
                // Destroying the promise (via moving into scope-local, then out of scope)
                // triggers the cleanup action.
                promise = promise_result<int> {};
            }

            // give cleanup action a moment to fire (if it's async)
            std::this_thread::sleep_for(cleanup_delay);
        }

        if (cleanup_called.load())
        {
            std::printf("timeout_and_cancellation: cancellation action confirmed executed\n");
        }
        else
        {
            std::printf("timeout_and_cancellation: no cancellation detected\n");
        }
    }

} // namespace

void run_example_async_processing()
{
    LOG_INFO("=== async processing (portable_concurrency v2) ===");

    test_async_result_chain();
    test_then_result_inspection();
    test_error_recovery();
    test_packaged_task();
    test_packaged_task_reference_wrapper();
    test_manual_promise_fulfillment();
    test_delegate_async_v2_chain();
    test_fan_out_gather();
    test_when_all_gather();
    test_timeout_detection();
    test_timeout_and_cancellation();

#if defined(ASYNC_EXAMPLE_HAS_COROUTINES) && defined(WORKER_TASK_HAS_PC_ASYNC)
    test_coroutine_schedule_and_await();
#endif
}
