/**
 * @file timer_scheduler_impl_freertos.inl
 * @brief Implementation of the timer scheduler using FreeRTOS for the Publish/Subscribe Pattern.
 *
 * This file contains the implementation of the timer scheduler using FreeRTOS timers.
 * It provides functions to add, remove, and manage timers within the FreeRTOS environment.
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#endif

#include "tools/platform_helpers.hpp"
#include "tools/timer_scheduler.hpp"

namespace
{
    /**
     * @brief Callback function for FreeRTOS timer events.
     *
     * This function is called when a FreeRTOS timer expires. It retrieves the timer context,
     * executes the associated callback, and handles auto-release of the timer if specified.
     *
     * @param x_timer Handle to the FreeRTOS timer that triggered the callback.
     */
    void timer_callback(TimerHandle_t x_timer)
    {
        // FreeRTOS platform

        auto* context = reinterpret_cast<tools::timer_scheduler::timer_context*>( // NOLINT only way to cast the void*
                                                                                  // timer param to context instance
            pvTimerGetTimerID(x_timer));
        if (nullptr != context)
        {
            (context->m_callback)(context->m_timer_handle);

            if (context->m_auto_release)
            {
                auto* timer_scheduler = context->m_this; // NOLINT initialized to this object instance
                timer_scheduler->remove_and_delete_timer(context->m_timer_handle);
            }
        }
    }

#if defined(ESP_PLATFORM)
    /**
     * @brief Callback function for esp_timer events.
     *
     * @param arg Context pointer stored when the timer was created.
     */
    void esp_timer_callback(void* arg)
    {
        auto* context = static_cast<tools::timer_scheduler::timer_context*>(arg);
        if (nullptr != context)
        {
            (context->m_callback)(context->m_timer_handle);

            if (context->m_auto_release)
            {
                auto* timer_scheduler = context->m_this;
                timer_scheduler->remove_and_delete_timer(context->m_timer_handle);
            }
        }
    }
#endif
}

namespace tools
{
    timer_scheduler::~timer_scheduler()
    {
        // FreeRTOS platform

        std::scoped_lock<tools::critical_section> guard(m_mutex);
        for (const auto& context : m_contexts)
        {
            if (nullptr == context)
            {
                continue;
            }

            if (timer_backend_kind::freertos_tick == context->m_backend)
            {
                constexpr const int timeout_ticks = 100;
                xTimerStop(
                    static_cast<TimerHandle_t>(context->m_native_handle), static_cast<TickType_t>(timeout_ticks));
                tools::sleep_for(1);
                xTimerDelete(
                    static_cast<TimerHandle_t>(context->m_native_handle), static_cast<TickType_t>(timeout_ticks));
            }
#if defined(ESP_PLATFORM)
            else
            {
                (void)esp_timer_stop(static_cast<esp_timer_handle_t>(context->m_native_handle));
                (void)esp_timer_delete(static_cast<esp_timer_handle_t>(context->m_native_handle));
            }
#endif
        }
    }

    timer_handle timer_scheduler::add_tick(const std::string& timer_name, const TickType_t period,
        std::function<void(timer_handle)>&& handler, timer_type type)
    {
        // FreeRTOS platform

        auto context = std::make_unique<timer_context>();
        context->m_callback = std::move(handler); // NOLINT keep common platform interface
        const bool auto_reload = (timer_type::periodic == type);
        context->m_auto_release = (timer_type::one_shot == type);
        context->m_backend = timer_backend_kind::freertos_tick;
        context->m_policy = timer_resolution_policy::low_resolution;
        context->m_this = this;
        context->m_timer_handle = m_next_timer_handle++;
        const timer_handle timer_id = context->m_timer_handle;

        // https://mcuoneclipse.com/2018/05/27/tutorial-understanding-and-using-freertos-software-timers/
        // https://freertos.org/Documentation/02-Kernel/04-API-references/11-Software-timers/01-xTimerCreate
        // https://stackoverflow.com/questions/71199868/c-use-a-class-non-static-method-as-a-function-pointer-callback-in-freertos-xti
        const BaseType_t reload_flag = auto_reload ? pdTRUE : pdFALSE;
        TimerHandle_t hnd = xTimerCreate(timer_name.c_str(), period, reload_flag, context.get(), timer_callback);

        if (nullptr != hnd)
        {
            context->m_native_handle = hnd;
            {
                std::scoped_lock<tools::critical_section> guard(m_mutex);
                m_contexts.emplace_back(std::move(context));
            }

            constexpr const int start_timeout_ticks = 100;
            while (xTimerStart(hnd, static_cast<TickType_t>(start_timeout_ticks)) != pdPASS)
            {
            }

            return timer_id;
        }

        return 0;
    }

    timer_handle timer_scheduler::add(const std::string& timer_name, std::uint64_t period,
        std::function<void(timer_handle)>&& handler, timer_type type)
    {
        return add(timer_name, period, std::move(handler), type, timer_resolution_policy::low_resolution);
    }

    timer_handle timer_scheduler::add(const std::string& timer_name, std::uint64_t period,
        std::function<void(timer_handle)>&& handler, timer_type type, timer_resolution_policy policy)
    {
        if (timer_resolution_policy::high_resolution == policy)
        {
#if defined(ESP_PLATFORM)
            constexpr const std::uint64_t us_per_ms = 1000U;
            return add_esp_timer(timer_name, period * us_per_ms, std::move(handler), type);
#else
            (void)policy;
#endif
        }

        TickType_t tick_period = pdMS_TO_TICKS(static_cast<TickType_t>(period));
        if ((period > 0U) && (tick_period == 0U))
        {
            tick_period = 1U;
        }
        return add_tick(timer_name, tick_period, std::move(handler), type);
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
        if (timer_resolution_policy::high_resolution == policy)
        {
#if defined(ESP_PLATFORM)
            return add_esp_timer(timer_name, period.count(), std::move(handler), type);
#else
            (void)policy;
#endif
        }

        constexpr const std::uint64_t us_per_ms = 1000U;
        const std::uint64_t period_ms = (period.count() + (us_per_ms - 1U)) / us_per_ms;
        TickType_t x_period = pdMS_TO_TICKS(static_cast<TickType_t>(period_ms));
        if ((period.count() > 0U) && (x_period == 0U))
        {
            x_period = 1U;
        }
        return add_tick(timer_name, x_period, std::move(handler), type);
    }

    bool timer_scheduler::remove(timer_handle hnd)
    {
        bool success = false;

        if (0 != hnd)
        {
            timer_context* context_ptr = nullptr;

            {
                std::scoped_lock<tools::critical_section> guard(m_mutex);
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
                auto itr = std::ranges::find_if(
                    m_contexts, [&hnd](const auto& context) -> bool { return (context->m_timer_handle == hnd); });
#else
                auto itr = std::find_if(m_contexts.begin(), m_contexts.end(),
                    [&hnd](const auto& context) -> bool { return (context->m_timer_handle == hnd); });
#endif

                if (itr != m_contexts.end())
                {
                    context_ptr = itr->get();
                }
            }

            if (nullptr != context_ptr)
            {
                if (timer_backend_kind::freertos_tick == context_ptr->m_backend)
                {
                    constexpr const int stop_timeout_ticks = 100;
                    while (xTimerStop(static_cast<TimerHandle_t>(context_ptr->m_native_handle),
                               static_cast<TickType_t>(stop_timeout_ticks))
                        != pdPASS)
                    {
                    }
                }
#if defined(ESP_PLATFORM)
                else
                {
                    (void)esp_timer_stop(static_cast<esp_timer_handle_t>(context_ptr->m_native_handle));
                }
#endif

                remove_and_delete_timer(hnd);

                success = true;
            }
        }

        return success;
    }

    void timer_scheduler::remove_and_delete_timer(timer_handle hnd)
    {
        // FreeRTOS platform

        if (0 != hnd)
        {
            std::unique_ptr<timer_context> context_to_delete = {};

            {
                std::scoped_lock<tools::critical_section> guard(m_mutex);
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
                auto itr = std::ranges::find_if(
                    m_contexts, [&hnd](const auto& context) -> bool { return (context->m_timer_handle == hnd); });
#else
                auto itr = std::find_if(m_contexts.begin(), m_contexts.end(),
                    [&hnd](const auto& context) -> bool { return (context->m_timer_handle == hnd); });
#endif

                if (itr != m_contexts.end())
                {
                    context_to_delete = std::move(*itr);
                    m_contexts.erase(itr);
                }
            }

            if (nullptr != context_to_delete)
            {
                if (timer_backend_kind::freertos_tick == context_to_delete->m_backend)
                {
                    constexpr const int delete_timeout_ticks = 100;
                    xTimerDelete(static_cast<TimerHandle_t>(context_to_delete->m_native_handle),
                        static_cast<TickType_t>(delete_timeout_ticks));
                }
#if defined(ESP_PLATFORM)
                else
                {
                    (void)esp_timer_delete(static_cast<esp_timer_handle_t>(context_to_delete->m_native_handle));
                }
#endif
            }
        }
    }

#if defined(ESP_PLATFORM)
    timer_handle timer_scheduler::add_esp_timer(const std::string& timer_name, std::uint64_t period,
        std::function<void(timer_handle)>&& handler, timer_type type)
    {
        auto context = std::make_unique<timer_context>();
        context->m_callback = std::move(handler);
        context->m_auto_release = (timer_type::one_shot == type);
        context->m_backend = timer_backend_kind::esp_timer;
        context->m_policy = timer_resolution_policy::high_resolution;
        context->m_this = this;
        context->m_timer_handle = m_next_timer_handle++;

        esp_timer_create_args_t timer_args = {};
        timer_args.callback = &esp_timer_callback;
        timer_args.arg = context.get();
        timer_args.dispatch_method = ESP_TIMER_TASK;
        timer_args.name = timer_name.c_str();

        esp_timer_handle_t native_handle = nullptr;
        const esp_err_t create_status = esp_timer_create(&timer_args, &native_handle);
        if (ESP_OK != create_status)
        {
            return 0;
        }

        context->m_native_handle = native_handle;
        const timer_handle timer_id = context->m_timer_handle;

        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_contexts.emplace_back(std::move(context));
        }

        const esp_err_t start_status = (timer_type::periodic == type) ? esp_timer_start_periodic(native_handle, period)
                                                                      : esp_timer_start_once(native_handle, period);

        if (ESP_OK != start_status)
        {
            remove_and_delete_timer(timer_id);
            return 0;
        }

        return timer_id;
    }
#endif

}
