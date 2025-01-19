//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois      //
//                                                                             //
// https://github.com/type-one/PublishSubscribe                                //
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

#include "sync_object.hpp"

#include "platform_detection.hpp"

#if defined(FREERTOS_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <cstdio>
#else
#include <atomic>
#include <condition_variable>
#include <mutex>
#endif

namespace tools
{

#if defined(FREERTOS_PLATFORM)
    sync_object::sync_object(bool initial_state)
        : m_event_group(xEventGroupCreate())
    {
        if (nullptr == m_event_group)
        {
            fprintf(stderr, "FATAL error: xEventGroupCreate() failed (%s line %d function %s)\n", __FILE__, __LINE__, __FUNCTION__);
        }
    }

    sync_object::~sync_object()
    {
        if (nullptr != m_event_group)
        {
            vEventGroupDelete(m_event_group);
        }
    }

    void sync_object::signal()
    {
        if (nullptr != m_event_group)
        {
            xEventGroupSetBits(m_event_group, BIT0);
        }
    }

    void sync_object::isr_signal()
    {
        if (nullptr != m_event_group)
        {
            BaseType_t px_higher_priority_task_woken = 0;
            xEventGroupSetBitsFromISR(m_event_group, BIT0, &px_higher_priority_task_woken);
        }
    }

    void sync_object::wait_for_signal()
    {
        if (nullptr != m_event_group)
        {
            xEventGroupWaitBits(m_event_group, BIT0, pdTRUE /* clear on exit */, pdFALSE /* wait for all bits */, portMAX_DELAY);
        }
    }

    void sync_object::wait_for_signal(const std::chrono::duration<int, std::micro>& timeout)
    {
        if (nullptr != m_event_group)
        {
            const TickType_t ticks_to_wait = (timeout.count() * portTICK_PERIOD_MS) / 1000;
            xEventGroupWaitBits(m_event_group, BIT0, pdTRUE, pdFALSE, ticks_to_wait);
        }
    }

#else // standard implementation

    sync_object::sync_object(bool initial_state)
        : m_signaled(initial_state)
        , m_stop(false)
    {
    }

    sync_object::~sync_object()
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_signaled = true;
            m_stop = true;
        }
        m_cond.notify_all();
    }

    void sync_object::signal()
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_signaled = true;
        }
        m_cond.notify_one();
    }

    void sync_object::wait_for_signal()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [&]() { return m_signaled; });
        m_signaled = m_stop;
    }

    void sync_object::wait_for_signal(const std::chrono::duration<int, std::micro>& timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait_for(lock, timeout, [&]() { return m_signaled; });
        m_signaled = m_stop;
    }

#endif // end standard implementation

} // end namespace tools
