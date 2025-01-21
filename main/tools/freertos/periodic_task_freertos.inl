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
#include <cstdio>
#include <functional>
#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "tools/non_copyable.hpp"

namespace tools
{
    template <typename Context>
    class periodic_task : public non_copyable
    {

    public:
        periodic_task() = delete;

        periodic_task(std::function<void(std::shared_ptr<Context>)>&& startup_routine,
            std::function<void(std::shared_ptr<Context>)>&& periodic_routine, std::shared_ptr<Context> context,
            const std::string& task_name, const std::chrono::duration<int, std::micro>& period, std::size_t stack_size = 2048U)
            : m_startup_routine(std::move(startup_routine))
            , m_periodic_routine(std::move(periodic_routine))
            , m_context(context)
            , m_task_name(task_name)
            , m_period(period)
            , m_stack_size(stack_size)
        {
            auto ret = xTaskCreate(periodic_call, m_task_name.c_str(), m_stack_size,
                reinterpret_cast<void*>(this), /* Parameter passed into the task. */
                tskIDLE_PRIORITY, &m_task);

            if (pdPASS == ret)
            {
                m_task_created = true;
            }
            else
            {
                fprintf(stderr, "FATAL error: xTaskCreate() failed for task %s (%s line %d function %s)\n", m_task_name.c_str(), __FILE__,
                    __LINE__, __FUNCTION__);
            }
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

        void* native_handle() const { return reinterpret_cast<void*>(m_task); }

    private:
        static void periodic_call(void* object_instance)
        {
            periodic_task* instance = reinterpret_cast<periodic_task*>(object_instance);

            auto x_last_wake_time = xTaskGetTickCount();
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(instance->m_period);
            const TickType_t x_period = (pdMS_TO_TICKS(us.count()) / 1000);

            // execute given startup function
            instance->m_startup_routine(instance->m_context);

            while (!instance->m_stop_task.load())
            {
                vTaskDelayUntil(&x_last_wake_time, x_period);

                // execute given periodic function
                instance->m_periodic_routine(instance->m_context);
            }
        }

        std::function<void(std::shared_ptr<Context>)> m_startup_routine;
        std::function<void(std::shared_ptr<Context>)> m_periodic_routine;
        std::shared_ptr<Context> m_context;
        std::string m_task_name;
        std::chrono::duration<int, std::micro> m_period;
        const std::size_t m_stack_size;
        std::atomic_bool m_stop_task = false;

        TaskHandle_t m_task;
        bool m_task_created = false;
    };
}
