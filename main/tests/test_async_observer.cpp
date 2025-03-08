/**
 * @file test_async_observer.cpp
 * @brief Unit tests for the async_observer class.
 *
 * This file contains a series of unit tests for the async_observer class,
 * verifying its behavior in various scenarios including single and multiple
 * observers, single and multiple events, and concurrent event handling.
 *
 * @author Laurent Lardinois and Copilot GPT-4o
 * @date February 2025
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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "tools/async_observer.hpp"

/**
 * @class AsyncObserverTest
 * @brief Test fixture for async_observer tests.
 *
 * This class sets up the necessary environment for testing the async_observer class.
 */

class AsyncObserverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup code here
        subject1 = std::make_unique<tools::sync_subject<std::string, std::string>>("TestSubject1");
        subject2 = std::make_unique<tools::sync_subject<std::string, std::string>>("TestSubject2");
    }

    void TearDown() override
    {
        // Cleanup code here
        subject1.reset();
        subject2.reset();
    }

    std::unique_ptr<tools::sync_subject<std::string, std::string>> subject1;
    std::unique_ptr<tools::sync_subject<std::string, std::string>> subject2;
};

/**
 * @brief Test case for single observer receiving a single event.
 *
 * This test case verifies that a single observer can correctly receive and process a single event.
 * It creates an async observer for events with an integer and a string, starts a thread to wait for events,
 * and then informs the observer of an event. The test checks that the observer correctly receives the event
 * and that the event data matches the expected values.
 *
 * @test This test case covers the following scenarios:
 * - An observer is created and waits for events.
 * - An event is informed to the observer.
 * - The observer correctly receives the event.
 * - The event data matches the expected values.
 */
TEST_F(AsyncObserverTest, SingleObserverSingleEvent)
{
    auto observer = std::make_shared<tools::async_observer<std::string, std::string>>();
    std::atomic_bool subscribed { false };

    std::thread observer_thread(
        [this, &observer, &subscribed]()
        {
            subject1->subscribe("topic_1", observer);
            subscribed.store(true);

            observer->wait_for_events();
            auto events = observer->pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
        });

    while (!subscribed.load())
    {
        std::this_thread::yield();
    }

    subject1->publish("topic_1", "event1");

    observer_thread.join();
}

/**
 * @brief Test case for verifying the behavior of a single observer handling multiple events.
 *
 * This test creates an instance of `tools::async_observer` and starts a thread to wait for events.
 * It then informs the observer of three events and verifies that the observer correctly receives
 * and processes all three events.
 *
 * @test Verifies that the observer receives exactly three events.
 * @test Verifies that the first event has the correct values ("topic_1", "event1", "TestSubject1").
 * @test Verifies that the second event has the correct values ("topic_2", "event2", "TestSubject1").
 * @test Verifies that the third event has the correct values ("topic_3", "event3", "TestSubject1").
 */
TEST_F(AsyncObserverTest, SingleObserverMultipleEvents)
{
    auto observer = std::make_shared<tools::async_observer<std::string, std::string>>();
    std::atomic_bool subscribed { false };

    std::thread observer_thread(
        [this, &observer, &subscribed]()
        {
            subject1->subscribe("topic_1", observer);
            subject1->subscribe("topic_2", observer);
            subject1->subscribe("topic_3", observer);
            subscribed.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer->wait_for_events();
                auto current_events = observer->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_2");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_3");
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    while (!subscribed.load())
    {
        std::this_thread::yield();
    }

    subject1->publish("topic_1", "event1");
    subject1->publish("topic_2", "event2");
    subject1->publish("topic_3", "event3");

    observer_thread.join();
}

/**
 * @brief Test case for verifying the behavior of a single observer handling multiple events belonging to 
 * a same topic.
 *
 * This test creates an instance of `tools::async_observer` and starts a thread to wait for events.
 * It then informs the observer of three events and verifies that the observer correctly receives
 * and processes all three events.
 *
 * @test Verifies that the observer receives exactly three events.
 * @test Verifies that the first event has the correct values ("topic_1", "event1", "TestSubject1").
 * @test Verifies that the second event has the correct values ("topic_1", "event2", "TestSubject1").
 * @test Verifies that the third event has the correct values ("topic_1", "event3", "TestSubject1").
 */
TEST_F(AsyncObserverTest, SingleObserverMultipleEventsSameTopic)
{
    auto observer = std::make_shared<tools::async_observer<std::string, std::string>>();
    std::atomic_bool subscribed { false };

    std::thread observer_thread(
        [this, &observer, &subscribed]()
        {
            subject1->subscribe("topic_1", observer);
            subscribed.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer->wait_for_events();
                auto current_events = observer->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_1");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_1");
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    while (!subscribed.load())
    {
        std::this_thread::yield();
    }

    subject1->publish("topic_1", "event1");
    subject1->publish("topic_1", "event2");
    subject1->publish("topic_1", "event3");

    observer_thread.join();
}

/**
 * @brief Test case for verifying multiple observers receiving a single event.
 *
 * This test case creates two async observers and two threads to wait for events.
 * Each observer is informed of a single event and the threads verify that the
 * event is received correctly by each observer.
 *
 * @details
 * - Creates two async observers: observer1 and observer2.
 * - Starts two threads to wait for events on each observer.
 * - Informs both observers of the same event.
 * - Verifies that each observer receives exactly one event with the expected values.
 *
 * @test
 * - observer1 and observer2 should each receive one event.
 * - The event should have the values: ("topic_1", "event1", "TestSubject1").
 */
TEST_F(AsyncObserverTest, MultipleObserversSingleEvent)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subscribed1.store(true);

            observer1->wait_for_events();
            auto events = observer1->pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject1->subscribe("topic_1", observer2);
            subscribed2.store(true);

            observer2->wait_for_events();
            auto events = observer2->pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
        });

    while (!subscribed1.load() || !subscribed2.load())
    {
        std::this_thread::yield();
    }

    subject1->publish("topic_1", "event1");

    observer_thread1.join();
    observer_thread2.join();
}

/**
 * @brief Test case for multiple observers receiving multiple events.
 *
 * This test creates two async observers and two threads to wait for events on each observer.
 * It then informs both observers of two events and verifies that each observer receives the correct events.
 *
 * @test
 * - Create two async observers.
 * - Create two threads to wait for events on each observer.
 * - Inform both observers of two events.
 * - Verify that each observer receives the correct events.
 *
 * @note
 * - The observers are informed of the events before the threads are joined.
 * - The test checks that each observer receives exactly two events with the correct data.
 */
TEST_F(AsyncObserverTest, MultipleObserversMultipleEvents)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject1->subscribe("topic_2", observer1);
            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 2) && (events.size() < 2); ++i)
            {
                observer1->wait_for_events();
                auto current_events = observer1->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_2");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject1->subscribe("topic_1", observer2);
            subject1->subscribe("topic_2", observer2);
            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 2) && (events.size() < 2); ++i)
            {
                observer2->wait_for_events();
                auto current_events = observer2->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_2");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
        });

    while (!subscribed1.load() || !subscribed2.load())
    {
        std::this_thread::yield();
    }

    subject1->publish("topic_1", "event1");
    subject1->publish("topic_2", "event2");

    observer_thread1.join();
    observer_thread2.join();
}

/**
 * @brief Test case for verifying multiple observers handling concurrent events.
 *
 * This test creates two async observers and a publisher thread that informs both observers
 * with a series of events. Each observer runs in its own thread and waits for events.
 * The test checks if each observer correctly receives and processes the events.
 *
 * @test This test case verifies the following:
 * - Both observers receive exactly 3 events.
 * - The events received by each observer match the expected values.
 *
 * @note The test uses threads to simulate concurrent event handling.
 */
TEST_F(AsyncObserverTest, MultipleObserversConcurrentEvents)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject1->subscribe("topic_2", observer1);
            subject1->subscribe("topic_3", observer1);
            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer1->wait_for_events();
                auto current_events = observer1->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_2");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_3");
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject1->subscribe("topic_1", observer2);
            subject1->subscribe("topic_2", observer2);
            subject1->subscribe("topic_3", observer2);
            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer2->wait_for_events();
                auto current_events = observer2->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_2");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_3");
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    std::thread publisher_thread(
        [this, &subscribed1, &subscribed2]()
        {
            while (!subscribed1.load() || !subscribed2.load())
            {
                std::this_thread::yield();
            }

            subject1->publish("topic_1", "event1");
            subject1->publish("topic_2", "event2");
            subject1->publish("topic_3", "event3");
        });

    observer_thread1.join();
    observer_thread2.join();
    publisher_thread.join();
}

/**
 * @brief Test case for multiple observers handling concurrent events with a timeout.
 *
 * This test creates two async observers and a publisher thread. The publisher thread
 * informs both observers of three events each. The observer threads wait for events
 * with a timeout of 100 milliseconds, then pop all events and verify that the events
 * received match the expected values.
 *
 * @test This test verifies that:
 * - Each observer receives exactly three events.
 * - The events received by each observer have the correct values.
 */
TEST_F(AsyncObserverTest, MultipleObserversConcurrentEventsWithTimeout)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject1->subscribe("topic_2", observer1);
            subject1->subscribe("topic_3", observer1);
            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer1->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer1->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_2");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_3");
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject1->subscribe("topic_1", observer2);
            subject1->subscribe("topic_2", observer2);
            subject1->subscribe("topic_3", observer2);
            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer2->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer2->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_2");
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_3");
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    std::thread publisher_thread(
        [this, &subscribed1, &subscribed2]()
        {
            while (!subscribed1.load() || !subscribed2.load())
            {
                std::this_thread::yield();
            }

            subject1->publish("topic_1", "event1");
            subject1->publish("topic_2", "event2");
            subject1->publish("topic_3", "event3");
        });

    observer_thread1.join();
    observer_thread2.join();
    publisher_thread.join();
}

/**
 * @brief Test case for multiple observers handling concurrent events with an expiring timeout.
 *
 * This test creates two async observers and a publisher thread. The publisher thread
 * informs both observers of three events each but too late. The observer threads wait for events
 * with a timeout of 100 milliseconds, then try to pop all events and verify that it didn't receive
 * anything because it was too early.
 *
 * @test This test verifies that:
 * - Each observer receives nothing.
 */
TEST_F(AsyncObserverTest, MultipleObserversConcurrentEventsWithTimeoutExpired)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject1->subscribe("topic_2", observer1);
            subject1->subscribe("topic_3", observer1);
            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer1->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer1->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 0); // nothing received
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject1->subscribe("topic_1", observer2);
            subject1->subscribe("topic_2", observer2);
            subject1->subscribe("topic_3", observer2);
            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer2->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer2->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 0); // nothing received
        });

    std::thread publisher_thread(
        [this, &subscribed1, &subscribed2]()
        {
            while (!subscribed1.load() || !subscribed2.load())
            {
                std::this_thread::yield();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(325)); // NOLINT test

            subject1->publish("topic_1", "event1");
            subject1->publish("topic_2", "event2");
            subject1->publish("topic_3", "event3");
        });

    observer_thread1.join();
    observer_thread2.join();
    publisher_thread.join();
}

/**
 * @brief Test case for multiple observers handling concurrent events with a timeout
 * and unsubscribe one of the topic.
 *
 * This test creates two async observers and a publisher thread. The publisher thread
 * informs both observers of three events each. The observer threads subscribe then 
 * unsubscribe one of the topic and wait for events with a timeout of 100 milliseconds, 
 * then pop all events and verify the received events correspond to the active subscriptions.
 *
 * @test This test verifies that:
 * - Each observer receives only received events for their active subscription.
 */
TEST_F(AsyncObserverTest, MultipleObserversConcurrentEventsWithTimeoutAndUnsubscribe)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject1->subscribe("topic_2", observer1);
            subject1->subscribe("topic_3", observer1);

            subject1->unsubscribe("topic_2", observer1);

            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer1->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer1->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 2); // only two
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_3");
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject1->subscribe("topic_1", observer2);
            subject1->subscribe("topic_2", observer2);
            subject1->subscribe("topic_3", observer2);

            subject1->unsubscribe("topic_1", observer2);

            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer2->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer2->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 2); // only two
            ASSERT_EQ(std::get<0>(events[0]), "topic_2");
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_3");
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
        });

    std::thread publisher_thread(
        [this, &subscribed1, &subscribed2]()
        {
            while (!subscribed1.load() || !subscribed2.load())
            {
                std::this_thread::yield();
            }

            subject1->publish("topic_1", "event1");
            subject1->publish("topic_2", "event2");
            subject1->publish("topic_3", "event3");
        });

    observer_thread1.join();
    observer_thread2.join();
    publisher_thread.join();
}

/**
 * @brief Test case for async_observer with different subjects and a single event.
 *
 * This test creates two async_observer instances and two threads to wait for events.
 * Each observer is informed with a single event and the test verifies that the events
 * are correctly received and match the expected values.
 *
 * @test This test case verifies that:
 * - Each observer receives exactly one event.
 * - The event received by observer1 has the values ("topic_1", "event1", "TestSubject1").
 * - The event received by observer2 has the values ("topic_2", "event2", "TestSubject2").
 */
TEST_F(AsyncObserverTest, DifferentSubjectsSingleEvent)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subscribed1.store(true);

            observer1->wait_for_events();
            auto events = observer1->pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject2->subscribe("topic_2", observer2);
            subscribed2.store(true);

            observer2->wait_for_events();
            auto events = observer2->pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), "topic_2");
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject2");
        });

    while (!subscribed1.load() || !subscribed2.load())
    {
        std::this_thread::yield();
    }

    subject1->publish("topic_1", "event1");
    subject2->publish("topic_2", "event2");

    observer_thread1.join();
    observer_thread2.join();
}

/**
 * @brief Test case for async_observer with different subjects and multiple events.
 *
 * This test creates two async_observer instances and two threads to wait for events
 * on each observer. It then informs each observer with two events and verifies that
 * the events are received correctly.
 *
 * @test
 * - Creates two async_observer instances (observer1 and observer2).
 * - Starts two threads to wait for events on each observer.
 * - Informs observer1 with events ("topic_1", "event1", "TestSubject1") and ("topic_3", "event3", "TestSubject2").
 * - Informs observer2 with events ("topic_2", "event2", "TestSubject2") and ("topic_4", "event4", "TestSubject1").
 * - Verifies that observer1 receives the correct events.
 * - Verifies that observer2 receives the correct events.
 */
TEST_F(AsyncObserverTest, DifferentSubjectsMultipleEvents)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject2->subscribe("topic_3", observer1);
            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 2) && (events.size() < 2); ++i)
            {
                observer1->wait_for_events();
                auto current_events = observer1->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_3");
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject2");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject2->subscribe("topic_2", observer2);
            subject1->subscribe("topic_4", observer2);
            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 2) && (events.size() < 2); ++i)
            {
                observer2->wait_for_events();
                auto current_events = observer2->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), "topic_2");
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject2");
            ASSERT_EQ(std::get<0>(events[1]), "topic_4");
            ASSERT_EQ(std::get<1>(events[1]), "event4");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
        });

    while (!subscribed1.load() || !subscribed2.load())
    {
        std::this_thread::yield();
    }

    subject1->publish("topic_1", "event1");
    subject2->publish("topic_3", "event3");
    subject2->publish("topic_2", "event2");
    subject1->publish("topic_4", "event4");

    observer_thread1.join();
    observer_thread2.join();
}

/**
 * @brief Test case for handling concurrent events from different subjects.
 *
 * This test case creates two async observers and two threads to handle events
 * concurrently. Each observer waits for events and then pops all events to
 * verify their correctness. A publisher thread informs both observers with
 * different events.
 *
 * @test This test case verifies that:
 * - Observer1 receives 3 events with the correct values and origins.
 * - Observer2 receives 3 events with the correct values and origins.
 */
TEST_F(AsyncObserverTest, DifferentSubjectsConcurrentEvents)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject2->subscribe("topic_3", observer1);
            subject1->subscribe("topic_5", observer1);
            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer1->wait_for_events();
                auto current_events = observer1->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_3");
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject2");
            ASSERT_EQ(std::get<0>(events[2]), "topic_5");
            ASSERT_EQ(std::get<1>(events[2]), "event5");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject2->subscribe("topic_2", observer2);
            subject1->subscribe("topic_4", observer2);
            subject2->subscribe("topic_6", observer2);
            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer2->wait_for_events();
                auto current_events = observer2->pop_all_events();
                events.insert(events.end(), current_events.begin(), current_events.end());
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_2");
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject2");
            ASSERT_EQ(std::get<0>(events[1]), "topic_4");
            ASSERT_EQ(std::get<1>(events[1]), "event4");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_6");
            ASSERT_EQ(std::get<1>(events[2]), "event6");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject2");
        });

    std::thread publisher_thread(
        [this, &subscribed1, &subscribed2]()
        {
            while (!subscribed1.load() || !subscribed2.load())
            {
                std::this_thread::yield();
            }

            subject1->publish("topic_1", "event1"); // NOLINT test
            subject2->publish("topic_3", "event3"); // NOLINT test
            subject1->publish("topic_5", "event5"); // NOLINT test
            subject2->publish("topic_2", "event2"); // NOLINT test
            subject1->publish("topic_4", "event4"); // NOLINT test
            subject2->publish("topic_6", "event6"); // NOLINT test
        });

    observer_thread1.join();
    observer_thread2.join();
    publisher_thread.join();
}

/**
 * @brief Test case for handling concurrent events from different subjects with a timeout.
 *
 * This test creates two async observers and a publisher thread. The publisher thread informs
 * both observers with different events. Each observer waits for events with a timeout of 100 milliseconds,
 * then pops all events and verifies that the received events match the expected values.
 *
 * @test
 * - Observer1 should receive three events with the following values:
 *   - ("topic_1", "event1", "TestSubject1")
 *   - ("topic_3", "event3", "TestSubject2")
 *   - ("topic_5", "event5", "TestSubject1")
 * - Observer2 should receive three events with the following values:
 *   - ("topic_2", "event2", "TestSubject2")
 *   - ("topic_4", "event4", "TestSubject1")
 *   - ("topic_6", "event6", "TestSubject2")
 */
TEST_F(AsyncObserverTest, DifferentSubjectsConcurrentEventsWithTimeout)
{
    auto observer1 = std::make_shared<tools::async_observer<std::string, std::string>>();
    auto observer2 = std::make_shared<tools::async_observer<std::string, std::string>>();

    std::atomic_bool subscribed1 { false };
    std::atomic_bool subscribed2 { false };

    std::thread observer_thread1(
        [this, &observer1, &subscribed1]()
        {
            subject1->subscribe("topic_1", observer1);
            subject2->subscribe("topic_3", observer1);
            subject1->subscribe("topic_5", observer1);
            subscribed1.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer1->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer1->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_1");
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[1]), "topic_3");
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject2");
            ASSERT_EQ(std::get<0>(events[2]), "topic_5");
            ASSERT_EQ(std::get<1>(events[2]), "event5");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject1");
        });

    std::thread observer_thread2(
        [this, &observer2, &subscribed2]()
        {
            subject2->subscribe("topic_2", observer2);
            subject1->subscribe("topic_4", observer2);
            subject2->subscribe("topic_6", observer2);
            subscribed2.store(true);

            std::vector<std::tuple<std::string, std::string, std::string>> events;

            for (int i = 0; (i < 3) && (events.size() < 3); ++i)
            {
                observer2->wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
                auto current_events = observer2->pop_all_events();
                if (!current_events.empty())
                {
                    events.insert(events.end(), current_events.begin(), current_events.end());
                }
            }

            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), "topic_2");
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "TestSubject2");
            ASSERT_EQ(std::get<0>(events[1]), "topic_4");
            ASSERT_EQ(std::get<1>(events[1]), "event4");
            ASSERT_EQ(std::get<2>(events[1]), "TestSubject1");
            ASSERT_EQ(std::get<0>(events[2]), "topic_6");
            ASSERT_EQ(std::get<1>(events[2]), "event6");
            ASSERT_EQ(std::get<2>(events[2]), "TestSubject2");
        });

    std::thread publisher_thread(
        [this, &subscribed1, &subscribed2]()
        {
            while (!subscribed1.load() || !subscribed2.load())
            {
                std::this_thread::yield();
            }

            subject1->publish("topic_1", "event1"); // NOLINT test
            subject2->publish("topic_3", "event3"); // NOLINT test
            subject1->publish("topic_5", "event5"); // NOLINT test
            subject2->publish("topic_2", "event2"); // NOLINT test
            subject1->publish("topic_4", "event4"); // NOLINT test
            subject2->publish("topic_6", "event6"); // NOLINT test
        });

    observer_thread1.join();
    observer_thread2.join();
    publisher_thread.join();
}
