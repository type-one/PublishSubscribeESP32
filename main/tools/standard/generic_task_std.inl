/**
 * @file generic_task_std.inl
 * @brief A generic task class template for executing tasks in separate threads.
 *
 * This file contains the implementation of a generic task class template that inherits from base_task.
 * It allows for the execution of tasks in separate threads with specified parameters such as context,
 * task name, stack size, CPU affinity, and priority.
 *
 * @author Laurent Lardinois
 * @date February 2025
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
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "tools/base_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"

namespace tools
{
    /**
     * @brief A generic task class template that inherits from base_task.
     *
     * This class represents a generic task that can be executed in a separate thread.
     * It is initialized with a callback function, context, task name, stack size, CPU affinity, and priority.
     *
     * @tparam Context The type of the context object associated with the task.
     */
    template <typename Context>
    class generic_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        generic_task() = delete;

        /**
         * @brief Type alias for a callback function used in the task system.
         *
         * This callback function is invoked with a shared pointer to a Context object
         * and the name of the task as a string.
         *
         * @param context A shared pointer to the Context object associated with the task.
         * @param task_name The name of the task as a string.
         */
        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;

        /**
         * @brief Constructs a generic_task object.
         *
         * This constructor initializes a generic_task with the given parameters and starts a new thread
         * to execute the provided routine.
         *
         * @param routine The callback function to be executed by the task.
         * @param context A shared pointer to the Context object associated with the task.
         * @param task_name The name of the task.
         * @param stack_size The stack size for the task.
         * @param cpu_affinity The CPU affinity for the task.
         * @param priority The priority of the task.
         */
        generic_task(call_back&& routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_routine(std::move(routine))
            , m_context(context)
        {
            m_task = std::make_unique<std::thread>(
                [this]()
                {
                    set_current_thread_params(this->task_name(), this->cpu_affinity(), this->priority());

                    single_call();
                });
        }

        /**
         * @brief Constructs a generic_task object with default priority and default cpu affinity.
         *
         * This constructor initializes a generic_task with the given parameters and starts a new thread
         * to execute the provided routine.
         *
         * @param routine The callback function to be executed by the task.
         * @param context A shared pointer to the Context object associated with the task.
         * @param task_name The name of the task.
         * @param stack_size The stack size for the task.
         */
        generic_task(call_back&& routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size)
            : generic_task(std::move(routine), context, task_name, stack_size, base_task::run_on_all_cores,
                base_task::default_priority)
        {
        }

        /**
         * @brief Destructor for the generic_task class.
         *
         * This destructor ensures that the task is properly joined before the object is destroyed.
         */
        ~generic_task() override
        {
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        /**
         * @brief Retrieves the native handle of the task.
         *
         * This method overrides the base class implementation to return the native handle
         * of the task, wrapped as a void pointer.
         *
         * @return void* The native handle of the task, cast to a void pointer.
         */
        void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle()); // NOLINT native handler wrapping as a void*
        }

    private:
        /**
         * @brief Executes the given function with the provided context and task name.
         */
        void single_call()
        {
            // execute given function
            m_routine(m_context, this->task_name());
        }

        call_back m_routine;
        std::shared_ptr<Context> m_context;
        std::unique_ptr<std::thread> m_task;
    };
}
