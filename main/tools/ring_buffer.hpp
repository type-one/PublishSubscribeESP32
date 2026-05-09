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

#if !defined(RING_BUFFER_HPP_)
#define RING_BUFFER_HPP_

#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

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
        ring_buffer(const ring_buffer& other) = default;

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
            , m_push_index { other.m_push_index }
            , m_pop_index { other.m_pop_index }
            , m_last_index { other.m_last_index }
            , m_size { other.m_size }
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
                m_push_index = other.m_push_index;
                m_pop_index = other.m_pop_index;
                m_last_index = other.m_last_index;
                m_size = other.m_size;
            }

            return *this;
        }

        /**
         * @brief Pushes a copy of an element into the ring buffer.
         *
         * This overload keeps brace-init and exact-T calls unambiguous while
         * preserving legacy call sites.
         *
         * @param elem The element to be copied into the ring buffer.
         */
        bool push(const T& elem)
        {
            return write_value(elem, overflow_policy::reject) != write_status::rejected;
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
            return write_value(std::move(elem), overflow_policy::reject) != write_status::rejected;
        }

        /**
         * @brief Pushes an element into the ring buffer with perfect forwarding.
         *
         * This template method uses perfect forwarding to efficiently handle both
         * lvalue and rvalue references, eliminating the need for separate push/emplace methods.
         * In C++20, this method is constrained to only accept constructible types.
         *
         * @tparam U The type of the element (deduced, supports both const T& and T&&).
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
            return write_value(std::forward<U>(elem), overflow_policy::reject) != write_status::rejected;
        }

        /**
         * @brief Constructs an element in place in the ring buffer.
         *
         * This method allows flexible construction of elements with multiple arguments
         * using perfect forwarding.
         * In C++20, this method is constrained to only accept constructible arguments.
         *
         * @tparam Args Parameter types for T's constructor (deduced).
         * @param args Arguments to forward to T's constructor.
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
            return write_value(T(std::forward<Args>(args)...), overflow_policy::reject) != write_status::rejected;
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
         * @brief Retrieves and removes the oldest element using move semantics.
         *
         * This method is suitable for move-only payload types as it returns
         * the extracted value by move in an optional container.
         *
         * @return The moved front element, or none if the ring buffer is empty.
         */
        std::optional<T> pop_move()
        {
            std::optional<T> item;
            if (!empty())
            {
                item.emplace(std::move(m_ring_buffer.at(m_pop_index)));
                m_pop_index = next_index(m_pop_index);
                --m_size;
            }
            return item;
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
            ,
            typename = typename std::enable_if<std::is_constructible<T,
                decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange> && std::is_constructible_v<T, std::ranges::range_value_t<TRange>>
#endif
        std::size_t push_range(TRange&& range)
        {
            std::size_t inserted = 0U;
            for (auto&& elem : std::forward<TRange>(range))
            {
                if (!push(T(std::forward<decltype(elem)>(elem))))
                {
                    break;
                }
                ++inserted;
            }
            return inserted;
        }

        /**
         * @brief Pushes all elements from an initializer-list into the ring buffer.
         *
         * This overload enables direct brace-init calls like
         * `buffer.push_range({ ... })` while preserving type constraints.
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
            requires std::is_constructible_v<T, const U&>
#endif
        std::size_t push_range(std::initializer_list<U> range)
        {
            std::size_t inserted = 0U;
            for (const auto& elem : range)
            {
                if (!push(T(elem)))
                {
                    break;
                }
                ++inserted;
            }
            return inserted;
        }

        /**
         * @brief Result of a push_range_overwrite operation.
         *
         * Reports how many items were freshly inserted into available capacity
         * vs. how many items overwrote an existing (oldest) entry.
         */
        struct push_range_overwrite_result
        {
            std::size_t inserted = 0U;
            std::size_t overwritten = 0U;
        };

        /**
         * @brief Pushes a copy of an element, overwriting the oldest entry if full.
         *
         * @param elem The element to be copied into the ring buffer.
         */
        bool push_overwrite(const T& elem)
        {
            return write_value(elem, overflow_policy::overwrite) == write_status::overwritten;
        }

        /**
         * @brief Pushes an rvalue element, overwriting the oldest entry if full.
         *
         * @param elem The element to be moved into the ring buffer.
         */
        bool push_overwrite(T&& elem)
        {
            return write_value(std::move(elem), overflow_policy::overwrite) == write_status::overwritten;
        }

        /**
         * @brief Pushes an element using perfect forwarding, overwriting the oldest entry if full.
         *
         * @tparam U The type of the element (deduced).
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
            return write_value(std::forward<U>(elem), overflow_policy::overwrite) == write_status::overwritten;
        }

        /**
         * @brief Constructs an element in place, overwriting the oldest entry if full.
         *
         * @tparam Args Parameter types for T's constructor (deduced).
         * @param args Arguments to forward to T's constructor.
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
            return write_value(T(std::forward<Args>(args)...), overflow_policy::overwrite) == write_status::overwritten;
        }

        /**
         * @brief Pushes all elements from a range, overwriting the oldest entries if full.
         *
         * @tparam TRange The range type (deduced).
         * @param range The source range of elements to push.
         * @return Result indicating how many elements were inserted vs. how many overwrote existing entries.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<T,
                decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange> && std::is_constructible_v<T, std::ranges::range_value_t<TRange>>
#endif
        push_range_overwrite_result push_range_overwrite(TRange&& range)
        {
            push_range_overwrite_result result;
            for (auto&& elem : std::forward<TRange>(range))
            {
                const bool overwritten = push_overwrite(T(std::forward<decltype(elem)>(elem)));
                result.overwritten += overwritten ? 1U : 0U;
                result.inserted += overwritten ? 0U : 1U;
            }
            return result;
        }

        /**
         * @brief Pushes all elements from an initializer-list, overwriting the oldest entries if full.
         *
         * @tparam U The initializer-list element type.
         * @param range The source initializer-list.
         * @return Result indicating how many elements were inserted vs. how many overwrote existing entries.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<T, const U&>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<T, const U&>
#endif
        push_range_overwrite_result push_range_overwrite(std::initializer_list<U> range)
        {
            push_range_overwrite_result result;
            for (const auto& elem : range)
            {
                const bool overwritten = push_overwrite(T(elem));
                result.overwritten += overwritten ? 1U : 0U;
                result.inserted += overwritten ? 0U : 1U;
            }
            return result;
        }

        /**
         * @brief Pops a batch of elements into an output range.
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
            while ((first != last) && !empty())
            {
                *first = front();
                ++first;
                pop();
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
        enum class overflow_policy : unsigned char
        {
            reject,
            overwrite
        };

        enum class write_status : unsigned char
        {
            rejected,
            inserted,
            overwritten
        };

        template <typename U>
        write_status write_value(U&& elem, overflow_policy policy)
        {
            const bool is_full = (m_size >= Capacity);
            if (is_full && (policy == overflow_policy::reject))
            {
                return write_status::rejected;
            }

            if (is_full)
            {
                m_pop_index = next_index(m_pop_index);
            }
            else
            {
                ++m_size;
            }

            m_ring_buffer.at(m_push_index) = std::forward<U>(elem);
            m_last_index = m_push_index;
            m_push_index = next_index(m_push_index);

            return is_full ? write_status::overwritten : write_status::inserted;
        }

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
