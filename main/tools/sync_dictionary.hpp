/**
 * @file sync_dictionary.hpp
 * @brief A thread-safe dictionary class for C++ Publish/Subscribe Pattern.
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

#if !defined(SYNC_DICTIONARY_HPP_)
#define SYNC_DICTIONARY_HPP_

#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <unordered_map>

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
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_dictionary[key] = value;
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
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_dictionary.erase(key);
        }

        /**
         * @brief Adds a collection of key-value pairs to the dictionary.
         *
         * This method takes a collection of key-value pairs and adds them to the
         * internal dictionary. It uses a mutex to ensure thread safety during the
         * operation.
         *
         * @param collection A map containing the key-value pairs to be added to the dictionary.
         */
        void add_collection(const std::map<K, T> collection)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            for (const auto& [key, value] : collection)
            {
                m_dictionary[key] = value;
            }
        }

        /**
         * @brief Adds an unordered collection of key-value pairs to the dictionary.
         *
         * This method takes an unordered map of key-value pairs and adds each pair to the dictionary.
         * It uses a lock guard to ensure thread safety during the operation.
         *
         * @param collection The unordered map containing key-value pairs to be added to the dictionary.
         */
        void add_collection(const std::unordered_map<K, T> collection)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            for (const auto& [key, value] : collection)
            {
                m_dictionary[key] = value;
            }
        }

        /**
         * @brief Retrieves a snapshot of the current dictionary.
         *
         * This method returns a copy of the internal dictionary, ensuring thread safety
         * by using a lock guard to protect the dictionary during the copy operation.
         *
         * @return A copy of the current dictionary.
         */
        std::map<K, T> snapshot()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
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
        std::optional<T> find(const K& key)
        {
            std::optional<T> result;
            std::lock_guard<tools::critical_section> guard(m_mutex);
            const auto& itr = m_dictionary.find(key);
            if (m_dictionary.cend() != itr)
            {
                result = itr->second;
            }
            return result;
        }

        /**
         * @brief Checks if the dictionary is empty.
         *
         * This method acquires a lock on the mutex to ensure thread safety
         * and then checks if the dictionary is empty.
         *
         * @return true if the dictionary is empty, false otherwise.
         */
        bool empty()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
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
        std::size_t size()
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
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
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_dictionary.clear();
        }

    private:
        std::map<K, T> m_dictionary;
        critical_section m_mutex;
    };
}

#endif //  SYNC_DICTIONARY_HPP_
