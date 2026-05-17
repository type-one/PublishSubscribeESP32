/**
 * @file timer_scheduler_impl_std.inl
 * @brief Timer scheduler class for managing timers.
 *
 * This file contains the implementation of the timer_scheduler class, which provides
 * functionality to add and remove timers with specified periods and handlers.
 *
 * @author Laurent Lardinois
 * @date February 2025
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "cpptime/cpptime.hpp"
#include "tools/timer_scheduler.hpp"

namespace tools
{
    timer_handle timer_scheduler::add(const std::string& timer_name, std::uint64_t period,
        std::function<void(timer_handle)>&& handler, timer_type type)
    {
        return add(timer_name, period, std::move(handler), type, timer_resolution_policy::low_resolution);
    }

    timer_handle timer_scheduler::add(const std::string& timer_name, std::uint64_t period,
        std::function<void(timer_handle)>&& handler, timer_type type, timer_resolution_policy policy)
    {
        (void)timer_name;
        (void)policy;
        // inputs are in us
        // auto-reload true:  start after period and then repeat every period
        // auto-reload false: start once after period
        const bool auto_reload = (timer_type::periodic == type);
        constexpr const std::uint64_t micro_sec_coeff = 1000U;
        auto user_handler = std::move(handler);
        auto hnd = m_timer_scheduler.add(
            period * micro_sec_coeff, [user_handler = std::move(user_handler)](CppTime::timer_id internal_id) mutable
            { user_handler(internal_id + 1U); }, auto_reload ? (period * micro_sec_coeff) : 0U);
        return hnd + 1U; // valid handle is non zero
    }

    timer_handle timer_scheduler::add(const std::string& timer_name,
        const std::chrono::duration<std::uint64_t, std::micro>& period, std::function<void(timer_handle)>&& handler,
        timer_type type)
    {
        return add(timer_name, period, std::move(handler), type, timer_resolution_policy::low_resolution);
    }

    timer_handle timer_scheduler::add(const std::string& timer_name,
        const std::chrono::duration<std::uint64_t, std::micro>& period, std::function<void(timer_handle)>&& handler,
        timer_type type, timer_resolution_policy policy)
    {
        (void)timer_name;
        (void)policy;
        const bool auto_reload = (timer_type::periodic == type);
        auto user_handler = std::move(handler);
        auto hnd = m_timer_scheduler.add(
            period.count(), [user_handler = std::move(user_handler)](CppTime::timer_id internal_id) mutable
            { user_handler(internal_id + 1U); }, auto_reload ? period.count() : 0U);
        return hnd + 1U; // valid handle is non zero
    }

    bool timer_scheduler::remove(timer_handle hnd)
    {
        return m_timer_scheduler.remove(hnd - 1U); // valid handle minus 1 for the cpptime api
    }
}
