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

#include "tools/non_copyable.hpp"

namespace tools
{
    class critical_section : public non_copyable
    {
    public:
        critical_section()
            : m_mutex(xSemaphoreCreateMutex())
        {
            if (nullptr == m_mutex)
            {
                fprintf(stderr, "FATAL error: xSemaphoreCreateMutex() failed (%s line %d function %s)\n", __FILE__, __LINE__, __FUNCTION__);
            }
        }

        ~critical_section()
        {
            if (nullptr != m_mutex)
            {
                vSemaphoreDelete(m_mutex);
            }
        }

        void lock()
        {
            if (nullptr != m_mutex)
            {
                while (xSemaphoreTake(m_mutex, static_cast<TickType_t>(100)) != pdTRUE)
                {
                }
            }
        }

        void isr_lock()
        {
            if (nullptr != m_mutex)
            {
                BaseType_t px_higher_priority_task_woken = 0;
                while (xSemaphoreTakeFromISR(m_mutex, &px_higher_priority_task_woken) != pdTRUE)
                {
                }
            }
        }

        bool try_lock() { return (pdTRUE == xSemaphoreTake(m_mutex, static_cast<TickType_t>(0))); }

        bool try_isr_lock()
        {
            BaseType_t px_higher_priority_task_woken = 0;
            return (pdTRUE == xSemaphoreTakeFromISR(m_mutex, &px_higher_priority_task_woken));
        }

        void unlock()
        {
            if (nullptr != m_mutex)
            {
                xSemaphoreGive(m_mutex);
            }
        }

        void isr_unlock()
        {
            if (nullptr != m_mutex)
            {
                BaseType_t px_higher_priority_task_woken = 0;
                xSemaphoreGiveFromISR(m_mutex, &px_higher_priority_task_woken);
            }
        }

    private:
        SemaphoreHandle_t m_mutex;
    };

    template <typename T>
    class isr_lock_guard : public non_copyable
    {
    public:
        isr_lock_guard() = delete;
        isr_lock_guard(T& lockable_object)
            : m_lockable_object_ref(lockable_object)
        {
            m_lockable_object_ref.isr_lock();
        }

        ~isr_lock_guard() { m_lockable_object_ref.isr_unlock(); }

    private:
        T& m_lockable_object_ref;
    };
}
