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

#include <cstdio>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "tools/logger.hpp"
#include "tools/non_copyable.hpp"

namespace tools
{
    class critical_section : public non_copyable // NOLINT inherits from non copyable and non movable
    {
    public:
        critical_section()
            : m_mutex(xSemaphoreCreateMutex())
        {
            if (nullptr == m_mutex)
            {
                LOG_ERROR("FATAL error: xSemaphoreCreateMutex() failed");
            }
        }

        ~critical_section()
        {
            if (nullptr != m_mutex)
            {
                vSemaphoreDelete(m_mutex);
            }
        }

        void lock() // NOLINT keep same interface than standard mutex
        {
            if (nullptr != m_mutex)
            {
                while (xSemaphoreTake(m_mutex, static_cast<TickType_t>(100)) != pdTRUE)
                {
                }
            }
        }

        void isr_lock() // NOLINT keep same interface than standard implementation
        {
            if (nullptr != m_mutex)
            {
                BaseType_t px_higher_priority_task_woken = pdFALSE; // NOLINT initialized to pdFALSE
                while (xSemaphoreTakeFromISR(m_mutex, &px_higher_priority_task_woken) != pdTRUE)
                {
                    portYIELD_FROM_ISR(px_higher_priority_task_woken);
                    px_higher_priority_task_woken = pdFALSE;
                }
            }
        }

        bool try_lock() // NOLINT keep same interface than standard mutex
        {
            bool result = false;

            if (nullptr != m_mutex)
            {
                result = (pdTRUE == xSemaphoreTake(m_mutex, static_cast<TickType_t>(0)));
            }

            return result;
        }

        bool try_isr_lock() // NOLINT keep same interface than standard implementation
        {
            bool result = false;

            if (nullptr != m_mutex)
            {
                BaseType_t px_higher_priority_task_woken = pdFALSE; // NOLINT initialized to pdFALSE
                result = (pdTRUE == xSemaphoreTakeFromISR(m_mutex, &px_higher_priority_task_woken));
                portYIELD_FROM_ISR(px_higher_priority_task_woken);
            }

            return result;
        }

        void unlock() // NOLINT keep same interface than standard mutex
        {
            if (nullptr != m_mutex)
            {
                xSemaphoreGive(m_mutex);
            }
        }

        void isr_unlock() // NOLINT keep same interface than standard implementation
        {
            if (nullptr != m_mutex)
            {
                BaseType_t px_higher_priority_task_woken = pdFALSE; // NOLINT initialized to pdFALSE
                xSemaphoreGiveFromISR(m_mutex, &px_higher_priority_task_woken);
                portYIELD_FROM_ISR(px_higher_priority_task_woken);
            }
        }

    private:
        SemaphoreHandle_t m_mutex = {};
    };

    template <typename T>
    class isr_lock_guard : public non_copyable // NOLINT inherits from non copyable and non movable
    {
    public:
        isr_lock_guard() = delete;
        isr_lock_guard(T& lockable_object)
            : m_lockable_object_ref(lockable_object)
        {
            m_lockable_object_ref.isr_lock();
        }

        ~isr_lock_guard()
        {
            m_lockable_object_ref.isr_unlock();
        }

    private:
        T& m_lockable_object_ref;
    };
}
