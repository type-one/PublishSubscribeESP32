/**
 * @file async_observer.hpp
 * @brief A header file for the async_observer class providing asynchronous observation capabilities.
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

#if !defined(ASYNC_OBSERVER_HPP_)
#define ASYNC_OBSERVER_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "tools/sync_object.hpp"
#include "tools/sync_observer.hpp"

namespace tools
{

    /**
     * @brief Checks if a type T has a constructor that takes a single argument of type U.
     *
     * @tparam T The type to check.
     */
    template <typename T>
    struct has_ctor_1_args
    {
        // https://stackoverflow.com/questions/16137468/sfinae-detect-constructor-with-one-argument
        /**
         * @brief A helper struct to detect the presence of a constructor with one argument.
         *
         * This struct is used in conjunction with SFINAE to determine if a type T has a constructor
         * that can be called with a single argument of type U.
         *
         * @tparam U The type of the argument to the constructor.
         */
        struct any
        {
            template <typename U, typename SFINAE = typename std::enable_if<!std::is_same<U, T>::value, U>::type>
            operator U() const;
        };

        template <typename U>
        static std::int32_t SFINAE(decltype(U(any()))*);

        template <typename U>
        static std::int8_t SFINAE(...);

        static const bool value = sizeof(SFINAE<T>(nullptr)) == sizeof(std::int32_t);
    };

    /**
     * @brief A class that provides asynchronous observation capabilities.
     *
     * This class inherits from sync_observer and allows events to be queued and processed asynchronously.
     *
     * @tparam Topic The type of the topic associated with the events.
     * @tparam Evt The type of the event data.
     * @tparam Sync_Container The type of the synchronization container used for event queuing.
     */
    template <typename Topic, typename Evt, template <class> class Sync_Container>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        requires Sync_Container<std::tuple<Topic, Evt, std::string>>::thread_safe::value
#endif
    class async_observer : public sync_observer<Topic, Evt> // NOLINT inherits from non copyable and non movable class
    {
    public:
        static_assert(Sync_Container<std::tuple<Topic, Evt, std::string>>::thread_safe::value,
            "Sync_Container has to provide a thread-safe container");

        ~async_observer() override = default;

        /**
         * @brief Default constructor for async_observer if the underlying container is default constructible.
         *
         * This constructor initializes the async_observer with an empty event queue and a wakeable object.
         */
        template <typename T = Sync_Container<std::tuple<Topic, Evt, std::string>>,
            typename = std::enable_if_t<std::is_default_constructible_v<T>>>
        async_observer()
            : sync_observer<Topic, Evt>()
            , m_evt_queue()
        {
        }

        /**
         * @brief Construct a new async observer object with a given container capacity if the underlying container has
         * a single argument constructor.
         *
         * @param container_capacity The capacity of the event queue.
         */
        template <typename T = Sync_Container<std::tuple<Topic, Evt, std::string>>,
            typename = std::enable_if_t<has_ctor_1_args<T>::value>>
        async_observer(std::size_t container_capacity)
            : sync_observer<Topic, Evt>()
            , m_evt_queue(container_capacity)
        {
        }

        /**
         * @brief Informs the observer of a new event.
         *
         * This method pushes the event information onto the event queue and signals the wakeable object.
         *
         * @param topic The topic associated with the event.
         * @param event The event data.
         * @param origin The origin of the event.
         */
        void inform(const Topic& topic, const Evt& event, const std::string& origin) override
        {
            m_evt_queue.push({ topic, event, origin });
            m_wakeable.signal();
        }

        using event_entry = std::tuple<Topic, Evt, std::string>;

        /**
         * @brief Pops all events from the event queue.
         *
         * This function removes all events from the internal event queue and returns them as a vector.
         *
         * @return std::vector<event_entry> A vector containing all the events that were in the queue.
         */
        std::vector<event_entry> pop_all_events()
        {
            std::vector<event_entry> events;

            while (!m_evt_queue.empty())
            {
                auto tmp = m_evt_queue.front_pop();
                if (tmp.has_value())
                {
                    events.emplace_back(tmp.value());
                }
            }

            return events;
        }

        /**
         * @brief Pops the first event from the event queue.
         *
         * This method retrieves and removes the first event from the event queue if it is not empty.
         *
         * @return std::optional<event_entry> The first event in the queue, or an empty optional if the queue is empty.
         */
        std::optional<event_entry> pop_first_event()
        {
            std::optional<event_entry> entry;

            if (!m_evt_queue.empty())
            {
                auto tmp = m_evt_queue.front_pop();
                if (tmp.has_value())
                {
                    entry = tmp;
                }
            }

            return entry;
        }

        /**
         * @brief Pops the last event from the event queue.
         *
         * This function retrieves the last event from the event queue and clears the queue.
         *
         * @return std::optional<event_entry> The last event entry if the queue is not empty, otherwise std::nullopt.
         */
        std::optional<event_entry> pop_last_event()
        {
            std::optional<event_entry> entry;

            if (!m_evt_queue.empty())
            {
                auto tmp = m_evt_queue.back();

                if (tmp.has_value())
                {
                    entry = tmp;

                    while (!m_evt_queue.empty())
                    {
                        m_evt_queue.pop();
                    }
                }
            }

            return entry;
        }

        /**
         * @brief Checks if there are any events in the queue.
         *
         * @return true if the event queue is not empty, false otherwise.
         */
        bool has_events()
        {
            return !m_evt_queue.empty();
        }

        /**
         * @brief Returns the number of events in the event queue.
         *
         * @return The number of events currently in the event queue.
         */
        std::size_t number_of_events()
        {
            return m_evt_queue.size();
        }

        /**
         * @brief Waits for events by blocking until a signal is received.
         */
        void wait_for_events()
        {
            m_wakeable.wait_for_signal();
        }

        /**
         * @brief Waits for events to occur within a specified timeout duration.
         *
         * This function blocks the calling thread until an event occurs or the specified timeout duration elapses.
         *
         * @param timeout The maximum duration to wait for events, specified as a std::chrono::duration with microsecond
         * precision.
         */
        void wait_for_events(const std::chrono::duration<std::uint64_t, std::micro>& timeout)
        {
            m_wakeable.wait_for_signal(timeout);
        }

    private:
        /**
         * @brief A synchronization object used for waking up threads or tasks.
         */
        sync_object m_wakeable;

        /**
         * @brief A synchronized queue that holds tuples of Topic, Evt, and std::string.
         */
        Sync_Container<std::tuple<Topic, Evt, std::string>> m_evt_queue;
    };

}

#endif //  ASYNC_OBSERVER_HPP_
