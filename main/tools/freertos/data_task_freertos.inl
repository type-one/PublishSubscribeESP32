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
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "tools/base_task.hpp"
#include "tools/logger.hpp"
#include "tools/platform_helpers.hpp"

namespace tools
{
    /**
     * @brief Class template for managing FreeRTOS tasks that process data.
     *
     * This class template provides a framework for creating and managing FreeRTOS tasks that process data of a
     * specified type. It ensures that the data type is standard layout and trivial, and provides mechanisms for task
     * startup, data processing, and task termination.
     *
     * @tparam Context The type of the context object shared among tasks.
     * @tparam DataType The type of data to be processed by the task.
     */
    template <typename Context, typename DataType>
#if __cplusplus >= 202002L
    requires std::is_standard_layout_v<DataType> && std::is_trivial_v<DataType>
#endif
    class data_task : public base_task // NOLINT base_task is non copyable and non movable
    {
        static_assert(std::is_standard_layout<DataType>::value, "DataType has to provide standard layout");
        static_assert(std::is_trivial<DataType>::value, "DataType has to be trivial type");

    public:
        data_task() = delete;

        /**
         * @brief Type alias for a callback function used in the context of FreeRTOS tasks.
         *
         * This callback function takes a shared pointer to a Context object and a task name as parameters.
         *
         * @param context A shared pointer to the Context object.
         * @param task_name The name of the task as a string.
         */
        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;

        /**
         * @brief Type alias for a callback function that processes data.
         *
         * This callback function is used to process data within a specific context and task.
         *
         * @param context A shared pointer to the Context object.
         * @param data The data to be processed.
         * @param task_name The name of the task that is processing the data.
         */
        using data_call_back = std::function<void(
            const std::shared_ptr<Context>& context, const DataType& data, const std::string& task_name)>;

        /**
         * @brief Constructor for the data_task class.
         *
         * @param startup_routine The startup routine callback function.
         * @param process_routine The process routine callback function.
         * @param context Shared pointer to the Context object.
         * @param data_queue_depth The depth of the data queue.
         * @param task_name The name of the task.
         * @param stack_size The stack size for the task.
         * @param cpu_affinity The CPU affinity for the task.
         * @param priority The priority of the task.
         */
        data_task(call_back&& startup_routine, data_call_back&& process_routine,
            const std::shared_ptr<Context>& context, std::size_t data_queue_depth, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_process_routine(std::move(process_routine))
            , m_context(context)
        {
            // FreeRTOS platform
            m_data_queue = xQueueCreate(data_queue_depth, sizeof(DataType));

            if (nullptr == m_data_queue)
            {
                LOG_ERROR("FATAL error: xQueueCreate() failed for task %s", this->task_name().c_str());
            }

            m_task_created = task_create(&m_task, this->task_name(), run_loop,
                reinterpret_cast<void*>(this), // NOLINT only way to pass the instance as a void* to the task
                this->stack_size(), this->cpu_affinity(), this->priority());
        }

        /**
         * @brief Constructor for the data_task class with default priority and cpu affinity.
         *
         * @param startup_routine The startup routine callback function.
         * @param process_routine The process routine callback function.
         * @param context Shared pointer to the Context object.
         * @param data_queue_depth The depth of the data queue.
         * @param task_name The name of the task.
         * @param stack_size The stack size for the task.
         */
        data_task(call_back&& startup_routine, data_call_back&& process_routine,
            const std::shared_ptr<Context>& context, std::size_t data_queue_depth, const std::string& task_name,
            std::size_t stack_size)
            : data_task(std::move(startup_routine), std::move(process_routine), context, data_queue_depth, task_name,
                stack_size, base_task::run_on_all_cores, base_task::default_priority)
        {
        }

        /**
         * @brief Destructor for the data_task class.
         *
         * This destructor stops the task by setting the m_stop_task flag to true.
         * If the data queue is not null, it sends a dummy value to the queue to unblock any waiting operations.
         * If the task was created, it suspends and deletes the task.
         */
        ~data_task()
        {
            // FreeRTOS platform

            m_stop_task.store(true);
            if (nullptr != m_data_queue)
            {
                constexpr const TickType_t x_block_time = 20 * portTICK_PERIOD_MS;
                DataType value = {};
                xQueueSend(m_data_queue, &value, x_block_time);
            }

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
         * of the task, which is wrapped as a void pointer.
         *
         * @return A void pointer to the native handle of the task.
         */
        void* native_handle() override
        {
            return reinterpret_cast<void*>(&m_task); // NOLINT native handler wrapping as a void*
        }

        /**
         * @brief Submits data to the FreeRTOS queue.
         *
         * This function sends the provided data to the FreeRTOS queue if the queue is not null.
         * It blocks indefinitely until the data is successfully sent to the queue.
         *
         * @param data The data to be submitted to the queue.
         */
        void submit(const DataType& data)
        {
            // FreeRTOS platform

            if (nullptr != m_data_queue)
            {
                constexpr const TickType_t x_block_time = portMAX_DELAY; /* NOLINT init to Block indefinitely. */
                xQueueSend(m_data_queue, &data, x_block_time);
            }
        }

        /**
         * @brief Submits data to the queue from an ISR context.
         *
         * This function is designed to be called from an Interrupt Service Routine (ISR).
         * It sends the provided data to the FreeRTOS queue and yields if a higher priority
         * task was woken up by the send operation.
         *
         * @param data The data to be sent to the queue.
         */
        void isr_submit(const DataType& data)
        {
            // FreeRTOS platform

            if (nullptr != m_data_queue)
            {
                BaseType_t px_higher_priority_task_woken = pdFALSE; // NOLINT initialized with pdFALSE
                xQueueSendFromISR(m_data_queue, &data, &px_higher_priority_task_woken);
                portYIELD_FROM_ISR(px_higher_priority_task_woken);
            }
        }

    private:
        /**
         * @brief FreeRTOS task run loop for the data_task class.
         *
         * This function serves as the main loop for a FreeRTOS task. It executes a startup routine,
         * then continuously processes data from a queue until the task is signaled to stop.
         *
         * @param object_instance Pointer to the instance of the data_task class.
         */
        static void run_loop(void* object_instance)
        {
            // FreeRTOS platform

            data_task* instance = reinterpret_cast<data_task*>( // NOLINT only way to convert the passed void* to the
                                                                // task to a object instance
                object_instance);

            const std::string task_name = instance->task_name();
            constexpr const TickType_t x_block_time = portMAX_DELAY; /* Block indefinitely. */

            // execute given startup function
            instance->m_startup_routine(instance->m_context, task_name);

            while (!instance->m_stop_task.load())
            {
                if (nullptr != instance->m_data_queue)
                {
                    DataType data = {};
                    while (xQueueReceive(instance->m_data_queue, &data, x_block_time))
                    {
                        instance->m_process_routine(instance->m_context, data, task_name);
                    }
                }
            } // run loop

            vTaskSuspend(NULL);
        }

        call_back m_startup_routine;
        data_call_back m_process_routine;
        QueueHandle_t m_data_queue = {};
        std::shared_ptr<Context> m_context;

        std::atomic_bool m_stop_task = false;

        TaskHandle_t m_task = {};
        bool m_task_created = false;
    };
}
