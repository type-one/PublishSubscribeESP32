/**
 * @file critical_section_freertos.inl
 * @brief Implementation of a critical section using FreeRTOS mutexes.
 * 
 * This file contains the implementation of a critical section class and an ISR lock guard class
 * using FreeRTOS mutexes. It provides methods to manage critical sections and ensure that only
 * one task can enter the critical section at a time.
 * 
 * @author Laurent Lardinois
 * @date January 2025
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

#include <cstdio>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "tools/logger.hpp"
#include "tools/non_copyable.hpp"

namespace tools
{
    /**
     * @brief Class representing a critical section using FreeRTOS mutex.
     *
     * This class provides methods to manage critical sections using FreeRTOS mutexes.
     * It ensures that only one task can enter the critical section at a time.
     */
    class critical_section : public non_copyable // NOLINT inherits from non copyable and non movable
    {
    public:
        /**
         * @brief Constructor for the critical_section class.
         *
         * This constructor initializes a mutex using FreeRTOS's xSemaphoreCreateMutex function.
         * If the mutex creation fails, it logs a fatal error.
         */
        critical_section()
            : m_mutex(xSemaphoreCreateMutex())
        {
            // FreeRTOS platform
            if (nullptr == m_mutex)
            {
                LOG_ERROR("FATAL error: xSemaphoreCreateMutex() failed");
            }
        }

        /**
         * @brief Destructor for the critical_section class.
         *
         * This destructor is responsible for deleting the FreeRTOS semaphore
         * associated with the critical section if it exists.
         */
        ~critical_section()
        {
            // FreeRTOS platform
            if (nullptr != m_mutex)
            {
                vSemaphoreDelete(m_mutex);
            }
        }

        /**
         * @brief Locks the mutex to enter a critical section.
         *
         * This function attempts to take the FreeRTOS semaphore (mutex) and will block until the mutex is successfully
         * taken. It retries every 100 ticks until it acquires the mutex.
         */
        void lock() // NOLINT keep same interface than standard mutex
        {
            // FreeRTOS platform
            if (nullptr != m_mutex)
            {
                while (xSemaphoreTake(m_mutex, static_cast<TickType_t>(100)) != pdTRUE)
                {
                }
            }
        }

        /**
         * @brief Locks the ISR (Interrupt Service Routine) by taking the semaphore.
         *
         * This function attempts to take the semaphore in an ISR context. If the semaphore is not available,
         * it yields the processor to higher priority tasks and retries until the semaphore is successfully taken.
         */
        void isr_lock() // NOLINT keep same interface than standard implementation
        {
            // FreeRTOS platform
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

        /**
         * @brief Attempts to lock the mutex without blocking.
         *
         * This function tries to take the mutex immediately. If the mutex is already taken,
         * the function will return false without blocking.
         *
         * @return true if the mutex was successfully taken, false otherwise.
         */
        bool try_lock() // NOLINT keep same interface than standard mutex
        {
            // FreeRTOS platform
            bool result = false;

            if (nullptr != m_mutex)
            {
                result = (pdTRUE == xSemaphoreTake(m_mutex, static_cast<TickType_t>(0)));
            }

            return result;
        }

        /**
         * @brief Attempts to lock the mutex from an ISR context.
         *
         * This function tries to take the mutex in an interrupt service routine (ISR) context.
         * If the mutex is successfully taken, it returns true. Otherwise, it returns false.
         *
         * @return true if the mutex was successfully taken, false otherwise.
         */
        bool try_isr_lock() // NOLINT keep same interface than standard implementation
        {
            // FreeRTOS platform
            bool result = false;

            if (nullptr != m_mutex)
            {
                BaseType_t px_higher_priority_task_woken = pdFALSE; // NOLINT initialized to pdFALSE
                result = (pdTRUE == xSemaphoreTakeFromISR(m_mutex, &px_higher_priority_task_woken));
                portYIELD_FROM_ISR(px_higher_priority_task_woken);
            }

            return result;
        }

        /**
         * @brief Unlocks the mutex.
         *
         * This function releases the mutex if it is currently held. It uses the FreeRTOS
         * xSemaphoreGive function to release the semaphore associated with the mutex.
         */
        void unlock() // NOLINT keep same interface than standard mutex
        {
            // FreeRTOS platform
            if (nullptr != m_mutex)
            {
                xSemaphoreGive(m_mutex);
            }
        }

        /**
         * @brief Unlocks the ISR critical section.
         *
         * This function releases the mutex that was acquired in the ISR context.
         * It ensures that the FreeRTOS scheduler is aware of the change in task priority.
         */
        void isr_unlock() // NOLINT keep same interface than standard implementation
        {
            // FreeRTOS platform
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

    /**
     * @brief A guard class that locks an ISR (Interrupt Service Routine) lockable object upon creation and unlocks it
     * upon destruction.
     *
     * @tparam T The type of the lockable object.
     */
    template <typename T>
    class isr_lock_guard : public non_copyable // NOLINT inherits from non copyable and non movable
    {
    public:
        /**
         * @brief Deleted default constructor to prevent creating an instance without a lockable object.
         */
        isr_lock_guard() = delete;

        /**
         * @brief Constructs the guard and locks the provided lockable object.
         *
         * @param lockable_object The lockable object to be locked.
         */
        isr_lock_guard(T& lockable_object)
            : m_lockable_object_ref(lockable_object)
        {
            m_lockable_object_ref.isr_lock();
        }

        /**
         * @brief Destructs the guard and unlocks the lockable object.
         */
        ~isr_lock_guard()
        {
            m_lockable_object_ref.isr_unlock();
        }

    private:
        T& m_lockable_object_ref; ///< Reference to the lockable object.
    };
}
