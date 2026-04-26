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
        /**
         * @brief Explicit instantiation declaration for queue of pool tasks.
         */
        extern template class closable_queue<unique_function<void()>>;

        /**
         * @brief Executor adapter that enqueues tasks into a thread-pool work queue.
         */
        class queue_executor
        {
        public:
            /**
             * @brief Creates an executor bound to a queue.
             * @param queue Pointer to queue used to enqueue tasks.
             */
            queue_executor(closable_queue<unique_function<void()>>* queue) noexcept
                : queue_ { queue }
            {
            }

        private:
            /**
             * @brief Submits task to underlying queue through ADL executor customization point.
             * @param exec Queue-backed executor.
             * @param fun Task to enqueue.
             */
            friend void post(queue_executor exec, unique_function<void()> fun)
            {
                exec.queue_->push(std::move(fun));
            }

            /** @brief Non-owning pointer to pool work queue. */
            closable_queue<unique_function<void()>>* queue_;
        };

    } // namespace detail

    /**
     * @brief Fixed-size thread pool implementation.
     *
     * Owns worker threads and a shared work queue. Tasks posted through
     * `executor()` are drained by attached worker threads.
     *
     * @headerfile portable_concurrency/thread_pool
     * @ingroup thread_pool
     */
    class static_thread_pool
    {
    public:
        /** @brief Executor type associated with this pool. */
        using executor_type = detail::queue_executor;

        /**
         * @brief Creates thread pool with a fixed number of worker threads.
         * @param num_threads Number of worker threads to launch.
         */
        explicit static_thread_pool(std::size_t num_threads);

        static_thread_pool(const static_thread_pool&) = delete;
        static_thread_pool& operator=(const static_thread_pool&) = delete;
        static_thread_pool(static_thread_pool&&) = delete;
        static_thread_pool& operator=(static_thread_pool&&) = delete;

        /**
         * @brief Stops accepting work and waits until queued work is drained.
         */
        ~static_thread_pool();

        /**
         * @brief Attaches the current thread as a worker for this pool.
         */
        void attach();

        /**
         * @brief Signals pool shutdown and unblocks waiting workers.
         */
        void stop();

        /**
         * @brief Joins all worker threads owned by this pool.
         */
        void wait();

        /**
         * @brief Returns an executor that posts tasks to this pool.
         * @return Queue-backed executor handle.
         */
        executor_type executor() noexcept
        {
            return { &queue_ };
        }

    private:
        /** @brief Shared task queue consumed by worker threads. */
        detail::closable_queue<unique_function<void()>> queue_;

        /** @brief Worker threads owned by this pool. */
        std::vector<std::thread> threads_;

        /** @brief Number of externally attached worker threads. */
        unsigned attached_threads_ = 0;

        /** @brief Synchronization primitive protecting shared counters/state. */
        tools::critical_section mutex_;

        /** @brief Condition variable used for worker coordination and shutdown waits. */
        tools::cond_var cv_;

        /** @brief Shutdown flag set by stop() and observed by workers. */
        std::atomic<bool> stopped_ { false };
    };

    template <>
    /**
     * @brief Marks static_thread_pool::executor_type as a valid executor.
     */
    struct is_executor<static_thread_pool::executor_type> : std::true_type
    {
    };

} // namespace pco
