/**
 * @file closable_queue_fwd.hpp
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

#include "tools/cond_var.hpp"
#include "tools/critical_section.hpp"
#include <condition_variable>
#include <mutex>
#include <queue>


namespace pco::detail
{

    /**
     * @brief Thread-safe queue with close semantics for producer/consumer coordination.
     * @tparam T Value type stored in the queue.
     */
    template <typename T>
    class closable_queue
    {
    public:
        /**
         * @brief Pops a value from the queue, waiting until data is available or queue is closed.
         * @param dest Destination receiving the popped value.
         * @return true when a value was popped, false when queue is closed and empty.
         */
        bool pop(T& dest);

        /**
         * @brief Pushes a value into the queue.
         * @param val Value to enqueue.
         */
        void push(T&& val);

        /**
         * @brief Closes the queue and wakes waiting consumers.
         */
        void close();

    private:
        tools::critical_section mutex_;
        tools::cond_var cv_;
        std::queue<T> queue_;
        bool closed_ = false;
    };

} // namespace pco::detail
