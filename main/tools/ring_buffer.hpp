/**
 * @file ring_buffer.hpp
 * @brief A header file defining a thread-unsafe ring buffer class template with a fixed capacity.
 *
 * This file contains the definition of the ring_buffer class template, which provides
 * a circular buffer implementation with a fixed capacity. The ring buffer supports
 * operations such as push, pop, front, back, and various utility functions.
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

#if !defined(RING_BUFFER_HPP_)
#define RING_BUFFER_HPP_

#include <array>
#include <cstddef>
#include <iterator>

namespace tools
{
    /**
     * @brief A class representing a ring buffer with a fixed capacity.
     *
     * This class provides a circular buffer implementation with a fixed capacity.
     *
     * @tparam T The type of elements stored in the ring buffer.
     * @tparam Capacity The maximum number of elements the ring buffer can hold.
     */
    template <typename T, std::size_t Capacity>
    class ring_buffer
    {
    public:
        /**
         * @brief A struct indicating whether the ring buffer is thread-safe.
         *
         * This struct can be specialized to enable thread-safe operations in derived classes.
         */
        struct thread_safe
        {
            static constexpr bool value = false;
        };

        ring_buffer() = default;
        ~ring_buffer() = default;

        /**
         * @brief Copy constructor for the ring_buffer class.
         *
         * This constructor initializes a new ring_buffer object by copying the state
         * from another ring_buffer object.
         *
         * @param other The ring_buffer object to copy from.
         */
        ring_buffer(const ring_buffer& other)
            : m_ring_buffer { other.m_ring_buffer }
            , m_push_index { other.m_push_index }
            , m_pop_index { other.m_pop_index }
            , m_last_index { other.m_last_index }
            , m_size { other.m_size }
        {
        }

        /**
         * @brief Move constructor for the ring_buffer class.
         *
         * This constructor initializes a ring_buffer object by transferring ownership
         * of the resources from another ring_buffer object.
         *
         * @param other The ring_buffer object to move from.
         */
        ring_buffer(ring_buffer&& other) noexcept
            : m_ring_buffer { std::move(other.m_ring_buffer) }
            , m_push_index { std::move(other.m_push_index) }
            , m_pop_index { std::move(other.m_pop_index) }
            , m_last_index { std::move(other.m_last_index) }
            , m_size { std::move(other.m_size) }
        {
        }

        /**
         * @brief Copy assignment operator for the ring_buffer class.
         *
         * This operator assigns the contents of another ring_buffer instance to this instance.
         * It performs a deep copy of the internal buffer and indices.
         *
         * @param other The ring_buffer instance to copy from.
         * @return A reference to this ring_buffer instance.
         */
        ring_buffer& operator=(const ring_buffer& other)
        {
            if (this != &other)
            {
                m_ring_buffer = other.m_ring_buffer;
                m_push_index = other.m_push_index;
                m_pop_index = other.m_pop_index;
                m_last_index = other.m_last_index;
                m_size = other.m_size;
            }

            return *this;
        }

        /**
         * @brief Move assignment operator for the ring_buffer class.
         *
         * This operator moves the contents of another ring_buffer instance to this instance.
         * It ensures that the current instance is not the same as the other instance before
         * performing the move operations.
         *
         * @param other The ring_buffer instance to move from.
         * @return A reference to this ring_buffer instance.
         */
        ring_buffer& operator=(ring_buffer&& other) noexcept
        {
            if (this != &other)
            {
                m_ring_buffer = std::move(other.m_ring_buffer);
                m_push_index = std::move(other.m_push_index);
                m_pop_index = std::move(other.m_pop_index);
                m_last_index = std::move(other.m_last_index);
                m_size = std::move(other.m_size);
            }

            return *this;
        }

        /**
         * @brief Pushes an element into the ring buffer.
         *
         * @param elem The element to be pushed into the ring buffer.
         */
        void push(const T& elem)
        {
            m_ring_buffer.at(m_push_index) = elem;
            m_last_index = m_push_index;
            m_push_index = next_index(m_push_index);            
            ++m_size;
            if (m_size > Capacity)
            {
                // first entry is overwritten
                m_pop_index = next_index(m_pop_index);
            }
        }

        /**
         * @brief Inserts in place an element into the ring buffer at the current push index.
         *
         * @param elem The element to be inserted into the ring buffer.
         */
        void emplace(T&& elem)
        {
            m_ring_buffer.at(m_push_index) = std::move(elem);
            m_last_index = m_push_index;
            m_push_index = next_index(m_push_index);
            ++m_size;
            if (m_size > Capacity)
            {
                // first entry is overwritten
                m_pop_index = next_index(m_pop_index);
            }
        }

        /**
         * @brief Removes the oldest element from the ring buffer.
         */
        void pop()
        {
            if (!empty())
            {
                m_pop_index = next_index(m_pop_index);
                --m_size;
            }
        }

        /**
         * @brief Retrieves the front element of the ring buffer.
         *
         * This method returns the element at the front of the ring buffer
         * without removing it.
         *
         * @return The element at the front of the ring buffer.
         */
        [[nodiscard]] T front() const
        {
            return m_ring_buffer.at(m_pop_index);
        }

        /**
         * @brief Retrieves the last element in the ring buffer.
         *
         * @return The last element of type T in the ring buffer.
         */
        [[nodiscard]] T back() const
        {
            return m_ring_buffer.at(m_last_index);
        }

        /**
         * @brief Checks if the ring buffer is empty.
         *
         * This method returns true if the ring buffer has no elements
         * i.e. the push index is equal to the pop index.
         *
         * @return true if the ring buffer is empty, false otherwise.
         */
        [[nodiscard]] bool empty() const
        {
            return 0U == m_size;
        }

        /**
         * @brief Checks if the ring buffer is full.
         *
         * @return true if the ring buffer is full, false otherwise.
         */
        [[nodiscard]] bool full() const
        {
            return Capacity <= m_size;
        }

        /**
         * @brief Clears the ring buffer, resetting all indices and size to zero.
         */
        void clear()
        {
            m_push_index = 0U;
            m_pop_index = 0U;
            m_last_index = 0U;
            m_size = 0U;
            m_ring_buffer = {};
        }

        /**
         * @brief Returns the size of the ring buffer.
         *
         * @return std::size_t The current size of the ring buffer.
         */
        [[nodiscard]] std::size_t size() const
        {
            return m_size;
        }

        /**
         * @brief Get the capacity of the ring buffer.
         *
         * @return constexpr std::size_t The capacity of the ring buffer.
         */
        [[nodiscard]] constexpr std::size_t capacity() const
        {
            return Capacity;
        }

        /**
         * @brief Accesses the element at the specified index in the ring buffer.
         *
         * @param index The index of the element to access.
         * @return T& Reference to the element at the specified index.
         */
        T& operator[](std::size_t index)
        {
            return m_ring_buffer.at(next_step_index(m_pop_index, index));
        };

    private:
        /**
         * @brief Computes the next index in a circular buffer.
         *
         * This function calculates the next index in a circular buffer
         * based on the current index and the buffer's capacity.
         *
         * @param index The current index in the buffer.
         * @return The next index in the circular buffer.
         */
        static constexpr std::size_t next_index(std::size_t index)
        {
            return ((index + 1U) % Capacity);
        }

        /**
         * @brief Calculates the next index in the ring buffer given a current index and a step.
         *
         * This function computes the next index by adding the step to the current index
         * and taking the result modulo the buffer capacity.
         *
         * @param index The current index in the ring buffer.
         * @param step The number of steps to move forward in the ring buffer.
         * @return The next index in the ring buffer after moving forward by the specified step.
         */
        static constexpr std::size_t next_step_index(std::size_t index, std::size_t step)
        {
            return ((index + step) % Capacity);
        }

        std::array<T, Capacity> m_ring_buffer = {};
        std::size_t m_push_index = 0U;
        std::size_t m_pop_index = 0U;
        std::size_t m_last_index = 0U;
        std::size_t m_size = 0U;
    };
}

#endif //  RING_BUFFER_HPP_
