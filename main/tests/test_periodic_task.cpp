/**
 * @file test_periodic_task.cpp
 * @brief Unit tests for the periodic task functionality.
 * 
 * This file contains unit tests for verifying the behavior of the periodic task
 * implemented in the periodic_task_std.inl file. The tests ensure that the 
 * periodic task updates the context value correctly over time.
 * 
 * The tests use the Google Test framework and include setup and teardown methods
 * to initialize and clean up the test environment.
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
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "tests/test_helper.hpp"
#include "tools/periodic_task.hpp"

class TestContext
{
public:
    void set_value(int value)
    {
        m_value.store(value);
    }
    int get_value() const
    {
        return m_value.load();
    }
    void inc_value()
    {
        m_value.fetch_add(1);
    }

private:
    std::atomic<int> m_value { 0 };
};

void startup_routine(const std::shared_ptr<TestContext>& context, const std::string& task_name)
{
    (void)context;
    (void)task_name;
    //TEST_COUT << "Startup routine for task: " << task_name << std::endl;
    context->set_value(1);
}

void periodic_routine(const std::shared_ptr<TestContext>& context, const std::string& task_name)
{
    (void)context;
    (void)task_name;
    //TEST_COUT << "Periodic routine for task: " << task_name << std::endl;
    context->inc_value();
}

/**
 * @class PeriodicTaskTest
 * @brief Unit test class for testing the periodic task functionality.
 *
 * This class inherits from ::testing::Test and sets up the necessary context
 * and parameters for testing the periodic task. It includes setup and teardown
 * methods to initialize and clean up the test environment.
 *
 */
class PeriodicTaskTest : public ::testing::Test
{
protected:
    /**
     * @brief Sets up the test environment before each test.
     *
     * This method initializes the test context, task name, period, stack size, and
     * creates an instance of the periodic task with the specified parameters.
     *
     */
    void SetUp() override
    {
        context = std::make_shared<TestContext>();
        task_name = "TestTask";
        period = std::chrono::milliseconds(100);
        stack_size = 1024;
        task = std::make_unique<tools::periodic_task<TestContext>>(
            startup_routine, periodic_routine, context, task_name, period, stack_size);
    }

    /**
     * @brief Cleans up the test environment after each test.
     *
     * This method resets the periodic task instance to clean up resources.
     *
     */
    void TearDown() override
    {
        task.reset();
    }

    /**
     * @brief Shared pointer to the test context.
     */
    std::shared_ptr<TestContext> context;

    /**
     * @brief Name of the periodic task.
     */
    std::string task_name;

    /**
     * @brief Period of the periodic task in milliseconds.
     */
    std::chrono::milliseconds period;

    /**
     * @brief Stack size for the periodic task.
     */
    std::size_t stack_size;

    /**
     * @brief Unique pointer to the periodic task instance.
     */
    std::unique_ptr<tools::periodic_task<TestContext>> task;
};

/**
 * @brief Test case to verify that the context value increases over time.
 *
 * This test case allows some time for the periodic task to run and then checks
 * if the context value has been updated correctly. The test passes if the 
 * context value is greater than 1 after the sleep period.
 *
 * @test
 * @ingroup PeriodicTaskTest
 */
TEST_F(PeriodicTaskTest, ContextValueIncreases)
{
    // Allow some time for the task to run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Check if the context value has been updated correctly
    ASSERT_GT(context->get_value(), 1);
    TEST_COUT << "Test passed. Context value: " << context->get_value() << '\n';
}

/**
 * @brief Test to verify the context value after multiple periods.
 *
 * This test allows the periodic task to run for a longer duration (2 seconds)
 * and then checks if the context value has been updated correctly.
 *
 * @details
 * - The test sleeps for 500 milliseconds to allow the periodic task to run.
 * - It then asserts that the context value is greater than 2.
 * - If the assertion passes, it prints the context value.
 *
 * @note This test assumes that the periodic task updates the context value periodically.
 */
TEST_F(PeriodicTaskTest, ContextValueAfterMultiplePeriods)
{
    // Allow more time for the task to run
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Check if the context value has been updated correctly
    ASSERT_GT(context->get_value(), 2);
    TEST_COUT << "Test passed. Context value: " << context->get_value() << '\n';
}

/**
 * @brief Test to verify the context value after a short run of the periodic task.
 *
 * This test allows the periodic task to run for a short duration (100 milliseconds)
 * and then checks if the context value has been updated correctly.
 *
 * @test
 * - Sleep for 100 milliseconds to allow the task to run.
 * - Assert that the context value is greater than 0.
 * - Output the context value to the console.
 */
TEST_F(PeriodicTaskTest, ContextValueAfterShortRun)
{
    // Allow less time for the task to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if the context value has been updated correctly
    ASSERT_GT(context->get_value(), 0);
    TEST_COUT << "Test passed. Context value: " << context->get_value() << '\n';
}
