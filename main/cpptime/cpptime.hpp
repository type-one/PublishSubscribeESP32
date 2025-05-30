#pragma once

#ifndef CPPTIME_HPP_
#define CPPTIME_HPP_

// modified to pass clang-tidy issues

/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Michael Egli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \author    Michael Egli
 * \copyright Michael Egli
 * \date      11-Jul-2015
 *
 * \file cpptime.h
 *
 * C++11 timer component
 * =====================
 *
 * A portable, header-only C++11 timer component.
 *
 * Overview
 * --------
 *
 * This component can be used to manage a set of timeouts. It is implemented in
 * pure C++11. It is therefore very portable given a compliant compiler.
 *
 * A timeout can be added with one of the `add()` functions, and removed with
 * the `remove()` function. A timeout can be set to be either one-shot or
 * periodic. If it is one-shot, the callback is invoked once and the timeout
 * event is then automatically removed. If the timeout is periodic, it is
 * always renewed and never automatically removed.
 *
 * When a timeout is removed or when a one-shot timeout expires, the handler
 * will be deleted to clean-up any resources.
 *
 * Removing a timeout is possible from within the callback. In this case, you
 * must be careful not to access any captured variables, if any, after calling
 * `remove()`, because they are no longer valid.
 *
 * Timeout Units
 * -------------
 *
 * The preferred functions for adding timeouts are those that take a
 * `std::chrono::...` argument. However, for convenience, there is also an API
 * that takes a uint64_t. When using this API, all values are expected to be
 * given in microseconds (us).
 *
 * For periodic timeouts, a separate timeout can be specified for the initial
 * (first) timeout, and the periodicity after that.
 *
 * To avoid drifts, times are added by simply adding the period to the initially
 * calculated (or provided) time. Also, we use `wait until` type of API to wait
 * for a timeout instead of a `wait for` API.
 *
 * Data Structure
 * --------------
 *
 * Internally, a std::vector is used to store timeout events. The timer_id
 * returned from the `add` functions are used as index to this vector.
 *
 * In addition, a std::multiset is used that holds all time points when
 * timeouts expire.
 *
 * Using a vector to store timeout events has some implications. It is very
 * fast to remove an event, because the timer_id is the vector's index. On the
 * other hand, this makes it also more complicated to manage the timer_ids. The
 * current solution is to keep track of ids that are freed in order to re-use
 * them. A stack is used for this.
 *
 * Examples
 * --------
 *
 * More examples can be found in the `tests` folder.
 *
 * ~~~
 * CppTime::Timer t;
 * t.add(std::chrono::seconds(1), [](CppTime::timer_id){ std::cout << "got it!"; });
 * std::this_thread::sleep_for(std::chrono::seconds(2));
 * ~~~
 */

// Includes
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <stack>
#include <thread>
#include <utility>
#include <vector>

namespace CppTime
{

    // Public types
    using timer_id = std::size_t;
    using handler_t = std::function<void(timer_id)>;
    using clock = std::chrono::steady_clock;
    using timestamp = std::chrono::time_point<clock>;
    using duration = std::chrono::microseconds;

    // Private definitions. Do not rely on this namespace.
    namespace detail
    {

        // The event structure that holds the information about a timer.
        struct Event // NOLINT move constructor/operator are deleted, copy constructor/operator are defined
        {
            timer_id tid = 0;
            timestamp start;
            duration period = duration::zero();
            handler_t handler = nullptr;
            bool valid = false;

            Event() = default;

            template <typename Func>
            Event(timer_id tid_, timestamp start, duration period, Func&& handler)
                : tid(tid_)
                , start(start)
                , period(period)
                , handler(std::forward<Func>(handler))
                , valid(true)
            {
            }
            Event(Event&& other) = default;
            Event& operator=(Event&& other) = default;
            Event(const Event& other) = delete;
            Event& operator=(const Event& other) = delete;
        };

        // A time event structure that holds the next timeout and a reference to its
        // Event struct.
        struct Time_event
        {
            timestamp next;
            timer_id ref;
        };

        inline bool operator<(const Time_event& left, const Time_event& right)
        {
            return left.next < right.next;
        }

    } // end namespace detail

    class Timer
    {
        // Thread and locking variables.
        std::mutex mutex;
        std::condition_variable cond;
        std::thread worker;

        // Use to terminate the timer thread.
        bool done = false;

        // The vector that holds all active events.
        std::vector<detail::Event> events;
        // Sorted queue that has the next timeout at its top.
        std::multiset<detail::Time_event> time_events;

        // A list of ids to be re-used. If possible, ids are used from this pool.
        std::stack<CppTime::timer_id> free_ids;

    public:
        Timer();
        ~Timer();

        // non copyable and not movable
        Timer(const Timer&) = delete;
        Timer(Timer&&) noexcept = delete;
        Timer& operator=(const Timer&) = delete;
        Timer& operator=(Timer&&) noexcept = delete;

        /**
         * Add a new timer.
         *
         * \param when The time at which the handler is invoked.
         * \param handler The callable that is invoked when the timer fires.
         * \param period The periodicity at which the timer fires. Only used for periodic timers.
         */
        timer_id add(const timestamp& when, handler_t&& handler, const duration& period);

        /**
         * Add a oneshot new timer.
         *
         * \param when The time at which the handler is invoked.
         * \param handler The callable that is invoked when the timer fires.
         */
        timer_id add(const timestamp& when, handler_t&& handler)
        {
            return add(when, std::move(handler), duration::zero());
        }

        /**
         * Overloaded `add` function that uses a `std::chrono::duration` instead of a
         * `time_point` for the first timeout.
         */
        template <class Rep, class Period>
        timer_id add(const std::chrono::duration<Rep, Period>& when, handler_t&& handler, const duration& period)
        {
            return add(
                clock::now() + std::chrono::duration_cast<std::chrono::microseconds>(when), std::move(handler), period);
        }

        template <class Rep, class Period>
        timer_id add(const std::chrono::duration<Rep, Period>& when, handler_t&& handler)
        {
            return add<Rep, Period>(when, std::move(handler), duration::zero());
        }

        /**
         * Overloaded `add` function that uses a uint64_t instead of a `time_point` for
         * the first timeout and the period.
         */
        timer_id add(std::uint64_t when, handler_t&& handler, std::uint64_t period);

        /**
         * Overloaded `add` function (one shot) that uses a uint64_t instead of a `time_point` for
         * the first timeout.
         */
        timer_id add(std::uint64_t when, handler_t&& handler)
        {
            return add(when, std::move(handler), 0U);
        }

        /**
         * Removes the timer with the given id.
         */
        bool remove(timer_id tid);

    private:
        void run();
    };

} // end namespace CppTime

#endif // CPPTIME_HPP_
