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

#include "cpptime/cpptime.hpp"

// Includes
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <stack>
#include <thread>
#include <vector>

namespace CppTime
{
    Timer::Timer()
    {
        std::unique_lock<std::mutex> lock(mutex);
        worker = std::thread([this] { run(); });
    }

    Timer::~Timer()
    {
        std::unique_lock<std::mutex> lock(mutex);
        done = true;
        lock.unlock();
        cond.notify_all();
        worker.join();
        events.clear();
        time_events.clear();
        while (!free_ids.empty())
        {
            free_ids.pop();
        }
    }

    /**
     * Add a new timer.
     *
     * \param when The time at which the handler is invoked.
     * \param handler The callable that is invoked when the timer fires.
     * \param period The periodicity at which the timer fires. Only used for periodic timers.
     */
    timer_id Timer::add(const timestamp& when, handler_t&& handler, const duration& period)
    {
        std::unique_lock<std::mutex> lock(mutex);
        timer_id tid = 0;
        // Add a new event. Prefer an existing and free id. If none is available, add
        // a new one.
        if (free_ids.empty())
        {
            tid = events.size();
            detail::Event evt(tid, when, period, std::move(handler));
            events.emplace_back(std::move(evt));
        }
        else
        {
            tid = free_ids.top();
            free_ids.pop();
            detail::Event evt(tid, when, period, std::move(handler));
            events.at(tid) = std::move(evt);
        }
        time_events.insert(detail::Time_event { when, tid });
        lock.unlock();
        cond.notify_all();
        return tid;
    }

    /**
     * Overloaded `add` function that uses a uint64_t instead of a `time_point` for
     * the first timeout and the period.
     */
    timer_id Timer::add(std::uint64_t when, handler_t&& handler, std::uint64_t period)
    {
        return add(duration(when), std::move(handler), duration(period));
    }

    /**
     * @brief
     *
     */
    bool Timer::remove(timer_id tid)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (events.empty() || (events.size() <= tid))
        {
            return false;
        }
        events.at(tid).valid = false;
        events.at(tid).handler = nullptr;
        auto itr = std::find_if(
            time_events.begin(), time_events.end(), [&](const detail::Time_event& tev) { return tev.ref == tid; });
        if (itr != time_events.end())
        {
            free_ids.push(itr->ref);
            time_events.erase(itr);
        }
        lock.unlock();
        cond.notify_all();
        return true;
    }

    void Timer::run()
    {
        std::unique_lock<std::mutex> lock(mutex);

        while (!done)
        {

            if (time_events.empty())
            {
                // Wait for work
                cond.wait(lock);
            }
            else
            {
                detail::Time_event tev = *time_events.begin();
                if (CppTime::clock::now() >= tev.next)
                {

                    // Remove time event
                    time_events.erase(time_events.begin());

                    // Invoke the handler
                    lock.unlock();
                    events.at(tev.ref).handler(tev.ref);
                    lock.lock();

                    if (events.at(tev.ref).valid && events.at(tev.ref).period.count() > 0)
                    {
                        // The event is valid and a periodic timer.
                        tev.next += events.at(tev.ref).period;
                        time_events.insert(tev);
                    }
                    else
                    {
                        // The event is either no longer valid because it was removed in the
                        // callback, or it is a one-shot timer.
                        events.at(tev.ref).valid = false;
                        events.at(tev.ref).handler = nullptr;
                        free_ids.push(tev.ref);
                    }
                }
                else
                {
                    cond.wait_until(lock, tev.next);
                }
            }
        }
    }

} // end namespace CppTime
