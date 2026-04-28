/**
 * @file closable_queue.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-10-13
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-10-13                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include "closable_queue_fwd.hpp"
namespace pco::detail
{

    /**
     * @brief Waits for queued value or closed state, then pops one value if available.
     * @tparam T Queue value type.
     * @param dest Output destination for popped value.
     * @return true when a value is popped, false when queue is closed and empty.
     */
    template <typename T>
    bool closable_queue<T>::pop(T& dest)
    {
        std::unique_lock<tools::critical_section> lock(mutex_);
        cv_.wait(lock, [this]() { return closed_ || !queue_.empty(); });
        if (closed_ && queue_.empty())
        {
            return false;
        }
        std::swap(dest, queue_.front());
        queue_.pop();
        return true;
    }

    /**
     * @brief Pushes a value to queue and notifies one waiting consumer.
     * @tparam T Queue value type.
     * @param val Value to enqueue.
     */
    template <typename T>
    void closable_queue<T>::push(T&& val)
    {
        std::scoped_lock<tools::critical_section> guard(mutex_);
        if (closed_)
        {
            return;
        }
        queue_.emplace(std::move(val));
        cv_.notify_one();
    }

    /**
     * @brief Marks queue as closed and notifies all waiting consumers.
     * @tparam T Queue value type.
     */
    template <typename T>
    void closable_queue<T>::close()
    {
        std::scoped_lock<tools::critical_section> guard(mutex_);
        closed_ = true;
        cv_.notify_all();
    }

} // namespace pco::detail
