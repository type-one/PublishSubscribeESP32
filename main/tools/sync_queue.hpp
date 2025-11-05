/**
 * @file sync_queue.hpp
 * @brief A thread-safe queue implementation.
 *
 * This file contains the definition of the sync_queue class, which provides a thread-safe
 * queue with various methods to manipulate the queue. It is part of the Publish/Subscribe
 * Pattern project for ESP32.
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

#pragma once

#if !defined(SYNC_QUEUE_HPP_)
#define SYNC_QUEUE_HPP_

#include <mutex>
#include <optional>
#include <queue>
#include <utility>

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"

namespace tools
{
    /**
     * @brief A thread-safe queue implementation.
     *
     * This class provides a thread-safe queue with various methods to manipulate the queue.
     * It inherits from non_copyable to prevent copying and moving.
     *
     * @tparam T The type of elements stored in the queue.
     */
    template <typename T>
    class sync_queue : public non_copyable // NOLINT inherits from non copyable/non movable
    {
    public:
        sync_queue() = default;
        ~sync_queue() = default;
        struct thread_safe
        {
            static constexpr bool value = true;
        };

        /**
         * @brief Pushes an element into the queue.
         *
         * This method adds a new element to the end of the queue in a thread-safe manner.
         *
         * @param elem The element to be pushed into the queue.
         */
        void push(const T& elem)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.push(elem);
        }

        /**
         * @brief Adds a new element to the queue.
         *
         * This method locks the mutex to ensure thread safety and then
         * adds a new element to the queue using perfect forwarding.
         *
         * @param elem The element to be added to the queue.
         */
        void emplace(T&& elem)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.emplace(std::move(elem));
        }

        /**
         * @brief Removes the front element from the queue.
         *
         * This method locks the mutex to ensure thread safety and then removes the front element from the queue.
         */
        void pop()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            if (!m_queue.empty())
            {
                m_queue.pop();
            }
        }

        /**
         * @brief Retrieves the front element of the queue.
         *
         * This method returns the front element of the queue in a thread-safe manner.
         *
         * @return The front element of the queue, or none if the queue is empty.
         */
        std::optional<T> front() const
        {
            std::optional<T> item;
            std::lock_guard<tools::critical_section> guard(m_mutex);
            if (!m_queue.empty())
            {
                item = m_queue.front();
            }
            return item;
        }

        /**
         * @brief Retrieves and removes the front element of the queue.
         *
         * This method returns and removes the front element of the queue in a thread-safe manner.
         *
         * @return The front element of the queue, or none if the queue is empty.
         */
        std::optional<T> front_pop()
        {
            std::optional<T> item;
            std::lock_guard<tools::critical_section> guard(m_mutex);
            if (!m_queue.empty())
            {
                item = m_queue.front();
                m_queue.pop();
            }
            return item;
        }

        /**
         * @brief Retrieves the last element in the queue.
         *
         * This method returns the last element in the queue. It uses a lock guard to ensure
         * thread safety while accessing the queue.
         *
         * @return The last element in the queue, or none if the queue is empty.
         */
        std::optional<T> back() const
        {
            std::optional<T> item;
            std::lock_guard<tools::critical_section> guard(m_mutex);
            if (!m_queue.empty())
            {
                item = m_queue.back();
            }
            return item;
        }

        /**
         * @brief Retrieves a snapshot of the internal queue.
         *
         * This method returns a copy of the internal queue. It uses a lock guard to ensure
         * thread safety while copying the queue.
         *
         * @return A copy of the internal queue
         */
        std::queue<T> snapshot() const
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_queue;
        }

        /**
         * @brief Checks if the queue is empty.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * and then checks if the underlying queue is empty.
         *
         * @return true if the queue is empty, false otherwise.
         */
        bool empty() const
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_queue.empty();
        }

        /**
         * @brief Returns the number of elements in the queue.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * while accessing the underlying queue's size.
         *
         * @return The number of elements in the queue.
         */
        std::size_t size() const
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_queue.size();
        }

        /**
         * @brief Pushes an element into the queue in an interrupt service routine (ISR) safe manner.
         *
         * This function uses an ISR lock guard to ensure that the push operation is thread-safe
         * and can be safely called from an ISR context.
         *
         * @param elem The element to be pushed into the queue.
         */
        void isr_push(const T& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.push(elem);
        }

        /**
         * @brief Inserts an element into the queue in an interrupt-safe manner.
         *
         * This function uses a lock guard to ensure that the insertion operation
         * is safe to perform within an interrupt service routine (ISR).
         *
         * @param elem The element to be inserted into the queue.
         */
        void isr_emplace(T&& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.emplace(std::move(elem));
        }

        /**
         * @brief Returns the size of the queue in an interrupt service routine (ISR) safe manner.
         *
         * This function acquires a lock using an ISR-safe lock guard to ensure that the size of the queue
         * is read in a thread-safe manner, even when called from an ISR.
         *
         * @return The size of the queue.
         */
        std::size_t isr_size() const
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_queue.size();
        }

    private:
        std::queue<T> m_queue;
        mutable critical_section m_mutex;
    };
}

#endif //  SYNC_QUEUE_HPP_
