/**
 * @file thread_pool_impl.hpp
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
#include <atomic>
#include <cstdint>
#include <thread>
#include <type_traits>


#include "closable_queue_fwd.hpp"
#include "execution_impl.hpp"
#include "unique_function.hpp"

namespace pco
{
    namespace detail
    {
        extern template class closable_queue<unique_function<void()>>;

        class queue_executor
        {
        public:
            queue_executor(closable_queue<unique_function<void()>>* queue) noexcept
                : queue_ { queue }
            {
            }

        private:
            friend void post(queue_executor exec, unique_function<void()> fun)
            {
                exec.queue_->push(std::move(fun));
            }
            closable_queue<unique_function<void()>>* queue_;
        };

    } // namespace detail

    /**
     * @headerfile portable_concurrency/thread_pool
     * @ingroup thread_pool
     */
    class static_thread_pool
    {
    public:
        using executor_type = detail::queue_executor;

        explicit static_thread_pool(std::size_t num_threads);

        static_thread_pool(const static_thread_pool&) = delete;
        static_thread_pool& operator=(const static_thread_pool&) = delete;
        static_thread_pool(static_thread_pool&&) = delete;
        static_thread_pool& operator=(static_thread_pool&&) = delete;

        /// stop accepting incoming work and wait for work to drain
        ~static_thread_pool();

        /// attach current thread to the thread pools list of worker threads
        void attach();

        /// signal all work to complete
        void stop();

        /// wait for all threads in the thread pool to complete
        void wait();

        executor_type executor() noexcept
        {
            return { &queue_ };
        }

    private:
        detail::closable_queue<unique_function<void()>> queue_;
        std::vector<std::thread> threads_;
        unsigned attached_threads_ = 0;
        tools::critical_section mutex_;
        tools::cond_var cv_;
        std::atomic<bool> stopped_ { false };
    };


    template <>
    struct is_executor<static_thread_pool::executor_type> : std::true_type
    {
    };

} // namespace pco
