/**
 * @file sync_dictionary.hpp
 * @brief A thread-safe dictionary class.
 *
 * This file contains the definition of the sync_dictionary class, which provides
 * a thread-safe dictionary implementation using a mutex to protect access to the
 * internal dictionary. It supports adding, removing, and retrieving key-value pairs,
 * as well as checking the size and emptiness of the dictionary.
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

#if !defined(SYNC_DICTIONARY_HPP_)
#define SYNC_DICTIONARY_HPP_

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#endif

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"

namespace tools
{
    /**
     * @brief A thread-safe dictionary class.
     *
     * This class provides a thread-safe dictionary implementation using a mutex
     * to protect access to the internal dictionary. It supports adding, removing,
     * and retrieving key-value pairs, as well as checking the size and emptiness
     * of the dictionary.
     *
     * @tparam K The type of the keys in the dictionary.
     * @tparam T The type of the values in the dictionary.
     */
    template <typename K, typename T>
    class sync_dictionary : public non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        sync_dictionary() = default;
        ~sync_dictionary() = default;
        struct thread_safe
        {
            static constexpr bool value = true;
        };

        /**
         * @brief Adds a key-value pair to the dictionary.
         *
         * This method adds a new key-value pair to the dictionary. If the key already exists,
         * its value will be updated with the new value provided.
         *
         * @param key The key to be added or updated in the dictionary.
         * @param value The value associated with the key.
         */
        void add(const K& key, const T& value)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_dictionary.insert_or_assign(key, value);
        }

        /**
         * @brief Adds a key-value pair to the dictionary using rvalue references.
         *
         * This overload preserves exact-type brace-init and move call compatibility.
         * If the key already exists, its value is updated.
         *
         * @param key The key to be added or updated in the dictionary.
         * @param value The value associated with the key.
         */
        void add(K&& key, T&& value)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_dictionary.insert_or_assign(std::move(key), std::move(value));
        }

        /**
         * @brief Adds a key-value pair to the dictionary with perfect forwarding.
         *
         * This method supports conversions beyond the exact-type overloads.
         * In C++20, this method is constrained to constructible key/value types.
         *
         * @tparam KU The deduced key type.
         * @tparam TU The deduced value type.
         * @param key The key to be forwarded into the dictionary.
         * @param value The value to be forwarded into the dictionary.
         */
        template <typename KU, typename TU>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<K, KU> && std::is_constructible_v<T, TU>
#endif
        auto add(KU&& key, TU&& value)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<K, KU>::value && std::is_constructible<T, TU>::value,
                void>::type
#endif
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_dictionary.insert_or_assign(std::forward<KU>(key), std::forward<TU>(value));
        }

        /**
         * @brief Removes the element with the specified key from the dictionary.
         *
         * This function acquires a lock to ensure thread safety while removing
         * the element associated with the given key from the dictionary.
         *
         * @param key The key of the element to be removed.
         */
        void remove(const K& key)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_dictionary.erase(key);
        }

        /**
         * @brief Adds values from an ordered map range.
         *
         * @param collection A map containing key-value pairs to add.
         */
        void add_range(const std::map<K, T>& collection)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (const auto& [key, value] : collection)
            {
                m_dictionary.insert_or_assign(key, value);
            }
        }

        /**
         * @brief Adds values from an unordered map range.
         *
         * @param collection The unordered map containing key-value pairs to add.
         */
        void add_range(const std::unordered_map<K, T>& collection)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (const auto& [key, value] : collection)
            {
                m_dictionary.insert_or_assign(key, value);
            }
        }

        /**
         * @brief Adds key-value pairs from a generic range-like source.
         *
         * In C++20, accepts any std::ranges::input_range of pair-like entries.
         * In C++17, accepts any iterable of pair-like entries whose key/value are constructible to K/T.
         *
         * @tparam TRange The range type (deduced).
         * @param values The source range of key-value entries.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<
                std::is_constructible<
                    K,
                    decltype(std::get<0>(*std::begin(std::declval<typename std::decay<TRange>::type&>())))
                >::value
                && std::is_constructible<
                    T,
                    decltype(std::get<1>(*std::begin(std::declval<typename std::decay<TRange>::type&>())))
                >::value
            >::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange>
                  && std::is_constructible_v<K, decltype(std::get<0>(*std::begin(std::declval<TRange&>())))>
                  && std::is_constructible_v<T, decltype(std::get<1>(*std::begin(std::declval<TRange&>())))>
#endif
        void add_range(TRange&& values)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (auto&& entry : std::forward<TRange>(values))
            {
                m_dictionary.insert_or_assign(
                    K(std::get<0>(std::forward<decltype(entry)>(entry))),
                    T(std::get<1>(std::forward<decltype(entry)>(entry))));
            }
        }

        /**
         * @brief Adds key-value pairs from an initializer-list range.
         *
         * @param values The source initializer-list of key-value pairs.
         */
        void add_range(std::initializer_list<std::pair<K, T>> values)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (const auto& [key, value] : values)
            {
                m_dictionary.insert_or_assign(key, value);
            }
        }

        /**
         * @brief Backward-compatible alias for add_range(map).
         */
        void add_collection(const std::map<K, T>& collection)
        {
            add_range(collection);
        }

        /**
         * @brief Backward-compatible alias for add_range(unordered_map).
         */
        void add_collection(const std::unordered_map<K, T>& collection)
        {
            add_range(collection);
        }

        /**
         * @brief Backward-compatible alias for add_range(generic range).
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<
                std::is_constructible<
                    K,
                    decltype(std::get<0>(*std::begin(std::declval<typename std::decay<TRange>::type&>())))
                >::value
                && std::is_constructible<
                    T,
                    decltype(std::get<1>(*std::begin(std::declval<typename std::decay<TRange>::type&>())))
                >::value
            >::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange>
                  && std::is_constructible_v<K, decltype(std::get<0>(*std::begin(std::declval<TRange&>())))>
                  && std::is_constructible_v<T, decltype(std::get<1>(*std::begin(std::declval<TRange&>())))>
#endif
        void add_collection(TRange&& collection)
        {
            add_range(std::forward<TRange>(collection));
        }

        /**
         * @brief Backward-compatible alias for add_range(initializer_list).
         */
        void add_collection(std::initializer_list<std::pair<K, T>> collection)
        {
            add_range(collection);
        }

        /**
         * @brief Retrieves a snapshot of the current dictionary.
         *
         * This method returns a copy of the internal dictionary, ensuring thread safety
         * by using a lock guard to protect the dictionary during the copy operation.
         *
         * @return A copy of the current dictionary.
         */
        [[nodiscard]] std::map<K, T> snapshot() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_dictionary;
        }

        /**
         * @brief Finds the value associated with the given key in the dictionary.
         *
         * This method searches for the specified key in the dictionary and returns
         * the associated value if the key is found. If the key is not found, it
         * returns an empty std::optional.
         *
         * @param key The key to search for in the dictionary.
         * @return std::optional<T> The value associated with the key if found,
         *         otherwise an empty std::optional.
         */
        [[nodiscard]] std::optional<T> find(const K& key) const
        {
            std::optional<T> result;
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            const auto& itr = m_dictionary.find(key);
            if (m_dictionary.cend() != itr)
            {
                result = itr->second;
            }
            return result;
        }

        /**
         * @brief Checks whether a key exists in the dictionary.
         *
         * Uses std::map::contains in C++20 and find-based fallback in C++17.
         *
         * @param key The key to check.
         * @return true when the key exists, otherwise false.
         */
        [[nodiscard]] bool contains(const K& key) const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            return m_dictionary.contains(key);
#else
            return m_dictionary.find(key) != m_dictionary.cend();
#endif
        }

        /**
         * @brief Removes keys from a generic range-like collection.
         *
         * In C++20, accepts any std::ranges::input_range whose elements are constructible to K.
         * In C++17, accepts any iterable whose elements are constructible to K.
         *
         * @tparam TRange The range type (deduced).
         * @param keys The source collection of keys to remove.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<
                std::is_constructible<
                    K,
                    decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))
                >::value
            >::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange>
                  && std::is_constructible_v<K, decltype(*std::begin(std::declval<TRange&>()))>
#endif
        void remove_collection(TRange&& keys)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (auto&& key : std::forward<TRange>(keys))
            {
                m_dictionary.erase(K(std::forward<decltype(key)>(key)));
            }
        }

        /**
         * @brief Removes keys from an initializer-list.
         *
         * @param keys The source initializer-list of keys to remove.
         */
        void remove_collection(std::initializer_list<K> keys)
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            for (const auto& key : keys)
            {
                m_dictionary.erase(key);
            }
        }

        /**
         * @brief Checks if the dictionary is empty.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * and then checks if the dictionary is empty.
         *
         * @return true if the dictionary is empty, false otherwise.
         */
        [[nodiscard]] bool empty() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_dictionary.empty();
        }

        /**
         * @brief Returns the number of elements in the dictionary.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * while accessing the dictionary's size.
         *
         * @return The number of elements in the dictionary.
         */
        [[nodiscard]] std::size_t size() const
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            return m_dictionary.size();
        }

        /**
         * @brief Clears all elements from the dictionary.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * and then clears all elements from the dictionary.
         */
        void clear()
        {
            std::scoped_lock<tools::critical_section> guard(m_mutex);
            m_dictionary.clear();
        }

    private:
        std::map<K, T> m_dictionary;
        mutable critical_section m_mutex;
    };
}

#endif //  SYNC_DICTIONARY_HPP_
