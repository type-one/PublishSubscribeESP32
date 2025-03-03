/**
 * @file periodic_task_freertos.inl
 * @brief A header file defining a periodic task class for FreeRTOS.
 *
 * This file contains the definition of the periodic_task class template, which
 * inherits from base_task and provides functionality to execute a startup routine
 * and a periodic routine at specified intervals in a FreeRTOS environment.
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

#include <algorithm>
#include <atomic>
#include <chrono>
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
     * @brief A class representing a periodic task in FreeRTOS.
     *
     * This class inherits from base_task and provides functionality to execute
     * a startup routine and a periodic routine at specified intervals.
     *
     * @tparam Context The type of the context object used in the task.
     */

    template <typename Context>
    class periodic_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        periodic_task() = delete;

        /**
         * @brief Type alias for a callback function used in the periodic task.
         *
         * This callback function is invoked with a shared pointer to a Context object
         * and the name of the task as a string.
         *
         * @param context A shared pointer to the Context object.
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
         * @param stack_size The stack size of the task.
         * @param cpu_affinity The CPU affinity of the task.
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
            // FreeRTOS platform

            m_task_created = task_create(&m_task, this->task_name(), periodic_call,
                reinterpret_cast<void*>(this), // NOLINT only way to pass the instance as a void* to the task
                this->stack_size(), this->cpu_affinity(), this->priority());
        }

        /**
         * @brief Constructs a periodic_task object with default priority and cpu affinity.
         *
         * @param startup_routine The callback function to be executed at startup.
         * @param periodic_routine The callback function to be executed periodically.
         * @param context Shared pointer to the context object.
         * @param task_name The name of the task.
         * @param period The period of the task in microseconds.
         * @param stack_size The stack size of the task.
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
         * This destructor stops the periodic task by setting the m_stop_task flag to true.
         * If the task was created, it suspends and deletes the FreeRTOS task.
         */
        ~periodic_task()
        {
            // FreeRTOS platform

            m_stop_task.store(true);

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
         * @brief Periodic call function for FreeRTOS tasks.
         *
         * This function is executed as a FreeRTOS task and periodically calls the
         * provided periodic routine. It also executes a startup routine before entering
         * the periodic loop.
         *
         * @param object_instance Pointer to the object instance of type periodic_task.
         */
        static void periodic_call(void* object_instance)
        {
            // FreeRTOS platform

            periodic_task* instance = reinterpret_cast<periodic_task*>( // NOLINT only way to convert the passed void*
                                                                        // to the task to a object instance
                object_instance);

            auto x_last_wake_time = xTaskGetTickCount();
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(instance->m_period);
            const TickType_t x_period = static_cast<TickType_t>((pdMS_TO_TICKS(us.count()) / 1000U));

            const std::string& task_name = instance->task_name();

            // execute given startup function
            instance->m_startup_routine(instance->m_context, task_name);

            while (!instance->m_stop_task.load())
            {
                vTaskDelayUntil(&x_last_wake_time, x_period);

                // execute given periodic function
                instance->m_periodic_routine(instance->m_context, task_name);
            }

            vTaskSuspend(NULL);
        }

        call_back m_startup_routine;
        call_back m_periodic_routine;
        std::shared_ptr<Context> m_context;
        std::chrono::duration<std::uint64_t, std::micro> m_period;
        std::atomic_bool m_stop_task = false;

        TaskHandle_t m_task = {};
        bool m_task_created = false;
    };
}
