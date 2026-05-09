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
 * @file example_worker_and_command.cpp
 * @brief Implements the queued command and worker task examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
    /** @brief Demonstrates using a @c tools::sync_queue of @c std::function callables as a simple command queue. */
    void test_queued_commands()
    {
        LOG_INFO("-- queued commands --");
        print_stats();

        tools::sync_queue<std::function<void()>> commands_queue;

        commands_queue.emplace([]() { std::printf("hello\n"); });
        commands_queue.emplace([]() { std::printf("world\n"); });

        // Execute queued commands in FIFO order to model a simple command dispatcher loop.
        while (!commands_queue.empty())
        {
            auto call = commands_queue.front_pop();
            if (call.has_value())
            {
                call.value()();
            }
        }
    }

    /** @brief Demonstrates using a @c tools::sync_ring_buffer of @c std::function callables as a fixed-capacity command
     * ring queue. */
    void test_ring_buffer_commands()
    {
        LOG_INFO("-- ring buffer commands --");
        print_stats();

        constexpr const std::size_t commands_queue_depth = 128U;
        tools::sync_ring_buffer<std::function<void()>, commands_queue_depth> commands_queue;

        commands_queue.emplace([]() { std::printf("hello\n"); });
        commands_queue.emplace([]() { std::printf("world\n"); });

        while (!commands_queue.empty())
        {
            auto call = commands_queue.front_pop();
            if (call.has_value())
            {
                call.value()();
            }
        }
    }

    /** @brief Shared worker-task state storing loop count and execution timestamps for timing statistics. */
    struct my_worker_task_context
    {
        std::atomic<int> loop_counter = 0;
        tools::sync_queue<std::chrono::high_resolution_clock::time_point> time_points;
    };

    using my_worker_task = tools::worker_task<my_worker_task_context>;

    /** @brief Demonstrates concurrent work distribution across two worker tasks with randomised delegation and timing
     * measurements. */
    void test_worker_tasks()
    {
        LOG_INFO("-- worker tasks --");
        print_stats();

        auto startup1 = [](const std::shared_ptr<my_worker_task_context>& context, const std::string& task_name)
        {
            (void)context;
            std::printf("welcome 1\n");
            std::printf("task %s started\n", task_name.c_str());
        };

        auto startup2 = [](const std::shared_ptr<my_worker_task_context>& context, const std::string& task_name)
        {
            (void)context;
            std::printf("welcome 2\n");
            std::printf("task %s started\n", task_name.c_str());
        };

        auto context = std::make_shared<my_worker_task_context>();

        constexpr const std::size_t stack_size = 4096U;
        auto task1 = std::make_unique<my_worker_task>(std::move(startup1), context, "worker_1", stack_size);
        auto task2 = std::make_unique<my_worker_task>(std::move(startup2), context, "worker_2", stack_size);

        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, 1);
        std::array<std::unique_ptr<my_worker_task>, 2> tasks = { std::move(task1), std::move(task2) };

        constexpr const int wait_tasks_ms = 100;
        tools::sleep_for(wait_tasks_ms);

        const auto start_timepoint = std::chrono::high_resolution_clock::now();
        constexpr const int nb_loops = 20;

        for (int i = 0; i < nb_loops; ++i)
        {
            // Randomly distribute work to illustrate task-pool style delegation.
            auto idx = distribution(generator);

            const std::array<my_worker_task::call_back, 2> work_batch
                = { [](const auto& local_context, const auto& local_task_name)
                      {
                          std::printf("job %d on worker task %s\n", local_context->loop_counter.load(),
                              local_task_name.c_str());
                          local_context->loop_counter++;
                          local_context->time_points.emplace(std::chrono::high_resolution_clock::now());
                      },
                      [](const auto& local_context, const auto& local_task_name)
                      {
                          std::printf("job %d on worker task %s\n", local_context->loop_counter.load(),
                              local_task_name.c_str());
                          local_context->loop_counter++;
                          local_context->time_points.emplace(std::chrono::high_resolution_clock::now());
                      } };

            // Queue a small batch so each worker can process multiple jobs per wake-up.
            tasks.at(idx)->delegate_range(work_batch);

            std::this_thread::yield();
        }

        constexpr const int wait_loops_ms = 2000;
        tools::sleep_for(wait_loops_ms);

        std::printf("nb of loops = %d\n", context->loop_counter.load());

        auto previous_timepoint = start_timepoint;
        while (!context->time_points.empty())
        {
            const auto measured_timepoint = context->time_points.front_pop();

            if (measured_timepoint.has_value())
            {
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    measured_timepoint.value() - previous_timepoint);
                std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
                previous_timepoint = measured_timepoint.value();
            }
        }
    }

    /** @brief Exercises perfect-forwarding delegate overloads of @c tools::worker_task with lvalue, rvalue, conversion,
     * and batch dispatch variants. */
    void test_worker_task_perfect_forwarding()
    {
        LOG_INFO("-- worker task perfect forwarding --");
        print_stats();

        auto context = std::make_shared<my_worker_task_context>();

        my_worker_task::call_back startup_lvalue
            = [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
        {
            std::printf("startup on %s\n", task_name.c_str());
            ctx->loop_counter.store(0);
        };

        std::string task_name_lvalue = "worker_forwarding";
        constexpr const std::size_t stack_size = 4096U;
        my_worker_task worker(startup_lvalue, context, task_name_lvalue, stack_size);

        my_worker_task::call_back delegate_lvalue
            = [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
        {
            std::printf("delegate-lvalue on %s\n", task_name.c_str());
            ctx->loop_counter.fetch_add(1);
        };

        worker.delegate(delegate_lvalue);
        worker.delegate(
            [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
            {
                std::printf("delegate-rvalue on %s\n", task_name.c_str());
                ctx->loop_counter.fetch_add(1);
            });
        worker.delegate(
            [](auto ctx, const auto& task_name)
            {
                std::printf("delegate-conversion on %s\n", task_name.c_str());
                ctx->loop_counter.fetch_add(1);
            });

        const std::vector<my_worker_task::call_back> work_batch
            = { [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
                  {
                      std::printf("delegate-range on %s\n", task_name.c_str());
                      ctx->loop_counter.fetch_add(1);
                  },
                  [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
                  {
                      std::printf("delegate-range on %s\n", task_name.c_str());
                      ctx->loop_counter.fetch_add(1);
                  } };
        worker.delegate_range(work_batch);
        worker.delegate_range({ my_worker_task::call_back(
            [](const std::shared_ptr<my_worker_task_context>& ctx, const std::string& task_name)
            {
                std::printf("delegate-range-init on %s\n", task_name.c_str());
                ctx->loop_counter.fetch_add(1);
            }) });

        constexpr const int wait_time_ms = 200;
        tools::sleep_for(wait_time_ms);
        std::printf("worker forwarding loop counter = %d\n", context->loop_counter.load());
    }
} // namespace

void run_example_worker_and_command()
{
    // Begin with a minimal sync_queue command pattern for command dispatch basics.
    test_queued_commands();
    // Then switch to ring-buffer-backed commands for fixed-capacity/low-allocation scenarios.
    test_ring_buffer_commands();
    // Move on to multi-worker execution and delegation across worker tasks.
    test_worker_tasks();
    // Finish with forwarding-focused API usage to avoid unnecessary copies in command payloads.
    test_worker_task_perfect_forwarding();
}
