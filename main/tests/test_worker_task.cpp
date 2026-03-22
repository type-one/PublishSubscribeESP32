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
#include <initializer_list>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <type_traits>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#endif

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

TEST_F(WorkerTaskTest, PerfectForwardingConstructorAndDelegate)
{
    tools::worker_task<TestContext>::call_back startup_lvalue
        = [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
    {
        (void)task_name;
        startup_called.store(true);
        ctx->computation_result = 1;
    };

    std::string task_name_lvalue = "worker-forwarding";
    constexpr std::size_t stack_size = 4096U;

    {
        tools::worker_task<TestContext> task(startup_lvalue, context, task_name_lvalue, stack_size);

        tools::worker_task<TestContext>::call_back delegate_lvalue
            = [&](const std::shared_ptr<TestContext>& ctx, const std::string& delegate_task_name)
        {
            (void)delegate_task_name;
            work_called.store(true);
            ctx->computation_result = 42;
        };

        task.delegate(delegate_lvalue); // exact lvalue callback path
        task.delegate(
            [](const std::shared_ptr<TestContext>& ctx, const std::string& delegate_task_name)
            {
                (void)delegate_task_name;
                ctx->computation_result = 43;
            }); // exact rvalue callback path
        task.delegate(free_function_work); // forwarding conversion path

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    EXPECT_TRUE(startup_called.load());
    EXPECT_TRUE(work_called.load());
    EXPECT_EQ(context->computation_result, 42);
}

TEST_F(WorkerTaskTest, DelegateRangeSupportsInitializerAndRange)
{
    {
        tools::worker_task<TestContext> task(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                startup_called.store(true);
                ctx->computation_result = 0;
            },
            context, "test_task", 4096);

        const std::vector<tools::worker_task<TestContext>::call_back> callbacks = {
            [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                ctx->computation_result += 1;
            },
            [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                ctx->computation_result += 2;
            }
        };

        task.delegate_range(callbacks);
        task.delegate_range({
            tools::worker_task<TestContext>::call_back(
                [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
                {
                    (void)task_name;
                    ctx->computation_result += 3;
                })
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    EXPECT_TRUE(startup_called.load());
    EXPECT_EQ(context->computation_result, 6);
}

TEST_F(WorkerTaskTest, IsrDelegateRangeSupportsInitializerAndRange)
{
    {
        tools::worker_task<TestContext> task(
            [&](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                startup_called.store(true);
                ctx->computation_result = 0;
            },
            context, "test_task", 4096);

        const std::vector<tools::worker_task<TestContext>::call_back> callbacks = {
            [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                ctx->computation_result += 4;
            },
            [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
            {
                (void)task_name;
                ctx->computation_result += 5;
            }
        };

        task.isr_delegate_range(callbacks);
        task.isr_delegate_range({
            tools::worker_task<TestContext>::call_back(
                [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
                {
                    (void)task_name;
                    ctx->computation_result += 6;
                })
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    EXPECT_TRUE(startup_called.load());
    EXPECT_EQ(context->computation_result, 15);
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
TEST(WorkerTaskCompileTimeChecks, PerfectForwardingConstraints)
{
    using worker_task_t = tools::worker_task<TestContext>;
    using callback_t = worker_task_t::call_back;

    static_assert(std::is_constructible_v<worker_task_t,
        callback_t,
        std::shared_ptr<TestContext>,
        std::string,
        std::size_t>);

    static_assert(std::is_constructible_v<worker_task_t,
        callback_t,
        std::shared_ptr<TestContext>,
        const char*,
        std::size_t,
        int,
        int>);

    static_assert(!std::is_constructible_v<worker_task_t,
        int,
        std::shared_ptr<TestContext>,
        std::string,
        std::size_t>);

    static_assert(!std::is_constructible_v<worker_task_t,
        callback_t,
        int,
        std::string,
        std::size_t>);

    static_assert(!std::is_constructible_v<worker_task_t,
        callback_t,
        std::shared_ptr<TestContext>,
        int,
        std::size_t>);

    static_assert(std::is_invocable_v<decltype(&worker_task_t::template delegate<callback_t>), worker_task_t&, callback_t>);
}

TEST(WorkerTaskCompileTimeChecks, RangeConstraints)
{
    using worker_task_t = tools::worker_task<TestContext>;
    using callback_t = worker_task_t::call_back;

    static_assert(requires(worker_task_t& task, std::vector<callback_t>& callbacks)
    {
        task.delegate_range(callbacks);
    });
    static_assert(requires(worker_task_t& task, std::vector<callback_t>& callbacks)
    {
        task.isr_delegate_range(callbacks);
    });
    static_assert(requires(worker_task_t& task)
    {
        task.delegate_range(std::initializer_list<callback_t> {});
    });
    static_assert(requires(worker_task_t& task)
    {
        task.isr_delegate_range(std::initializer_list<callback_t> {});
    });

    const auto transformed = std::views::iota(0, 2)
        | std::views::transform([](int)
          {
              return callback_t(
                  [](const std::shared_ptr<TestContext>& ctx, const std::string& task_name)
                  {
                      (void)task_name;
                      ctx->do_something();
                  });
          });

    static_assert(requires(worker_task_t& task)
    {
        task.delegate_range(transformed);
    });
    static_assert(requires(worker_task_t& task)
    {
        task.isr_delegate_range(transformed);
    });

    SUCCEED();
}
#endif
