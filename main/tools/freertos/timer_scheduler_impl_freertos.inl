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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include "tools/platform_helpers.hpp"
#include "tools/timer_scheduler.hpp"

namespace
{
    void timer_callback(tools::timer_handle x_timer)
    {
        auto* context = reinterpret_cast<tools::timer_scheduler::timer_context*>(pvTimerGetTimerID(x_timer));
        if (nullptr != context)
        {
            (context->m_callback)(x_timer);

            if (context->m_auto_release)
            {
                auto* timer_scheduler = context->m_this; // NOLINT initialized to this object instance
                delete context;
                timer_scheduler->remove_and_delete_timer(x_timer);
            }
        }
    }
}

namespace tools
{

    timer_scheduler::~timer_scheduler()
    {
        std::lock_guard guard(m_mutex);
        for (auto hnd : m_active_timers)
        {
            constexpr const int timeout_ticks = 100;
            xTimerStop(hnd, static_cast<TickType_t>(timeout_ticks));
            tools::sleep_for(1);
            xTimerDelete(hnd, static_cast<TickType_t>(timeout_ticks));
        }
    }

    timer_handle timer_scheduler::add_tick(const std::string& timer_name,  // NOLINT keep common platform interface
        const TickType_t period,                                           // NOLINT keep common platform interface
        std::function<void(timer_handle)>&& handler, timer_type type)      // NOLINT keep common platform interface
    {
        auto context = std::make_unique<timer_context>();
        context->m_callback = std::move(handler); // NOLINT keep common platform interface
        context->m_auto_release = !auto_reload;
        context->m_this = this;

        // https://mcuoneclipse.com/2018/05/27/tutorial-understanding-and-using-freertos-software-timers/
        // https://freertos.org/Documentation/02-Kernel/04-API-references/11-Software-timers/01-xTimerCreate
        // https://stackoverflow.com/questions/71199868/c-use-a-class-non-static-method-as-a-function-pointer-callback-in-freertos-xti
        timer_handle hnd = nullptr;

        const bool auto_reload = (timer_type::periodic == type);

        if (auto_reload)
        {
            hnd = xTimerCreate(timer_name.c_str(), period, pdTRUE, context.get(), timer_callback);
        }
        else
        {
            hnd = xTimerCreate(timer_name.c_str(), period, pdFALSE, context.release(), timer_callback);
        }

        if (nullptr != hnd)
        {
            {
                std::lock_guard guard(m_mutex);

                m_active_timers.emplace_back(hnd);

                if (auto_reload)
                {
                    context->m_timer_handle = hnd;
                    m_contexts.emplace_back(std::move(context));
                }
            }

            constexpr const int start_timeout_ticks = 100;
            while (xTimerStart(hnd, static_cast<TickType_t>(start_timeout_ticks)) != pdPASS)
            {
            }
        }

        return hnd;
    }

    timer_handle timer_scheduler::add(const std::string& timer_name, std::uint64_t period,
        std::function<void(timer_handle)>&& handler, timer_type type)
    {
        return add_tick(timer_name, pdMS_TO_TICKS(period), std::move(handler), type);
    }

    timer_handle timer_scheduler::add(const std::string& timer_name,
        const std::chrono::duration<std::uint64_t, std::micro>& period, std::function<void(timer_handle)>&& handler,
        timer_type type)
    {
        const TickType_t x_period = static_cast<TickType_t>((pdMS_TO_TICKS(period.count()) / 1000U));
        return add_tick(timer_name, x_period, std::move(handler), type);
    }

    bool timer_scheduler::remove(timer_handle hnd)
    {
        constexpr const int stop_timeout_ticks = 100;
        while (xTimerStop(hnd, static_cast<TickType_t>(stop_timeout_ticks)) != pdPASS)
        {
        }

        {
            std::lock_guard guard(m_mutex);
            auto itr = std::find_if(m_contexts.begin(), m_contexts.end(),
                [&hnd](const auto& context) -> bool { return (context->m_timer_handle == hnd); });

            if (itr != m_contexts.end())
            {
                m_contexts.erase(itr);
            }
        }

        remove_and_delete_timer(hnd);

        return true;
    }

    void timer_scheduler::remove_and_delete_timer(timer_handle hnd)
    {
        {
            std::lock_guard guard(m_mutex);

            auto itr = std::find_if(m_active_timers.begin(), m_active_timers.end(),
                [&hnd](const auto* active_hnd) -> bool { return (active_hnd == hnd); });

            if (itr != m_active_timers.end())
            {
                m_active_timers.erase(itr);
            }
        }

        constexpr const int delete_timeout_ticks = 100;
        xTimerDelete(hnd, static_cast<TickType_t>(delete_timeout_ticks));
    }

}
