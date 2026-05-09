/**
 * @file test_sync_observer.cpp
 * @brief Unit tests for the synchronization observer pattern using Google Test framework.
 *
 * This file contains the implementation of unit tests for the sync_subject and TestObserver classes.
 * The tests cover various scenarios including subscribing, unsubscribing, handling multiple observers,
 * handling multiple topics, and concurrent operations.
 *
 * @author Laurent Lardinois and Copilot GPT-4o
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

#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "tests/test_helper.hpp"
#include "tools/sync_observer.hpp"

/**
 * @class TestObserver
 * @brief A test observer class that inherits from tools::sync_observer to handle string topics and integer events.
 *
 * This class is used to observe and handle events associated with specific topics. It stores the last observed topic,
 * event, and origin.
 *
 * @tparam std::string The type of the topic.
 * @tparam int The type of the event.
 */

/**
 * @brief Informs the observer about an event.
 *
 * This method is called to notify the observer about an event associated with a specific topic and origin.
 *
 * @param topic The topic associated with the event.
 * @param event The event that occurred.
 * @param origin The origin of the event.
 */
class TestObserver : public tools::sync_observer<std::string, int>
{
public:
    void inform(const std::string& topic, const int& event, const std::string& origin) override
    {
        // TEST_COUT << "Observer informed: Topic = " << topic << ", Event = " << event << ", Origin = " << origin
        //           << std::endl;
        last_topic = topic;
        last_event = event;
        last_origin = origin;
    }

    std::string last_topic;
    int last_event;
    std::string last_origin;
};

/**
 * @class SyncObserverTest
 * @brief A test fixture class for testing the synchronization observer pattern.
 *
 * This class sets up a test environment for testing the sync_subject and TestObserver classes.
 * It inherits from ::testing::Test and overrides the SetUp and TearDown methods to initialize
 * and clean up the test environment.
 *
 * @protected
 * @fn void SetUp() override
 * @brief Sets up the test environment.
 *
 * This method is called before each test case. It initializes the subject and observer objects.
 *
 * @protected
 * @fn void TearDown() override
 * @brief Cleans up the test environment.
 *
 * This method is called after each test case. It resets the subject and observer objects.
 *
 * @protected
 * @var std::unique_ptr<tools::sync_subject<std::string, int>> subject
 * @brief A unique pointer to the sync_subject object being tested.
 *
 * @protected
 * @var std::shared_ptr<TestObserver> observer
 * @brief A shared pointer to the TestObserver object being tested.
 */
class SyncObserverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        subject = std::make_unique<tools::sync_subject<std::string, int>>("TestSubject");
        observer = std::make_shared<TestObserver>();
    }

    void TearDown() override
    {
        subject.reset();
        observer.reset();
    }

    std::unique_ptr<tools::sync_subject<std::string, int>> subject;
    std::shared_ptr<TestObserver> observer;
};

/**
 * @brief Test case for subscribing to a topic and publishing an event.
 *
 * This test case verifies that an observer can successfully subscribe to a topic
 * and receive the correct event when the subject publishes to that topic.
 *
 * @test
 * - The observer subscribes to the "TestTopic".
 * - The subject publishes an event with value 42 to the "TestTopic".
 * - The test asserts that the observer's last received topic is "TestTopic".
 * - The test asserts that the observer's last received event is 42.
 * - The test asserts that the observer's last received origin is "TestSubject".
 */
TEST_F(SyncObserverTest, SubscribeAndPublish)
{
    subject->subscribe("TestTopic", observer);

    subject->publish("TestTopic", 42);

    ASSERT_EQ(observer->last_topic, "TestTopic");
    ASSERT_EQ(observer->last_event, 42);
    ASSERT_EQ(observer->last_origin, "TestSubject");
}

/**
 * @brief Test case for unsubscribing from a topic.
 *
 * This test case verifies that an observer can successfully unsubscribe from a topic
 * and no longer receive events when the subject publishes to that topic.
 *
 * @test
 * - The observer subscribes to the "TestTopic".
 * - The observer unsubscribes from the "TestTopic".
 * - The subject publishes an event with value 100 to the "TestTopic".
 * - The test asserts that the observer's last received event is not 100.
 */
TEST_F(SyncObserverTest, Unsubscribe)
{
    subject->subscribe("TestTopic", observer);
    subject->unsubscribe("TestTopic", observer);

    subject->publish("TestTopic", 100);

    ASSERT_NE(observer->last_event, 100);
}

/**
 * @brief Test case for multiple observers subscribing to the same topic.
 *
 * This test case verifies that multiple observers can successfully subscribe to the same topic
 * and receive the correct event when the subject publishes to that topic.
 *
 * @test
 * - Two observers subscribe to the "TestTopic".
 * - The subject publishes an event with value 42 to the "TestTopic".
 * - The test asserts that both observers' last received topic is "TestTopic".
 * - The test asserts that both observers' last received event is 42.
 * - The test asserts that both observers' last received origin is "TestSubject".
 */
TEST_F(SyncObserverTest, MultipleObservers)
{
    auto observer2 = std::make_shared<TestObserver>();
    subject->subscribe("TestTopic", observer);
    subject->subscribe("TestTopic", observer2);

    subject->publish("TestTopic", 42);

    ASSERT_EQ(observer->last_topic, "TestTopic");
    ASSERT_EQ(observer->last_event, 42);
    ASSERT_EQ(observer->last_origin, "TestSubject");

    ASSERT_EQ(observer2->last_topic, "TestTopic");
    ASSERT_EQ(observer2->last_event, 42);
    ASSERT_EQ(observer2->last_origin, "TestSubject");
}

/**
 * @brief Test case for verifying the behavior of SyncObserver with multiple topics.
 *
 * This test case subscribes an observer to "Topic1" and publishes events to both "Topic1" and "Topic2".
 * It then checks if the observer correctly receives the event for "Topic1" and does not receive the event for "Topic2".
 *
 * @test
 * - Subscribe the observer to "Topic1".
 * - Publish an event with value 42 to "Topic1".
 * - Verify that the observer's last_topic is "Topic1" and last_event is 42.
 * - Publish an event with value 100 to "Topic2".
 * - Verify that the observer's last_topic is not "Topic2" and last_event is not 100.
 */
TEST_F(SyncObserverTest, MultipleTopics)
{
    subject->subscribe("Topic1", observer);

    subject->publish("Topic1", 42);
    ASSERT_EQ(observer->last_topic, "Topic1");
    ASSERT_EQ(observer->last_event, 42);

    subject->publish("Topic2", 100);
    ASSERT_NE(observer->last_topic, "Topic2");
    ASSERT_NE(observer->last_event, 100);
}

/**
 * @brief Test case for concurrent observers receiving events.
 *
 * This test case verifies that multiple observers can successfully receive events concurrently
 * when the subject publishes to the same topic from different threads.
 *
 * @test
 * - Two observers subscribe to the "TestTopic".
 * - Two threads publish events with values 42 and 84 to the "TestTopic".
 * - The test asserts that each observer's last received event is either 42 or 84.
 */
TEST_F(SyncObserverTest, ConcurrentObservers)
{
    auto observer2 = std::make_shared<TestObserver>();
    subject->subscribe("TestTopic", observer);
    subject->subscribe("TestTopic", observer2);

    std::thread t1([&]() { subject->publish("TestTopic", 42); });

    std::thread t2([&]() { subject->publish("TestTopic", 84); });

    t1.join();
    t2.join();

    ASSERT_TRUE((observer->last_event == 42 || observer->last_event == 84));
    ASSERT_TRUE((observer2->last_event == 42 || observer2->last_event == 84));
}

/**
 * @brief Test case for concurrent subscription and unsubscription.
 *
 * This test case verifies that the subject can handle concurrent subscribe and unsubscribe
 * operations without deadlocks or crashes. It creates two threads that repeatedly subscribe
 * and unsubscribe observers to the same topic.
 *
 * @test This test case creates two threads that each subscribe and unsubscribe an observer
 *       to the "TestTopic" 100 times. It ensures that no deadlocks or crashes occur during
 *       these operations.
 */
TEST_F(SyncObserverTest, ConcurrentSubscribeAndUnsubscribe)
{
    auto observer2 = std::make_shared<TestObserver>();

    std::thread t1(
        [&]()
        {
            for (int i = 0; i < 100; ++i)
            {
                subject->subscribe("TestTopic", observer);
                subject->unsubscribe("TestTopic", observer);
            }
        });

    std::thread t2(
        [&]()
        {
            for (int i = 0; i < 100; ++i)
            {
                subject->subscribe("TestTopic", observer2);
                subject->unsubscribe("TestTopic", observer2);
            }
        });

    t1.join();
    t2.join();

    // Ensure no deadlocks or crashes occurred
    SUCCEED();
}

/**
 * @brief Test case for concurrent publishing to the same topic.
 *
 * This test subscribes an observer to a topic and then concurrently publishes
 * events to that topic from two different threads. It ensures that the observer
 * receives events within the pco::expected range.
 *
 * @test
 * - Subscribes an observer to "TestTopic".
 * - Starts two threads that publish events to "TestTopic".
 * - Ensures that the last event received by the observer is within the range [0, 200).
 */
TEST_F(SyncObserverTest, ConcurrentPublish)
{
    subject->subscribe("TestTopic", observer);

    std::thread t1(
        [&]()
        {
            for (int i = 0; i < 100; ++i)
            {
                subject->publish("TestTopic", i);
            }
        });

    std::thread t2(
        [&]()
        {
            for (int i = 100; i < 200; ++i)
            {
                subject->publish("TestTopic", i);
            }
        });

    t1.join();
    t2.join();

    // Ensure the last event is within the pco::expected range
    ASSERT_TRUE(observer->last_event >= 0 && observer->last_event < 200);
}

/**
 * @brief Observer used for perfect-forwarding tests with string event payloads.
 */
class StringEventObserver : public tools::sync_observer<std::string, std::string>
{
public:
    void inform(const std::string& topic, const std::string& event, const std::string& origin) override
    {
        last_topic = topic;
        last_event = event;
        last_origin = origin;
    }

    std::string last_topic;
    std::string last_event;
    std::string last_origin;
};

/**
 * @brief Verifies sync_subject subscribe/publish support exact and conversion call paths.
 */
TEST(SyncObserverPerfectForwardingTest, SubscribeAndPublishSupportExactAndConversionPaths)
{
    tools::sync_subject<std::string, std::string> subject("ForwardingSubject");
    auto observer_ref = std::make_shared<StringEventObserver>();

    std::string topic_lvalue = "topic-lvalue";
    std::string event_lvalue = "event-lvalue";

    subject.subscribe(topic_lvalue, observer_ref);
    subject.publish(topic_lvalue, event_lvalue); // exact lvalue path

    EXPECT_EQ(observer_ref->last_topic, "topic-lvalue");
    EXPECT_EQ(observer_ref->last_event, "event-lvalue");
    EXPECT_EQ(observer_ref->last_origin, "ForwardingSubject");

    subject.publish(std::string("topic-lvalue"), std::string("event-rvalue")); // exact rvalue path
    EXPECT_EQ(observer_ref->last_event, "event-rvalue");

    subject.publish("topic-lvalue", "event-conversion"); // conversion forwarding path
    EXPECT_EQ(observer_ref->last_event, "event-conversion");
}

/**
 * @brief Verifies loosely-coupled handler subscribe supports forwarding argument paths.
 */
TEST(SyncObserverPerfectForwardingTest, HandlerSubscribeSupportsForwardingPaths)
{
    tools::sync_subject<std::string, std::string> subject("ForwardingSubject");

    int handler_call_count = 0;
    std::string captured_event;

    std::string topic_lvalue = "handler-topic";
    std::string handler_name_lvalue = "handler-name";

    subject.subscribe(topic_lvalue, handler_name_lvalue,
        [&](const std::string&, const std::string& event, const std::string&)
        {
            ++handler_call_count;
            captured_event = event;
        });

    subject.publish("handler-topic", "handler-event");

    EXPECT_EQ(handler_call_count, 1);
    EXPECT_EQ(captured_event, "handler-event");
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
template <typename SubjectT, typename TopicArg, typename EventArg>
concept has_sync_publish_call = requires(SubjectT& subject_ref, TopicArg&& topic_arg, EventArg&& event_arg) {
    subject_ref.publish(std::forward<TopicArg>(topic_arg), std::forward<EventArg>(event_arg));
};

template <typename SubjectT, typename TopicArg, typename ObserverArg>
concept has_sync_subscribe_observer_call
    = requires(SubjectT& subject_ref, TopicArg&& topic_arg, ObserverArg&& observer_arg) {
          subject_ref.subscribe(std::forward<TopicArg>(topic_arg), std::forward<ObserverArg>(observer_arg));
      };

/**
 * @brief Verifies C++20 requires constraints for sync_subject forwarding APIs.
 */
TEST(SyncObserverPerfectForwardingTest, Cpp20RequiresConstraints)
{
    using subject_t = tools::sync_subject<std::string, std::string>;
    using observer_ptr_t = std::shared_ptr<tools::sync_observer<std::string, std::string>>;

    static_assert(has_sync_publish_call<subject_t, const char*, const char*>);
    static_assert(has_sync_publish_call<subject_t, std::string&&, std::string&&>);
    static_assert(!has_sync_publish_call<subject_t, int, const char*>);

    static_assert(has_sync_subscribe_observer_call<subject_t, const char*, observer_ptr_t>);
    static_assert(!has_sync_subscribe_observer_call<subject_t, int, observer_ptr_t>);

    SUCCEED();
}
#endif
