/**
 * @file generic_task_freertos.inl
 * @brief A generic task class template for FreeRTOS tasks.
 *
 * This file contains the implementation of a generic task class template for FreeRTOS tasks.
 * It provides a generic implementation for creating and managing FreeRTOS tasks.
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
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "tools/base_task.hpp"
#include "tools/platform_helpers.hpp"

namespace tools
{
    /**
     * @brief A generic task class template for FreeRTOS tasks.
     *
     * This class template provides a generic implementation for creating and managing FreeRTOS tasks.
     * It inherits from the base_task class and allows tasks to be created with specific routines,
     * contexts, and configurations.
     *
     * @tparam Context The type of the context object associated with the task.
     */
    template <typename Context>
    class generic_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        generic_task() = delete;

        /**
         * @brief Type alias for a callback function used in FreeRTOS tasks.
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
         * This constructor initializes a FreeRTOS task with the given parameters and
         * sets up the task routine and context.
         *
         * @param routine The callback routine to be executed by the task.
         * @param context A shared pointer to the context in which the task operates.
         * @param task_name The name of the task.
         * @param stack_size The stack size allocated for the task.
         * @param cpu_affinity The CPU core affinity for the task.
         * @param priority The priority of the task.
         */
        generic_task(call_back&& routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_routine(std::move(routine))
            , m_context(context)
        {
            // FreeRTOS platform
            m_task_created = task_create(&m_task, this->task_name(), single_call,
                reinterpret_cast<void*>(this), // NOLINT only way to pass the instance as a void* to the task
                this->stack_size(), this->cpu_affinity(), this->priority());
        }

        /**
         * @brief Constructs a generic_task object with default priority and cpu affinity.
         *
         * This constructor initializes a FreeRTOS task with the given parameters and
         * sets up the task routine and context.
         *
         * @param routine The callback routine to be executed by the task.
         * @param context A shared pointer to the context in which the task operates.
         * @param task_name The name of the task.
         * @param stack_size The stack size allocated for the task.
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
         * This destructor suspends and deletes the FreeRTOS task if it was created.
         */
        ~generic_task()
        {
            // FreeRTOS platform
            if (m_task_created)
            {
                vTaskSuspend(m_task);
                vTaskDelete(m_task);
            }
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        /**
         * @brief Retrieves the native handle of the task.
         *
         * This function overrides the base class implementation to return the native handle
         * of the task, which is wrapped as a void*.
         *
         * @return A void* pointing to the native handle of the task.
         */
        void* native_handle() override
        {
            return reinterpret_cast<void*>(&m_task); // NOLINT native handler wrapping as a void*
        }

    private:
        /**
         * @brief Executes the routine of a generic task instance in a FreeRTOS environment.
         *
         * This static function is intended to be used as a FreeRTOS task entry point. It retrieves the
         * generic_task instance from the provided void pointer, executes the task's routine, and then
         * suspends the task.
         *
         * @param object_instance A pointer to the generic_task instance to be executed.
         */
        static void single_call(void* object_instance)
        {
            // FreeRTOS platform

            generic_task* instance = reinterpret_cast<generic_task*>( // NOLINT only way to convert the passed void* to
                                                                      // the task to a object instance
                object_instance);
            const std::string& task_name = instance->task_name();

            // execute given function
            instance->m_routine(instance->m_context, task_name);

            vTaskSuspend(NULL);
        }

        call_back m_routine;
        std::shared_ptr<Context> m_context;

        TaskHandle_t m_task = {};
        bool m_task_created = false;
    };
}
