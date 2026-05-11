/**
 * @file sync_time_list.hpp
 * @brief Thread-safe chronological timestamp/value helper based on tools::time_list.
 *
 * This file defines tools::sync_time_list, a thread-safe adapter that derives from
 * tools::time_list and protects all public operations with tools::critical_section.
 *
 * @author Laurent Lardinois (with the help of Github Copilot)
 * @date May 2026
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

#if !defined(SYNC_TIME_LIST_HPP_)
#define SYNC_TIME_LIST_HPP_

#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <concepts>
#endif

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"
#include "tools/time_list.hpp"

namespace tools
{
    /**
     * @brief Thread-safe adapter over tools::time_list.
     *
     * This class derives from @c tools::time_list and wraps all public operations
     * with @c tools::critical_section to provide synchronized access.
     *
     * @tparam TTimestamp Timestamp type accepted by tools::time_list.
     * @tparam TValue Value type associated with each timestamp.
     */
    template <typename TTimestamp, typename TValue>
    class sync_time_list : public time_list<TTimestamp, TValue>, public non_copyable // NOLINT non-copyable by design
    {
    public:
        using base_type = time_list<TTimestamp, TValue>;
        using timestamp_type = typename base_type::timestamp_type;
        using value_type = typename base_type::value_type;
        using entry_type = typename base_type::entry_type;

        struct thread_safe
        {
            static constexpr bool value = true;
        };

        sync_time_list() = default;
        ~sync_time_list() = default;

        /** @brief Pushes an entry by copy in a thread-safe manner. */
        void push(const timestamp_type& timestamp_value, const value_type& payload_value)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            base_type::push(timestamp_value, payload_value);
        }

        /** @brief Pushes an entry by move in a thread-safe manner. */
        void push(timestamp_type&& timestamp_value, value_type&& payload_value)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            base_type::push(std::move(timestamp_value), std::move(payload_value));
        }

        /** @brief Pushes an entry via perfect forwarding in a thread-safe manner. */
        template <typename TT, typename TV>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<timestamp_type, TT> && std::constructible_from<value_type, TV>
#endif
        auto push(TT&& timestamp_value, TV&& payload_value)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<timestamp_type, TT>::value
                    && std::is_constructible<value_type, TV>::value,
                void>::type
#endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            base_type::push(std::forward<TT>(timestamp_value), std::forward<TV>(payload_value));
        }

        /** @brief Emplaces a value with explicit timestamp in a thread-safe manner. */
        template <typename... TArgs>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::constructible_from<value_type, TArgs...>
#endif
        auto emplace(const timestamp_type& timestamp_value, TArgs&&... value_args)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<value_type, TArgs...>::value, void>::type
#endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            base_type::emplace(timestamp_value, std::forward<TArgs>(value_args)...);
        }

        /** @brief Returns the earliest entry in a thread-safe manner. */
        [[nodiscard]] std::optional<entry_type> top() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return base_type::top();
        }

        /** @brief Removes the earliest entry in a thread-safe manner. */
        void pop()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            base_type::pop();
        }

        /** @brief Returns and removes the earliest entry in a thread-safe manner. */
        [[nodiscard]] std::optional<entry_type> top_pop()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return base_type::top_pop();
        }

        /** @brief Returns true when empty in a thread-safe manner. */
        [[nodiscard]] bool empty() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return base_type::empty();
        }

        /** @brief Returns number of stored entries in a thread-safe manner. */
        [[nodiscard]] std::size_t size() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return base_type::size();
        }

        /** @brief Clears all entries in a thread-safe manner. */
        void clear()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            base_type::clear();
        }

        /** @brief Returns a chronological snapshot in a thread-safe manner. */
        [[nodiscard]] std::vector<entry_type> snapshot_sorted() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return base_type::snapshot_sorted();
        }

    private:
        mutable tools::critical_section m_mutex;
    };

} // namespace tools

#endif // SYNC_TIME_LIST_HPP_
