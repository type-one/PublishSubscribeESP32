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
 * @file example_allocator_stress.cpp
 * @brief Implements the allocator stress example.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

#include "tools/platform_helpers.hpp"

namespace
{
    /** @brief Minimal empty context used by the allocator stress worker task. */
    struct smp_task_context
    {
    };

    using worker_task1 = tools::worker_task<smp_task_context>;

    constexpr std::size_t alloc_max_size = 512;
    constexpr std::size_t alloc_iterations = 100000;

    /** @brief Data block holding a heap-allocated byte buffer for allocation stress tests. */
    struct alloc_data
    {
        std::vector<std::uint8_t> data;

        explicit alloc_data(std::size_t size)
            : data(size)
        {
        }
    };

    /**
     * @brief Performs repeated heap allocations and deallocations of increasing sizes.
     * @param wid Worker identifier printed at the start of the run.
     */
    void alloc_dealloc_worker(int wid)
    {
        std::printf("-- worker %d\n", wid);

        for (std::size_t i = 0; i < alloc_iterations; ++i)
        {
            // Vary allocation size to exercise allocator fast-path and fragmentation paths.
            auto heap_block = std::make_unique<alloc_data>((i % alloc_max_size) + 1);
            (void)heap_block;
        }
    }

    /** @brief Runs a concurrent allocator stress test across two workers executing on separate cores. */
    void test_allocator_stress()
    {
        std::printf("-- allocator stress --\n");

        print_stats();

        auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
        {
            (void)context;
            (void)task_name;
        };

        auto context = std::make_shared<smp_task_context>();

        constexpr const std::size_t task_stack_size = 4096U;
        const int core1 = tools::get_nb_of_cpu_cores() - 1;

        worker_task1 task1(
            startup, context, "worker_task1", task_stack_size, core1, tools::base_task::default_priority);

        const auto start = std::chrono::high_resolution_clock::now();

        // Run one worker on a separate core while the caller thread runs the same stress loop.
        task1.delegate(
            [](auto delegate_context, const auto& delegate_task_name)
            {
                (void)delegate_context;
                (void)delegate_task_name;
                alloc_dealloc_worker(2);
            });

        alloc_dealloc_worker(1);

        const auto end = std::chrono::high_resolution_clock::now();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::printf("allocation/deallocation total time: %d ms\n", static_cast<int>(millis));
    }
} // namespace

/** @brief Example entry point: runs the allocator stress test. */
void run_example_allocator_stress()
{
    // Use this stress scenario to validate allocator stability before deploying higher-level task pipelines.
    test_allocator_stress();
}
