/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Michael Egli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \author    Michael Egli
 * \copyright Michael Egli
 * \date      11-Jul-2015
 *
 * \file test_cpptime.cpp
 *
 * Tests for cpptime component.
 *
 * modified to GTest syntax and completed by Laurent Lardinois and Copilot GPT-4o in February 2025
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>

#include "cpptime/cpptime.hpp"

using namespace std::chrono;

/**
 * @class TimerTest
 * @brief A test fixture class for testing the CppTime::Timer class.
 *
 * This class inherits from ::testing::Test and provides setup and teardown
 * functionality for testing the CppTime::Timer class.
 *
 */
class TimerTest : public ::testing::Test
{
protected:
    /**
     * @brief A unique pointer to a CppTime::Timer instance used in the tests.
     */
    std::unique_ptr<CppTime::Timer> timer;

    /**
     * @brief A flag to check if timer has been triggered
     *
     */
    std::atomic_bool called{false};

    /**
     * @brief Sets up the test environment by creating a new CppTime::Timer instance.
     */
    void SetUp() override
    {
        called = false;
        timer = std::make_unique<CppTime::Timer>();
    }

    /**
     * @brief Tears down the test environment by resetting the CppTime::Timer instance.
     */
    void TearDown() override
    {
        timer.reset();
    }
};

/**
 * @brief Unit test for starting and stopping the timer.
 *
 * This test case creates a unique pointer to a CppTime::Timer object.
 * It verifies the basic functionality of starting and stopping the timer.
 */
TEST_F(TimerTest, TestStartAndStop)
{
    std::unique_ptr<CppTime::Timer> timer = std::make_unique<CppTime::Timer>();
}

/**
 * @test TimerTest.AddOneshotTimer
 * @brief Tests adding a one-shot timer.
 *
 * This test adds a one-shot timer that triggers after 30 milliseconds and sets the `called` flag to true. The test then waits
 * for 100 milliseconds and checks if the `called` flag is true.
 */

TEST_F(TimerTest, AddOneshotTimer)
{
    timer->add(std::chrono::milliseconds(30), [this](CppTime::timer_id) { called.store(true); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(called.load());
}

/**
 * @test TimerTest.AddPeriodicTimer
 * @brief Tests adding a periodic timer.
 *
 * This test adds a periodic timer that triggers every 50 milliseconds and increments a counter. The test waits for 200
 * milliseconds, removes the timer, and checks if the `called` flag is true and the counter is at least 3.
 */

TEST_F(TimerTest, AddPeriodicTimer)
{
    std::atomic<int> call_count{0};
    auto id0 = timer->add(
        std::chrono::milliseconds(50),
        [this, &call_count](CppTime::timer_id)
        {
            called.store(true);
            call_count.fetch_add(1);
        },
        std::chrono::milliseconds(50));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    timer->remove(id0);
    ASSERT_TRUE(called.load());
    ASSERT_GE(call_count.load(), 3);
}

/*
 * @test TimerTest.RemoveTimer
 * @brief Tests removing a timer.
 *
 * This test adds a one-shot timer that triggers after 60 milliseconds and sets the `called` flag to true. The test then
 * removes the timer before it triggers and waits for 120 milliseconds. It checks if the `called` flag is false.
 */

TEST_F(TimerTest, RemoveTimer)
{
    auto id0 = timer->add(std::chrono::milliseconds(60), [this](CppTime::timer_id) { called.store(true); });

    timer->remove(id0);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    ASSERT_FALSE(called.load());
}

/**
 * @test TimerTest.AddMultipleTimers
 * @brief Tests adding multiple timers.
 *
 * This test adds two one-shot timers that trigger after 100 millisecond and 200 milliseconds, respectively. Each timer sets a
 * separate boolean flag to true. The test waits for 300 milliseconds and checks if both flags are true.
 */

TEST_F(TimerTest, AddMultipleTimers)
{
    std::atomic_bool called1{false};
    std::atomic_bool called2{false};

    timer->add(std::chrono::milliseconds(100), [&called1](CppTime::timer_id) { called1.store(true); });

    timer->add(std::chrono::milliseconds(200), [&called2](CppTime::timer_id) { called2.store(true); });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ASSERT_TRUE(called1.load());
    ASSERT_TRUE(called2.load());
}


/**
 * @brief Unit test for the Timer class.
 *
 * This test fixture class is used to test the Timer class functionality.
 * It contains tests to verify the behavior of the add method with different arguments.
 */
TEST_F(TimerTest, TestTwoArgumentAdd)
{
    // Test case 1: Adding a timer with a duration of 100000 microseconds
    // and verifying the callback sets the value of i to 42.
    std::atomic_int i{0};
    timer->add(100000, [&](CppTime::timer_id) { i.store(42); });
    std::this_thread::sleep_for(milliseconds(120));
    EXPECT_EQ(i.load(), 42);

    // Test case 2: Adding a timer with a duration of 100 milliseconds
    // and verifying the callback sets the value of i to 43.
    i.store(0);
    timer->add(milliseconds(100), [&](CppTime::timer_id) { i.store(43); });
    std::this_thread::sleep_for(milliseconds(120));
    EXPECT_EQ(i.load(), 43);

    // Test case 3: Adding a timer with a specific time point
    // and verifying the callback sets the value of i to 44.
    i.store(0);
    timer->add(CppTime::clock::now() + milliseconds(100), [&](CppTime::timer_id) { i.store(44); });
    std::this_thread::sleep_for(milliseconds(120));
    EXPECT_EQ(i.load(), 44);
}

/**
 * @brief Test case for adding a timer with three arguments.
 *
 * This test case verifies the functionality of adding a timer with three arguments:
 * - The first argument is the interval in microseconds.
 * - The second argument is a callback function that increments a counter.
 * - The third argument is the number of times the timer should repeat.
 *
 * The test case performs the following steps:
 * 1. Initializes a counter to zero.
 * 2. Adds a timer with a 100000 microseconds interval, a callback function that increments the counter, and a repeat
 * count of 10000.
 * 3. Sleeps for 125 milliseconds.
 * 4. Removes the timer.
 * 5. Checks if the counter has been incremented 3 times.
 * 6. Resets the counter to zero.
 * 7. Adds a timer with a 100 milliseconds interval, a callback function that increments the counter, and a repeat count
 * of 10000 microseconds.
 * 8. Sleeps for 135 milliseconds.
 * 9. Removes the timer.
 * 10. Checks if the counter has been incremented 4 times.
 */
TEST_F(TimerTest, TestThreeArgumentAdd)
{
    std::atomic<std::size_t> count{0};

    auto id = timer->add(
        100000, [&](CppTime::timer_id) { count.fetch_add(1U); }, 10000);
    std::this_thread::sleep_for(milliseconds(125));
    timer->remove(id);
    EXPECT_EQ(count.load(), 3U);

    count.store(0U);
    id = timer->add(
        milliseconds(100), [&](CppTime::timer_id) { count.fetch_add(1U); }, microseconds(10000));
    std::this_thread::sleep_for(milliseconds(135));
    timer->remove(id);
    EXPECT_EQ(count.load(), 4U);
}

/**
 * @brief Unit test for deleting a timer within its callback.
 *
 * This test case verifies the behavior of the timer when a timer is deleted
 * within its own callback. It ensures that the timer is correctly removed
 * and that subsequent timers are assigned the expected IDs.
 *
 * @details
 * - Adds a timer with a 10ms interval that removes itself in the callback.
 * - Sleeps for 50ms to allow the timer to trigger.
 * - Verifies that the callback was called exactly once.
 * - Adds multiple timers with different intervals and verifies their IDs.
 * - Repeats the process to ensure consistency.
 *
 * @note This test case uses the Google Test framework.
 */
TEST_F(TimerTest, TestDeleteTimerInCallback)
{
    std::atomic<std::size_t> count{0U};

    timer->add(
        milliseconds(10),
        [&](CppTime::timer_id id)
        {
            count.fetch_add(1U);
            timer->remove(id);
        },
        milliseconds(10));
    std::this_thread::sleep_for(milliseconds(50));
    EXPECT_EQ(count.load(), 1U);

    auto id1 = timer->add(milliseconds(40), [](CppTime::timer_id) {});
    auto id2 = timer->add(milliseconds(10), [&](CppTime::timer_id id) { timer->remove(id); });
    std::this_thread::sleep_for(milliseconds(30));
    auto id3 = timer->add(microseconds(100), [](CppTime::timer_id) {});
    auto id4 = timer->add(microseconds(100), [](CppTime::timer_id) {});
    EXPECT_EQ(id3, id2);
    EXPECT_NE(id4, id1);
    EXPECT_NE(id4, id2);
    std::this_thread::sleep_for(milliseconds(20));

    id1 = timer->add(milliseconds(10), [&](CppTime::timer_id id) { timer->remove(id); });
    id2 = timer->add(milliseconds(40), [](CppTime::timer_id) {});
    std::this_thread::sleep_for(milliseconds(30));
    id3 = timer->add(microseconds(100), [](CppTime::timer_id) {});
    id4 = timer->add(microseconds(100), [](CppTime::timer_id) {});
    EXPECT_EQ(id3, id1);
    EXPECT_NE(id4, id1);
    EXPECT_NE(id4, id2);
    std::this_thread::sleep_for(milliseconds(20));
}

/**
 * @brief Test case for verifying two identical timeouts.
 *
 * This test case sets two identical timeouts using the CppTime library and
 * verifies that both timeouts execute correctly by checking the values of
 * the variables `i` and `j`.
 *
 * @details
 * - The first timeout sets the variable `i` to 42.
 * - The second timeout sets the variable `j` to 43.
 * - The test waits for 50 milliseconds to ensure both timeouts have a chance to execute.
 * - Finally, it checks that `i` is 42 and `j` is 43.
 *
 * @note This test uses the Google Test framework.
 */
TEST_F(TimerTest, TestTwoIdenticalTimeouts)
{
    std::atomic<int> i{0};
    std::atomic<int> j{0};
    CppTime::timestamp ts = CppTime::clock::now() + milliseconds(40);
    timer->add(ts, [&](CppTime::timer_id) { i.store(42); });
    timer->add(ts, [&](CppTime::timer_id) { j.store(43); });
    std::this_thread::sleep_for(milliseconds(50));
    EXPECT_EQ(i.load(), 42);
    EXPECT_EQ(j.load(), 43);
}

/**
 * @brief Test case for verifying timer timeouts from the past.
 *
 * This test case checks the behavior of the timer when timeouts are set in the past.
 * It verifies that the callbacks are executed immediately if the timeout is in the past.
 *
 * @test
 * - Sets two timeouts in the past and verifies that the callbacks are executed immediately.
 * - Sets two timeouts in the future, with one callback causing a delay, and verifies that the second callback is
 * executed after the delay.
 *
 * @param TimerTest The test fixture class.
 */
TEST_F(TimerTest, TestTimeoutsFromThePast)
{
    std::atomic<int> i{0};
    std::atomic<int> j{0};
    CppTime::timestamp ts1 = CppTime::clock::now() - milliseconds(10);
    CppTime::timestamp ts2 = CppTime::clock::now() - milliseconds(20);
    timer->add(ts1, [&](CppTime::timer_id) { i.store(42); });
    timer->add(ts2, [&](CppTime::timer_id) { j.store(43); });
    std::this_thread::sleep_for(microseconds(20));
    EXPECT_EQ(i.load(), 42);
    EXPECT_EQ(j.load(), 43);

    i.store(0);
    CppTime::timestamp ts3 = CppTime::clock::now() + milliseconds(10);
    CppTime::timestamp ts4 = CppTime::clock::now() + milliseconds(20);
    timer->add(ts3, [&](CppTime::timer_id) { std::this_thread::sleep_for(milliseconds(20)); });
    timer->add(ts4, [&](CppTime::timer_id) { i.store(42); });
    std::this_thread::sleep_for(milliseconds(50));
    EXPECT_EQ(i.load(), 42);
}

/**
 * @brief Test the order of multiple timeouts.
 *
 * This test adds multiple timeouts to the timer and checks if the final value of `i`
 * is set correctly after all timeouts have elapsed. The timeouts are added in
 * increasing order of duration, and the final value of `i` should reflect the
 * last timeout's effect.
 *
 * @test This test verifies that the timer correctly handles multiple timeouts
 * and executes them in the correct order.
 */
TEST_F(TimerTest, TestOrderOfMultipleTimeouts)
{
    std::atomic<int> i{0};
    timer->add(10000, [&](CppTime::timer_id) { i.store(42); });
    timer->add(20000, [&](CppTime::timer_id) { i.store(43); });
    timer->add(30000, [&](CppTime::timer_id) { i.store(44); });
    timer->add(40000, [&](CppTime::timer_id) { i.store(45); });
    std::this_thread::sleep_for(milliseconds(50));
    EXPECT_EQ(i.load(), 45);
}

/**
 * @brief Unit test for testing multiple timers using the CppTime library.
 *
 * This test case verifies the behavior of multiple timers and their interactions.
 * It checks if the timers trigger at the correct intervals and if the removal of a timer works as expected.
 *
 * @test
 * - Creates two unique pointers to CppTime::Timer objects.
 * - Adds two timers to the first timer object with intervals of 40ms and 80ms.
 * - Verifies that the first timer triggers after 40ms and sets the value of `i` to 42.
 * - Verifies that the second timer triggers after 80ms and sets the value of `i` to 43.
 * - Adds another timer to the first timer object with an interval of 40ms and removes it after 20ms.
 * - Verifies that the timer was removed before it could trigger and the value of `i` remains unchanged.
 * - Verifies that the second timer triggers after 80ms and sets the value of `i` to 43.
 */
TEST_F(TimerTest, TestWithMultipleTimers)
{
    std::atomic<int> i{0};
    std::unique_ptr<CppTime::Timer> t1 = std::make_unique<CppTime::Timer>();
    std::unique_ptr<CppTime::Timer> t2 = std::make_unique<CppTime::Timer>();

    t1->add(milliseconds(40), [&](CppTime::timer_id) { i.store(42); });
    t1->add(milliseconds(80), [&](CppTime::timer_id) { i.store(43); });
    std::this_thread::sleep_for(milliseconds(60));
    EXPECT_EQ(i.load(), 42);
    std::this_thread::sleep_for(milliseconds(40));
    EXPECT_EQ(i.load(), 43);

    i.store(0);
    auto id1 = t1->add(milliseconds(40), [&](CppTime::timer_id) { i.store(42); });
    t1->add(milliseconds(80), [&](CppTime::timer_id) { i.store(43); });
    std::this_thread::sleep_for(milliseconds(20));
    t1->remove(id1);
    std::this_thread::sleep_for(milliseconds(40));
    EXPECT_EQ(i.load(), 0);
    std::this_thread::sleep_for(milliseconds(40));
    EXPECT_EQ(i.load(), 43);
}

/**
 * @brief Test fixture for Timer tests.
 *
 * This test fixture sets up the necessary environment for testing the Timer functionality.
 */

/**
 * @brief Test the removal of a timer by its ID.
 *
 * This test verifies the behavior of the timer removal function. It checks the following scenarios:
 * - Attempting to remove a non-existent timer ID.
 * - Removing an existing timer and verifying the use count of a shared pointer.
 * - Adding a timer and ensuring the shared pointer's use count is correctly managed after the timer expires.
 *
 * @test
 * - Add a timer and attempt to remove a non-existent timer ID, expecting the removal to fail.
 * - Add a timer with a shared pointer, verify the use count, remove the timer, and check the use count again.
 * - Add a timer with a shared pointer, verify the use count, wait for the timer to expire, and check the use count
 * again.
 */
TEST_F(TimerTest, TestRemoveTimerId)
{
    auto id = timer->add(milliseconds(20), [](CppTime::timer_id) {});
    std::this_thread::sleep_for(microseconds(10));
    auto res = timer->remove(id + 1);
    EXPECT_FALSE(res);

    auto shared = std::make_shared<int>(10);
    CppTime::handler_t func = [=](CppTime::timer_id) { auto shared2 = shared; };
    id = timer->add(milliseconds(20), std::move(func));
    EXPECT_EQ(shared.use_count(), 2);
    std::this_thread::sleep_for(microseconds(10));
    res = timer->remove(id);
    EXPECT_TRUE(res);
    EXPECT_EQ(shared.use_count(), 1);

    shared = std::make_shared<int>(10);
    func = [=](CppTime::timer_id) { auto shared2 = shared; };
    timer->add(milliseconds(20), std::move(func));
    EXPECT_EQ(shared.use_count(), 2);
    std::this_thread::sleep_for(milliseconds(30));
    EXPECT_EQ(shared.use_count(), 1);
}

/**
 * @brief Test case to verify passing an argument to an action in a timer.
 *
 * This test case creates a shared pointer to a struct `PushMe` and sets its member `i` to 41.
 * It then adds a timer action that increments the value of `i` by 1 and assigns it to `res`.
 * The test verifies that the initial value of `res` is 0 and that it becomes 42 after the timer action is executed.
 */
TEST_F(TimerTest, PassAnArgumentToAnAction)
{
    struct PushMe
    {
        std::atomic<int> i { 0 };
    };
    auto push_me = std::make_shared<PushMe>();
    push_me->i.store(41);

    int res = 0;

    timer->add(milliseconds(20), [&res, push_me](CppTime::timer_id) { res = push_me->i.load() + 1; });

    EXPECT_EQ(res, 0);
    std::this_thread::sleep_for(milliseconds(30));
    EXPECT_EQ(res, 42);
}
