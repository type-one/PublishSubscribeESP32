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
 * @file example_smp.cpp
 * @brief Implements the SMP, inter-core queue, memory pipe, and priority examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

#include "tools/platform_helpers.hpp"

namespace
{

/** @brief Empty shared context used by basic SMP affinity and priority task demos. */
struct smp_task_context
{
};

using periodic_task0 = tools::periodic_task<smp_task_context>;
using worker_task1 = tools::worker_task<smp_task_context>;

/** @brief Demonstrates launching a periodic task pinned to core 0 and a worker task pinned to core 1 using explicit CPU affinity. */
void test_smp_tasks_cpu_affinity()
{
    LOG_INFO("-- smp tasks with cpu affinity --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_task_context>();

    constexpr const std::size_t task1_stack_size = 2048U;
    const int core1 = tools::get_nb_of_cpu_cores() - 1;
    // Pin worker task to core 1, then drive it from a periodic producer pinned to core 0.
    worker_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        static int count = 0;
        std::printf("%s (core 0): count %d\n", task_name.c_str(), count);
        ++count;

        task1.delegate([](auto local_context, const auto& local_task_name)
        {
            (void)local_context;
            std::printf("%s (core 1): work\n", local_task_name.c_str());
        });
    };

    constexpr const auto period_100ms = std::chrono::duration<std::uint64_t, std::milli>(100);
    constexpr const std::size_t task0_stack_size = 4096U;
    constexpr const int core0 = 0;

    periodic_task0 task0(startup, periodic_lambda, context, "periodic_task0", period_100ms, task0_stack_size, core0, tools::base_task::default_priority);

    constexpr const int wait_task_ms = 2000;
    tools::sleep_for(wait_task_ms);
}

constexpr const std::size_t worker_pipe_depth = 5U;
/** @brief Shared context for lock-free ring-buffer communication between periodic and worker SMP tasks. */
struct smp_ring_task_context
{
    tools::lock_free_ring_buffer<std::uint8_t, worker_pipe_depth> m_to_worker_pipe;
    tools::lock_free_ring_buffer<std::uint8_t, worker_pipe_depth> m_from_worker_pipe;
};

using periodic_ring_task0 = tools::periodic_task<smp_ring_task_context>;
using worker_ring_task1 = tools::worker_task<smp_ring_task_context>;

/** @brief Demonstrates inter-core communication between a periodic and a worker task using a lock-free ring buffer as a bidirectional pipe. */
void test_smp_tasks_lock_free_ring_buffer()
{
    LOG_INFO("-- smp tasks with lock free ring buffer --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_ring_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_ring_task_context>();

    constexpr const std::size_t task1_stack_size = 2048U;
    const int core1 = tools::get_nb_of_cpu_cores() - 1;
    worker_ring_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_ring_task_context>& local_context, const std::string& task_name)
    {
        static int count = 0;
        std::printf("%s (core 0): count %d\n", task_name.c_str(), count);
        ++count;

        constexpr const int mask = 0xff;
        // Send one byte to core 1 and read back its processed response.
        (void)local_context->m_to_worker_pipe.push(static_cast<std::uint8_t>(count & mask));

        std::uint8_t val = 0U;
        if (local_context->m_from_worker_pipe.pop(val))
        {
            std::printf("(core 0): received computed (from core 1) %" PRIu8 "\n", val);
        }

        task1.delegate([](const auto& delegate_context, const auto& delegate_task_name)
        {
            (void)delegate_task_name;
            std::uint8_t value = 0U;
            if (delegate_context->m_to_worker_pipe.pop(value))
            {
                (void)delegate_context->m_from_worker_pipe.push(static_cast<std::uint8_t>(value * value));
            }
        });
    };

    constexpr const auto period_100ms = std::chrono::duration<std::uint64_t, std::milli>(100);
    constexpr const std::size_t task0_stack_size = 4096U;
    constexpr const int core0 = 0;
    periodic_ring_task0 task0(startup, periodic_lambda, context, "periodic_task0", period_100ms, task0_stack_size, core0, tools::base_task::default_priority);

    constexpr const int wait_tasks_ms = 2000;
    tools::sleep_for(wait_tasks_ms);
}

constexpr const std::size_t static_storage_size = 1000U;

tools::memory_pipe::static_buffer_holder& get_static_buf_holder()
{
    static tools::memory_pipe::static_buffer_holder static_buf_holder = {};
    return static_buf_holder;
}

std::array<std::uint8_t, static_storage_size>& get_static_storage()
{
    static std::array<std::uint8_t, static_storage_size> static_storage = {};
    return static_storage;
}

/** @brief Shared context owning the memory pipe used for SMP chunk transfer examples. */
struct smp_mem_task_context
{
    tools::memory_pipe m_to_worker_pipe;

    explicit smp_mem_task_context(std::size_t to_size)
        : m_to_worker_pipe(to_size, get_static_storage().data(), &get_static_buf_holder())
    {
    }
};

using periodic_mem_task0 = tools::periodic_task<smp_mem_task_context>;
using worker_mem_task1 = tools::worker_task<smp_mem_task_context>;

/** @brief Demonstrates chunked inter-core data transfer between a periodic and a worker task via a @c tools::memory_pipe. */
void test_smp_tasks_memory_pipe()
{
    LOG_INFO("-- smp tasks with memory pipe --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_mem_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto& static_buf_holder = get_static_buf_holder();
    auto& static_storage = get_static_storage();

    std::memset(&static_buf_holder, 0, sizeof(static_buf_holder));
    // Reinitialize static memory-pipe bookkeeping before each run.
    auto context = std::make_shared<smp_mem_task_context>(static_storage.size());

    {
        constexpr const std::size_t task1_stack_size = 2048U;
        const int core1 = tools::get_nb_of_cpu_cores() - 1;
        worker_mem_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

        static const std::string label = "this\nis\na\ntest\nto\ntransmit\nseveral\nmessages\nbetween\ntwo\ncores\n";

        std::atomic_bool stop(false);

        task1.delegate([&stop](const auto& delegate_context, const auto& task_name)
        {
            std::printf("%s (core 1)\n", task_name.c_str());

            const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(20);

            // Worker side: continuously consume chunks until producer signals shutdown.
            while (!stop.load())
            {
                constexpr const std::size_t bytes_to_receive = 128U;
                std::array<std::uint8_t, bytes_to_receive> received = {};
                const auto received_bytes = delegate_context->m_to_worker_pipe.receive_range(
                    received.begin(), received.end(), timeout);
                if (received_bytes > 0U)
                {
                    auto* iter = std::data(received);
                    for (std::size_t idx = 0U; idx < received_bytes; ++idx)
                    {
                        std::printf("%c", *iter++);
                    }
                }
            }

            std::printf("\n");
        });

        std::size_t offset = 0U;
        auto periodic_lambda = [&offset](const std::shared_ptr<smp_mem_task_context>& local_context, const std::string& task_name)
        {
            std::printf(" / %s (core 0)\n", task_name.c_str());

            static const std::string label_local = "this\nis\na\ntest\nto\ntransmit\nseveral\nmessages\nbetween\ntwo\ncores\n";
            constexpr const std::size_t chunk_size = 16U;
            std::size_t to_send = (std::min)(chunk_size, label_local.size() - offset);

            if (offset < label_local.size())
            {
                // Producer side: stream the payload in bounded chunks to emulate packetized transport.
                const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(10);
                const std::string_view chunk(label_local.data() + offset, to_send);
                const auto sent = local_context->m_to_worker_pipe.send_range(chunk, timeout);
                offset += sent;
            }
        };

        constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(50);
        constexpr const std::size_t task0_stack_size = 4096U;
        constexpr const int core0 = 0;
        periodic_mem_task0 task0(startup, periodic_lambda, context, "periodic_task0", period, task0_stack_size, core0, tools::base_task::default_priority);

        constexpr const int wait_processing_ms = 2000;
        constexpr const int wait_join_ms = 250;
        tools::sleep_for(wait_processing_ms);
        stop.store(true);
        tools::sleep_for(wait_join_ms);
    }
}

/** @brief Exercises perfect-forwarding send/receive overloads of @c tools::memory_pipe with lvalue, rvalue, range, conversion, and ISR variants. */
void test_memory_pipe_perfect_forwarding()
{
    LOG_INFO("-- memory pipe perfect forwarding --");
    print_stats();

    constexpr const std::size_t capacity = 64U;
    tools::memory_pipe pipe(capacity);
    constexpr const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(100);
    constexpr const std::array<std::uint8_t, 4> lvalue_payload = { 1U, 2U, 3U, 4U };
    constexpr const std::array<std::uint8_t, 3> rvalue_payload = { 5U, 6U, 7U };
    constexpr const std::array<std::uint8_t, 4> conversion_payload = { 9U, 8U, 7U, 6U };

    std::vector<std::uint8_t> lvalue_data(lvalue_payload.begin(), lvalue_payload.end());
    std::vector<std::uint8_t> received;

    const auto lvalue_sent = pipe.send(lvalue_data, timeout);
    const auto lvalue_received = pipe.receive(received, lvalue_data.size(), timeout);
    std::printf("lvalue bytes: sent=%zu received=%zu\n", lvalue_sent, lvalue_received);

    const auto rvalue_sent = pipe.send(std::vector<std::uint8_t>(rvalue_payload.begin(), rvalue_payload.end()), timeout);
    const auto rvalue_received = pipe.receive(received, rvalue_payload.size(), timeout);
    std::printf("rvalue bytes: sent=%zu received=%zu\n", rvalue_sent, rvalue_received);

    const std::vector<std::uint8_t> range_values = { 10U, 11U, 12U, 13U };
    const auto range_sent = pipe.send_range(range_values, timeout);
    constexpr const std::size_t range_batch_size = 4U;
    std::array<std::uint8_t, range_batch_size> range_received = { 0U, 0U, 0U, 0U };
    const auto range_received_count = pipe.receive_range(range_received.begin(), range_received.end(), timeout);
    std::printf("range bytes: sent=%zu received=%zu\n", range_sent, range_received_count);

    /** @brief Adapter type used to test implicit rvalue conversion to @c std::vector<std::uint8_t>. */
    struct vector_convertible_data
    {
        std::vector<std::uint8_t> payload;

        operator std::vector<std::uint8_t>() &&
        {
            return std::move(payload);
        }
    };

    auto converted = vector_convertible_data {
        std::vector<std::uint8_t>(conversion_payload.begin(), conversion_payload.end())
    };
    const auto conversion_sent = pipe.send(std::move(converted), timeout);
    const auto conversion_received = pipe.receive(received, conversion_payload.size(), timeout);
    std::printf("conversion bytes: sent=%zu received=%zu\n", conversion_sent, conversion_received);

    const auto isr_sent = pipe.isr_send_range({ 14U, 15U, 16U });
    std::array<std::uint8_t, 3> isr_received = { 0U, 0U, 0U };
    const auto isr_received_count = pipe.isr_receive_range(isr_received.begin(), isr_received.end());
    std::printf("isr range bytes: sent=%zu received=%zu\n", isr_sent, isr_received_count);
}

/** @brief Demonstrates launching a high-priority periodic task alongside a low-priority worker task to illustrate FreeRTOS priority scheduling. */
void test_tasks_priority()
{
    LOG_INFO("-- tasks with priority --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_task_context>();

    constexpr const auto task1_stack_size = 2048U;
    constexpr const auto task1_priority = 0;
    worker_task1 task1(startup, context, "worker_task1", task1_stack_size, tools::base_task::run_on_all_cores, task1_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_task_context>& local_context, const std::string& task_name)
    {
        (void)local_context;
        static int count = 0;
        std::printf("%s (hi prio): count %d\n", task_name.c_str(), count);
        ++count;

        task1.delegate([](const auto& delegate_context, const auto& delegate_task_name)
        {
            (void)delegate_context;
            std::printf("%s (lo prio): work\n", delegate_task_name.c_str());
        });
    };

    constexpr const auto period_ms = 100;
    constexpr const auto task0_stack_size = 4096U;
    constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(period_ms);
    periodic_task0 task0(startup, periodic_lambda, context, "periodic_task0", period, task0_stack_size, tools::base_task::run_on_all_cores, 3);

    constexpr const auto sleep_time_ms = 2000;
    tools::sleep_for(sleep_time_ms);
}
} // namespace

void run_example_smp()
{
    // Start by pinning tasks to explicit cores to understand affinity control.
    test_smp_tasks_cpu_affinity();
    // Then exchange data across cores with lock-free ring buffers.
    test_smp_tasks_lock_free_ring_buffer();
    // Review memory_pipe forwarding APIs before applying them in SMP task traffic.
    test_memory_pipe_perfect_forwarding();
    // Use memory_pipe for larger/chunked transfers between core-bound tasks.
    test_smp_tasks_memory_pipe();
    // End with task priority interplay to visualize scheduling effects.
    test_tasks_priority();
}
