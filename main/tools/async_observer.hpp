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
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "tools/sync_object.hpp"
#include "tools/sync_observer.hpp"
#include "tools/sync_queue.hpp"

namespace tools
{
    /**
     * @brief A class that provides asynchronous observation capabilities.
     *
     * This class inherits from sync_observer and allows events to be queued and processed asynchronously.
     *
     * @tparam Topic The type of the topic associated with the events.
     * @tparam Evt The type of the event data.
     */
    template <typename Topic, typename Evt>
    class async_observer : public sync_observer<Topic, Evt> // NOLINT inherits from non copyable and non movable class
    {
    public:
        async_observer() = default;
        ~async_observer() override = default;

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
        sync_queue<std::tuple<Topic, Evt, std::string>> m_evt_queue;
    };

}

#endif //  ASYNC_OBSERVER_HPP_
