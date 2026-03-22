/**
 * @file lock_free_ring_buffer.hpp
 * @brief A lock-free ring buffer implementation for single producer and single consumer.
 *
 * This header file contains the implementation of a lock-free ring buffer that supports
 * single producer and single consumer. It ensures thread-safe operations without using locks.
 *
 * @author Laurent Lardinois
 * @date February 2025
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
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

#if !defined(LOCK_FREE_RING_BUFFER_HPP_)
#define LOCK_FREE_RING_BUFFER_HPP_

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

#include "tools/non_copyable.hpp"

namespace tools
{
    /**
     * @brief A lock-free ring buffer implementation.
     *
     * This class provides a lock-free ring buffer that supports single producer and single consumer.
     * It ensures that the operations are thread-safe without using locks.
     *
     * @tparam T The type of elements stored in the ring buffer.
     * @tparam Pow2 The power of 2 that determines the size of the ring buffer.
     */
    template <typename T, std::size_t Pow2>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        requires std::is_standard_layout_v<T> && std::is_trivial_v<T> && (std::is_scalar_v<T> || std::is_pointer_v<T>)
#endif
    class lock_free_ring_buffer : public non_copyable // NOLINT inherits from non copyable and non movable class
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

        /**
         * @brief Pushes a copy of an element into the ring buffer.
         *
         * This overload keeps brace-init and exact-T calls unambiguous while
         * preserving legacy call sites.
         *
         * @param elem The element to be pushed into the ring buffer.
         * @return true if the element was successfully pushed, false if the buffer is full.
         */
        [[nodiscard]] bool push(const T& elem)
        {
            return push_val(elem);
        }

        /**
         * @brief Pushes an rvalue element into the ring buffer.
         *
         * This overload preserves brace-init and exact-T call compatibility.
         * For trivial types, move and copy are semantically equivalent; the value
         * is passed by value to the shared push_val helper.
         *
         * @param elem The rvalue element to be pushed into the ring buffer.
         * @return true if the element was successfully pushed, false if the buffer is full.
         */
        [[nodiscard]] bool push(T&& elem)
        {
            return push_val(std::move(elem));
        }

        /**
         * @brief Pushes an element into the ring buffer with perfect forwarding.
         *
         * This template method uses perfect forwarding to support conversions
         * (e.g. pushing an int into a float buffer) beyond the basic exact-T overloads.
         * In C++20, this method is constrained to only accept constructible types.
         *
         * @tparam U The type of the element (deduced, supports conversions).
         * @param elem The element to be forwarded and pushed into the ring buffer.
         * @return true if the element was successfully pushed, false if the buffer is full.
         */
        template <typename U>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, U>
#endif
        [[nodiscard]] auto push(U&& elem)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, U>::value, bool>::type
    #endif
        {
            return push_val(T(std::forward<U>(elem)));
        }

        /**
         * @brief Pops an element from the ring buffer.
         *
         * This function attempts to pop an element from the ring buffer. If the buffer is empty,
         * the function returns false. If the buffer is not empty, it retrieves the next element
         * and updates the read index.
         *
         * @param elem Reference to the element where the popped value will be stored.
         * @return true if an element was successfully popped, false if the buffer is empty.
         */
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
            elem = m_ring_buffer.at(read_idx & ring_buffer_mask).load();
            m_reading.store(false);

            return true;
        }

        /**
         * @brief Pops an element from the ring buffer and returns it wrapped in std::optional.
         *
         * This method provides an alternative to the output-parameter pop() that returns
         * the popped value inside a std::optional, or std::nullopt if the buffer is empty.
         *
         * @return An optional containing the popped element, or std::nullopt if empty.
         */
        [[nodiscard]] std::optional<T> pop_opt()
        {
            T elem {};
            if (pop(elem))
            {
                return elem;
            }
            return std::nullopt;
        }

        /**
         * @brief Pushes all elements from a range into the ring buffer.
         *
         * Returns the count of elements actually pushed; fewer may be pushed if
         * the buffer becomes full before the range is exhausted.
         *
         * In C++20, accepts any std::ranges::input_range whose value type is constructible to T.
         * In C++17, accepts any iterable whose element type is constructible to T.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
         * @return The number of elements successfully pushed.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<
                std::is_constructible<
                    T,
                    decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))
                >::value
            >::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange>
                  && std::is_constructible_v<T, std::ranges::range_value_t<TRange>>
#endif
        [[nodiscard]] std::size_t push_range(TRange&& range)
        {
            std::size_t pushed_count = 0U;
            for (auto&& elem : std::forward<TRange>(range))
            {
                if (push(T(std::forward<decltype(elem)>(elem))))
                {
                    ++pushed_count;
                }
            }
            return pushed_count;
        }

        /**
         * @brief Pushes all elements from an initializer-list into the ring buffer.
         *
         * Returns the count of elements actually pushed; fewer may be pushed if
         * the buffer becomes full before the list is exhausted.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         * @return The number of elements successfully pushed.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, const U&>
#endif
        [[nodiscard]] std::size_t push_range(std::initializer_list<U> range)
        {
            std::size_t pushed_count = 0U;
            for (const auto& elem : range)
            {
                if (push(T(elem)))
                {
                    ++pushed_count;
                }
            }
            return pushed_count;
        }

        /**
         * @brief Pops a batch of elements into an output range.
         *
         * Extracts up to the destination capacity in FIFO order, stopping early
         * if the buffer becomes empty.
         *
         * @tparam OutputIt Output iterator type.
         * @param first Destination begin iterator.
         * @param last Destination end iterator.
         * @return The effective number of elements extracted.
         */
        template <typename OutputIt>
        [[nodiscard]] std::size_t pop_range(OutputIt first, OutputIt last)
        {
            std::size_t popped_count = 0U;
            while (first != last)
            {
                T elem {};
                if (!pop(elem))
                {
                    break;
                }
                *first = elem;
                ++first;
                ++popped_count;
            }
            return popped_count;
        }

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        /**
         * @brief C++20 span-based batch pop into contiguous storage.
         *
         * @param destination Span over writable destination storage.
         * @return The effective number of elements extracted.
         */
        [[nodiscard]] std::size_t pop_range(std::span<T> destination)
        {
            return pop_range(destination.begin(), destination.end());
        }
#endif

        /**
         * @brief Returns the capacity of the ring buffer.
         *
         * This function calculates the capacity of the ring buffer based on the
         * template parameter Pow2. The capacity is determined as 2 raised to the
         * power of Pow2.
         *
         * @return The capacity of the ring buffer.
         */
        [[nodiscard]] constexpr std::size_t capacity() const
        {
            return 1U << Pow2;
        }

    private:
        static constexpr const std::size_t ring_buffer_size = (1U << Pow2);
        static constexpr const std::size_t ring_buffer_mask = (ring_buffer_size - 1U);

        /**
         * @brief Shared push implementation used by all public push overloads.
         *
         * Accepts T by value to allow both copy and move callers to converge on a
         * single code path, avoiding duplication of the lock-free state-machine logic.
         *
         * @param elem The element value to store.
         * @return true if successfully stored, false if the buffer was full.
         */
        bool push_val(T elem)
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
            m_ring_buffer.at(write_idx & ring_buffer_mask).store(elem);
            m_writing.store(false);

            return true;
        }

        std::array<std::atomic<T>, ring_buffer_size> m_ring_buffer;
        std::atomic<std::size_t> m_push_index = 0U;
        std::atomic<std::size_t> m_pop_index = 0U;
        std::atomic_bool m_reading = false;
        std::atomic_bool m_writing = false;
    };
}

#endif //  LOCK_FREE_RING_BUFFER_HPP_
