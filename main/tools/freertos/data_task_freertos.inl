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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "tools/base_task.hpp"
#include "tools/logger.hpp"
#include "tools/platform_detection.hpp"
#include "tools/sync_queue.hpp"

#if defined(ESP_PLATFORM)
#include <freertos/idf_additions.h>
#endif

namespace tools
{
    template <typename Context, typename DataType, std::size_t Capacity>
#if __cplusplus >= 202002L
        requires std::is_standard_layout_v<DataType> && std::is_trivial_v<DataType>
#endif
    class data_task : public base_task
    {

    public:
        data_task() = delete;

        using call_back = std::function<void(std::shared_ptr<Context>, const std::string& task_name)>;
        using data_call_back = std::function<void(std::shared_ptr<Context>, const DataType& data, const std::string& task_name)>;

        data_task(call_back&& startup_routine, data_call_back&& process_routine, std::shared_ptr<Context> context,
            const std::string& task_name, std::size_t stack_size, int cpu_affinity = base_task::run_on_all_cores,
            int priority = base_task::default_priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_process_routine(std::move(process_routine))
            , m_context(context)
        {

            m_data_queue = xQueueCreate(Capacity, sizeof(DataType));

            if (nullptr == m_data_queue)
            {
                LOG_ERROR("FATAL error: xQueueCreate() failed for task %s", this->task_name().c_str());
            }

            BaseType_t ret = 0;

            UBaseType_t freertos_priority = tskIDLE_PRIORITY;

            if (priority >= 0)
            {
                // https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/01-xTaskCreate
                freertos_priority = std::clamp(static_cast<UBaseType_t>(priority + tskIDLE_PRIORITY),
                    static_cast<UBaseType_t>(tskIDLE_PRIORITY), static_cast<UBaseType_t>(configMAX_PRIORITIES - 1));
            }

#if defined(ESP_PLATFORM)
            // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_idf.html
            if (cpu_affinity >= 0)
            {
                ret = xTaskCreatePinnedToCore(run_loop, this->task_name().c_str(), this->stack_size(),
                    reinterpret_cast<void*>(this), /* Parameter passed into the task. */
                    freertos_priority, &m_task, static_cast<BaseType_t>(cpu_affinity));

                if (pdPASS != ret)
                {
                    LOG_ERROR("xTaskCreatePinnedToCore() failed for task %s on core %d", this->task_name().c_str(), cpu_affinity);
                }
            }
#endif

            if (pdPASS != ret)
            {
                ret = xTaskCreate(run_loop, this->task_name().c_str(), this->stack_size(),
                    reinterpret_cast<void*>(this), /* Parameter passed into the task. */
                    freertos_priority, &m_task);
            }

            if (pdPASS == ret)
            {
#if defined(configUSE_CORE_AFFINITY) && !defined(ESP_PLATFORM)
                if (cpu_affinity >= 0)
                {
                    // https://github.com/dogusyuksel/rtos_hal_stm32
                    // https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/13-Symmetric-multiprocessing-introduction
                    UBaseType_t mask = (1 << cpu_affinity);
                    vTaskCoreAffinitySet(m_task, mask);
                }
#endif

                m_task_created = true;
            }
            else
            {
                LOG_ERROR("FATAL error: xTaskCreate() failed for task %s", this->task_name().c_str());
            }
        }

        ~data_task()
        {
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
        virtual void* native_handle() override
        {
            return reinterpret_cast<void*>(&m_task);
        }

        void submit(const DataType& data)
        {
            if (nullptr != m_data_queue)
            {
                constexpr const TickType_t x_block_time = portMAX_DELAY; /* Block indefinitely. */
                xQueueSend(m_data_queue, &data, x_block_time);
            }
        }

        void isr_submit(const DataType& data)
        {
            if (nullptr != m_data_queue)
            {
                BaseType_t px_higher_priority_task_woken = pdFALSE;
                xQueueSendFromISR(m_data_queue, &data, &px_higher_priority_task_woken);
                portYIELD_FROM_ISR(px_higher_priority_task_woken);
            }
        }

    private:
        static void run_loop(void* object_instance)
        {
            data_task* instance = reinterpret_cast<data_task*>(object_instance);

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
        }

        call_back m_startup_routine;
        data_call_back m_process_routine;
        QueueHandle_t m_data_queue;
        std::shared_ptr<Context> m_context;

        std::atomic_bool m_stop_task = false;

        TaskHandle_t m_task;
        bool m_task_created = false;
    };
}
