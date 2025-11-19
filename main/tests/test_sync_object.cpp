/**
 * @file test_sync_object.cpp
 * @brief Unit tests for the tools::sync_object class.
 *
 * This file contains a series of unit tests for the tools::sync_object class,
 * which provides synchronization primitives for multithreaded applications.
 * The tests are implemented using the Google Test framework.
 *
 *
 * @author Laurent Lardinois and Copilot GPT-4o
 *
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

#include "tools/sync_object.hpp"

/**
 * @class SyncObjectTest
 * @brief Test fixture class for testing synchronization objects.
 *
 * This class inherits from ::testing::Test and provides setup and teardown
 * methods that are called before and after each test, respectively.
 *
 */
class SyncObjectTest : public ::testing::Test
{
protected:
    /**
     * @brief Code here will be called immediately after the constructor (right before each test).
     *
     */
    void SetUp() override
    {
    }

    /**
     * @brief Code here will be called immediately after each test (right before the destructor).
     */
    void TearDown() override
    {
    }
};

/**
 * @brief Test case to verify the initial state of the sync_object.
 *
 * This test case checks that a newly created sync_object with an initial state
 * of false is not signaled.
 */
TEST_F(SyncObjectTest, InitialState)
{
    tools::sync_object sync;
    EXPECT_FALSE(sync.is_signaled());
}

/**
 * @brief Test case for signaling a sync_object.
 *
 * This test case creates a sync_object with an initial state of false,
 * signals it, and then checks if the sync_object is in the signaled state.
 */
TEST_F(SyncObjectTest, Signal)
{
    tools::sync_object sync;
    sync.signal();
    EXPECT_TRUE(sync.is_signaled());
}

/**
 * @brief Test case for waiting for a signal in SyncObject.
 *
 * This test creates a sync_object and a separate thread that signals
 * the sync_object after a delay of 100 milliseconds. The main thread
 * waits for the signal and measures the elapsed time to ensure it is
 * at least 100 milliseconds.
 *
 * @test
 * - Create a sync_object with initial state as false.
 * - Start a thread that sleeps for 100 milliseconds and then signals the sync_object.
 * - Measure the time taken for the main thread to wait for the signal.
 * - Verify that the elapsed time is greater than or equal to 100 milliseconds.
 */
TEST_F(SyncObjectTest, WaitForSignal)
{
    tools::sync_object sync;
    std::thread signal_thread(
        [&sync]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sync.signal();
        });

    auto start = std::chrono::high_resolution_clock::now();
    sync.wait_for_signal();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_GE(elapsed.count(), 100);
    signal_thread.join();
}

/**
 * @brief Test case for waiting for a signal with a timeout.
 *
 * This test case verifies that the `wait_for_signal` method of the `tools::sync_object`
 * class correctly waits for the specified timeout duration when no signal is received.
 *
 * @details
 * - A `tools::sync_object` is created with an initial state of `false`.
 * - The current time is recorded before calling `wait_for_signal` with a timeout of 100 milliseconds.
 * - After the wait, the current time is recorded again.
 * - The elapsed time is calculated and checked to ensure it is greater than or equal to 100 milliseconds.
 *
 * @note This test assumes that the system clock is accurate and that the `wait_for_signal`
 * method behaves as expected.
 */
TEST_F(SyncObjectTest, WaitForSignalWithTimeout)
{
    tools::sync_object sync;
    auto start = std::chrono::high_resolution_clock::now();
    sync.wait_for_signal(std::chrono::milliseconds(100));
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_GE(elapsed.count(), 100);
}

/**
 * @brief Test case for multiple signals on a sync_object.
 *
 * This test case verifies that a sync_object can handle multiple signals.
 * It creates a sync_object with an initial state of false, signals it twice,
 * and then checks if the sync_object is in the signaled state.
 */
TEST_F(SyncObjectTest, MultipleSignals)
{
    tools::sync_object sync;
    sync.signal();
    sync.signal();
    EXPECT_TRUE(sync.is_signaled());
}

/**
 * @brief Test case with timeout and signal before wait
 *
 */
TEST_F(SyncObjectTest, WaitForSignalTimeoutWithSignalBeforeWait)
{
    tools::sync_object sync;
    sync.signal(); // Signal before waiting
    auto start = std::chrono::high_resolution_clock::now();
    sync.wait_for_signal(std::chrono::milliseconds(100));
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_GE(elapsed.count(), 100);
    EXPECT_FALSE(sync.is_signaled()); // wait_for_signal should reset the signal
}

/**
 * @brief Test case for SyncObjectTest to verify the behavior of wait_for_signal with a timeout when the signal is not
 * set.
 *
 * This test creates a sync_object with an initial state of not signaled. It then waits for the signal with a timeout of
 * 100 milliseconds. The test measures the elapsed time and checks that it is greater than or equal to 100 milliseconds,
 * indicating that the wait timed out. Additionally, it verifies that the sync_object is still not signaled after the
 * wait.
 *
 * @test
 * - Create a sync_object with an initial state of not signaled.
 * - Wait for the signal with a timeout of 100 milliseconds.
 * - Measure the elapsed time and check that it is greater than or equal to 100 milliseconds.
 * - Verify that the sync_object is not signaled after the wait.
 */
TEST_F(SyncObjectTest, WaitForSignalTimeoutNotSignaled)
{
    tools::sync_object sync;
    auto start = std::chrono::high_resolution_clock::now();
    sync.wait_for_signal(std::chrono::milliseconds(100));
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_GE(elapsed.count(), 100);
    EXPECT_FALSE(sync.is_signaled());
}
