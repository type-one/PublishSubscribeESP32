

/**
 * @file test_generic_task.cpp
 * @brief Unit tests for the GenericTask functionality using Google Test framework.
 * 
 * This file contains unit tests for the GenericTask class, which is part of the 
 * Publish/Subscribe Pattern implementation. The tests verify various 
 * aspects of the GenericTask, including callback execution, handling of null 
 * contexts, default priority and CPU affinity, and communication between tasks.
 * 
 * @author Laurent Lardinois
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

#include "tools/generic_task.hpp"

/**
 * @class GenericTaskTest
 * @brief Unit test class for testing the GenericTask functionality.
 *
 * This class uses Google Test framework to set up and tear down test contexts
 * for each test case. It provides a shared context for testing the generic task.
 */
class GenericTaskTest : public ::testing::Test
{
protected:
    /**
     * @brief Sets up the test context before each test case.
     * This method is called before each test case to initialize the shared test contexts.
     */
    void SetUp() override
    {
        context1 = std::make_shared<TestContext>();
        context2 = std::make_shared<TestContext>();
    }

    /**
     * @brief Tears down the test context after each test case.
     * This method is called after each test case to clean up the shared test contexts.
     */
    void TearDown() override
    {
        context1.reset();
        context2.reset();
    }

    /**
     * @brief Structure representing the test context.
     * This structure contains an atomic integer value used for testing purposes.
     */
    struct TestContext
    {
        std::atomic<int> value = { 0 };
    };

    /**
     * @brief Alias for the generic task using TestContext.
     */
    using TestTask = tools::generic_task<TestContext>;

    /**
     * @brief Shared pointer to the first test context.
     */
    std::shared_ptr<TestContext> context1;

    /**
     * @brief Shared pointer to the second test context.
     */
    std::shared_ptr<TestContext> context2;
};

/**
 * @brief Test case to verify that the task executes the provided callback.
 *
 * This test case sets up a context with an initial value of 0, creates a task
 * with a callback that sets the context's value to 42, and verifies that the
 * task name is correctly set and the callback is executed by checking the
 * context's value after the task is reset.
 *
 * @test This test case verifies the following:
 * - The task name is correctly set to "TestTask".
 * - The callback is executed and sets the context's value to 42.
 */
TEST_F(GenericTaskTest, TaskExecutesCallback)
{
    context1->value.store(0);

    auto callback = [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        (void)task_name;
        ctx->value.store(42);
    };

    auto task1 = std::make_unique<TestTask>(std::move(callback), context1, "TestTask", 2048);

    ASSERT_EQ(task1->task_name(), "TestTask");

    task1.reset(); // Explicitly reset the task to join the thread

    ASSERT_EQ(context1->value.load(), 42);
}

/**
 * @brief Test case for handling a null context in a task.
 *
 * This test verifies that a task can handle a null context without causing any issues.
 * It creates a task with a null context and a no-op callback, then explicitly resets
 * the task to join the thread and checks that the context remains null.
 *
 * @test
 * - Sets the context to null.
 * - Creates a task with a null context and a no-op callback.
 * - Resets the task to join the thread.
 * - Asserts that the context is still null.
 */
TEST_F(GenericTaskTest, TaskHandlesNullContext)
{
    context1 = nullptr;

    auto callback = [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        (void)ctx;
        (void)task_name;
        // Do nothing
    };

    auto task1 = std::make_unique<TestTask>(std::move(callback), context1, "TestTaskNullContext", 2048);

    task1.reset(); // Explicitly reset the task to join the thread

    ASSERT_EQ(context1, nullptr);
}

/**
 * @brief Test case for verifying the default priority and CPU affinity of a task.
 *
 * This test case creates a task with a default priority and CPU affinity, and then
 * verifies that the task's CPU affinity is set to run on all cores and its priority
 * is set to the default priority.
 *
 * @param GenericTaskTest The test fixture class.
 */
TEST_F(GenericTaskTest, TaskWithDefaultPriorityAndAffinity)
{
    auto callback = [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        (void)ctx;
        (void)task_name;
        // Do nothing
    };

    auto task1 = std::make_unique<TestTask>(std::move(callback), context1, "TestTaskDefault", 2048);

    ASSERT_EQ(task1->cpu_affinity(), tools::base_task::run_on_all_cores);
    ASSERT_EQ(task1->priority(), tools::base_task::default_priority);
}

/**
 * @brief Test case for verifying communication between two tasks.
 * 
 * This test initializes two contexts with a value of 0 and creates two tasks.
 * Task1 sets the value of context1 to 1 after a delay, and Task2 waits for 
 * Task1 to complete before setting the value of context2 to 2.
 * 
 * @test This test checks if Task1 and Task2 communicate correctly by verifying
 *       the final values of context1 and context2.
 * 
 * @note The tasks are explicitly reset to join the threads before assertions.
 */
TEST_F(GenericTaskTest, TwoTasksCommunicate)
{
    context1->value.store(0);
    context2->value.store(0);

    auto callback1 = [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        (void)task_name;
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate work
        ctx->value.store(1);
    };

    auto callback2 = [this](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        while (context1->value.load() == 0)
        {
            (void)task_name;
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Wait for task1 to complete
        }
        ctx->value.store(2);
    };

    auto task1 = std::make_unique<TestTask>(std::move(callback1), context1, "Task1", 2048);
    auto task2 = std::make_unique<TestTask>(std::move(callback2), context2, "Task2", 2048);

    task1.reset(); // Explicitly reset the task to join the thread
    task2.reset(); // Explicitly reset the task to join the thread

    ASSERT_EQ(context1->value, 1);
    ASSERT_EQ(context2->value, 2);
}
