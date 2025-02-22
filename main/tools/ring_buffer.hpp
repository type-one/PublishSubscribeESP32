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
    template <typename T, std::size_t Capacity>
    class ring_buffer
    {
    public:
        struct thread_safe
        {
            static constexpr bool value = false;
        };

        ring_buffer() = default;
        ~ring_buffer() = default;

        ring_buffer(const ring_buffer& other)
        {
            m_ring_buffer = other.m_ring_buffer;
            m_push_index = other.m_push_index;
            m_pop_index = other.m_pop_index;
            m_last_index = other.m_last_index;
            m_size = other.m_size;
        }

        ring_buffer(ring_buffer&& other)
        {
            m_ring_buffer = std::move(other.m_ring_buffer);
            m_push_index = std::move(other.m_push_index);
            m_pop_index = std::move(other.m_pop_index);
            m_last_index = std::move(other.m_last_index);
            m_size = std::move(other.m_size);
        }

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

        ring_buffer& operator=(ring_buffer&& other)
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

        void push(const T& elem)
        {
            m_ring_buffer[m_push_index] = elem;
            m_last_index = m_push_index;
            m_push_index = next_index(m_push_index);
            ++m_size;
        }

        void emplace(T&& elem)
        {
            m_ring_buffer[m_push_index] = std::move(elem);
            m_last_index = m_push_index;
            m_push_index = next_index(m_push_index);
            ++m_size;
        }

        void pop()
        {
            m_pop_index = next_index(m_pop_index);
            --m_size;
        }

        T front() const
        {
            return m_ring_buffer[m_pop_index];
        }

        T back() const
        {
            return m_ring_buffer[m_last_index];
        }

        bool empty() const
        {
            return m_push_index == m_pop_index;
        }

        bool full() const
        {
            return next_index(m_push_index) == m_pop_index;
        }

        void clear()
        {
            m_push_index = 0U;
            m_pop_index = 0U;
            m_last_index = 0U;
            m_size = 0U;
            m_ring_buffer = {};
        }

        std::size_t size() const
        {
            return m_size;
        }

        constexpr std::size_t capacity() const
        {
            return Capacity;
        }

        T& operator[](std::size_t index)
        {
            return m_ring_buffer[next_step_index(m_pop_index, index)];
        };

    private:
        static constexpr std::size_t next_index(std::size_t index)
        {
            return ((index + 1U) % Capacity);
        }
        static constexpr std::size_t next_step_index(std::size_t index, std::size_t step)
        {
            return ((index + step) % Capacity);
        }

        std::array<T, Capacity> m_ring_buffer;
        std::size_t m_push_index = 0U;
        std::size_t m_pop_index = 0U;
        std::size_t m_last_index = 0U;
        std::size_t m_size = 0U;
    };
}

#endif //  RING_BUFFER_HPP_
