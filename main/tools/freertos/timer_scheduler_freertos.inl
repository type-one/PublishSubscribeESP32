/**
 * @file timer_scheduler_freertos.inl
 * @brief Definition of the timer scheduler using FreeRTOS for the Publish/Subscribe Pattern.
 *
 * This file contains the definition of the timer scheduler using FreeRTOS timers.
 * It provides functions to add, remove, and manage timers within the FreeRTOS environment.
 *
 * @author Laurent Lardinois
 * @date February 2025
 */

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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"

namespace tools
{
    /**
     * @brief timer types (one shot or periodic)
     *
     */
    enum class timer_type
    {
        one_shot, ///< one shot timer
        periodic  ///< periodic timer
    };

    /**
     * @brief Alias for the FreeRTOS timer handle type.
     */
    using timer_handle = TimerHandle_t;

    /**
     * @brief Class for managing FreeRTOS timers.
     *
     * The timer_scheduler class provides functionality to create, manage, and delete FreeRTOS timers.
     * It ensures thread safety by using a mutex to protect access to the list of active timers.
     */
    class timer_scheduler : non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        /**
         * @brief Default constructor for the timer_scheduler class.
         */
        timer_scheduler() = default;

        /**
         * @brief Destructor for the timer_scheduler class.
         *
         * This destructor stops and deletes all active timers managed by the timer_scheduler instance.
         * It acquires a lock on the mutex to ensure thread safety while modifying the active timers list.
         * Each timer is stopped with a timeout of 100 ticks, followed by a brief sleep, and then the timer is deleted.
         */
        ~timer_scheduler();

        /**
         * @brief Adds a new timer to the scheduler.
         *
         * This function creates a new timer with the specified name, period, handler, and type,
         * and adds it to the FreeRTOS timer scheduler.
         *
         * If periodic, then the timer will expire repeatedly with a frequency set by the period
         * parameter. If set to one_shot, then the timer will be a one-shot timer.
         *
         * @param timer_name The name of the timer.
         * @param period The period of the timer in microseconds.
         * @param handler The function to be called when the timer expires.
         * @param type The type of the timer (e.g., one-shot or periodic).
         * @return A handle to the created timer.
         */
        timer_handle add(const std::string& timer_name, std::uint64_t period,
            std::function<void(timer_handle)>&& handler, timer_type type);


        /**
         * @brief Adds a new timer to the scheduler.
         *
         * This function creates a new timer with the specified name, period, handler, and type,
         * and adds it to the FreeRTOS timer scheduler.
         *
         * If periodic, then the timer will expire repeatedly with a frequency set by the period
         * parameter. If set to one_shot, then the timer will be a one-shot timer.
         *
         * @param timer_name The name of the timer.
         * @param period The period of the timer in microseconds as std:chrono::duration.
         * @param handler The function to be called when the timer expires.
         * @param type The type of the timer (e.g., one-shot or periodic).
         * @return A handle to the created timer.
         */
        timer_handle add(const std::string& timer_name, const std::chrono::duration<std::uint64_t, std::micro>& period,
            std::function<void(timer_handle)>&& handler, timer_type type);

        /**
         * @brief Removes a timer from the scheduler.
         *
         * This function stops the timer associated with the given handle and removes it from the internal context list.
         * It also deletes the timer after removal.
         *
         * @param hnd The handle of the timer to be removed.
         * @return true if the timer was successfully removed.
         */
        bool remove(timer_handle hnd);

        struct timer_context
        {
            std::function<void(timer_handle)> m_callback = {};
            timer_handle m_timer_handle = nullptr;
            bool m_auto_release = false;
            timer_scheduler* m_this = nullptr;
        };

        /**
         * @brief Removes and deletes a timer from the scheduler.
         *
         * This function removes the specified timer from the list of active timers
         * and deletes it using the FreeRTOS xTimerDelete function.
         *
         * @param hnd The handle of the timer to be removed and deleted.
         */
        void remove_and_delete_timer(timer_handle hnd);

    private:
        /**
         * @brief Adds a tick timer to the scheduler.
         *
         * This function creates and starts a FreeRTOS software timer with the specified parameters.
         *
         * @param timer_name The name of the timer.
         * @param period The period of the timer in ticks.
         * @param handler The callback function to be called when the timer expires.
         * @param type The type of the timer (one-shot or periodic).
         * @return The handle to the created timer, or nullptr if the timer could not be created.
         */
        timer_handle add_tick(const std::string& timer_name, const TickType_t period,
            std::function<void(timer_handle)>&& handler, timer_type type);

        tools::critical_section m_mutex;
        std::list<std::unique_ptr<timer_context>> m_contexts = {};
        std::list<timer_handle> m_active_timers = {};
    };
}
