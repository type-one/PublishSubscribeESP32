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

#if !defined(__RING_VECTOR_HPP__)
#define __RING_VECTOR_HPP__

#include <cstddef>
#include <iterator>
#include <vector>

namespace tools
{
    template <typename T>
    class ring_vector
    {
    public:
        struct thread_safe
        {
            static constexpr bool value = false;
        };

        ring_vector() = delete;
        ~ring_vector() = default;

        ring_vector(std::size_t capacity)
            : m_ring_vector(capacity)
            , m_capacity(capacity)
        {
        }

        ring_vector(const ring_vector& other)
        {
            m_ring_vector = other.m_ring_vector;
            m_push_index = other.m_push_index;
            m_pop_index = other.m_pop_index;
            m_last_index = other.m_last_index;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
        }

        ring_vector(ring_vector&& other)
        {
            m_ring_vector = std::move(other.m_ring_vector);
            m_push_index = std::move(other.m_push_index);
            m_pop_index = std::move(other.m_pop_index);
            m_last_index = std::move(other.m_last_index);
            m_size = std::move(other.m_size);
            m_capacity = std::move(other.m_capacity);
        }

        ring_vector& operator=(const ring_vector& other)
        {
            if (this != &other)
            {
                m_ring_vector = other.m_ring_vector;
                m_push_index = other.m_push_index;
                m_pop_index = other.m_pop_index;
                m_last_index = other.m_last_index;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
            }

            return *this;
        }

        ring_vector& operator=(ring_vector&& other)
        {
            if (this != &other)
            {
                m_ring_vector = std::move(other.m_ring_vector);
                m_push_index = std::move(other.m_push_index);
                m_pop_index = std::move(other.m_pop_index);
                m_last_index = std::move(other.m_last_index);
                m_size = std::move(other.m_size);
                m_capacity = std::move(other.m_capacity);
            }

            return *this;
        }

        void push(const T& elem)
        {
            m_ring_vector[m_push_index] = elem;
            m_last_index = m_push_index;
            m_push_index = next_index(m_push_index);
            ++m_size;
        }

        void emplace(T&& elem)
        {
            m_ring_vector[m_push_index] = std::move(elem);
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
            return m_ring_vector[m_pop_index];
        }

        T back() const
        {
            return m_ring_vector[m_last_index];
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
            m_ring_vector.clear();
            m_ring_vector.resize(m_capacity);
        }

        std::size_t size() const
        {
            return m_size;
        }

        std::size_t capacity() const
        {
            return m_capacity;
        }

        T& operator[](std::size_t index)
        {
            return *(m_ring_vector.data() + next_step_index(m_pop_index, index));
        };

        class iterator;

        iterator begin()
        {
            return ring_vector<T>::iterator(*this, 0);
        }

        iterator end()
        {
            return ring_vector<T>::iterator(*this, this->size());
        }

        // https://stackoverflow.com/questions/58887399/create-a-simple-forward-iterator-which-automatically-wraps-at-the-end-of-a-cir
        // https://internalpointers.com/post/writing-custom-iterators-modern-cpp
        // https://lorenzotoso.wordpress.com/2016/01/13/defining-a-custom-iterator-in-c/
        class iterator : public std::iterator<std::output_iterator_tag, T, int, T*, T&>
        {
            friend ring_vector<T>;

        public:
            T& operator*() const
            {
                return m_ring_vector_ref[m_iterator_index];
            }

            iterator& operator+(int step)
            {
                m_iterator_index = m_ring_vector_ref.next_step_index(m_iterator_index, static_cast<std::size_t>(step));
                return *this;
            }

            iterator& operator++()
            {
                m_iterator_index = m_ring_vector_ref.next_index(m_iterator_index);
                return *this;
            }

            iterator operator++(int)
            {
                return ++(*this);
            }

            bool operator!=(const iterator& rhs) const
            {
                return m_iterator_index != rhs.m_iterator_index;
            }

            bool operator==(const iterator& rhs) const
            {
                return m_iterator_index == rhs.m_iterator_index;
            }

            explicit iterator(ring_vector<T>& container, std::size_t index = 0U)
                : m_ring_vector_ref(container)
                , m_iterator_index(index % container.capacity())
            {
            }

        private:
            ring_vector<T>& m_ring_vector_ref;
            std::size_t m_iterator_index = 0U;
        };

    private:
        std::size_t next_index(std::size_t index) const
        {
            return ((index + 1U) % m_capacity);
        }
        std::size_t next_step_index(std::size_t index, std::size_t step) const
        {
            return ((index + step) % m_capacity);
        }

        std::vector<T> m_ring_vector;
        std::size_t m_push_index = 0U;
        std::size_t m_pop_index = 0U;
        std::size_t m_last_index = 0U;
        std::size_t m_size = 0U;
        std::size_t m_capacity = 1U;
    };
}

#endif //  __RING_VECTOR_HPP__
