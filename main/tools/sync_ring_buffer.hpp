/**
 * @file sync_ring_buffer.hpp
 * @brief A thread-safe ring buffer implementation.
 *
 * This file contains the definition of a thread-safe ring buffer class template.
 * The ring buffer provides basic operations such as push, pop, front, front_pop, back,
 * and size, and ensures thread safety using a mutex.
 *
 * @author Laurent Lardinois
 *
 * @date January 2025
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

#if !defined(SYNC_RING_BUFFER_HPP_)
#define SYNC_RING_BUFFER_HPP_

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"
#include "tools/ring_buffer.hpp"

namespace tools
{
    /**
     * @brief A thread-safe ring buffer implementation.
     *
     * This class provides a thread-safe ring buffer with a fixed capacity.
     * It supports basic operations such as push, pop, front, front_pop, back, and size,
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
         * @brief Pushes a copy of an element into the ring buffer.
         *
         * This overload keeps brace-init and exact-T calls unambiguous while
         * preserving legacy call sites in a thread-safe manner.
         *
         * @param elem The element to be copied into the ring buffer.
         */
        bool push(const T& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push(elem);
        }

        /**
         * @brief Pushes an rvalue element into the ring buffer.
         *
         * This overload preserves brace-init and exact-T call compatibility.
         *
         * @param elem The element to be moved into the ring buffer.
         */
        bool push(T&& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push(std::move(elem));
        }

        /**
         * @brief Pushes an element into the ring buffer using perfect forwarding.
         *
         * This method adds an element to the ring buffer in a thread-safe manner.
         * In C++20, this method is constrained to constructible types.
         *
         * @tparam U The deduced element type.
         * @param elem The element to be pushed into the ring buffer.
         */
        template <typename U>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, U>
#endif
        auto push(U&& elem)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, U>::value, bool>::type
    #endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push(std::forward<U>(elem));
        }

        /**
         * @brief Inserts an element into the ring buffer using perfect forwarding.
         *
         * This function locks the mutex to ensure thread safety and then
         * forwards the arguments into the ring buffer.
         *
         * @tparam Args The deduced constructor argument types.
         * @param args The arguments to be forwarded for insertion.
         */
        template <typename... Args>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, Args...>
#endif
        auto emplace(Args&&... args)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, Args...>::value, bool>::type
    #endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.emplace(std::forward<Args>(args)...);
        }

        /**
         * @brief Removes the oldest element from the ring buffer.
         */
        void pop()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_ring_buffer.empty())
            {
                m_ring_buffer.pop();
            }
        }

        /**
         * @brief Retrieves the front element of the ring buffer.
         *
         * This method locks the mutex to ensure thread safety and then returns the front element of the ring buffer.
         *
         * @return The front element of the ring buffer, or none if the buffer is empty.
         */
        [[nodiscard]] std::optional<T> front() const
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_ring_buffer.empty())
            {
                item = m_ring_buffer.front();
            }
            return item;
        }

        /**
         * @brief Retrieves and removes the front element of the ring buffer.
         *
         * This method locks the mutex to ensure thread safety and then returns and rermoves the front element of
         * the ring buffer.
         *
         * @return The front element of the ring buffer, or none if the buffer is empty.
         */
        [[nodiscard]] std::optional<T> front_pop()
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_ring_buffer.empty())
            {
                item = m_ring_buffer.front();
                m_ring_buffer.pop();
            }
            return item;
        }

        /**
         * @brief Retrieves and removes the front element using move semantics.
         *
         * This method is safe for move-only payload types and forwards extraction
         * to the underlying ring buffer move-pop API under lock.
         *
         * @return The moved front element, or none if the ring buffer is empty.
         */
        [[nodiscard]] std::optional<T> front_pop_move()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.pop_move();
        }

        /**
         * @brief Retrieves the last element from the ring buffer.
         *
         * This function locks the mutex to ensure thread safety and then returns
         * the last element in the ring buffer.
         *
         * @return The last element of type T in the ring buffer, or none if the buffer is empty.
         */
        [[nodiscard]] std::optional<T> back() const
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_ring_buffer.empty())
            {
                item = m_ring_buffer.back();
            }
            return item;
        }

        /**
         * @brief Retrieves a copy of the internal ring buffer.
         *
         * This method locks the mutex to ensure thread safety and then returns a copy of
         * the internal ring buffer.
         *
         * @return A copy of the internal ring buffer.
         */
        [[nodiscard]] tools::ring_buffer<T, Capacity> snapshot() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer;
        }

        /**
         * @brief Checks if the ring buffer is empty.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * while checking if the ring buffer is empty.
         *
         * @return true if the ring buffer is empty, false otherwise.
         */
        [[nodiscard]] bool empty() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
        [[nodiscard]] bool full() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
        [[nodiscard]] std::size_t size() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
         * @brief Pushes all elements from a range into the ring buffer.
         *
         * In C++20, accepts any std::ranges::input_range whose value type is constructible to T.
         * In C++17, accepts any iterable whose element type is constructible to T.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
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
        std::size_t push_range(TRange&& range)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range(std::forward<TRange>(range));
        }

        /**
         * @brief Pushes all elements from an initializer-list into the ring buffer.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, const U&>
#endif
        std::size_t push_range(std::initializer_list<U> range)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range(range);
        }

        /**
         * @brief Result of a push_range_overwrite operation.
         */
        using push_range_overwrite_result = typename tools::ring_buffer<T, Capacity>::push_range_overwrite_result;

        /**
         * @brief Pushes a copy of an element in a thread-safe manner, overwriting the oldest entry if full.
         *
         * @param elem The element to be copied into the ring buffer.
         */
        bool push_overwrite(const T& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_overwrite(elem);
        }

        /**
         * @brief Pushes an rvalue element in a thread-safe manner, overwriting the oldest entry if full.
         *
         * @param elem The element to be moved into the ring buffer.
         */
        bool push_overwrite(T&& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_overwrite(std::move(elem));
        }

        /**
         * @brief Pushes an element using perfect forwarding in a thread-safe manner, overwriting the oldest entry if full.
         *
         * @tparam U The deduced element type.
         * @param elem The element to be pushed into the ring buffer.
         */
        template <typename U>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, U>
#endif
        auto push_overwrite(U&& elem)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, U>::value, bool>::type
    #endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_overwrite(std::forward<U>(elem));
        }

        /**
         * @brief Constructs an element in place in a thread-safe manner, overwriting the oldest entry if full.
         *
         * @tparam Args The deduced constructor argument types.
         * @param args The arguments to be forwarded for construction.
         */
        template <typename... Args>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, Args...>
#endif
        auto emplace_overwrite(Args&&... args)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, Args...>::value, bool>::type
    #endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.emplace_overwrite(std::forward<Args>(args)...);
        }

        /**
         * @brief Pushes all elements from a range in a thread-safe manner, overwriting the oldest entries if full.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
         * @return Result indicating how many elements were inserted vs. how many overwrote existing entries.
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
        push_range_overwrite_result push_range_overwrite(TRange&& range)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range_overwrite(std::forward<TRange>(range));
        }

        /**
         * @brief Pushes all elements from an initializer-list in a thread-safe manner, overwriting the oldest entries if full.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         * @return Result indicating how many elements were inserted vs. how many overwrote existing entries.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, const U&>
#endif
        push_range_overwrite_result push_range_overwrite(std::initializer_list<U> range)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range_overwrite(range);
        }

        /**
         * @brief Pops a batch of elements into an output range under a single lock.
         *
         * @tparam OutputIt Output iterator type.
         * @param first Destination begin iterator.
         * @param last Destination end iterator.
         * @return The effective number of elements extracted.
         */
        template <typename OutputIt>
        [[nodiscard]] std::size_t pop_range(OutputIt first, OutputIt last)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.pop_range(first, last);
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
         * @brief Pushes a copy of an element into the ring buffer in an ISR-safe manner.
         *
         * This overload keeps brace-init and exact-T calls unambiguous while
         * preserving legacy ISR call sites.
         *
         * @param elem The element to be copied into the ring buffer.
         */
        void isr_push(const T& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.push(elem);
        }

        /**
         * @brief Pushes an rvalue element into the ring buffer in an ISR-safe manner.
         *
         * This overload preserves brace-init and exact-T call compatibility.
         *
         * @param elem The element to be moved into the ring buffer.
         */
        void isr_push(T&& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.push(std::move(elem));
        }

        /**
         * @brief Pushes an element into the ring buffer in an ISR-safe manner using perfect forwarding.
         *
         * This function uses an ISR lock guard to ensure that the push operation is thread-safe
         * and can be safely called from within an ISR.
         * In C++20, this method is constrained to constructible types.
         *
         * @tparam U The deduced element type.
         * @param elem The element to be pushed into the ring buffer.
         */
        template <typename U>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, U>
#endif
        auto isr_push(U&& elem)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, U>::value, void>::type
    #endif
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.push(std::forward<U>(elem));
        }

        /**
         * @brief Emplaces an element into the ring buffer in ISR context using perfect forwarding.
         *
         * This function is designed to be called from an ISR. It locks the critical section
         * to ensure thread safety and then emplaces the given element into the ring buffer.
         * In C++20, this method is constrained to constructible argument sets.
         *
         * @tparam Args The deduced constructor argument types.
         * @param args The arguments to be emplaced into the ring buffer.
         */
        template <typename... Args>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, Args...>
#endif
        auto isr_emplace(Args&&... args)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, Args...>::value, void>::type
    #endif
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_ring_buffer.emplace(std::forward<Args>(args)...);
        }

        /**
         * @brief Pushes all elements from a range into the ring buffer in an ISR-safe manner.
         *
         * In C++20, accepts any std::ranges::input_range whose value type is constructible to T.
         * In C++17, accepts any iterable whose element type is constructible to T.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
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
        std::size_t isr_push_range(TRange&& range)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range(std::forward<TRange>(range));
        }

        /**
         * @brief Pushes all elements from an initializer-list into the ring buffer in an ISR-safe manner.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, const U&>
#endif
        std::size_t isr_push_range(std::initializer_list<U> range)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range(range);
        }

        /**
         * @brief Pushes a copy of an element in an ISR-safe manner, overwriting the oldest entry if full.
         *
         * @param elem The element to be copied into the ring buffer.
         */
        bool isr_push_overwrite(const T& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_overwrite(elem);
        }

        /**
         * @brief Pushes an rvalue element in an ISR-safe manner, overwriting the oldest entry if full.
         *
         * @param elem The element to be moved into the ring buffer.
         */
        bool isr_push_overwrite(T&& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_overwrite(std::move(elem));
        }

        /**
         * @brief Pushes an element using perfect forwarding in an ISR-safe manner, overwriting the oldest entry if full.
         *
         * @tparam U The deduced element type.
         * @param elem The element to be pushed into the ring buffer.
         */
        template <typename U>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, U>
#endif
        auto isr_push_overwrite(U&& elem)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, U>::value, bool>::type
    #endif
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_overwrite(std::forward<U>(elem));
        }

        /**
         * @brief Constructs an element in place in an ISR-safe manner, overwriting the oldest entry if full.
         *
         * @tparam Args The deduced constructor argument types.
         * @param args The arguments to be forwarded for construction.
         */
        template <typename... Args>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, Args...>
#endif
        auto isr_emplace_overwrite(Args&&... args)
    #if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, Args...>::value, bool>::type
    #endif
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.emplace_overwrite(std::forward<Args>(args)...);
        }

        /**
         * @brief Pushes all elements from a range in an ISR-safe manner, overwriting the oldest entries if full.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
         * @return Result indicating how many elements were inserted vs. how many overwrote existing entries.
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
        push_range_overwrite_result isr_push_range_overwrite(TRange&& range)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range_overwrite(std::forward<TRange>(range));
        }

        /**
         * @brief Pushes all elements from an initializer-list in an ISR-safe manner, overwriting the oldest entries if full.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         * @return Result indicating how many elements were inserted vs. how many overwrote existing entries.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, const U&>
#endif
        push_range_overwrite_result isr_push_range_overwrite(std::initializer_list<U> range)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.push_range_overwrite(range);
        }

        /**
         * @brief Checks if the ring buffer is full in an interrupt service routine (ISR) context.
         *
         * This function uses a lock guard to ensure thread safety while checking if the ring buffer is full.
         *
         * @return true if the ring buffer is full, false otherwise.
         */
        [[nodiscard]] bool isr_full() const
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
        [[nodiscard]] std::size_t isr_size() const
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_ring_buffer.size();
        }

    private:
        ring_buffer<T, Capacity> m_ring_buffer;
        mutable critical_section m_mutex;
    };
}

#endif //  SYNC_RING_BUFFER_HPP_
