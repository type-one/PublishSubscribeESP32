//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

// FreeRTOS implementation of tools::cond_var.
//
// Implements the same interface as std::condition_variable_any using a
// FreeRTOS counting semaphore.
//
// Waiters are tracked with an atomic counter so notify_one()/notify_all()
// can be called either with or without holding the associated mutex.

#pragma once

#include <atomic>
#include <chrono>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "tools/non_copyable.hpp"

namespace tools
{

    class cond_var : public non_copyable // NOLINT inherits from non copyable and non movable
    {
    public:
        cond_var()
            : m_sem(xSemaphoreCreateCounting(static_cast<UBaseType_t>(portMAX_DELAY), 0U))
            , m_waiters(0)
        {
        }

        ~cond_var()
        {
            if (m_sem != nullptr)
            {
                vSemaphoreDelete(m_sem);
            }
        }

        void notify_one()
        {
            // Only post if at least one waiter is currently blocked/pending.
            if (m_waiters.load(std::memory_order_acquire) > 0)
            {
                xSemaphoreGive(m_sem);
            }
        }

        void notify_all()
        {
            // Wake every waiter currently observed.
            const int num_waiters = m_waiters.load(std::memory_order_acquire);
            for (int i = 0; i < num_waiters; ++i)
            {
                xSemaphoreGive(m_sem);
            }
        }

        // Blocking wait with predicate (no timeout).
        // The caller must hold 'lock' on entry; it is released during the wait
        // and re-acquired before the predicate is re-evaluated or the function
        // returns.
        template <typename Lock, typename Predicate>
        void wait(Lock& lock, Predicate pred)
        {
            while (!pred())
            {
                m_waiters.fetch_add(1, std::memory_order_acq_rel);
                lock.unlock();
                xSemaphoreTake(m_sem, portMAX_DELAY);
                lock.lock();
                m_waiters.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        // Timed wait with relative timeout and predicate.
        // Returns true if the predicate became true before timeout, false otherwise.
        template <typename Lock, typename Rep, typename Period, typename Predicate>
        bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time, Predicate pred)
        {
            return wait_until(lock, std::chrono::steady_clock::now() + rel_time, pred);
        }

        // Timed wait with predicate.
        // Returns true if the predicate became true before the timeout, false
        // if the timeout expired before pred() returned true.
        template <typename Lock, typename Clock, typename Duration, typename Predicate>
        bool wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& abs_time, Predicate pred)
        {
            while (!pred())
            {
                auto now = Clock::now();
                if (now >= abs_time)
                {
                    return pred();
                }

                auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(abs_time - now).count();
                // Clamp to at least 1 tick to avoid immediate expiry.
                TickType_t ticks = (remaining_ms > 0) ? pdMS_TO_TICKS(static_cast<uint32_t>(remaining_ms))
                                                      : static_cast<TickType_t>(1U);

                m_waiters.fetch_add(1, std::memory_order_acq_rel);
                lock.unlock();
                bool signaled = (xSemaphoreTake(m_sem, ticks) == pdTRUE);
                lock.lock();
                m_waiters.fetch_sub(1, std::memory_order_acq_rel);

                if (!signaled && !pred())
                {
                    return false;
                }
            }
            return true;
        }

    private:
        SemaphoreHandle_t m_sem;
        std::atomic<int> m_waiters;
    };

} // namespace tools
