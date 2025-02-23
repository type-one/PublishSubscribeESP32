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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "tools/base_task.hpp"
#include "tools/platform_helpers.hpp"

namespace tools
{
    template <typename Context>
    class periodic_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        periodic_task() = delete;

        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;

        periodic_task(call_back&& startup_routine, call_back&& periodic_routine, const std::shared_ptr<Context>& context,
            const std::string& task_name, const std::chrono::duration<std::uint64_t, std::micro>& period,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_periodic_routine(std::move(periodic_routine))
            , m_context(context)
            , m_period(period)
        {
            m_task_created = task_create(&m_task, this->task_name(), periodic_call, reinterpret_cast<void*>(this),
                this->stack_size(), this->cpu_affinity(), this->priority());
        }

        periodic_task(call_back&& startup_routine, call_back&& periodic_routine, const std::shared_ptr<Context>& context,
            const std::string& task_name, const std::chrono::duration<std::uint64_t, std::micro>& period,
            std::size_t stack_size)
            : periodic_task(std::move(startup_routine), std::move(periodic_routine), context, task_name, period, stack_size, base_task::run_on_all_cores, base_task::default_priority)
        {            
        }

        ~periodic_task()
        {
            m_stop_task.store(true);

            if (m_task_created)
            {
                vTaskSuspend(m_task);
                vTaskDelete(m_task);
            }
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        void* native_handle() override
        {
            return reinterpret_cast<void*>(&m_task);
        }

    private:
        static void periodic_call(void* object_instance)
        {
            periodic_task* instance = reinterpret_cast<periodic_task*>(object_instance);

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
