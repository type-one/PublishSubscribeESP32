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

#if !defined(__LOCK_FREE_RING_BUFFER_HPP__)
#define __LOCK_FREE_RING_BUFFER_HPP__

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "tools/non_copyable.hpp"

namespace tools
{
    template <typename T, std::size_t Pow2>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    requires std::is_standard_layout_v<T> && std::is_trivial_v<T> &&(std::is_scalar_v<T> || std::is_pointer_v<T>)
#endif
        class lock_free_ring_buffer : public non_copyable
    {
    public:
        static_assert(std::is_standard_layout<T>::value, "T has to provide standard layout");
        static_assert(std::is_trivial<T>::value, "T has to be trivial type");
        static_assert(std::is_scalar<T>::value || std::is_pointer<T>::value, "T has to be scalar or pointer");

        struct thread_safe
        {
            // Single Producer - Single Consumer only
            static constexpr bool value = false;
        };

        lock_free_ring_buffer() = default;
        ~lock_free_ring_buffer() = default;

        bool push(const T& elem)
        {
            const std::size_t snap_write_idx = m_push_index.load();
            const std::size_t snap_read_idx = m_pop_index.load();

            // is full ?
            if ((snap_read_idx & ring_buffer_mask) == ((snap_write_idx + 1U) & ring_buffer_mask))
            {
                return false;
            }

            // getting close or wrap around, risk of race condition
            if (((snap_write_idx - snap_read_idx) <= 2U) || (snap_write_idx < snap_read_idx))
            {
                do
                {
                } while (m_reading.load());
            }

            m_writing.store(true);
            const std::size_t write_idx = m_push_index.fetch_add(1U);
            m_ring_buffer[write_idx & ring_buffer_mask].store(elem);
            m_writing.store(false);

            return true;
        }

        bool pop(T& elem)
        {
            const std::size_t snap_write_idx = m_push_index.load();
            const std::size_t snap_read_idx = m_pop_index.load();

            // is empty ?
            if ((snap_read_idx & ring_buffer_mask) == (snap_write_idx & ring_buffer_mask))
            {
                return false;
            }

            // getting close or wrap around, risk of race condition
            if (((snap_write_idx - snap_read_idx) <= 2U) || (snap_write_idx < snap_read_idx))
            {
                do
                {
                } while (m_writing.load());
            }

            m_reading.store(true);
            const std::size_t read_idx = m_pop_index.fetch_add(1U);
            elem = m_ring_buffer[read_idx & ring_buffer_mask].load();
            m_reading.store(false);

            return true;
        }

        constexpr std::size_t capacity() const
        {
            return 1U << Pow2;
        }

    private:
        static constexpr const std::size_t ring_buffer_size = (1U << Pow2);
        static constexpr const std::size_t ring_buffer_mask = (ring_buffer_size - 1U);

        std::array<std::atomic<T>, ring_buffer_size> m_ring_buffer;
        std::atomic<std::size_t> m_push_index = 0U;
        std::atomic<std::size_t> m_pop_index = 0U;
        std::atomic_bool m_reading = false;
        std::atomic_bool m_writing = false;
    };
}

#endif //  __LOCK_FREE_RING_BUFFER_HPP__
