/**
 * @file periodic_task_std.inl
 * @brief Implementation of a periodic task using the Publish/Subscribe pattern.
 *
 * This file contains the implementation of the periodic_task class, which inherits from base_task
 * and provides functionality to execute a startup routine and a periodic routine at specified intervals.
 *
 * @author Laurent Lardinois
 * @date January 2025
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois      //
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


#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "tools/base_task.hpp"
#include "tools/linux/linux_sched_deadline.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"

namespace tools
{
    /**
     * @brief Class representing a periodic task.
     *
     * This class inherits from base_task and provides functionality to execute a startup routine
     * and a periodic routine at specified intervals.
     *
     * @tparam Context The type of the context object associated with the task.
     */
    template <typename Context>
    class periodic_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        periodic_task() = delete;

        /**
         * @brief Type alias for a callback function used in periodic tasks.
         *
         * This callback function is invoked with a shared pointer to a Context object
         * and the name of the task as a string.
         *
         * @param context A shared pointer to the Context object associated with the task.
         * @param task_name The name of the task as a string.
         */
        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;


        /**
         * @brief Constructs a periodic_task object.
         *
         * @param startup_routine The callback function to be executed at startup.
         * @param periodic_routine The callback function to be executed periodically.
         * @param context Shared pointer to the context object.
         * @param task_name The name of the task.
         * @param period The period of the task in microseconds.
         * @param stack_size The stack size for the task.
         * @param cpu_affinity The CPU affinity for the task.
         * @param priority The priority of the task.
         */
        periodic_task(call_back&& startup_routine, call_back&& periodic_routine,
            const std::shared_ptr<Context>& context, const std::string& task_name,
            const std::chrono::duration<std::uint64_t, std::micro>& period, std::size_t stack_size, int cpu_affinity,
            int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_periodic_routine(std::move(periodic_routine))
            , m_context(context)
            , m_period(period)
        {
            m_task = std::make_unique<std::thread>(
                [this]()
                {
                    set_current_thread_params(this->task_name(), this->cpu_affinity(), this->priority());

                    periodic_call();
                });
        }

        /**
         * @brief Constructs a periodic_task object with default priority and default cpu affinity.
         *
         * @param startup_routine The callback function to be executed at startup.
         * @param periodic_routine The callback function to be executed periodically.
         * @param context Shared pointer to the context object.
         * @param task_name The name of the task.
         * @param period The period of the task in microseconds.
         * @param stack_size The stack size for the task.
         */
        periodic_task(call_back&& startup_routine, call_back&& periodic_routine,
            const std::shared_ptr<Context>& context, const std::string& task_name,
            const std::chrono::duration<std::uint64_t, std::micro>& period, std::size_t stack_size)
            : periodic_task(std::move(startup_routine), std::move(periodic_routine), context, task_name, period,
                stack_size, base_task::run_on_all_cores, base_task::default_priority)
        {
        }

        /**
         * @brief Destructor for the periodic_task class.
         *
         * This destructor sets the m_stop_task flag to true and waits for the task to finish by calling join on m_task.
         */
        ~periodic_task() override
        {
            m_stop_task.store(true);
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        /**
         * @brief Retrieves the native handle of the task.
         *
         * This method overrides the base class implementation to return the native handle
         * of the task, wrapped as a void pointer.
         *
         * @return void* The native handle of the task.
         */
        void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle()); // NOLINT native handler wrapping as a void*
        }

    private:
        /**
         * @brief Executes a periodic task with a specified period.
         *
         * This function runs a startup routine once and then repeatedly executes a periodic routine
         * at intervals defined by `m_period`. It uses high-resolution clock for precise timing and
         * supports earliest deadline scheduling.
         *
         * The function will continue to run until `m_stop_task` is set to true.
         *
         * @note The function uses active waiting and sleeping to achieve precise timing.
         */
        void periodic_call()
        {
            auto start_time = std::chrono::high_resolution_clock::now();
            auto deadline = start_time + m_period;

            bool earliest_deadline_enabled = set_earliest_deadline_scheduling(start_time, m_period);

            // execute given startup function
            m_startup_routine(m_context, this->task_name());

            while (!m_stop_task.load())
            {
                // active wait loop
                std::chrono::high_resolution_clock::time_point current_time;
                do
                {
                    current_time = std::chrono::high_resolution_clock::now();
                } while (deadline > current_time);

                // execute given periodic function
                m_periodic_routine(m_context, this->task_name());

                // compute next deadline
                deadline += m_period;

                current_time = std::chrono::high_resolution_clock::now();

                // wait period
                if (deadline > current_time)
                {
                    const auto remaining_time
                        = std::chrono::duration_cast<std::chrono::microseconds>(deadline - current_time);
                    // wait between 90% and 96% of the remaining time depending on scheduling mode
                    const double ratio = (earliest_deadline_enabled) ? 0.96 : 0.9;

                    // sleep until we are close to the deadline
                    const auto sleep_time = std::chrono::duration<std::uint64_t, std::micro>(
                        static_cast<int>(ratio * static_cast<double>(remaining_time.count())));
                    std::this_thread::sleep_for(sleep_time);

                } // end if wait period needed
            }     // periodic task loop
        }

        call_back m_startup_routine;
        call_back m_periodic_routine;
        std::shared_ptr<Context> m_context;
        std::chrono::duration<std::uint64_t, std::micro> m_period;
        std::atomic_bool m_stop_task = false;
        std::unique_ptr<std::thread> m_task;
    };
}
