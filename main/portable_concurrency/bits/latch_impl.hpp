/**
 * @file latch_impl.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-06-14
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-06-14                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include "tools/cond_var.hpp"
#include "tools/critical_section.hpp"
#include <condition_variable>
#include <cstddef>
#include <mutex>


namespace pco
{

    /**
     * @headerfile portable_concurrency/latch
     * @ingroup latch
     *
     * The latch class is a downward counter of type ptrdiff_t which can be used to
     * synchronize threads. The value of the counter is initialized on creation.
     * Threads may block on the latch until the counter is decremented to zero.
     * There is no possibility to increase or reset the counter, which makes the
     * latch a single-use barrier.
     */
    class latch
    {
    public:
        /**
         * @brief Constructs latch with initial counter value.
         * @param count Initial countdown value.
         */
        explicit latch(ptrdiff_t count)
            : counter_(count)
        {
        }

        latch(const latch&) = delete;
        latch& operator=(const latch&) = delete;
        latch(latch&&) = delete;
        latch& operator=(latch&&) = delete;

        /** @brief Destroys latch object. */
        ~latch();

        /** @brief Decrements counter and blocks until latch is ready. */
        void count_down_and_wait();

        /** @brief Decrements counter by one and notifies waiters when ready. */
        void count_down();

        /**
         * @brief Decrements counter by `n` and notifies waiters when ready.
         * @param n Decrement amount.
         */
        void count_down(ptrdiff_t n);

        /**
         * @brief Checks whether latch counter reached zero.
         * @return true when ready; false otherwise.
         */
        bool is_ready() const noexcept;

        /** @brief Blocks until latch counter reaches zero. */
        void wait() const;

    private:
        /** @brief Remaining countdown value. */
        ptrdiff_t counter_;
        /** @brief Number of currently waiting threads. */
        mutable unsigned waiters_ = 0;
        /** @brief Mutex protecting latch state. */
        mutable tools::critical_section mutex_;
        /** @brief Condition variable used to wake waiting threads. */
        mutable tools::cond_var cv_;
    };

} // namespace pco
