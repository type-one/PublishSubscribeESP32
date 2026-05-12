/**
 * @file sync_priority_queue.hpp
 * @brief A thread-safe priority queue implementation.
 *
 * This file contains the definition of the sync_priority_queue class, which provides a thread-safe
 * priority queue with various methods to manipulate the queue. It is part of the Publish/Subscribe
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

#if !defined(SYNC_PRIORITY_QUEUE_HPP_)
#define SYNC_PRIORITY_QUEUE_HPP_

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>
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
     * @brief A thread-safe priority queue implementation.
     *
     * This class provides a thread-safe priority queue with various methods to manipulate the queue.
     * It inherits from non_copyable to prevent copying and moving.
     *
     * The priority queue uses `std::greater<T>` as the default comparator, making min-heap behavior the default.
     * For max-heap behavior, use the sync_max_priority_queue convenience alias or instantiate with `std::less<T>`.
     *
     * Usage with async_observer is transparent:
     * - async_observer<Topic, Evt, sync_priority_queue> uses default std::greater<T>
     * - For custom comparators, create a template alias or wrapper.
     *
     * @tparam T The type of elements stored in the priority queue.
     * @tparam Compare The comparison function to use for ordering elements (default: std::greater<T>).
     */
    template <typename T, typename Compare = std::greater<T>>
    class sync_priority_queue : public non_copyable // NOLINT inherits from non copyable/non movable
    {
    public:
        sync_priority_queue() = default;
        ~sync_priority_queue() = default;

        struct thread_safe
        {
            static constexpr bool value = true;
        };

        /**
         * @brief Pushes a copy of an element into the priority queue.
         *
         * This overload keeps brace-init and exact-T calls unambiguous while
         * preserving legacy call sites.
         *
         * @param elem The element to be copied into the priority queue.
         */
        void push(const T& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_priority_queue.push(elem);
        }

        /**
         * @brief Pushes an rvalue element into the priority queue.
         *
         * This overload preserves brace-init and exact-T call compatibility.
         *
         * @param elem The element to be moved into the priority queue.
         */
        void push(T&& elem)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_priority_queue.push(std::move(elem));
        }

        /**
         * @brief Perfectly forwards and pushes an element into the priority queue.
         *
         * This template method uses perfect forwarding to efficiently handle conversions
         * and optimizations beyond the basic exact-type push overloads.
         * In C++20, this method is constrained to only accept constructible types.
         *
         * @tparam U The type of the element (deduced, supports conversions).
         * @param elem The element to be pushed into the priority queue.
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
            m_priority_queue.push(std::forward<U>(elem));
        }

        /**
         * @brief Emplaces variadic arguments into the priority queue.
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
            m_priority_queue.emplace(std::forward<Args>(args)...);
        }

        /**
         * @brief Removes the top element from the priority queue.
         *
         * This method locks the mutex to ensure thread safety and then removes the top element
         * (highest priority element) from the priority queue.
         */
        void pop()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_priority_queue.empty())
            {
                m_priority_queue.pop();
            }
        }

        /**
         * @brief Retrieves the top element of the priority queue.
         *
         * This method returns the top element of the priority queue in a thread-safe manner.
         * The top element is the one with the highest priority (determined by the comparator).
         *
         * @return The top element of the priority queue, or none if the queue is empty.
         */
        [[nodiscard]] std::optional<T> top() const
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_priority_queue.empty())
            {
                item = m_priority_queue.top();
            }
            return item;
        }

        /**
         * @brief Retrieves and removes the top element of the priority queue.
         *
         * This method returns and removes the top element of the priority queue in a thread-safe manner.
         *
         * @return The top element of the priority queue, or none if the queue is empty.
         */
        [[nodiscard]] std::optional<T> top_pop()
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_priority_queue.empty())
            {
                item = m_priority_queue.top();
                m_priority_queue.pop();
            }
            return item;
        }

        /**
         * @brief Retrieves and removes the top element of the priority queue with move semantics.
         *
         * This method returns and removes the top element of the priority queue using move semantics
         * in a thread-safe manner. Useful for move-only types.
         *
         * @return The top element of the priority queue, or none if the queue is empty.
         */
        [[nodiscard]] std::optional<T> top_pop_move()
        {
            std::optional<T> item;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            if (!m_priority_queue.empty())
            {
                item = std::move(const_cast<T&>(m_priority_queue.top())); // NOLINT const_cast for move
                m_priority_queue.pop();
            }
            return item;
        }

        /**
         * @brief Retrieves the front (highest priority) element of the priority queue.
         *
         * This is a compatibility method for APIs expecting queue-like semantics.
         * Delegates to top() for priority queue semantics.
         *
         * @return The front (highest priority) element, or none if the queue is empty.
         */
        [[nodiscard]] std::optional<T> front() const
        {
            return top();
        }

        /**
         * @brief Retrieves and removes the front (highest priority) element of the priority queue.
         *
         * This is a compatibility method for APIs expecting queue-like semantics.
         * Delegates to top_pop() for priority queue semantics. Used by async_observer
         * for transparent container integration.
         *
         * @return The front (highest priority) element, or none if the queue is empty.
         */
        [[nodiscard]] std::optional<T> front_pop()
        {
            return top_pop();
        }

        /**
         * @brief Checks if the priority queue is empty.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * and then checks if the underlying priority queue is empty.
         *
         * @return true if the priority queue is empty, false otherwise.
         */
        [[nodiscard]] bool empty() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_priority_queue.empty();
        }

        /**
         * @brief Returns the number of elements in the priority queue.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * while accessing the underlying priority queue's size.
         *
         * @return The number of elements in the priority queue.
         */
        [[nodiscard]] std::size_t size() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_priority_queue.size();
        }

        /**
         * @brief Pushes all elements from an iterator pair into the priority queue.
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
                m_priority_queue.push(T(*first));
            }
        }

        /**
         * @brief Pushes all elements from a range into the priority queue.
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
                m_priority_queue.push(T(std::forward<decltype(elem)>(elem)));
            }
        }

        /**
         * @brief Pushes all elements from an initializer-list into the priority queue.
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
                m_priority_queue.push(T(elem));
            }
        }

        /**
         * @brief Pops a batch of elements into an output range under a single lock.
         *
         * Extracts up to destination capacity in priority order (highest priority first).
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
            while ((first != last) && !m_priority_queue.empty())
            {
                *first = std::move(const_cast<T&>(m_priority_queue.top())); // NOLINT const_cast for move
                ++first;
                m_priority_queue.pop();
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
         * @brief Pushes a copy of an element into the priority queue in an ISR-safe manner.
         *
         * This overload keeps brace-init and exact-T calls unambiguous in ISR context.
         *
         * @param elem The element to be copied into the priority queue.
         */
        void isr_push(const T& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            m_priority_queue.push(elem);
        }

        /**
         * @brief Pushes an rvalue element into the priority queue in an ISR-safe manner.
         *
         * This overload preserves brace-init and exact-T call compatibility in ISR context.
         *
         * @param elem The rvalue element to be pushed into the priority queue.
         */
        void isr_push(T&& elem)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            m_priority_queue.push(std::move(elem));
        }

        /**
         * @brief Perfectly forwards and pushes an element into the priority queue in an ISR-safe manner.
         *
         * This template method uses perfect forwarding to efficiently handle conversions
         * and optimizations beyond the basic exact-type push overloads, in ISR context.
         * In C++20, this method is constrained to only accept constructible types.
         *
         * @tparam U The type of the element (deduced, supports conversions).
         * @param elem The element to be pushed into the priority queue.
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
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            m_priority_queue.push(std::forward<U>(elem));
        }

        /**
         * @brief Emplaces variadic arguments into the priority queue in an interrupt-safe manner.
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
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            m_priority_queue.emplace(std::forward<Args>(args)...);
        }

        /**
         * @brief Returns the size of the priority queue in an interrupt service routine (ISR) safe manner.
         *
         * This function acquires a lock using an ISR-safe lock guard to ensure that the size of the queue
         * is read in a thread-safe manner, even when called from an ISR.
         *
         * @return The size of the priority queue.
         */
        [[nodiscard]] std::size_t isr_size() const
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            return m_priority_queue.size();
        }

        /**
         * @brief Pushes all elements from an iterator pair into the priority queue in an ISR-safe manner.
         *
         * @tparam InputIt Input iterator type.
         * @param first Begin iterator.
         * @param last End iterator.
         */
        template <typename InputIt>
        void isr_push_range(InputIt first, InputIt last)
        {
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            for (; first != last; ++first)
            {
                m_priority_queue.push(T(*first));
            }
        }

        /**
         * @brief Pushes all elements from a range into the priority queue in an ISR-safe manner.
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
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            for (auto&& elem : std::forward<TRange>(range))
            {
                m_priority_queue.push(T(std::forward<decltype(elem)>(elem)));
            }
        }

        /**
         * @brief Pushes all elements from an initializer-list into the priority queue in an ISR-safe manner.
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
            tools::isr_lock_guard<tools::critical_section> guard(m_mutex); // NOLINT(modernize-use-scoped-lock)
            for (const auto& elem : range)
            {
                m_priority_queue.push(T(elem));
            }
        }

    private:
        std::priority_queue<T, std::vector<T>, Compare> m_priority_queue;
        mutable critical_section m_mutex;
    };

    /**
     * @brief Template alias for creating a max-heap priority queue (highest priority first).
     *
     * This is a convenience alias for `sync_priority_queue<T, std::less<T>>` to create
     * a priority queue where larger elements have higher priority.
     *
     * @tparam T The type of elements stored in the priority queue.
     *
     * Example usage:
     * ```cpp
     * sync_max_priority_queue<int> max_queue;  // Larger ints have higher priority
     * ```
     */
    template <typename T>
    using sync_max_priority_queue = sync_priority_queue<T, std::less<T>>;

}

#endif //  SYNC_PRIORITY_QUEUE_HPP_
