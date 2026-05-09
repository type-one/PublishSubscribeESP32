/**
 * @file ring_vector.hpp
 * @brief A thread-unsafe ring buffer implementation using a vector.
 *
 * This file contains the definition of the ring_vector class template, which provides
 * a ring buffer (circular buffer) implementation using a vector. It supports basic
 * operations such as push, pop, front, back, and resizing.
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

#if !defined(RING_VECTOR_HPP_)
#define RING_VECTOR_HPP_

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

namespace tools
{

    /**
     * @brief A thread-unsafe ring buffer implementation using a vector.
     *
     * This class provides a ring buffer (circular buffer) implementation using a vector.
     * It supports basic operations such as push, pop, front, back, and resizing.
     *
     * @tparam T The type of elements stored in the ring buffer.
     */
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

        /**
         * @brief Constructs a ring_vector with the specified capacity.
         *
         * @param capacity The capacity of the ring_vector
         */
        explicit ring_vector(std::size_t capacity)
            : m_ring_vector(capacity)
            , m_capacity(capacity)
        {
        }

        /**
         * @brief Copy constructor for the ring_vector class.
         *
         * This constructor creates a new ring_vector object by copying the contents
         * of another ring_vector object.
         *
         * @param other The ring_vector object to copy from.
         */
        ring_vector(const ring_vector& other) = default;

        /**
         * @brief Move constructor for the ring_vector class.
         *
         * This constructor initializes a ring_vector object by transferring ownership
         * of the data from another ring_vector object. The other ring_vector object
         * is left in a valid but unspecified state.
         *
         * @param other The ring_vector object to move from.
         */
        ring_vector(ring_vector&& other) noexcept
            : m_ring_vector { std::move(other.m_ring_vector) }
            , m_push_index { other.m_push_index }
            , m_pop_index { other.m_pop_index }
            , m_last_index { other.m_last_index }
            , m_size { other.m_size }
            , m_capacity { other.m_capacity }
        {
        }

        /**
         * @brief Assignment operator for the ring_vector class.
         *
         * This operator assigns the contents of another ring_vector instance to this instance.
         * It performs a deep copy of the internal state of the ring_vector.
         *
         * @param other The ring_vector instance to be copied.
         * @return A reference to this ring_vector instance.
         */
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

        /**
         * @brief Move assignment operator for ring_vector.
         *
         * This operator moves the contents of another ring_vector into this one.
         * It transfers ownership of the internal data from the source object to the destination object.
         *
         * @param other The ring_vector to move from.
         * @return A reference to this ring_vector.
         */
        ring_vector& operator=(ring_vector&& other) noexcept
        {
            if (this != &other)
            {
                m_ring_vector = std::move(other.m_ring_vector);
                m_push_index = other.m_push_index;
                m_pop_index = other.m_pop_index;
                m_last_index = other.m_last_index;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
            }

            return *this;
        }

        /**
         * @brief Pushes a copy of an element into the ring vector.
         *
         * This overload keeps brace-init and exact-T calls unambiguous while
         * preserving legacy call sites.
         *
         * @param elem The element to be copied into the ring vector.
         */
        bool push(const T& elem)
        {
            return write_value(elem, overflow_policy::reject) != write_status::rejected;
        }

        /**
         * @brief Pushes an rvalue element into the ring vector.
         *
         * This overload preserves brace-init and exact-T call compatibility.
         *
         * @param elem The element to be moved into the ring vector.
         */
        bool push(T&& elem)
        {
            return write_value(std::move(elem), overflow_policy::reject) != write_status::rejected;
        }

        /**
         * @brief Pushes an element into the ring vector with perfect forwarding.
         *
         * In C++20, this method is constrained to constructible types.
         *
         * @tparam U The deduced element type.
         * @param elem The element to be pushed into the ring vector.
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
         * @brief Inserts an element in place into the ring vector at the current push index.
         *
         * In C++20, this method is constrained to constructible argument sets.
         *
         * @tparam Args The deduced constructor argument types.
         * @param args The arguments to be forwarded to T's constructor.
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
         *
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
         * This API supports move-only payload extraction.
         *
         * @return The moved front element, or none if the ring vector is empty.
         */
        std::optional<T> pop_move()
        {
            std::optional<T> item;
            if (!empty())
            {
                item.emplace(std::move(m_ring_vector[m_pop_index]));
                m_pop_index = next_index(m_pop_index);
                --m_size;
            }
            return item;
        }

        /**
         * @brief Returns the front element of the ring vector.
         *
         * This function retrieves the element at the front of the ring vector
         * without removing it.
         *
         * @return T The front element of the ring vector.
         */
        [[nodiscard]] T front() const
        {
            return m_ring_vector[m_pop_index];
        }

        /**
         * @brief Returns the last element in the ring vector.
         *
         * @return The last element of type T in the ring vector.
         */
        [[nodiscard]] T back() const
        {
            return m_ring_vector[m_last_index];
        }

        /**
         * @brief Checks if the ring buffer is empty.
         *
         * This function returns true if the ring buffer is empty, i.e.,
         * the push index is equal to the pop index.
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
         * This method determines if the ring buffer has reached its maximum capacity.
         *
         * @return true if the ring buffer is full, false otherwise.
         */
        [[nodiscard]] bool full() const
        {
            return m_capacity <= m_size;
        }

        /**
         * @brief Clears the ring vector, resetting all indices and size to zero.
         *
         * This function resets the push, pop, and last indices to zero, sets the size to zero,
         * clears the internal ring vector, and resizes it to its capacity.
         */
        void clear()
        {
            m_push_index = 0U;
            m_pop_index = 0U;
            m_last_index = 0U;
            m_size = 0U;
            m_ring_vector.clear();
            m_ring_vector.resize(m_capacity);
        }

        /**
         * @brief Returns the current size of the ring vector.
         *
         * @return std::size_t The number of elements in the ring vector.
         */
        [[nodiscard]] std::size_t size() const
        {
            return m_size;
        }

        /**
         * @brief Returns the capacity of the ring vector.
         *
         * @return std::size_t The capacity of the ring vector.
         */
        [[nodiscard]] std::size_t capacity() const
        {
            return m_capacity;
        }

        /**
         * @brief Pushes all elements from a range into the ring vector.
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
         * @brief Pushes all elements from an initializer-list into the ring vector.
         *
         * This overload enables direct brace-init calls like
         * `vector.push_range({ ... })` while preserving type constraints.
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
         * @param elem The element to be copied into the ring vector.
         */
        bool push_overwrite(const T& elem)
        {
            return write_value(elem, overflow_policy::overwrite) == write_status::overwritten;
        }

        /**
         * @brief Pushes an rvalue element, overwriting the oldest entry if full.
         *
         * @param elem The element to be moved into the ring vector.
         */
        bool push_overwrite(T&& elem)
        {
            return write_value(std::move(elem), overflow_policy::overwrite) == write_status::overwritten;
        }

        /**
         * @brief Pushes an element using perfect forwarding, overwriting the oldest entry if full.
         *
         * @tparam U The type of the element (deduced).
         * @param elem The element to be pushed into the ring vector.
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
         * @brief Accesses the element at the specified index in the ring vector.
         *
         * @param index The index of the element to access.
         * @return T& Reference to the element at the specified index.
         */
        T& operator[](std::size_t index)
        {
            return m_ring_vector[next_step_index(m_pop_index, index)];
        };

        /**
         * @brief Resizes the ring vector to the specified new capacity.
         *
         * This function resizes the ring vector to the given new capacity. If the new capacity is smaller than the
         * current size, the oldest elements will be discarded. If the new capacity is larger, the ring vector will be
         * expanded to accommodate the new capacity.
         *
         * @param new_capacity The new capacity for the ring vector.
         */
        void resize(std::size_t new_capacity)
        {
            if (m_size == new_capacity)
            {
                return;
            }

            std::vector<T> tmp;
            tmp.reserve(m_size);

            // vector filled from first to last pushed
            std::size_t idx = m_pop_index;
            for (std::size_t i = 0; i < m_size; ++i)
            {
                tmp.emplace_back(std::move(m_ring_vector[idx]));
                idx = next_index(idx);
            }

            m_ring_vector.clear();
            m_ring_vector.resize(new_capacity);

            if (m_size > new_capacity)
            {
                // shrink
                // skip first pushed elements if we resize with a lower capacity
                auto to_skip = (m_size - new_capacity);
                for (std::size_t i = to_skip; i < tmp.size(); ++i)
                {
                    m_ring_vector[i - to_skip] = std::move(tmp[i]);
                }
                m_size -= to_skip;
            }
            else
            {
                for (std::size_t i = 0; i < m_size; ++i)
                {
                    m_ring_vector[i] = std::move(tmp[i]);
                }
            }

            m_capacity = new_capacity;

            m_pop_index = 0U;

            if (m_size > 0U)
            {
                m_push_index = next_step_index(m_pop_index, m_size);
                m_last_index = next_step_index(m_pop_index, m_size - 1U);
            }
            else
            {
                m_push_index = 0U;
                m_last_index = 0U;
            }
        }

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
            const bool is_full = (m_size >= m_capacity);
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

            m_ring_vector[m_push_index] = std::forward<U>(elem);
            m_last_index = m_push_index;
            m_push_index = next_index(m_push_index);

            return is_full ? write_status::overwritten : write_status::inserted;
        }

        /**
         * @brief Calculates the next index in a circular buffer.
         *
         * This function computes the next index in a circular buffer based on the current index.
         * It wraps around to the beginning if the end of the buffer is reached.
         *
         * @param index The current index in the buffer.
         * @return The next index in the buffer.
         */
        [[nodiscard]] std::size_t next_index(std::size_t index) const
        {
            return ((index + 1U) % m_capacity);
        }

        /**
         * @brief Calculates the next index in the ring buffer given a current index and a step.
         *
         * This function computes the next index in a circular manner by adding the step to the current index
         * and taking the modulus with the capacity of the ring buffer.
         *
         * @param index The current index in the ring buffer.
         * @param step The number of steps to move forward from the current index.
         * @return The next index in the ring buffer after moving the specified number of steps.
         */
        [[nodiscard]] std::size_t next_step_index(std::size_t index, std::size_t step) const
        {
            return ((index + step) % m_capacity);
        }

        std::vector<T> m_ring_vector = {};
        std::size_t m_push_index = 0U;
        std::size_t m_pop_index = 0U;
        std::size_t m_last_index = 0U;
        std::size_t m_size = 0U;
        std::size_t m_capacity = 0U;
    };
}

#endif //  RING_VECTOR_HPP_
