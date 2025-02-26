/**
 * @file worker_task_std.inl
 * @brief A worker task class template for the Publish/Subscribe Pattern.
 *
 * This file contains the implementation of the worker_task class template, which
 * provides functionality to delegate tasks and manage their execution.
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
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "tools/base_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/sync_object.hpp"
#include "tools/sync_queue.hpp"

namespace tools
{
    /**
     * @brief A worker task class template.
     *
     * This class represents a worker task that inherits from the base_task class.
     * It provides functionality to delegate tasks and manage their execution.
     *
     * @tparam Context The type of the context object.
     */
    template <typename Context>
    class worker_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        worker_task() = delete;

        /**
         * @brief Type alias for a callback function.
         *
         * This callback function takes a shared pointer to a Context object and a task name string as parameters.
         *
         * @param context A shared pointer to a Context object.
         * @param task_name The name of the task as a string.
         */
        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;

        /**
         * @brief Constructs a worker_task object.
         *
         * @param startup_routine A callable object that represents the startup routine.
         * @param context A shared pointer to the Context object.
         * @param task_name The name of the task.
         * @param stack_size The size of the stack for the task.
         * @param cpu_affinity The CPU affinity for the task.
         * @param priority The priority of the task.
         */
        worker_task(call_back&& startup_routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_context(context)
            , m_task(std::make_unique<std::thread>(
                  [this]()
                  {
                      set_current_thread_params(this->task_name(), this->cpu_affinity(), this->priority());

                      run_loop();
                  }))
        {
        }

        /**
         * @brief Constructs a worker_task object with default priority and default cpu affinity.
         *
         * @param startup_routine A callable object that represents the startup routine.
         * @param context A shared pointer to the Context object.
         * @param task_name The name of the task.
         * @param stack_size The size of the stack for the task.
         */
        worker_task(call_back&& startup_routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size)
            : worker_task(std::move(startup_routine), context, task_name, stack_size, base_task::run_on_all_cores,
                base_task::default_priority)
        {
        }

        /**
         * @brief Destructor for the worker_task class.
         *
         * This destructor sets the stop flag to true, signals the work synchronization
         * object, and waits for the task to complete by joining the task thread.
         */
        ~worker_task() override
        {
            m_stop_task.store(true);
            m_work_sync.signal();
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        /**
         * @brief Retrieves the native handle of the task.
         *
         * This function overrides the base class implementation to return the native handle
         * of the task, wrapped as a void pointer.
         *
         * @return void* The native handle of the task, cast to a void pointer.
         */
        void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle()); // NOLINT native handler wrapping as a void*
        }

        /**
         * @brief Delegates a task to the worker by adding it to the work queue.
         *
         * @param work A callable object (e.g., lambda, function object) to be executed by the worker.
         */
        void delegate(call_back&& work)
        {
            m_work_queue.emplace(std::move(work));
            m_work_sync.signal();
        }

        /**
         * @brief Delegate a task to be executed, ensuring compatibility with ISR context.
         *
         * This function ensures that the provided task is executed in a context that is
         * compatible with standard C++ platforms, where direct calls from ISRs are not allowed.
         * It falls back to a standard call to delegate the task.
         *
         * @param work A callable object representing the task to be executed.
         */
        void isr_delegate(call_back&& work)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            delegate(work);
        }

    private:
        /**
         * @brief Executes the main loop of the worker task.
         *
         * This function runs the startup routine and then enters a loop where it waits for a signal
         * to process work items from the work queue. The loop continues until the stop task flag is set.
         */
        void run_loop()
        {
            // execute given startup function
            m_startup_routine(m_context, this->task_name());

            while (!m_stop_task.load())
            {
                m_work_sync.wait_for_signal();

                while (!m_work_queue.empty())
                {
                    auto work = m_work_queue.front();
                    m_work_queue.pop();

                    work(m_context, this->task_name());
                }
            } // run loop
        }

        call_back m_startup_routine;
        tools::sync_object m_work_sync;
        tools::sync_queue<call_back> m_work_queue;
        std::shared_ptr<Context> m_context;
        std::atomic_bool m_stop_task = false;
        std::unique_ptr<std::thread> m_task;
    };
}
