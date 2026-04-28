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

#if !defined(SYNC_QUEUE_HPP_)
#define SYNC_QUEUE_HPP_

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>
#include <utility>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <concepts>
#include <ranges>
#include <span>
#endif

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
         * @brief Pushes a copy of an element into the queue.
         *
         * This overload keeps brace-init and exact-T calls unambiguous while
         * preserving legacy call sites.
         *
         * @param elem The element to be copied into the queue.
         */
        void push(const T& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_queue.push(elem);
        }

        /**
         * @brief Pushes an rvalue element into the queue.
         *
         * This overload preserves brace-init and exact-T call compatibility.
         *
         * @param elem The element to be moved into the queue.
         */
        void push(T&& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_queue.push(std::move(elem));
        }

        /**
         * @brief Perfectly forwards and pushes an element into the queue.
         *
         * This template method uses perfect forwarding to efficiently handle conversions
         * and optimizations beyond the basic exact-type push overloads.
         * In C++20, this method is constrained to only accept constructible types.
         *
         * @tparam U The type of the element (deduced, supports conversions).
         * @param elem The element to be pushed into the queue.
         */
        template <typename U>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<T, U>
#endif
        auto push(U&& elem)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, U>::value, void>::type
#endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_queue.push(std::forward<U>(elem));
        }

        /**
         * @brief Emplaces variadic arguments into the queue.
         *
         * This method template constructs an element in-place using variadic forwarding,
         * enabling efficient construction without temporary objects.
         *
         * @tparam Args The types of arguments for in-place construction.
         * @param args The arguments to be forwarded to the element constructor.
         */
        template <typename... Args>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<T, Args...>
#endif
        auto emplace(Args&&... args)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, Args...>::value, void>::type
#endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_queue.emplace(std::forward<Args>(args)...);
        }

        /**
         * @brief Removes the front element from the queue.
         *
         * This method locks the mutex to ensure thread safety and then removes the front element from the queue.
         */
        void pop()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
        [[nodiscard]] std::optional<T> front() const
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
        [[nodiscard]] std::optional<T> front_pop()
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_queue.empty())
            {
                item = m_queue.front();
                m_queue.pop();
            }
            return item;
        }

        /**
         * @brief Retrieves and removes the front element of the queue with move semantics.
         *
         * This method returns and removes the front element of the queue using move semantics
         * in a thread-safe manner. Useful for move-only types.
         *
         * @return The front element of the queue, or none if the queue is empty.
         */
        [[nodiscard]] std::optional<T> front_pop_move()
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_queue.empty())
            {
                item = std::move(m_queue.front());
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
        [[nodiscard]] std::optional<T> back() const
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
        [[nodiscard]] std::queue<T> snapshot() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
        [[nodiscard]] bool empty() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
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
        [[nodiscard]] std::size_t size() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_queue.size();
        }

        /**
         * @brief Pushes all elements from an iterator pair into the queue.
         *
         * @tparam InputIt Input iterator type.
         * @param first Begin iterator.
         * @param last End iterator.
         */
        template <typename InputIt>
        void push_range(InputIt first, InputIt last)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (; first != last; ++first)
            {
                m_queue.push(T(*first));
            }
        }

        /**
         * @brief Pushes all elements from a range into the queue.
         *
         * In C++20, accepts any std::ranges::input_range whose value type is constructible to T.
         * In C++17, accepts any iterable whose element type is constructible to T.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<T,
                decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange> && std::constructible_from<T, std::ranges::range_value_t<TRange>>
#endif
        void push_range(TRange&& range)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (auto&& elem : std::forward<TRange>(range))
            {
                m_queue.push(T(std::forward<decltype(elem)>(elem)));
            }
        }

        /**
         * @brief Pushes all elements from an initializer-list into the queue.
         *
         * This overload enables direct brace-init calls like
         * `queue.push_range({ ... })` while preserving type constraints.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<T, const U&>
#endif
        void push_range(std::initializer_list<U> range)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (const auto& elem : range)
            {
                m_queue.push(T(elem));
            }
        }

        /**
         * @brief Pops a batch of elements into an output range under a single lock.
         *
         * Extracts up to destination capacity in FIFO order.
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
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            while ((first != last) && !m_queue.empty())
            {
                *first = std::move(m_queue.front());
                ++first;
                m_queue.pop();
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
         * @brief Pushes a copy of an element into the queue in an ISR-safe manner.
         *
         * This overload keeps brace-init and exact-T calls unambiguous in ISR context.
         *
         * @param elem The element to be copied into the queue.
         */
        void isr_push(const T& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.push(elem);
        }

        /**
         * @brief Pushes an rvalue element into the queue in an ISR-safe manner.
         *
         * This overload preserves brace-init and exact-T call compatibility in ISR context.
         *
         * @param elem The rvalue element to be pushed into the queue.
         */
        void isr_push(T&& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.push(std::move(elem));
        }

        /**
         * @brief Perfectly forwards and pushes an element into the queue in an ISR-safe manner.
         *
         * This template method uses perfect forwarding to efficiently handle conversions
         * and optimizations beyond the basic exact-type push overloads, in ISR context.
         * In C++20, this method is constrained to only accept constructible types.
         *
         * @tparam U The type of the element (deduced, supports conversions).
         * @param elem The element to be pushed into the queue.
         */
        template <typename U>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<T, U>
#endif
        auto isr_push(U&& elem)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, U>::value, void>::type
#endif
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.push(std::forward<U>(elem));
        }

        /**
         * @brief Emplaces variadic arguments into the queue in an interrupt-safe manner.
         *
         * This method template constructs an element in-place using variadic forwarding
         * in an ISR-safe manner.
         *
         * @tparam Args The types of arguments for in-place construction.
         * @param args The arguments to be forwarded to the element constructor.
         */
        template <typename... Args>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<T, Args...>
#endif
        auto isr_emplace(Args&&... args)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<T, Args...>::value, void>::type
#endif
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            m_queue.emplace(std::forward<Args>(args)...);
        }

        /**
         * @brief Returns the size of the queue in an interrupt service routine (ISR) safe manner.
         *
         * This function acquires a lock using an ISR-safe lock guard to ensure that the size of the queue
         * is read in a thread-safe manner, even when called from an ISR.
         *
         * @return The size of the queue.
         */
        [[nodiscard]] std::size_t isr_size() const
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            return m_queue.size();
        }

        /**
         * @brief Pushes all elements from an iterator pair into the queue in an ISR-safe manner.
         *
         * @tparam InputIt Input iterator type.
         * @param first Begin iterator.
         * @param last End iterator.
         */
        template <typename InputIt>
        void isr_push_range(InputIt first, InputIt last)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            for (; first != last; ++first)
            {
                m_queue.push(T(*first));
            }
        }

        /**
         * @brief Pushes all elements from a range into the queue in an ISR-safe manner.
         *
         * In C++20, accepts any std::ranges::input_range whose value type is constructible to T.
         * In C++17, accepts any iterable whose element type is constructible to T.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<T,
                decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange> && std::constructible_from<T, std::ranges::range_value_t<TRange>>
#endif
        void isr_push_range(TRange&& range)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            for (auto&& elem : std::forward<TRange>(range))
            {
                m_queue.push(T(std::forward<decltype(elem)>(elem)));
            }
        }

        /**
         * @brief Pushes all elements from an initializer-list into the queue in an ISR-safe manner.
         *
         * This overload enables direct brace-init calls like
         * `queue.isr_push_range({ ... })` while preserving type constraints.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<T, const U&>
#endif
        void isr_push_range(std::initializer_list<U> range)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex);
            for (const auto& elem : range)
            {
                m_queue.push(T(elem));
            }
        }

    private:
        std::queue<T> m_queue;
        mutable critical_section m_mutex;
    };
}

#endif //  SYNC_QUEUE_HPP_
