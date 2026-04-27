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
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "fpm/fixed.hpp"

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

constexpr std::size_t matrix_dimension = 4U;
constexpr std::size_t matrix_element_count = matrix_dimension * matrix_dimension;
constexpr std::size_t matrix_input_count = 16U;
constexpr std::size_t matrix_worker_count = 4U;

template <typename scalar_t>
struct matrix_4x4
{
    /** @brief Row-major 4x4 matrix coefficients. */
    std::vector<scalar_t> values;

    matrix_4x4()
        : values(matrix_element_count, scalar_t { 0 })
    {
    }
};

struct matrix_generation_options
{
    /** @brief Number of matrices to generate. */
    std::size_t matrix_count = 0U;
    /** @brief Start offset in the pre-generated coefficient table. */
    std::uint32_t seed_offset = 0U;
};

/** @brief Deterministic coefficient table constrained to [-1.5, 1.5]. */
const std::vector<float>& matrix_coefficients()
{
    static const std::vector<float> coefficients = {
        -1.50F, -1.25F, -1.00F, -0.75F, -0.50F, -0.25F, 0.00F, 0.25F,
        0.50F, 0.75F, 1.00F, 1.25F, 1.50F, 1.10F, 0.90F, 0.70F,
        0.30F, 0.10F, -0.10F, -0.30F, -0.70F, -0.90F, -1.10F, -1.30F,
        1.40F, 1.20F, 0.80F, 0.60F, 0.40F, 0.20F, -0.20F, -0.40F,
        -0.60F, -0.80F, -1.20F, -1.40F, 1.35F, 1.05F, 0.55F, 0.15F,
        -0.15F, -0.55F, -1.05F, -1.35F, 1.45F, 0.95F, 0.45F, -0.05F,
        -0.45F, -0.95F, -1.45F, 1.30F, 0.85F, 0.35F, -0.35F, -0.85F,
        -1.30F, 1.15F, 0.65F, 0.05F, -0.65F, -1.15F, 1.50F, -1.50F
    };
    return coefficients;
}

template <typename scalar_t>
/** @brief Multiplies two 4x4 matrices. */
matrix_4x4<scalar_t> multiply_matrix_4x4(const matrix_4x4<scalar_t>& left, const matrix_4x4<scalar_t>& right)
{
    matrix_4x4<scalar_t> output;

    for (std::size_t row = 0U; row < matrix_dimension; ++row)
    {
        for (std::size_t col = 0U; col < matrix_dimension; ++col)
        {
            scalar_t accumulator { 0 };
            for (std::size_t idx = 0U; idx < matrix_dimension; ++idx)
            {
                const std::size_t left_index = (row * matrix_dimension) + idx;
                const std::size_t right_index = (idx * matrix_dimension) + col;
                accumulator += left.values.at(left_index) * right.values.at(right_index);
            }

            output.values.at((row * matrix_dimension) + col) = accumulator;
        }
    }

    return output;
}

template <typename scalar_t>
/** @brief Builds deterministic matrix inputs from the static coefficient table. */
std::vector<matrix_4x4<scalar_t>> make_matrix_inputs(const matrix_generation_options& options)
{
    std::vector<matrix_4x4<scalar_t>> matrices;
    matrices.resize(options.matrix_count);

    const auto& coefficients = matrix_coefficients();
    const std::size_t coefficient_count = coefficients.size();
    const std::size_t start_index = static_cast<std::size_t>(options.seed_offset) % coefficient_count;
    std::size_t sequence_index = 0U;

    for (auto& matrix_item : matrices)
    {
        for (auto& coeff : matrix_item.values)
        {
            const std::size_t coeff_index = (start_index + sequence_index) % coefficient_count;
            coeff = scalar_t { coefficients.at(coeff_index) };
            ++sequence_index;
        }
    }

    return matrices;
}

template <typename scalar_t>
/** @brief Computes the sequential pairwise-reduction reference result. */
matrix_4x4<scalar_t> reduce_sequential(std::vector<matrix_4x4<scalar_t>> pending)
{
    while (pending.size() > 1U)
    {
        std::vector<matrix_4x4<scalar_t>> next_round;
        next_round.reserve((pending.size() / 2U) + (pending.size() % 2U));

        for (std::size_t pair_index = 0U; (pair_index + 1U) < pending.size(); pair_index += 2U)
        {
            next_round.emplace_back(multiply_matrix_4x4(pending.at(pair_index), pending.at(pair_index + 1U)));
        }

        if ((pending.size() % 2U) != 0U)
        {
            next_round.emplace_back(std::move(pending.back()));
        }

        pending = std::move(next_round);
    }

    return pending.front();
}

template <typename scalar_t>
/** @brief Summarizes matrix values into a scalar for tolerant equality checks. */
double matrix_checksum(const matrix_4x4<scalar_t>& matrix_value)
{
    scalar_t sum_value { 0 };
    for (const auto& coeff : matrix_value.values)
    {
        sum_value += coeff;
    }
    return static_cast<double>(sum_value);
}

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
/** @brief Coroutine matrix multiply helper on worker executor for float matrices. */
pco::future_result<matrix_4x4<float>> coroutine_matrix_multiply_float(
    worker_result_task& worker,
    const std::shared_ptr<worker_result_context>& context,
    matrix_4x4<float> left,
    matrix_4x4<float> right)
{
    co_await worker.schedule();
    context->loop_counter.fetch_add(1);
    co_return multiply_matrix_4x4(left, right);
}

/** @brief Coroutine matrix multiply helper on worker executor for fixed-point matrices. */
pco::future_result<matrix_4x4<fpm::fixed_16_16>> coroutine_matrix_multiply_fixed(
    worker_result_task& worker,
    const std::shared_ptr<worker_result_context>& context,
    matrix_4x4<fpm::fixed_16_16> left,
    matrix_4x4<fpm::fixed_16_16> right)
{
    co_await worker.schedule();
    context->loop_counter.fetch_add(1);
    co_return multiply_matrix_4x4(left, right);
}

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

TEST(PortableConcurrencyWorkerTaskResultTest, MatrixPairwiseReductionAcrossWorkersWithoutCoroutines)
{
    // Two deterministic input sets validate both floating-point and fixed-point reduction flows.
    auto input_float = make_matrix_inputs<float>(matrix_generation_options { matrix_input_count, 12345U });
    auto input_fixed = make_matrix_inputs<fpm::fixed_16_16>(matrix_generation_options { matrix_input_count, 54321U });

    const auto expected_float = reduce_sequential(input_float);
    const auto expected_fixed = reduce_sequential(input_fixed);

    std::vector<std::shared_ptr<worker_result_context>> contexts;
    contexts.reserve(matrix_worker_count);
    std::vector<std::unique_ptr<worker_result_task>> workers;
    workers.reserve(matrix_worker_count);

    for (std::size_t worker_index = 0U; worker_index < matrix_worker_count; ++worker_index)
    {
        auto context = std::make_shared<worker_result_context>();
        contexts.emplace_back(context);
        workers.emplace_back(make_worker_result_task(context, "matrix_no_coro_worker_" + std::to_string(worker_index)));
    }

    auto reduce_float = input_float;
    while (reduce_float.size() > 1U)
    {
        // Launch one multiply per matrix pair and gather each round with when_all.
        std::vector<pco::future_result<matrix_4x4<float>>> jobs;
        jobs.reserve(reduce_float.size() / 2U);

        for (std::size_t pair_index = 0U; (pair_index + 1U) < reduce_float.size(); pair_index += 2U)
        {
            const std::size_t worker_index = (pair_index / 2U) % matrix_worker_count;
            auto* worker_ptr = workers.at(worker_index).get();

            jobs.emplace_back(worker_ptr->delegate_async(
                [](const std::shared_ptr<worker_result_context>& context,
                    const std::string&,
                    matrix_4x4<float> left,
                    matrix_4x4<float> right)
                {
                    context->loop_counter.fetch_add(1);
                    return multiply_matrix_4x4(left, right);
                },
                reduce_float.at(pair_index),
                reduce_float.at(pair_index + 1U)));
        }

        const auto gathered = pco::when_all(std::move(jobs)).get_result();
        ASSERT_TRUE(gathered.has_value());

        std::vector<matrix_4x4<float>> next_round;
        next_round.reserve((reduce_float.size() / 2U) + (reduce_float.size() % 2U));

        for (const auto& item : gathered.value())
        {
            ASSERT_TRUE(item.has_value());
            next_round.emplace_back(item.value());
        }

        if ((reduce_float.size() % 2U) != 0U)
        {
            next_round.emplace_back(std::move(reduce_float.back()));
        }

        reduce_float = std::move(next_round);
    }

    auto reduce_fixed = input_fixed;
    while (reduce_fixed.size() > 1U)
    {
        // Repeat the same reduction topology for fixed_16_16 values.
        std::vector<pco::future_result<matrix_4x4<fpm::fixed_16_16>>> jobs;
        jobs.reserve(reduce_fixed.size() / 2U);

        for (std::size_t pair_index = 0U; (pair_index + 1U) < reduce_fixed.size(); pair_index += 2U)
        {
            const std::size_t worker_index = (pair_index / 2U) % matrix_worker_count;
            auto* worker_ptr = workers.at(worker_index).get();

            jobs.emplace_back(worker_ptr->delegate_async(
                [](const std::shared_ptr<worker_result_context>& context,
                    const std::string&,
                    matrix_4x4<fpm::fixed_16_16> left,
                    matrix_4x4<fpm::fixed_16_16> right)
                {
                    context->loop_counter.fetch_add(1);
                    return multiply_matrix_4x4(left, right);
                },
                reduce_fixed.at(pair_index),
                reduce_fixed.at(pair_index + 1U)));
        }

        const auto gathered = pco::when_all(std::move(jobs)).get_result();
        ASSERT_TRUE(gathered.has_value());

        std::vector<matrix_4x4<fpm::fixed_16_16>> next_round;
        next_round.reserve((reduce_fixed.size() / 2U) + (reduce_fixed.size() % 2U));

        for (const auto& item : gathered.value())
        {
            ASSERT_TRUE(item.has_value());
            next_round.emplace_back(item.value());
        }

        if ((reduce_fixed.size() % 2U) != 0U)
        {
            next_round.emplace_back(std::move(reduce_fixed.back()));
        }

        reduce_fixed = std::move(next_round);
    }

    ASSERT_EQ(reduce_float.size(), 1U);
    ASSERT_EQ(reduce_fixed.size(), 1U);

    EXPECT_NEAR(matrix_checksum(reduce_float.front()), matrix_checksum(expected_float), 1e-4);
    EXPECT_NEAR(matrix_checksum(reduce_fixed.front()), matrix_checksum(expected_fixed), 1e-4);

    const int expected_total_multiplications = static_cast<int>(matrix_input_count - 1U);
    int observed_calls = 0;
    for (const auto& context : contexts)
    {
        observed_calls += context->loop_counter.load();
    }

    // We run one complete reduction for float and one for fixed_16_16.
    EXPECT_EQ(observed_calls, expected_total_multiplications * 2);
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

TEST(PortableConcurrencyWorkerTaskResultTest, MatrixPairwiseReductionAcrossWorkersWithCoroutines)
{
    // Same deterministic data and topology as non-coroutine test, but execution hops use co_await schedule().
    auto input_float = make_matrix_inputs<float>(matrix_generation_options { matrix_input_count, 22345U });
    auto input_fixed = make_matrix_inputs<fpm::fixed_16_16>(matrix_generation_options { matrix_input_count, 64321U });

    const auto expected_float = reduce_sequential(input_float);
    const auto expected_fixed = reduce_sequential(input_fixed);

    std::vector<std::shared_ptr<worker_result_context>> contexts;
    contexts.reserve(matrix_worker_count);
    std::vector<std::unique_ptr<worker_result_task>> workers;
    workers.reserve(matrix_worker_count);

    for (std::size_t worker_index = 0U; worker_index < matrix_worker_count; ++worker_index)
    {
        auto context = std::make_shared<worker_result_context>();
        contexts.emplace_back(context);
        workers.emplace_back(make_worker_result_task(context, "matrix_coro_worker_" + std::to_string(worker_index)));
    }

    auto reduce_float = input_float;
    while (reduce_float.size() > 1U)
    {
        // Submit coroutine jobs for each pair, then gather synchronously per reduction round.
        std::vector<pco::future_result<matrix_4x4<float>>> jobs;
        jobs.reserve(reduce_float.size() / 2U);

        for (std::size_t pair_index = 0U; (pair_index + 1U) < reduce_float.size(); pair_index += 2U)
        {
            const std::size_t worker_index = (pair_index / 2U) % matrix_worker_count;
            jobs.emplace_back(coroutine_matrix_multiply_float(
                *workers.at(worker_index),
                contexts.at(worker_index),
                reduce_float.at(pair_index),
                reduce_float.at(pair_index + 1U)));
        }

        const auto gathered = pco::when_all(std::move(jobs)).get_result();
        ASSERT_TRUE(gathered.has_value());

        std::vector<matrix_4x4<float>> next_round;
        next_round.reserve((reduce_float.size() / 2U) + (reduce_float.size() % 2U));

        for (const auto& item : gathered.value())
        {
            ASSERT_TRUE(item.has_value());
            next_round.emplace_back(item.value());
        }

        if ((reduce_float.size() % 2U) != 0U)
        {
            next_round.emplace_back(std::move(reduce_float.back()));
        }

        reduce_float = std::move(next_round);
    }

    auto reduce_fixed = input_fixed;
    while (reduce_fixed.size() > 1U)
    {
        // Mirror the coroutine reduction path for fixed-point values.
        std::vector<pco::future_result<matrix_4x4<fpm::fixed_16_16>>> jobs;
        jobs.reserve(reduce_fixed.size() / 2U);

        for (std::size_t pair_index = 0U; (pair_index + 1U) < reduce_fixed.size(); pair_index += 2U)
        {
            const std::size_t worker_index = (pair_index / 2U) % matrix_worker_count;
            jobs.emplace_back(coroutine_matrix_multiply_fixed(
                *workers.at(worker_index),
                contexts.at(worker_index),
                reduce_fixed.at(pair_index),
                reduce_fixed.at(pair_index + 1U)));
        }

        const auto gathered = pco::when_all(std::move(jobs)).get_result();
        ASSERT_TRUE(gathered.has_value());

        std::vector<matrix_4x4<fpm::fixed_16_16>> next_round;
        next_round.reserve((reduce_fixed.size() / 2U) + (reduce_fixed.size() % 2U));

        for (const auto& item : gathered.value())
        {
            ASSERT_TRUE(item.has_value());
            next_round.emplace_back(item.value());
        }

        if ((reduce_fixed.size() % 2U) != 0U)
        {
            next_round.emplace_back(std::move(reduce_fixed.back()));
        }

        reduce_fixed = std::move(next_round);
    }

    ASSERT_EQ(reduce_float.size(), 1U);
    ASSERT_EQ(reduce_fixed.size(), 1U);

    EXPECT_NEAR(matrix_checksum(reduce_float.front()), matrix_checksum(expected_float), 1e-4);
    EXPECT_NEAR(matrix_checksum(reduce_fixed.front()), matrix_checksum(expected_fixed), 1e-4);

    const int expected_total_multiplications = static_cast<int>(matrix_input_count - 1U);
    int observed_calls = 0;
    for (const auto& context : contexts)
    {
        observed_calls += context->loop_counter.load();
    }

    // We run one complete reduction for float and one for fixed_16_16.
    EXPECT_EQ(observed_calls, expected_total_multiplications * 2);
}
#endif
