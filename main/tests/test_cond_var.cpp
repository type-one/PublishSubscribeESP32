/**
 * @file test_cond_var.cpp
 * @brief Unit tests for tools::cond_var with tools::critical_section.
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

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "tools/cond_var.hpp"
#include "tools/critical_section.hpp"

class CondVarTest : public ::testing::Test
{
protected:
    tools::critical_section mutex_;
    tools::cond_var cv_;
};

TEST_F(CondVarTest, WaitReturnsAfterNotifyOne)
{
    bool ready = false;
    std::atomic<bool> awakened { false };

    std::thread waiter(
        [&]()
        {
            std::unique_lock<tools::critical_section> lock(mutex_);
            cv_.wait(lock, [&]() { return ready; });
            awakened.store(true, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    {
        std::scoped_lock<tools::critical_section> lock(mutex_);
        ready = true;
        cv_.notify_one();
    }

    waiter.join();
    EXPECT_TRUE(awakened.load(std::memory_order_relaxed));
}

TEST_F(CondVarTest, WaitUntilTimesOutWhenPredicateStaysFalse)
{
    bool ready = false;

    std::unique_lock<tools::critical_section> lock(mutex_);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);

    const bool completed = cv_.wait_until(lock, deadline, [&]() { return ready; });

    EXPECT_FALSE(completed);
}

TEST_F(CondVarTest, WaitUntilReturnsTrueWhenNotified)
{
    bool ready = false;
    std::atomic<bool> completed { false };

    std::thread waiter(
        [&]()
        {
            std::unique_lock<tools::critical_section> lock(mutex_);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            const bool result = cv_.wait_until(lock, deadline, [&]() { return ready; });
            completed.store(result, std::memory_order_relaxed);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    {
        std::scoped_lock<tools::critical_section> lock(mutex_);
        ready = true;
        cv_.notify_one();
    }

    waiter.join();
    EXPECT_TRUE(completed.load(std::memory_order_relaxed));
}

TEST_F(CondVarTest, NotifyAllWakesAllWaiters)
{
    constexpr int waiter_count = 3;

    bool ready = false;
    std::atomic<int> awakened_count { 0 };
    std::vector<std::thread> waiters;
    waiters.reserve(waiter_count);

    for (int i = 0; i < waiter_count; ++i)
    {
        waiters.emplace_back(
            [&]()
            {
                std::unique_lock<tools::critical_section> lock(mutex_);
                cv_.wait(lock, [&]() { return ready; });
                awakened_count.fetch_add(1, std::memory_order_relaxed);
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    {
        std::scoped_lock<tools::critical_section> lock(mutex_);
        ready = true;
        cv_.notify_all();
    }

    for (auto& thread : waiters)
    {
        thread.join();
    }

    EXPECT_EQ(awakened_count.load(std::memory_order_relaxed), waiter_count);
}
