/**
 * @file test_critical_section.cpp
 * @brief Unit tests for critical section functionality.
 * 
 * This file contains unit tests for the critical section functionality using the Google Test framework.
 * It includes tests for locking, unlocking, try_lock, std::lock_guard, ISR lock guard, std::scoped_lock, and deadlock avoidance.
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

#include <mutex>
#include <thread>
#include <vector>

#include "tools/critical_section.hpp"

/**
 * @class CriticalSectionTest
 * @brief Unit test class for testing the critical section functionality.
 *
 * This class inherits from ::testing::Test and provides setup and teardown
 * methods for testing the critical section functionality.
 *
 */
class CriticalSectionTest : public ::testing::Test
{
protected:
    /**
     * @brief Critical section object used for testing.
     *
     */
    tools::critical_section cs;

    /**
     * @brief This method is called immediately after the constructor and right before each test.
     */
    void SetUp() override
    {
    }

    /**
     * @brief This method is called immediately after each test and right before the destructor.
     * It ensures that the critical section is unlocked if it was locked during the test.
     */
    void TearDown() override
    {
        if (cs.try_lock())
        {
            cs.unlock();
        }
    }
};

/**
 * @brief Test case for locking and unlocking the critical section.
 *
 * This test locks the critical section, verifies that it cannot be locked again,
 * then unlocks it and verifies that it can be locked again.
 */
TEST_F(CriticalSectionTest, LockUnlock)
{
    cs.lock();
    EXPECT_FALSE(cs.try_lock()); // should fail to lock again
    cs.unlock();
    EXPECT_TRUE(cs.try_lock()); // should succeed to lock after unlock
    if (cs.try_lock())
    {
        cs.unlock();
    }
}

/**
 * @brief Test case for trying to lock a critical section.
 *
 * This test verifies the behavior of the try_lock method of the critical section.
 * It first attempts to lock the critical section and expects it to succeed.
 * Then, it attempts to lock the critical section again and expects it to fail,
 * as the critical section is already locked. Finally, it unlocks the critical section.
 */
TEST_F(CriticalSectionTest, TryLock)
{
    EXPECT_TRUE(cs.try_lock());
    EXPECT_FALSE(cs.try_lock()); // should fail to lock again
    cs.unlock();
}

/**
 * @brief Test case for verifying the behavior of std::lock_guard with a critical section.
 *
 * This test case checks the following:
 * - When a std::lock_guard is holding the lock on a critical section, attempting to lock the same critical section
 * should fail.
 * - After the std::lock_guard is destroyed, locking the critical section should succeed.
 */
TEST_F(CriticalSectionTest, LockGuard)
{
    {
        std::lock_guard<tools::critical_section> lock(cs);
        EXPECT_FALSE(cs.try_lock()); // should fail to lock while lock_guard is holding the lock
    }
    EXPECT_TRUE(cs.try_lock()); // should succeed to lock after lock_guard is destroyed
    if (cs.try_lock())
    {
        cs.unlock();
    }
}

/**
 * @brief Test case for verifying the behavior of ISR lock guard with a critical section.
 *
 * This test case checks the following:
 * - When a tools::isr_lock_guard is holding the lock on a critical section, attempting to lock the same critical
 * section should fail.
 * - After the tools::isr_lock_guard is destroyed, locking the critical section should succeed.
 */
TEST_F(CriticalSectionTest, ISRLockGuard)
{
    {
        tools::isr_lock_guard<tools::critical_section> lock(cs);
        EXPECT_FALSE(cs.isr_try_lock()); // should fail to lock while lock_guard is holding the lock
    }
    EXPECT_TRUE(cs.isr_try_lock()); // should succeed to lock after lock_guard is destroyed
    if (cs.isr_try_lock())
    {
        cs.isr_unlock();
    }
}

/**
 * @brief Unit test for the ScopedLock functionality in the CriticalSectionTest fixture.
 *
 * This test verifies the behavior of the std::scoped_lock with multiple critical sections.
 *
 * - It first creates two critical sections, cs1 and cs2.
 * - Then, it locks both critical sections using std::scoped_lock.
 * - While the scoped_lock is holding the locks, it checks that trying to lock the critical sections again fails.
 * - After the scoped_lock is destroyed, it checks that locking the critical sections succeeds.
 *
 * @test
 * - EXPECT_FALSE(cs.try_lock()): Ensures that cs cannot be locked while scoped_lock is active.
 * - EXPECT_FALSE(cs1.try_lock()): Ensures that cs1 cannot be locked while scoped_lock is active.
 * - EXPECT_FALSE(cs2.try_lock()): Ensures that cs2 cannot be locked while scoped_lock is active.
 * - EXPECT_TRUE(cs.try_lock()): Ensures that cs can be locked after scoped_lock is destroyed.
 * - EXPECT_TRUE(cs1.try_lock()): Ensures that cs1 can be locked after scoped_lock is destroyed.
 * - EXPECT_TRUE(cs2.try_lock()): Ensures that cs2 can be locked after scoped_lock is destroyed.
 */
TEST_F(CriticalSectionTest, ScopedLock)
{
    tools::critical_section cs1, cs2;
    {
        std::scoped_lock lock(cs, cs1, cs2);
        EXPECT_FALSE(cs.try_lock()); // should fail to lock while scoped_lock is holding the lock
        EXPECT_FALSE(cs1.try_lock());
        EXPECT_FALSE(cs2.try_lock());
    }
    EXPECT_TRUE(cs.try_lock()); // should succeed to lock after scoped_lock is destroyed
    cs.unlock();
    EXPECT_TRUE(cs1.try_lock());
    cs1.unlock();
    EXPECT_TRUE(cs2.try_lock());
    cs2.unlock();
}

/**
 * @brief Test case for avoiding deadlock in critical sections.
 *
 * This test creates two critical sections and two threads that attempt to lock them in different orders.
 * It ensures that the threads do not cause a deadlock and that the critical sections can be locked and unlocked
 * properly.
 *
 * @test This test verifies that:
 * - Two threads can lock different critical sections without causing a deadlock.
 * - The critical sections can be locked and unlocked after the threads have finished.
 */
TEST_F(CriticalSectionTest, DeadlockAvoidance)
{
    tools::critical_section cs1, cs2;
    std::thread t1(
        [&]()
        {
            std::scoped_lock lock(cs, cs1);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });

    std::thread t2(
        [&]()
        {
            std::scoped_lock lock(cs1, cs2);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(cs.try_lock());
    cs.unlock();
    EXPECT_TRUE(cs1.try_lock());
    cs1.unlock();
    EXPECT_TRUE(cs2.try_lock());
    cs2.unlock();
}
