/**
 * @file sync_ring_buffer.hpp
 * @brief A thread-safe ring buffer implementation.
 *
 * This file contains the definition of a thread-safe ring buffer class template.
 * The ring buffer provides basic operations such as push, pop, front, back, and size,
 * and ensures thread safety using a mutex.
 *
 * @author Laurent Lardinois
 *
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

#if !defined(SYNC_RING_BUFFER_HPP_)
#define SYNC_RING_BUFFER_HPP_

#include <cstddef>
#include <mutex>

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"
#include "tools/ring_buffer.hpp"

namespace tools
{
    /**
     * @brief A thread-safe ring buffer implementation.
     *
     * This class provides a thread-safe ring buffer with a fixed capacity.
     * It supports basic operations such as push, pop, front, back, and size,
     * and ensures thread safety using a mutex.
     *
     * @tparam T The type of elements stored in the ring buffer.
     * @tparam Capacity The maximum number of elements the ring buffer can hold.
     */
    template <typename T, std::size_t Capacity>
    class sync_ring_buffer : public non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        struct thread_safe
        {
            static constexpr bool value = true;
        };

        sync_ring_buffer() = default;
        ~sync_ring_buffer() = default;

        /**
         * @brief Pushes an element into the ring buffer.
         *
         * This method adds an element to the ring buffer in a thread-safe manner.
         *
         * @param elem The element to be pushed into the ring buffer.
         */
        void push(const T& elem)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.push(elem);
        }

        /**
         * @brief Inserts an element into the ring buffer.
         *
         * This function locks the mutex to ensure thread safety and then
         * inserts the given element into the ring buffer using move semantics.
         *
         * @param elem The element to be inserted into the ring buffer.
         */
        void emplace(T&& elem)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.emplace(std::move(elem));
        }

        /**
         * @brief Removes the oldest element from the ring buffer.
         */
        void pop()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.pop();
        }

        /**
         * @brief Retrieves the front element of the ring buffer.
         *
         * This method locks the mutex to ensure thread safety and then returns the front element of the ring buffer.
         *
         * @return The front element of the ring buffer.
         */
        T front()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.front();
        }

        /**
         * @brief Retrieves the last element from the ring buffer.
         *
         * This function locks the mutex to ensure thread safety and then returns
         * the last element in the ring buffer.
         *
         * @return The last element of type T in the ring buffer.
         */
        T back()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.back();
        }

        /**
         * @brief Checks if the ring buffer is empty.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * while checking if the ring buffer is empty.
         *
         * @return true if the ring buffer is empty, false otherwise.
         */
        bool empty()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.empty();
        }

        /**
         * @brief Checks if the ring buffer is full.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * while checking if the ring buffer is full.
         *
         * @return true if the ring buffer is full, false otherwise.
         */
        bool full()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.full();
        }

        /**
         * @brief Returns the size of the ring buffer.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * while accessing the size of the ring buffer.
         *
         * @return The number of elements in the ring buffer.
         */
        std::size_t size()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.size();
        }

        /**
         * @brief Returns the capacity of the ring buffer.
         * @return The capacity of the ring buffer.
         */
        [[nodiscard]] constexpr std::size_t capacity() const
        {
            return m_ring_buffer.capacity();
        }

        /**
         * @brief Pushes an element into the ring buffer in an interrupt service routine (ISR) safe manner.
         *
         * This function uses an ISR lock guard to ensure that the push operation is thread-safe
         * and can be safely called from within an ISR.
         *
         * @param elem The element to be pushed into the ring buffer.
         */
        void isr_push(const T& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.push(elem);
        }

        /**
         * @brief Emplaces an element into the ring buffer in an interrupt service routine (ISR) context.
         *
         * This function is designed to be called from an ISR. It locks the critical section
         * to ensure thread safety and then emplaces the given element into the ring buffer.
         *
         * @param elem The element to be emplaced into the ring buffer.
         */
        void isr_emplace(T&& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.emplace(elem);
        }

        /**
         * @brief Checks if the ring buffer is full in an interrupt service routine (ISR) context.
         *
         * This function uses a lock guard to ensure thread safety while checking if the ring buffer is full.
         *
         * @return true if the ring buffer is full, false otherwise.
         */
        bool isr_full()
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.full();
        }

        /**
         * @brief Retrieves the size of the ring buffer in an interrupt-safe manner.
         *
         * This function acquires a lock using an interrupt-safe lock guard to ensure
         * that the size of the ring buffer is read safely in the presence of interrupts.
         *
         * @return The size of the ring buffer.
         */
        std::size_t isr_size()
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.size();
        }

    private:
        ring_buffer<T, Capacity> m_ring_buffer;
        critical_section m_mutex;
    };
}

#endif //  SYNC_RING_BUFFER_HPP_
