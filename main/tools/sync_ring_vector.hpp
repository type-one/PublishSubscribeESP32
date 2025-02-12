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

#if !defined(__SYNC_RING_VECTOR_HPP__)
#define __SYNC_RING_VECTOR_HPP__

#include <cstddef>
#include <mutex>

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"
#include "tools/ring_vector.hpp"

namespace tools
{
    template <typename T>
    class sync_ring_vector : public non_copyable
    {
    public:
        struct thread_safe
        {
            static constexpr bool value = true;
        };

        sync_ring_vector() = delete;
        ~sync_ring_vector() = default;

        sync_ring_vector(std::size_t capacity)
            : m_ring_vector(capacity)
        {
        }

        void push(const T& elem)
        {
            std::lock_guard guard(m_mutex);
            m_ring_vector.push(elem);
        }

        void emplace(T&& elem)
        {
            std::lock_guard guard(m_mutex);
            m_ring_vector.emplace(std::move(elem));
        }

        void pop()
        {
            std::lock_guard guard(m_mutex);
            m_ring_vector.pop();
        }

        T front()
        {
            std::lock_guard guard(m_mutex);
            return m_ring_vector.front();
        }

        T back()
        {
            std::lock_guard guard(m_mutex);
            return m_ring_vector.back();
        }

        bool empty()
        {
            std::lock_guard guard(m_mutex);
            return m_ring_vector.empty();
        }

        bool full()
        {
            std::lock_guard guard(m_mutex);
            return m_ring_vector.full();
        }

        std::size_t size()
        {
            std::lock_guard guard(m_mutex);
            return m_ring_vector.size();
        }

        std::size_t capacity() const
        {
            return m_ring_vector.capacity();
        }

        void resize(std::size_t new_size)
        {
            std::lock_guard guard(m_mutex);
            m_ring_vector.resize(new_size);
        }

        void isr_push(const T& elem)
        {
            tools::isr_lock_guard guard(m_mutex);
            m_ring_vector.push(elem);
        }

        void isr_emplace(T&& elem)
        {
            tools::isr_lock_guard guard(m_mutex);
            m_ring_vector.emplace(elem);
        }

        bool isr_full()
        {
            tools::isr_lock_guard guard(m_mutex);
            return m_ring_vector.full();
        }

        std::size_t isr_size()
        {
            tools::isr_lock_guard guard(m_mutex);
            return m_ring_vector.size();
        }

        void isr_resize(std::size_t new_size)
        {
            tools::isr_lock_guard guard(m_mutex);
            m_ring_vector.resize(new_size);
        }

    private:
        ring_vector<T> m_ring_vector;
        critical_section m_mutex;
    };
}

#endif //  __SYNC_RING_VECTOR_HPP__
