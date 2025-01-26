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
#include <freertos/event_groups.h>

#include "tools/sync_object.hpp"

namespace tools
{
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
            BaseType_t px_higher_priority_task_woken = pdFALSE;
            xEventGroupSetBitsFromISR(m_event_group, BIT0, &px_higher_priority_task_woken);
            portYIELD_FROM_ISR(px_higher_priority_task_woken);
        }
    }

    void sync_object::wait_for_signal()
    {
        if (nullptr != m_event_group)
        {
            constexpr const TickType_t x_block_time = portMAX_DELAY; /* Block indefinitely. */
            xEventGroupWaitBits(m_event_group, BIT0, pdTRUE /* clear on exit */, pdFALSE /* wait for all bits */, x_block_time);
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

} // end namespace tools
