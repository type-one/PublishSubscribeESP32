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
#include "tools/platform_helpers.hpp"
#include "tools/sync_queue.hpp"

namespace tools
{
    template <typename Context>
    class worker_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        worker_task() = delete;

        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;

        worker_task(call_back&& startup_routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_context(context)
        {
            m_task_created = task_create(&m_task, this->task_name(), run_loop,
                reinterpret_cast<void*>(this), // NOLINT only way to pass the instance as a void* to the task
                this->stack_size(), this->cpu_affinity(), this->priority());
        }

        worker_task(call_back&& startup_routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size)
            : worker_task(std::move(startup_routine), context, task_name, stack_size, base_task::run_on_all_cores,
                base_task::default_priority)
        {
        }

        ~worker_task()
        {
            m_stop_task.store(true);
            xTaskNotify(m_task, 0x01 /* BIT */, eSetBits);

            if (m_task_created)
            {
                vTaskSuspend(m_task);
                vTaskDelete(m_task);
            }
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        void* native_handle() override
        {
            return reinterpret_cast<void*>(&m_task); // NOLINT native handler wrapping as a void*
        }

        void delegate(call_back&& work)
        {
            m_work_queue.emplace(std::move(work));

            // Likewise, bits are set using the xTaskNotify() and xTaskNotifyFromISR() API functions (with their eAction
            // parameter set to eSetBits) in place of the xEventGroupSetBits() and xEventGroupSetBitsFromISR() functions
            // respectively.

            xTaskNotify(m_task, 0x01 /* BIT */, eSetBits);
        }

        void isr_delegate(call_back&& work)
        {
            m_work_queue.isr_emplace(std::move(work));

            BaseType_t x_higher_priority_task_woken = pdFALSE;
            xTaskNotifyFromISR(m_task, 0x01 /* BIT */, eSetBits, &x_higher_priority_task_woken);
            portYIELD_FROM_ISR(x_higher_priority_task_woken);
        }

    private:
        static void run_loop(void* object_instance)
        {
            worker_task* instance = reinterpret_cast<worker_task*>( // NOLINT only way to convert the passed void* to
                                                                    // the task to a object instance
                object_instance);
            const std::string task_name = instance->task_name();

            // execute given startup function
            instance->m_startup_routine(instance->m_context, task_name);

            while (!instance->m_stop_task.load())
            {
                // A direct to task notification is an event sent directly to a task, rather than indirectly to a task
                // via an intermediary object such as a queue, event group or semaphore. Unblocking an RTOS task with a
                // direct notification is 45% faster and uses less RAM than unblocking a task with a binary semaphore.

                // RTOS task notifications can only be used when there is only one task that can be the recipient of the
                // event. This condition is however met in the majority of real world use cases, such as an interrupt
                // unblocking a task that will process the data received by the interrupt.

                constexpr const TickType_t x_block_time = portMAX_DELAY; /* Block indefinitely. */

                std::uint32_t ul_notified_value = 0U;
                (void)xTaskNotifyWait(pdFALSE, /* Don't clear bits on entry. */
                    ULONG_MAX,                 /* Clear all bits on exit. */
                    &ul_notified_value,        /* Stores the notified value. */
                    x_block_time);

                while (!instance->m_work_queue.empty())
                {
                    auto work = instance->m_work_queue.front();
                    instance->m_work_queue.pop();

                    work(instance->m_context, task_name);
                }
            } // run loop

            vTaskSuspend(NULL);
        }

        call_back m_startup_routine;
        tools::sync_queue<call_back> m_work_queue;
        std::shared_ptr<Context> m_context;

        std::atomic_bool m_stop_task = false;

        TaskHandle_t m_task = {};
        bool m_task_created = false;
    };
}
