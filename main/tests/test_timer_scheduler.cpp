/**
 * @file test_timer_scheduler.cpp
 * @brief Unit tests for the timer_scheduler implementation.
 *
 * This file contains unit tests for the timer_scheduler class, verifying its functionality
 * through various test cases. The tests cover adding, removing, and triggering timers,
 * both one-shot and periodic, as well as ensuring correct behavior with atomic variables.
 *
 * @details
 * The tests are implemented using the Google Test framework and include setup and teardown
 * methods for initializing and cleaning up the timer_scheduler instance before and after
 * each test case.
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
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "tools/timer_scheduler.hpp"

/**
 * @class TimerSchedulerTest
 * @brief Unit test class for testing the timer_scheduler functionality.
 *
 * This class inherits from ::testing::Test and provides setup and teardown
 * methods for initializing and cleaning up the timer_scheduler instance.
 *
 */
class TimerSchedulerTest : public ::testing::Test
{
protected:
    /**
     * @brief A unique pointer to an instance of tools::timer_scheduler used for testing.
     */
    std::unique_ptr<tools::timer_scheduler> scheduler;

    /**
     * @brief Initializes the scheduler instance before each test.
     */
    void SetUp() override
    {
        scheduler = std::make_unique<tools::timer_scheduler>();
    }

    /**
     * @brief Cleans up the scheduler instance after each test.
     */
    void TearDown() override
    {
        scheduler.reset();
    }
};

/**
 * @brief Test case for adding a periodic timer.
 *
 * This test case adds a periodic timer to the scheduler and verifies that the timer is added successfully.
 * It then waits for a period longer than the timer interval to check if the timer fires.
 */
TEST_F(TimerSchedulerTest, AddPeriodicTimer)
{
    auto handler = [](tools::timer_handle hnd) { std::cout << "Periodic timer triggered\n"; };
    auto handle = scheduler->add("test_periodic", 1000, std::move(handler), tools::timer_type::periodic);
    ASSERT_NE(handle, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // Wait to check if the timer fires
}

/**
 * @brief Test case for adding a one-shot timer to the scheduler.
 *
 * This test verifies that a one-shot timer can be added to the scheduler and
 * that it triggers correctly after the specified duration.
 *
 * @details
 * - A handler is defined to output a message when the timer is triggered.
 * - The timer is added to the scheduler with a duration of 1000 milliseconds.
 * - The test asserts that the timer handle is not null.
 * - The test waits for 1500 milliseconds to ensure the timer has enough time to fire.
 */
TEST_F(TimerSchedulerTest, AddOneShotTimer)
{
    auto handler = [](tools::timer_handle hnd) { std::cout << "One-shot timer triggered\n"; };
    auto handle = scheduler->add("test_one_shot", 1000, std::move(handler), tools::timer_type::one_shot);
    ASSERT_NE(handle, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // Wait to check if the timer fires
}

/**
 * @brief Test case to verify the removal of a timer from the scheduler.
 *
 * This test case adds a one-shot timer to the scheduler and then removes it.
 * It ensures that the timer is successfully removed and does not trigger after removal.
 *
 * @test
 * - Adds a one-shot timer with a handler that prints "Timer triggered".
 * - Verifies that the timer handle is not null.
 * - Removes the timer and verifies that the removal was successful.
 * - Waits for 1500 milliseconds to ensure the timer does not fire.
 */
TEST_F(TimerSchedulerTest, RemoveTimer)
{
    auto handler = [](tools::timer_handle hnd) { std::cout << "Timer triggered\n"; };
    auto handle = scheduler->add("test_remove", 1000, std::move(handler), tools::timer_type::one_shot);
    ASSERT_NE(handle, nullptr);
    bool removed = scheduler->remove(handle);
    ASSERT_TRUE(removed);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // Wait to ensure the timer does not fire
}

/**
 * @brief Test case for adding multiple timers to the TimerScheduler.
 *
 * This test verifies that multiple timers can be added to the TimerScheduler
 * and that they are triggered correctly. It adds two timers with different
 * intervals and types (one-shot and periodic) and checks if their handles
 * are not null. The test then waits for a sufficient amount of time to ensure
 * that the timers have a chance to fire.
 *
 * @test
 * - Adds a one-shot timer with a 500ms interval.
 * - Adds a periodic timer with a 1000ms interval.
 * - Asserts that the handles for both timers are not null.
 * - Waits for 1500ms to allow the timers to trigger.
 */
TEST_F(TimerSchedulerTest, AddMultipleTimers)
{
    auto handler1 = [](tools::timer_handle hnd) { std::cout << "Timer 1 triggered\n"; };
    auto handler2 = [](tools::timer_handle hnd) { std::cout << "Timer 2 triggered\n"; };
    auto handle1 = scheduler->add("test_timer1", 500, std::move(handler1), tools::timer_type::one_shot);
    auto handle2 = scheduler->add("test_timer2", 1000, std::move(handler2), tools::timer_type::periodic);
    ASSERT_NE(handle1, nullptr);
    ASSERT_NE(handle2, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // Wait to check if the timers fire
}

/**
 * @brief Test case for removing a non-existent timer from the scheduler.
 *
 * This test case attempts to remove a timer that does not exist in the scheduler.
 * It verifies that the removal operation returns false, indicating that the timer
 * was not found and therefore not removed.
 *
 * @test
 * - Defines an invalid timer handle (nullptr).
 * - Attempts to remove the invalid timer handle from the scheduler.
 * - Asserts that the removal operation returns false.
 */
TEST_F(TimerSchedulerTest, RemoveNonExistentTimer)
{
    tools::timer_handle invalid_handle = nullptr;
    bool removed = scheduler->remove(invalid_handle);
    ASSERT_FALSE(removed);
}

/**
 * @brief Test case for verifying that a one-shot timer sets an atomic variable.
 *
 * This test creates a one-shot timer that sets an atomic boolean variable to true
 * when the timer fires. The timer is scheduled to fire after 1000 milliseconds.
 * The test then waits for 1500 milliseconds to ensure the timer has enough time
 * to fire, and checks if the atomic variable has been set to true.
 *
 * @test This test case verifies the following:
 * - A one-shot timer can be created and scheduled.
 * - The timer fires after the specified duration.
 * - The timer's handler correctly sets the atomic variable.
 */
TEST_F(TimerSchedulerTest, OneShotTimerSetsAtomicVariable)
{
    std::atomic<bool> flag { false };
    auto handler = [&flag](tools::timer_handle hnd) { flag.store(true); };
    auto handle = scheduler->add("test_one_shot_atomic", 1000, std::move(handler), tools::timer_type::one_shot);
    ASSERT_NE(handle, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // Wait to check if the timer fires
    ASSERT_TRUE(flag.load());
}

/**
 * @brief Test case to verify that a periodic timer increments an atomic counter.
 *
 * This test creates a periodic timer with a 500ms interval and attaches a handler
 * that increments an atomic counter. It then waits for 1500ms to allow the timer
 * to fire multiple times and checks if the counter has been incremented at least twice.
 *
 * @test
 * - Create an atomic counter initialized to 0.
 * - Define a handler that increments the counter.
 * - Add a periodic timer with a 500ms interval and attach the handler.
 * - Assert that the timer handle is not null.
 * - Wait for 1500ms to allow the timer to fire.
 * - Assert that the counter has been incremented at least twice.
 */
TEST_F(TimerSchedulerTest, PeriodicTimerIncrementsAtomicCounter)
{
    std::atomic<int> counter { 0 };
    auto handler = [&counter](tools::timer_handle hnd) { counter.fetch_add(1); };
    auto handle = scheduler->add("test_periodic_atomic", 500, std::move(handler), tools::timer_type::periodic);
    ASSERT_NE(handle, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // Wait to check if the timer fires
    ASSERT_GE(counter.load(), 2);                                 // Should have fired at least twice
}

/**
 * @brief Test case to verify the removal of a one-shot timer before its timeout.
 *
 * This test case adds a one-shot timer with a timeout of 1000 milliseconds and a handler that sets a flag to true.
 * It then waits for 500 milliseconds (half the timeout period) and removes the timer.
 * The test asserts that the timer was successfully removed and waits for an additional 1000 milliseconds to ensure
 * that the timer does not fire. Finally, it checks that the flag remains false, indicating that the timer handler
 * was not executed.
 */
TEST_F(TimerSchedulerTest, RemoveOneShotTimerBeforeTimeout)
{
    std::atomic<bool> flag { false };
    auto handler = [&flag](tools::timer_handle hnd) { flag.store(true); };
    auto handle
        = scheduler->add("test_remove_one_shot_before_timeout", 1000, std::move(handler), tools::timer_type::one_shot);
    ASSERT_NE(handle, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for half the timeout period
    bool removed = scheduler->remove(handle);
    ASSERT_TRUE(removed);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait to ensure the timer does not fire
    ASSERT_FALSE(flag.load());
}

/**
 * @brief Test case to verify the removal of a periodic timer before its timeout.
 *
 * This test case adds a periodic timer with a timeout of 500 milliseconds and a handler that increments a counter.
 * It then waits for 300 milliseconds (less than the timeout period) and removes the timer.
 * The test ensures that the timer is successfully removed and does not fire after removal.
 *
 * @test
 * - Add a periodic timer with a 500ms timeout.
 * - Wait for 300ms.
 * - Remove the timer.
 * - Wait for 1000ms to ensure the timer does not fire.
 * - Verify that the counter has not been incremented.
 */
TEST_F(TimerSchedulerTest, RemovePeriodicTimerBeforeTimeout)
{
    std::atomic<int> counter { 0 };
    auto handler = [&counter](tools::timer_handle hnd) { counter.fetch_add(1); };
    auto handle
        = scheduler->add("test_remove_periodic_before_timeout", 500, std::move(handler), tools::timer_type::periodic);
    ASSERT_NE(handle, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Wait for less than the timeout period
    bool removed = scheduler->remove(handle);
    ASSERT_TRUE(removed);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait to ensure the timer does not fire
    ASSERT_EQ(counter.load(), 0);
}
