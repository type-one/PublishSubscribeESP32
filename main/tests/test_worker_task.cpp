/**
 * @file test_worker_task.cpp
 * @brief Unit tests for worker tasks using the Google Test framework.
 * 
 * This file contains unit tests for the WorkerTask class, verifying the correct
 * execution of startup and work delegates, and ensuring the context's computation
 * result is set as expected.
 * 
 * The tests include:
 * - DestructorTest: Verifies the WorkerTask destructor calls the startup and work delegates.
 * - FreeFunctionWorkTest: Verifies the WorkerTask executes a free function correctly.
 * - ClassMethodWorkTest: Verifies the WorkerTask executes a class method correctly.
 * - LambdaWorkTest: Verifies the WorkerTask executes lambda functions correctly.
 * 
 * Each test sets up a shared context and state flags, and checks that the delegates
 * are called and the context's computation result is updated.
 * 
 * @note This file uses the Google Test framework.
 * 
 * @date February 2025
 * 
 * @author Laurent Lardinois and Copilot GPT-4o
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
#include <memory>
#include <string>
#include <thread>

#include "tools/worker_task.hpp"

class TestContext
{
public:
    int computation_result = 0;

    void do_something() const
    {
        // Simulate some work
    }
};

/**
 * @class WorkerTaskTest
 * @brief Unit test class for worker tasks using Google Test framework.
 *
 * This class sets up a test environment for worker tasks, providing
 * a shared context and flags to track the state of the task execution.
 */
class WorkerTaskTest : public ::testing::Test
{
protected:
    /**
     * @brief Sets up the test environment before each test.
     *
     * This method initializes the shared context and state flags.
     *
     */
    void SetUp() override
    {
        context = std::make_shared<TestContext>();
        startup_called = false;
        work_called = false;
        context->computation_result = 0;
    }

    /**
     * @brief Cleans up the test environment after each test.
     *
     * This method can be used to perform any necessary cleanup.
     *
     */
    void TearDown() override
    {
        // Cleanup if necessary
    }

    /**
     * @brief Shared context for the test.
     *
     * This shared pointer holds the context used during the test execution.
     */
    std::shared_ptr<TestContext> context;

    /**
     * @brief Flag indicating if the startup function was called.
     *
     * This atomic boolean tracks whether the startup function has been executed.
     */
    std::atomic_bool startup_called;

    /**
     * @brief Flag indicating if the work function was called.
     *
     * This atomic boolean tracks whether the work function has been executed.
     */
    std::atomic_bool work_called;
};

namespace
{
    void free_function_work(const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        (void)task_name;
        ctx->do_something();
        ctx->computation_result = 42;
    }
}

/**
 * @brief Unit test for the WorkerTask destructor.
 *
 * This test verifies that the WorkerTask correctly calls the startup and work delegates,
 * and that the context's computation result is set to the expected value.
 *
 * @test
 * - Creates a WorkerTask with a startup delegate that sets a flag and performs some computation.
 * - Assigns a work delegate to the WorkerTask that sets another flag and performs the same computation.
 * - Waits for a short duration to allow the task to execute.
 * - Checks that the startup and work delegates were called.
 * - Verifies that the context's computation result is as expected.
 */
TEST_F(WorkerTaskTest, DestructorTest)
{
    {
        tools::worker_task<TestContext> task(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                startup_called.store(true);
                ctx->do_something();
                ctx->computation_result = 42;
            },
            context, "test_task", 4096);

        task.delegate(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                work_called.store(true);
                ctx->do_something();
                ctx->computation_result = 42;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(startup_called.load());
    EXPECT_TRUE(work_called.load());
    EXPECT_EQ(context->computation_result, 42);
}

/**
 * @brief Unit test for the WorkerTask class using a free function as work.
 *
 * This test verifies that the WorkerTask correctly executes a free function
 * and updates the context as expected.
 *
 * @details
 * - A WorkerTask is created with a lambda function that sets a flag and performs
 *   some operations on the context.
 * - The task is then delegated to a free function.
 * - The test waits for a short duration to allow the task to execute.
 * - Finally, it checks that the startup flag was set and the context's computation
 *   result was updated correctly.
 *
 * @test
 * - The `startup_called` flag should be set to true.
 * - The `computation_result` in the context should be 42.
 */
TEST_F(WorkerTaskTest, FreeFunctionWorkTest)
{
    {
        tools::worker_task<TestContext> task(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                startup_called.store(true);
                ctx->do_something();
                ctx->computation_result = 42;
            },
            context, "test_task", 4096);

        task.delegate(free_function_work);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(startup_called.load());
    EXPECT_EQ(context->computation_result, 42);
}

/**
 * @class WorkerClass
 * @brief A class that performs work on a given context.
 * 
 * This class contains methods to perform tasks using a shared context.
 */

/**
 * @brief Performs work on the given context.
 * 
 * This method performs some operations on the provided context and sets a computation result.
 * 
 * @param ctx A shared pointer to the TestContext object.
 * @param task_name The name of the task that executes the work.
 */
class WorkerClass
{
public:
    void class_method_work(const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        (void)task_name;
        ctx->do_something();
        ctx->computation_result = 42;
    }
};

/**
 * @brief Test case for verifying the WorkerClass method work.
 *
 * This test case creates a WorkerClass instance and a worker_task instance.
 * It sets up a task with a lambda function that performs some operations on
 * the TestContext object, including setting a computation result.
 * The task is then delegated to the WorkerClass::class_method_work method.
 * The test verifies that the startup_called flag is set to true and the
 * computation result in the context is set to 42.
 */
TEST_F(WorkerTaskTest, ClassMethodWorkTest)
{
    WorkerClass worker;

    {
        tools::worker_task<TestContext> task(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                startup_called.store(true);
                ctx->do_something();
                ctx->computation_result = 42;
            },
            context, "test_task", 4096);

        task.delegate(
            std::bind(&WorkerClass::class_method_work, &worker, std::placeholders::_1, std::placeholders::_2));

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(startup_called.load());
    EXPECT_EQ(context->computation_result, 42);
}

/**
 * @brief Test case for WorkerTask using a lambda function.
 *
 * This test verifies that the worker task correctly calls the startup and work
 * lambda functions, and that the context's computation result is set to 42.
 *
 * @details
 * - The worker task is created with a startup lambda function that sets
 *   `startup_called` to true, calls `ctx->do_something()`, and sets
 *   `ctx->computation_result` to 42.
 * - The worker task's delegate is set to a work lambda function that sets
 *   `work_called` to true, calls `ctx->do_something()`, and sets
 *   `ctx->computation_result` to 42.
 * - The test waits for 100 milliseconds to allow the worker task to execute.
 * - The test asserts that `startup_called` and `work_called` are true, and that
 *   `context->computation_result` is 42.
 *
 * @note This test uses the Google Test framework.
 */
TEST_F(WorkerTaskTest, LambdaWorkTest)
{
    {
        tools::worker_task<TestContext> task(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                startup_called.store(true);
                ctx->do_something();
                ctx->computation_result = 42;
            },
            context, "test_task", 4096);

        task.delegate(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                work_called.store(true);
                ctx->do_something();
                ctx->computation_result = 42;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(startup_called.load());
    EXPECT_TRUE(work_called.load());
    EXPECT_EQ(context->computation_result, 42);
}
