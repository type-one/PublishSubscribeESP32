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

#include <chrono>
#include <thread>
#include <tuple>

#include "tools/async_observer.hpp"

/**
 * @class TestAsyncObserver
 * @brief Test fixture for async_observer tests.
 *
 * This class sets up the necessary environment for testing the async_observer class.
 */

class TestAsyncObserver : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup code here
    }

    void TearDown() override
    {
        // Cleanup code here
    }
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
TEST_F(TestAsyncObserver, SingleObserverSingleEvent)
{
    tools::async_observer<int, std::string> observer;
    std::thread observer_thread(
        [&observer]()
        {
            observer.wait_for_events();
            auto events = observer.pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
        });

    observer.inform(1, "event1", "origin1");
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
 * @test Verifies that the first event has the correct values (1, "event1", "origin1").
 * @test Verifies that the second event has the correct values (2, "event2", "origin2").
 * @test Verifies that the third event has the correct values (3, "event3", "origin3").
 */
TEST_F(TestAsyncObserver, SingleObserverMultipleEvents)
{
    tools::async_observer<int, std::string> observer;
    std::thread observer_thread(
        [&observer]()
        {
            observer.wait_for_events();
            auto events = observer.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 2);
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "origin2");
            ASSERT_EQ(std::get<0>(events[2]), 3);
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "origin3");
        });

    observer.inform(1, "event1", "origin1");
    observer.inform(2, "event2", "origin2");
    observer.inform(3, "event3", "origin3");
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
 * - The event should have the values: (1, "event1", "origin1").
 */
TEST_F(TestAsyncObserver, MultipleObserversSingleEvent)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events();
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events();
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
        });

    observer1.inform(1, "event1", "origin1");
    observer2.inform(1, "event1", "origin1");

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
TEST_F(TestAsyncObserver, MultipleObserversMultipleEvents)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events();
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 2);
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "origin2");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events();
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 2);
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "origin2");
        });

    observer1.inform(1, "event1", "origin1");
    observer1.inform(2, "event2", "origin2");
    observer2.inform(1, "event1", "origin1");
    observer2.inform(2, "event2", "origin2");

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
TEST_F(TestAsyncObserver, MultipleObserversConcurrentEvents)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events();
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 2);
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "origin2");
            ASSERT_EQ(std::get<0>(events[2]), 3);
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "origin3");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events();
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 2);
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "origin2");
            ASSERT_EQ(std::get<0>(events[2]), 3);
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "origin3");
        });

    std::thread publisher_thread(
        [&observer1, &observer2]()
        {
            observer1.inform(1, "event1", "origin1");
            observer1.inform(2, "event2", "origin2");
            observer1.inform(3, "event3", "origin3");
            observer2.inform(1, "event1", "origin1");
            observer2.inform(2, "event2", "origin2");
            observer2.inform(3, "event3", "origin3");
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
TEST_F(TestAsyncObserver, MultipleObserversConcurrentEventsWithTimeout)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 2);
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "origin2");
            ASSERT_EQ(std::get<0>(events[2]), 3);
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "origin3");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 2);
            ASSERT_EQ(std::get<1>(events[1]), "event2");
            ASSERT_EQ(std::get<2>(events[1]), "origin2");
            ASSERT_EQ(std::get<0>(events[2]), 3);
            ASSERT_EQ(std::get<1>(events[2]), "event3");
            ASSERT_EQ(std::get<2>(events[2]), "origin3");
        });

    std::thread publisher_thread(
        [&observer1, &observer2]()
        {
            observer1.inform(1, "event1", "origin1");
            observer1.inform(2, "event2", "origin2");
            observer1.inform(3, "event3", "origin3");
            observer2.inform(1, "event1", "origin1");
            observer2.inform(2, "event2", "origin2");
            observer2.inform(3, "event3", "origin3");
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
 * - The event received by observer1 has the values (1, "event1", "origin1").
 * - The event received by observer2 has the values (2, "event2", "origin2").
 */
TEST_F(TestAsyncObserver, DifferentSubjectsSingleEvent)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events();
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events();
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 1);
            ASSERT_EQ(std::get<0>(events[0]), 2);
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "origin2");
        });

    observer1.inform(1, "event1", "origin1");
    observer2.inform(2, "event2", "origin2");

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
 * - Informs observer1 with events (1, "event1", "origin1") and (3, "event3", "origin3").
 * - Informs observer2 with events (2, "event2", "origin2") and (4, "event4", "origin4").
 * - Verifies that observer1 receives the correct events.
 * - Verifies that observer2 receives the correct events.
 */
TEST_F(TestAsyncObserver, DifferentSubjectsMultipleEvents)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events();
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 3);
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "origin3");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events();
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 2);
            ASSERT_EQ(std::get<0>(events[0]), 2);
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "origin2");
            ASSERT_EQ(std::get<0>(events[1]), 4);
            ASSERT_EQ(std::get<1>(events[1]), "event4");
            ASSERT_EQ(std::get<2>(events[1]), "origin4");
        });

    observer1.inform(1, "event1", "origin1");
    observer1.inform(3, "event3", "origin3");
    observer2.inform(2, "event2", "origin2");
    observer2.inform(4, "event4", "origin4");

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
TEST_F(TestAsyncObserver, DifferentSubjectsConcurrentEvents)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events();
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 3);
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "origin3");
            ASSERT_EQ(std::get<0>(events[2]), 5);
            ASSERT_EQ(std::get<1>(events[2]), "event5");
            ASSERT_EQ(std::get<2>(events[2]), "origin5");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events();
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 2);
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "origin2");
            ASSERT_EQ(std::get<0>(events[1]), 4);
            ASSERT_EQ(std::get<1>(events[1]), "event4");
            ASSERT_EQ(std::get<2>(events[1]), "origin4");
            ASSERT_EQ(std::get<0>(events[2]), 6);
            ASSERT_EQ(std::get<1>(events[2]), "event6");
            ASSERT_EQ(std::get<2>(events[2]), "origin6");
        });

    std::thread publisher_thread(
        [&observer1, &observer2]()
        {
            observer1.inform(1, "event1", "origin1"); // NOLINT test
            observer1.inform(3, "event3", "origin3"); // NOLINT test
            observer1.inform(5, "event5", "origin5"); // NOLINT test
            observer2.inform(2, "event2", "origin2"); // NOLINT test
            observer2.inform(4, "event4", "origin4"); // NOLINT test
            observer2.inform(6, "event6", "origin6"); // NOLINT test
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
 *   - (1, "event1", "origin1")
 *   - (3, "event3", "origin3")
 *   - (5, "event5", "origin5")
 * - Observer2 should receive three events with the following values:
 *   - (2, "event2", "origin2")
 *   - (4, "event4", "origin4")
 *   - (6, "event6", "origin6")
 */
TEST_F(TestAsyncObserver, DifferentSubjectsConcurrentEventsWithTimeout)
{
    tools::async_observer<int, std::string> observer1;
    tools::async_observer<int, std::string> observer2;

    std::thread observer_thread1(
        [&observer1]()
        {
            observer1.wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
            auto events = observer1.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 1);
            ASSERT_EQ(std::get<1>(events[0]), "event1");
            ASSERT_EQ(std::get<2>(events[0]), "origin1");
            ASSERT_EQ(std::get<0>(events[1]), 3);
            ASSERT_EQ(std::get<1>(events[1]), "event3");
            ASSERT_EQ(std::get<2>(events[1]), "origin3");
            ASSERT_EQ(std::get<0>(events[2]), 5);
            ASSERT_EQ(std::get<1>(events[2]), "event5");
            ASSERT_EQ(std::get<2>(events[2]), "origin5");
        });

    std::thread observer_thread2(
        [&observer2]()
        {
            observer2.wait_for_events(std::chrono::milliseconds(100)); // NOLINT test
            auto events = observer2.pop_all_events();
            ASSERT_EQ(events.size(), 3);
            ASSERT_EQ(std::get<0>(events[0]), 2);
            ASSERT_EQ(std::get<1>(events[0]), "event2");
            ASSERT_EQ(std::get<2>(events[0]), "origin2");
            ASSERT_EQ(std::get<0>(events[1]), 4);
            ASSERT_EQ(std::get<1>(events[1]), "event4");
            ASSERT_EQ(std::get<2>(events[1]), "origin4");
            ASSERT_EQ(std::get<0>(events[2]), 6);
            ASSERT_EQ(std::get<1>(events[2]), "event6");
            ASSERT_EQ(std::get<2>(events[2]), "origin6");
        });

    std::thread publisher_thread(
        [&observer1, &observer2]()
        {
            observer1.inform(1, "event1", "origin1"); // NOLINT test
            observer1.inform(3, "event3", "origin3"); // NOLINT test
            observer1.inform(5, "event5", "origin5"); // NOLINT test
            observer2.inform(2, "event2", "origin2"); // NOLINT test
            observer2.inform(4, "event4", "origin4"); // NOLINT test
            observer2.inform(6, "event6", "origin6"); // NOLINT test
        });

    observer_thread1.join();
    observer_thread2.join();
    publisher_thread.join();
}
