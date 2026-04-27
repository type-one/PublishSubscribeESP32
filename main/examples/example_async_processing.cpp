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
 * @brief Demonstrates the portable_concurrency result-based async API.
 *
 * Covers: pco::future_result, pco::promise_result, pco::packaged_task_result, pco::async_result,
 * then_value, then_error, then_result, pco::when_all, pco::make_result_promise,
 * delegate_async, and — when coroutines are available — co_await on
 * pco::future_result and worker_task::schedule().
 *
 * @author Laurent Lardinois
 * @date 2026-05-25
 */

#include "example_common.hpp"
#include "examples.hpp"

#include <functional>
#include <numeric>

#include "portable_concurrency/bits/coro.hpp"
#include "portable_concurrency/execution.hpp"
#include "portable_concurrency/future.hpp"

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

    struct periodic_feed_context
    {
        std::atomic<int> submitted_samples { 0 };
        std::atomic<int> completed_samples { 0 };
        std::atomic<int> failed_samples { 0 };
        std::atomic<std::int64_t> aggregated_output { 0 };
        std::vector<pco::future_result<int>> pending_results;
    };

    class async_processing_worker_model
    {
    public:
        [[nodiscard]] static int process_sample(int sample_value)
        {
            constexpr int sample_multiplier = 2;
            constexpr int sample_offset = 1;
            return (sample_value * sample_multiplier) + sample_offset;
        }

        [[nodiscard]] static int process_chain_stage(int input_value, int stage_index)
        {
            constexpr int stage_weight = 3;
            return input_value + ((stage_index + 1) * stage_weight);
        }
    };

    using async_worker = tools::worker_task<async_context>;
    using periodic_feed_task = tools::periodic_task<periodic_feed_context>;

    /** @brief Factory that creates a named @c async_worker with a minimal stack. */
    std::unique_ptr<async_worker> make_async_worker(
        const std::shared_ptr<async_context>& context, const std::string& name)
    {
        constexpr std::size_t stack_size = 4096U;
        auto startup = [](const std::shared_ptr<async_context>&, const std::string&) {};
        return std::make_unique<async_worker>(std::move(startup), context, name, stack_size);
    }

    // -----------------------------------------------------------------------
    // coroutine helpers for the result-based API — compiled only when coroutine support is present

#if defined(ASYNC_EXAMPLE_HAS_COROUTINES)
    struct alternating_chain_request
    {
        std::shared_ptr<async_context> context_first;
        std::shared_ptr<async_context> context_second;
        std::shared_ptr<async_processing_worker_model> processor;
        int initial_value = 0;
        int chain_stage_count = 0;
    };

    /**
     * @brief Coroutine that hops onto @p worker, increments the call counter, and
     *        returns @p input scaled by three.
     */
    pco::future_result<int> schedule_and_compute(
        async_worker* worker_ptr, std::shared_ptr<async_context> context, int input_value)
    {
        co_await worker_ptr->schedule();
        context->call_count.fetch_add(1);
        co_return input_value * 3;
    }

    /**
     * @brief Coroutine that hops onto @p worker, awaits @p upstream, and
     *        adds an offset to the resolved value.
     */
    pco::future_result<int> schedule_and_await(
        async_worker* worker_ptr,
        std::shared_ptr<async_context> context,
        pco::future_result<int> upstream,
        int offset_value)
    {
        co_await worker_ptr->schedule();
        const auto upstream_result = co_await upstream;
        if (!upstream_result.has_value())
        {
            co_return -1;
        }

        const int base = upstream_result.value();
        context->call_count.fetch_add(1);
        co_return base + offset_value;
    }

    pco::future_result<int> schedule_and_process_sample(
        async_worker* worker_ptr,
        std::shared_ptr<async_context> context,
        std::shared_ptr<async_processing_worker_model> processor,
        int sample_value)
    {
        co_await worker_ptr->schedule();
        context->call_count.fetch_add(1);
        co_return processor->process_sample(sample_value);
    }

    pco::future_result<int> schedule_alternating_chain_on_two_workers(
        async_worker* worker_first_ptr, async_worker* worker_second_ptr, alternating_chain_request request)
    {
        int chain_value = request.initial_value;
        for (int stage_index = 0; stage_index < request.chain_stage_count; ++stage_index)
        {
            if ((stage_index % 2) == 0)
            {
                co_await worker_first_ptr->schedule();
                request.context_first->call_count.fetch_add(1);
            }
            else
            {
                co_await worker_second_ptr->schedule();
                request.context_second->call_count.fetch_add(1);
            }
            chain_value = request.processor->process_chain_stage(chain_value, stage_index);
        }
        co_return chain_value;
    }
#endif // ASYNC_EXAMPLE_HAS_COROUTINES

    // -----------------------------------------------------------------------
    // Example 1: pco::async_result with inplace_executor + then_value / then_error

    /**
     * @brief Shows a basic result-based chain: inline computation, value transformation,
     *        and an error recovery fallback.
     */
    void test_async_result_chain()
    {
        LOG_INFO("-- pco::async_result + then_value + then_error --");
        print_stats();


        constexpr int input_value = 7;

        auto future = pco::async_result(
            pco::inplace_executor,
            [](int squared_input)
            {
                return squared_input * squared_input;
            },
            input_value);

        const auto result = std::move(future)
                                .then_value([](int squared) { return squared + 1; })
                                .then_error([](pco::result_error) { return -1; })
                                .get_result();

        if (result.has_value())
        {
            std::printf("pco::async_result chain: 7^2 + 1 = %d\n", result.value());
        }
    }

    // -----------------------------------------------------------------------
    // Example 2: then_result to inspect the full pco::expected<T,E>

    /**
     * @brief Demonstrates @c then_result, which receives the full
     *        @c pco::expected<T,E> and can branch on value or error.
     */
    void test_then_result_inspection()
    {
        LOG_INFO("-- then_result full pco::expected inspection --");
        print_stats();


        constexpr int offset_value = 10;
        constexpr int input_base = 5;

        auto future = pco::async_result(
            pco::inplace_executor,
            [](int input_val) { return input_val + offset_value; },
            input_base);

        const auto description = std::move(future)
                                     .then_result(
                                         [](const pco::expected<int, pco::result_error>& res) -> std::string
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
     * @brief Shows that @c then_error can intercept @c pco::result_error::execution_failure
     *        and recover with a default value.
     */
    void test_error_recovery()
    {
        LOG_INFO("-- error recovery via then_error --");
        print_stats();


        // A broken_promise scenario: discard the promise without fulfilling it.
        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto future = std::move(pair.second);

        // Let the promise go out of scope — future sees broken_promise.
        promise = pco::promise_result<int> {};

        const auto recovered = std::move(future)
                                   .then_error(
                                       [](pco::result_error err) -> int
                                       {
                                           if (err == pco::result_error::broken_promise)
                                           {
                                               std::printf("then_error caught: broken_promise — recovering with 0\n");
                                           }
                                           return 0;
                                       })
                                   .get_result();

        std::printf("error_recovery result: %d\n", recovered.has_value() ? recovered.value() : -1);
    }

    // -----------------------------------------------------------------------
    // Example 4: pco::packaged_task_result — pre-packaged callable

    /**
     * @brief Demonstrates @c pco::packaged_task_result: package a callable once,
     *        then invoke it to obtain a @c pco::future_result.
     */
    void test_packaged_task()
    {
        LOG_INFO("-- pco::packaged_task_result --");
        print_stats();


        constexpr int first_arg = 30;
        constexpr int second_arg = 12;

        pco::packaged_task_result<int(int, int)> task { [](int arg_a, int arg_b) { return arg_a + arg_b; } };
        auto future = task.get_future();
        task(first_arg, second_arg);

        const auto result = future.get_result();
        if (result.has_value())
        {
            std::printf("pco::packaged_task_result: 30 + 12 = %d\n", result.value());
        }
    }

    // -----------------------------------------------------------------------
    // Example 5: pco::packaged_task_result with std::reference_wrapper return

    /**
     * @brief Shows the @c std::reference_wrapper workaround for returning a
     *        reference from @c pco::packaged_task_result (direct reference types are
     *        prohibited by the result-based API static_assert).
     */
    void test_packaged_task_reference_wrapper()
    {
        LOG_INFO("-- pco::packaged_task_result with reference_wrapper --");
        print_stats();


        constexpr int initial_value = 42;
        constexpr int increment_amount = 8;
        constexpr int expected_result = 50;

        int value = initial_value;

        pco::packaged_task_result<std::reference_wrapper<int>()> task { [&value]()
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
            std::printf("reference_wrapper result: original value is now %d (pco::expected %d)\n", value, expected_result);
        }
    }

    // -----------------------------------------------------------------------
    // Example 6: manual pco::promise_result fulfilled by a worker

    /**
     * @brief Creates a @c pco::promise_result / @c pco::future_result pair, fulfils the
     *        promise from a worker thread, and chains a transformation.
     */
    void test_manual_promise_fulfillment()
    {
        LOG_INFO("-- manual pco::promise_result fulfilled from worker --");
        print_stats();


        constexpr int promise_value = 77;
        constexpr int multiplication_factor = 2;

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "promise_fulfillment_worker");

        auto pair = pco::make_result_promise<int>();
        auto promise = std::move(pair.first);
        auto future = std::move(pair.second);

        // Wrap in shared_ptr so the worker lambda can capture it safely.
        auto shared_promise = std::make_shared<pco::promise_result<int>>(std::move(promise));

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
    // Example 7: delegate_async chain on a worker

    /**
     * @brief Dispatches work to a worker via @c delegate_async, then chains
     *        @c then_value and @c then_error on the returned @c pco::future_result.
     */
    void test_delegate_async_chain()
    {
        LOG_INFO("-- delegate_async chain --");
        print_stats();


        constexpr int multiplier = 6;
        constexpr int addition_offset = 2;
        constexpr int input_val = 7;

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "chain_worker");

        auto future = worker->delegate_async(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int value_input)
            {
                ctx->call_count.fetch_add(1);
                return value_input * multiplier;
            },
            input_val);

        const auto result = std::move(future)
                                .then_value([](int val) { return val + addition_offset; })
                                .then_error([](pco::result_error) { return -1; })
                                .get_result();

        if (result.has_value())
        {
            std::printf("delegate_async chain: 7*6+2 = %d\n", result.value());
        }
        std::printf("worker call_count: %d\n", context->call_count.load());
    }

    // -----------------------------------------------------------------------
    // Example 8: fan-out / fan-in gather with a loop of delegate_async

    /**
     * @brief Dispatches several independent jobs in a loop and accumulates
     *        their results after all have completed.
     */
    void test_fan_out_gather()
    {
        LOG_INFO("-- fan-out / fan-in gather --");
        print_stats();


        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "gather_worker");

        constexpr int job_count = 5;
        std::vector<pco::future_result<int>> jobs;
        jobs.reserve(static_cast<std::size_t>(job_count));

        for (int i = 1; i <= job_count; ++i)
        {
            jobs.emplace_back(worker->delegate_async(
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
        std::printf("fan-out gather sum of squares 1..5 = %d (pco::expected 55)\n", total);
        std::printf("worker call_count: %d\n", context->call_count.load());
    }

    // -----------------------------------------------------------------------
    // Example 9: pco::when_all gather two parallel delegate_async results

    /**
     * @brief Demonstrates @c pco::when_all on a vector of @c pco::future_result values,
     *        collecting two parallel worker results into a single future.
     */
    void test_when_all_gather()
    {
        LOG_INFO("-- pco::when_all gather --");
        print_stats();


        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "when_all_worker");

        constexpr int triple_multiplier = 3;
        constexpr int first_input = 10;
        constexpr int offset_val = 7;
        constexpr int second_input = 20;

        auto future_a = worker->delegate_async(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int val_a)
            {
                ctx->call_count.fetch_add(1);
                return val_a * triple_multiplier;
            },
            first_input);

        auto future_b = worker->delegate_async(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int val_b)
            {
                ctx->call_count.fetch_add(1);
                return val_b + offset_val;
            },
            second_input);

        std::vector<pco::future_result<int>> futures;
        futures.reserve(2U);
        futures.push_back(std::move(future_a));
        futures.push_back(std::move(future_b));

        using expected_int = tools::expected<int, pco::result_error>;

        auto combined = pco::when_all(std::move(futures)).then_value(
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
            std::printf("pco::when_all gather: 10*3 + (20+7) = %d (pco::expected 57)\n", result.value());
        }
        std::printf("worker call_count: %d\n", context->call_count.load());
    }

    // -----------------------------------------------------------------------
    // Example 10: coroutine schedule() + co_await pco::future_result

#if defined(ASYNC_EXAMPLE_HAS_COROUTINES)
    /**
     * @brief Shows a coroutine that hops onto a worker via @c schedule(), performs
     *        a computation, then awaits an upstream @c pco::future_result.
     */
    void test_coroutine_schedule_and_await()
    {
        LOG_INFO("-- coroutine schedule() + co_await pco::future_result --");
        print_stats();

        auto context = std::make_shared<async_context>();
        auto worker = make_async_worker(context, "coro_worker");

        constexpr int upstream_input_value = 5;
        constexpr int scheduled_input_value = 8;
        constexpr int scheduled_increment = 1;
        constexpr int awaited_offset = 3;

        // Fire an upstream async job that the second coroutine will await.
        auto upstream = worker->delegate_async(
            [](const std::shared_ptr<async_context>& ctx, const std::string&, int upstream_input)
            {
                ctx->call_count.fetch_add(1);
                return upstream_input * 4;
            },
            upstream_input_value);

        // schedule_and_compute hops onto the worker, increments the counter, returns input * 3.
        auto scheduled = schedule_and_compute(worker.get(), context, scheduled_input_value)
                             .then_value([](int computed_value) { return computed_value + scheduled_increment; });

        // schedule_and_await hops onto the worker, awaits upstream (5*4=20), adds offset 3.
        auto awaited = schedule_and_await(worker.get(), context, std::move(upstream), awaited_offset);

        const auto result_a = scheduled.get_result();
        const auto result_b = awaited.get_result();

        if (result_a.has_value())
        {
            std::printf("coro schedule: 8*3+1 = %d (pco::expected 25)\n", result_a.value());
        }
        if (result_b.has_value())
        {
            std::printf("coro await: 5*4+3 = %d (pco::expected 23)\n", result_b.value());
        }
        std::printf("worker call_count: %d\n", context->call_count.load());
    }
#endif // ASYNC_EXAMPLE_HAS_COROUTINES

    // -----------------------------------------------------------------------
    // Example 11: timeout with wait_for()

    /**
     * @brief Demonstrates using @c wait_for() on a @c pco::future_result to detect
     *        when an async operation exceeds its time budget.
     *
     * In this example, the slow task completes before the timeout, so we see
     * the result; if it didn't, @c is_ready() after @c wait_for() would return false.
     */
    void test_timeout_detection()
    {
        LOG_INFO("-- timeout detection with wait_for() --");
        print_stats();

        using namespace std::chrono_literals;

        constexpr int base_value = 50;
        constexpr auto timeout_duration = 100ms;

        auto future = pco::async_result(
            pco::inplace_executor,
            [](int input_base)
            {
                // Simulate a fast computation: just multiply.
                return input_base * 2;
            },
            base_value);

        // Wait up to 100 milliseconds for the result.
        const auto status = future.wait_for(timeout_duration);

        std::string status_str;
        if (status == pco::future_status::ready)
        {
            status_str = "ready";
            const auto result = future.get_result();
            if (result.has_value())
            {
                std::printf("timeout_detection: computation ready in time, result=%d\n", result.value());
            }
        }
        else if (status == pco::future_status::timeout)
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
        LOG_INFO("-- timeout and cancellation with cleanup action --");
        print_stats();

        using namespace std::chrono_literals;

        constexpr auto cancel_timeout_duration = 10ms;
        constexpr auto cleanup_delay = 5ms;

        std::atomic<bool> cleanup_called { false };

        {
            // Create a promise with a custom cancellation/cleanup action.
            // The lambda captures cleanup_called by reference; when the promise
            // is destroyed, the lambda fires.
            auto pair = pco::make_result_promise<int>(
                pco::canceler_arg,
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

            if (status == pco::future_status::timeout)
            {
                std::printf("timeout detected: destroying future to trigger cancellation\n");
                // In the result-based API, canceler callbacks are tied to downstream abandonment.
                future = pco::future_result<int, pco::result_error> {};
                promise = pco::promise_result<int> {};
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

    // -----------------------------------------------------------------------
    // Example 13: periodic task feeds async worker stress flow (no coroutine)

    /**
     * @brief Stresses async dispatch by using a periodic producer that feeds a
     *        worker class through @c delegate_async.
     */
    void test_periodic_feeds_worker_stress_no_coroutines()
    {
        LOG_INFO("-- periodic -> worker stress (no coroutine) --");
        print_stats();


        constexpr int target_sample_count = 40;
        constexpr std::size_t periodic_stack_size = 4096U;
        constexpr auto periodic_interval = std::chrono::milliseconds(2);
        constexpr auto completion_timeout = std::chrono::seconds(3);
        constexpr auto wait_poll_interval = std::chrono::milliseconds(5);

        auto async_worker_context = std::make_shared<async_context>();
        auto worker = make_async_worker(async_worker_context, "periodic_feed_worker");
        auto feed_context = std::make_shared<periodic_feed_context>();
        auto processor = std::make_shared<async_processing_worker_model>();
        feed_context->pending_results.reserve(static_cast<std::size_t>(target_sample_count));

        auto startup = [](const std::shared_ptr<periodic_feed_context>&, const std::string&) {};
        auto* worker_ptr = worker.get();

        auto periodic = [worker_ptr, async_worker_context, processor](
                    const std::shared_ptr<periodic_feed_context>& local_context,
                    const std::string&)
        {
            int submitted_index = local_context->submitted_samples.load();
            const int target_value = target_sample_count;
            while ((submitted_index < target_value)
                && !local_context->submitted_samples.compare_exchange_weak(submitted_index, submitted_index + 1))
            {
            }

            if (submitted_index >= target_value)
            {
                return;
            }

            auto chained = worker_ptr
                               ->delegate_async(
                                   [processor](const std::shared_ptr<async_context>& local_async_context,
                                       const std::string&,
                                       int sample_value)
                                   {
                                       local_async_context->call_count.fetch_add(1);
                                       return processor->process_sample(sample_value);
                                   },
                                   submitted_index)
                               .then_value([local_context](int processed_value)
                               {
                                   local_context->completed_samples.fetch_add(1);
                                   local_context->aggregated_output.fetch_add(processed_value);
                                   return processed_value;
                               })
                               .then_error([local_context](pco::result_error)
                               {
                                   local_context->failed_samples.fetch_add(1);
                                   return -1;
                               });

            local_context->pending_results.emplace_back(std::move(chained));
        };

        {
            periodic_feed_task producer(
                std::move(startup), std::move(periodic), feed_context, "periodic_feed_producer", periodic_interval, periodic_stack_size);

            const auto deadline = std::chrono::steady_clock::now() + completion_timeout;
            while ((std::chrono::steady_clock::now() < deadline)
                && (feed_context->completed_samples.load() < target_sample_count)
                && (feed_context->failed_samples.load() == 0))
            {
                std::this_thread::sleep_for(wait_poll_interval);
            }
        }

        std::printf("periodic feed (no coroutine): submitted=%d completed=%d failed=%d worker_calls=%d\n",
            feed_context->submitted_samples.load(),
            feed_context->completed_samples.load(),
            feed_context->failed_samples.load(),
            async_worker_context->call_count.load());

        const std::int64_t expected_sum
            = static_cast<std::int64_t>(target_sample_count) * static_cast<std::int64_t>(target_sample_count);
        std::printf("periodic feed (no coroutine): aggregated=%" PRId64 " pco::expected=%" PRId64 "\n",
            feed_context->aggregated_output.load(),
            expected_sum);
    }

    // -----------------------------------------------------------------------
    // Example 15: alternating processing chain across two worker executors

    /**
     * @brief Runs a sequential processing chain where each stage is dispatched
     *        alternately to worker A then worker B executors.
     */
    void test_alternating_chain_two_workers_no_coroutines()
    {
        LOG_INFO("-- alternating chain across two workers (no coroutine) --");
        print_stats();

        constexpr int initial_value = 10;
        constexpr int stage_count = 6;
        constexpr int expected_calls_per_worker = stage_count / 2;

        auto context_first = std::make_shared<async_context>();
        auto context_second = std::make_shared<async_context>();
        auto worker_first = make_async_worker(context_first, "alt_chain_worker_a");
        auto worker_second = make_async_worker(context_second, "alt_chain_worker_b");

        int chain_value = initial_value;
        for (int stage_index = 0; stage_index < stage_count; ++stage_index)
        {
            auto* active_worker = ((stage_index % 2) == 0) ? worker_first.get() : worker_second.get();

            auto stage_future = active_worker->delegate_async(
                [](const std::shared_ptr<async_context>& local_context,
                    const std::string&,
                    int input_chain_value,
                    int current_stage)
                {
                    local_context->call_count.fetch_add(1);
                    return async_processing_worker_model::process_chain_stage(input_chain_value, current_stage);
                },
                chain_value,
                stage_index);

            const auto stage_result = stage_future.get_result();
            if (!stage_result.has_value())
            {
                std::printf("alternating chain (no coroutine): stage %d failed\n", stage_index);
                return;
            }
            chain_value = stage_result.value();
        }

        int expected_value = initial_value;
        for (int stage_index = 0; stage_index < stage_count; ++stage_index)
        {
            expected_value = async_processing_worker_model::process_chain_stage(expected_value, stage_index);
        }

        std::printf("alternating chain (no coroutine): result=%d pco::expected=%d\n", chain_value, expected_value);
        std::printf("alternating chain (no coroutine): worker_a_calls=%d worker_b_calls=%d expected_each=%d\n",
            context_first->call_count.load(),
            context_second->call_count.load(),
            expected_calls_per_worker);
    }

#if defined(ASYNC_EXAMPLE_HAS_COROUTINES)
    // -----------------------------------------------------------------------
    // Example 14: periodic task feeds worker via coroutine schedule()

    /**
     * @brief Stresses async dispatch with coroutines by letting a periodic
     *        producer launch schedule()-based coroutine jobs.
     */
    void test_periodic_feeds_worker_stress_coroutines()
    {
        LOG_INFO("-- periodic -> worker stress (coroutine) --");
        print_stats();


        constexpr int target_sample_count = 40;
        constexpr std::size_t periodic_stack_size = 4096U;
        constexpr auto periodic_interval = std::chrono::milliseconds(2);
        constexpr auto completion_timeout = std::chrono::seconds(3);
        constexpr auto wait_poll_interval = std::chrono::milliseconds(5);

        auto async_worker_context = std::make_shared<async_context>();
        auto worker = make_async_worker(async_worker_context, "periodic_feed_coro_worker");
        auto feed_context = std::make_shared<periodic_feed_context>();
        auto processor = std::make_shared<async_processing_worker_model>();
        feed_context->pending_results.reserve(static_cast<std::size_t>(target_sample_count));

        auto startup = [](const std::shared_ptr<periodic_feed_context>&, const std::string&) {};
        auto* worker_ptr = worker.get();

        auto periodic = [worker_ptr, async_worker_context, processor](
                            const std::shared_ptr<periodic_feed_context>& local_context,
                            const std::string&)
        {
            int submitted_index = local_context->submitted_samples.load();
            const int target_value = target_sample_count;
            while ((submitted_index < target_value)
                && !local_context->submitted_samples.compare_exchange_weak(submitted_index, submitted_index + 1))
            {
            }

            if (submitted_index >= target_value)
            {
                return;
            }

            auto chained = schedule_and_process_sample(worker_ptr, async_worker_context, processor, submitted_index)
                               .then_value([local_context](int processed_value)
                               {
                                   local_context->completed_samples.fetch_add(1);
                                   local_context->aggregated_output.fetch_add(processed_value);
                                   return processed_value;
                               })
                               .then_error([local_context](pco::result_error)
                               {
                                   local_context->failed_samples.fetch_add(1);
                                   return -1;
                               });

            local_context->pending_results.emplace_back(std::move(chained));
        };

        {
            periodic_feed_task producer(std::move(startup),
                std::move(periodic),
                feed_context,
                "periodic_feed_coro_producer",
                periodic_interval,
                periodic_stack_size);

            const auto deadline = std::chrono::steady_clock::now() + completion_timeout;
            while ((std::chrono::steady_clock::now() < deadline)
                && (feed_context->completed_samples.load() < target_sample_count)
                && (feed_context->failed_samples.load() == 0))
            {
                std::this_thread::sleep_for(wait_poll_interval);
            }
        }

        std::printf("periodic feed (coroutine): submitted=%d completed=%d failed=%d worker_calls=%d\n",
            feed_context->submitted_samples.load(),
            feed_context->completed_samples.load(),
            feed_context->failed_samples.load(),
            async_worker_context->call_count.load());

        const std::int64_t expected_sum
            = static_cast<std::int64_t>(target_sample_count) * static_cast<std::int64_t>(target_sample_count);
        std::printf("periodic feed (coroutine): aggregated=%" PRId64 " pco::expected=%" PRId64 "\n",
            feed_context->aggregated_output.load(),
            expected_sum);
    }

    // -----------------------------------------------------------------------
    // Example 16: alternating chain across two workers with coroutine schedule()

    /**
     * @brief Runs the same alternating two-worker chain as a coroutine flow using
     *        schedule() hops between worker A and worker B.
     */
    void test_alternating_chain_two_workers_coroutines()
    {
        LOG_INFO("-- alternating chain across two workers (coroutine) --");
        print_stats();

        constexpr int initial_value = 10;
        constexpr int stage_count = 6;
        constexpr int expected_calls_per_worker = stage_count / 2;

        auto context_first = std::make_shared<async_context>();
        auto context_second = std::make_shared<async_context>();
        auto worker_first = make_async_worker(context_first, "alt_coro_chain_worker_a");
        auto worker_second = make_async_worker(context_second, "alt_coro_chain_worker_b");
        auto processor = std::make_shared<async_processing_worker_model>();

        alternating_chain_request request;
        request.context_first = context_first;
        request.context_second = context_second;
        request.processor = processor;
        request.initial_value = initial_value;
        request.chain_stage_count = stage_count;

        auto chained_future = schedule_alternating_chain_on_two_workers(worker_first.get(), worker_second.get(), request);
        const auto chained_result = chained_future.get_result();

        if (!chained_result.has_value())
        {
            std::printf("alternating chain (coroutine): failed\n");
            return;
        }

        int expected_value = initial_value;
        for (int stage_index = 0; stage_index < stage_count; ++stage_index)
        {
            expected_value = async_processing_worker_model::process_chain_stage(expected_value, stage_index);
        }

        std::printf("alternating chain (coroutine): result=%d pco::expected=%d\n", chained_result.value(), expected_value);
        std::printf("alternating chain (coroutine): worker_a_calls=%d worker_b_calls=%d expected_each=%d\n",
            context_first->call_count.load(),
            context_second->call_count.load(),
            expected_calls_per_worker);
    }
#endif

} // namespace

void run_example_async_processing()
{
    LOG_INFO("=== async processing (portable_concurrency) ===");

    test_async_result_chain();
    test_then_result_inspection();
    test_error_recovery();
    test_packaged_task();
    test_packaged_task_reference_wrapper();
    test_manual_promise_fulfillment();
    test_delegate_async_chain();
    test_fan_out_gather();
    test_when_all_gather();
    test_timeout_detection();
    test_timeout_and_cancellation();
    test_periodic_feeds_worker_stress_no_coroutines();
    test_alternating_chain_two_workers_no_coroutines();

#if defined(ASYNC_EXAMPLE_HAS_COROUTINES)
    test_coroutine_schedule_and_await();
    test_periodic_feeds_worker_stress_coroutines();
    test_alternating_chain_two_workers_coroutines();
#endif
}
