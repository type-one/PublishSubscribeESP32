/**
 * @file sync_observer.hpp
 * @brief Header file for the synchronous observer pattern implementation.
 *
 * This file contains the definition of the sync_observer and sync_subject classes,
 * which implement a synchronous observer pattern for event handling.
 *
 * @date January 2025
 * @author Laurent Lardinois
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

#if !defined(SYNC_OBSERVER_HPP_)
#define SYNC_OBSERVER_HPP_

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "tools/critical_section.hpp"
#include "tools/non_copyable.hpp"

namespace tools
{
    // https://juanchopanzacpp.wordpress.com/2013/02/24/simple-observer-pattern-implementation-c11/
    // http://www.codeproject.com/Articles/328365/Understanding-and-Implementing-Observer-Pattern

    /**
     * @brief A template class for synchronous observers.
     *
     * This class is used to create observers that can be notified synchronously.
     * It inherits from a non-copyable class to prevent copying.
     *
     * @tparam Topic The type of the topic.
     * @tparam Evt The type of the event.
     */
    template <typename Topic, typename Evt>
    class sync_observer : public non_copyable // NOLINT inherits from a non copyable and non movable class
    {
    public:
        /**
         * @brief Default constructor.
         */
        sync_observer() = default;

        /**
         * @brief Virtual destructor.
         */
        virtual ~sync_observer() = default;

        /**
         * @brief Inform the observer about an event.
         *
         * This method is called to notify the observer about an event.
         *
         * @param topic The topic of the event.
         * @param event The event data.
         * @param origin The origin of the event.
         */
        virtual void inform(const Topic& topic, const Evt& event, const std::string& origin) = 0;
    };

    /**
     * @brief Alias template for a subscription.
     *
     * This alias template represents a pair consisting of a Topic and a shared pointer
     * to a sync_observer for that Topic and event type Evt.
     *
     * @tparam Topic The type of the topic.
     * @tparam Evt The type of the event.
     */
    template <typename Topic, typename Evt>
    using sync_subscription = std::pair<Topic, std::shared_ptr<sync_observer<Topic, Evt>>>;

    /**
     * @brief Alias template for a loosely coupled event handler.
     *
     * This alias template defines a type for a function that handles events in a loosely coupled manner.
     * The handler function takes three parameters: a reference to a Topic, a reference to an Evt, and a string.
     *
     * @tparam Topic The type of the topic.
     * @tparam Evt The type of the event.
     */
    template <typename Topic, typename Evt>
    using loose_coupled_handler = std::function<void(const Topic&, const Evt&, const std::string&)>;

    /**
     * @brief A class that represents a synchronous subject in the publish-subscribe pattern.
     *
     * This class allows observers and handlers to subscribe to specific topics and be notified
     * when events are published to those topics.
     *
     * @tparam Topic The type of the topic.
     * @tparam Evt The type of the event.
     */
    template <typename Topic, typename Evt>
    class sync_subject : public non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        using sync_observer_shared_ptr = std::shared_ptr<sync_observer<Topic, Evt>>;
        using handler = loose_coupled_handler<Topic, Evt>;

        sync_subject() = delete;

        /**
         * @brief Constructs a sync_subject with the given name.
         *
         * @param name The name of the sync_subject.
         */
        explicit sync_subject(std::string name)
            : m_name { std::move(name) }
        {
        }

        virtual ~sync_subject() = default;

        /**
         * @brief Get the name of the observer.
         *
         * @return std::string The name of the observer.
         */
        [[nodiscard]] std::string name() const
        {
            return m_name;
        }

        /**
         * @brief Subscribes an observer to a specific topic.
         *
         * @param topic The topic to which the observer wants to subscribe.
         * @param observer The observer that wants to subscribe to the topic.
         */
        void subscribe(const Topic& topic, sync_observer_shared_ptr observer)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_subscribers.insert({ topic, observer });
        }

        /**
         * @brief Subscribes a handler to a specific topic.
         *
         * This method allows you to subscribe a handler to a given topic. The handler will be called whenever the topic
         * is published.
         *
         * @param topic The topic to subscribe to.
         * @param handler_name The name of the handler.
         * @param handler The handler function to be called when the topic is published.
         */
        void subscribe(const Topic& topic, const std::string& handler_name, handler handler)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);
            m_handlers.insert({ topic, std::make_pair(handler_name, handler) });
        }

        /**
         * @brief Unsubscribes an observer from a specific topic.
         *
         * This method removes the given observer from the list of subscribers for the specified topic.
         *
         * @param topic The topic from which the observer should be unsubscribed.
         * @param observer The observer to be unsubscribed.
         */
        void unsubscribe(const Topic& topic, sync_observer_shared_ptr observer)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);

            for (auto [it, range_end] = m_subscribers.equal_range(topic); it != range_end; ++it)
            {
                if (it->second == observer)
                {
                    m_subscribers.erase(it);
                    break;
                }
            }
        }

        /**
         * @brief Unsubscribes a handler from a specific topic.
         *
         * This function removes the handler identified by `handler_name` from the list of handlers
         * subscribed to the given `topic`. It ensures thread safety by using a mutex lock.
         *
         * @param topic The topic from which the handler should be unsubscribed.
         * @param handler_name The name of the handler to be unsubscribed.
         */
        void unsubscribe(const Topic& topic, const std::string& handler_name)
        {
            std::lock_guard<tools::critical_section> guard(m_mutex);

            for (auto [it, range_end] = m_handlers.equal_range(topic); it != range_end; ++it)
            {
                if (it->second.first == handler_name)
                {
                    m_handlers.erase(it);
                    break;
                }
            }
        }

        /**
         * @brief Publishes an event to all subscribers and handlers of the given topic.
         *
         * This method informs all registered observers and invokes all registered handlers
         * for the specified topic with the provided event.
         *
         * @param topic The topic to publish the event to.
         * @param event The event to be published.
         */
        virtual void publish(const Topic& topic, const Evt& event)
        {
            std::vector<sync_observer_shared_ptr> to_inform;
            std::vector<handler> to_invoke;

            {
                std::lock_guard<tools::critical_section> guard(m_mutex);

                for (auto [it, range_end] = m_subscribers.equal_range(topic); it != range_end; ++it)
                {
                    to_inform.push_back(it->second);
                }

                for (auto [it, range_end] = m_handlers.equal_range(topic); it != range_end; ++it)
                {
                    to_invoke.emplace_back(it->second.second);
                }
            }

            for (auto& observer : to_inform)
            {
                observer->inform(topic, event, m_name);
            }

            for (auto& handler : to_invoke)
            {
                handler(topic, event, m_name);
            }
        }

    private:
        critical_section m_mutex;
        std::multimap<Topic, sync_observer_shared_ptr> m_subscribers;
        std::multimap<Topic, std::pair<std::string, handler>> m_handlers;
        std::string m_name;
    };

}

#endif //  SYNC_OBSERVER_HPP_
