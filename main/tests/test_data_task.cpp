/**
 * @file test_data_task.cpp
 * @brief Unit tests for data_task functionality using Google Test framework.
 *
 * This file contains unit tests for the data_task class, including tests for stopping the task,
 * verifying data processing, dual task communication, and ping-pong interaction between tasks.
 *
 * The tests use a mock context and various routines to simulate task processing and verify the
 * correct behavior of the data_task class.
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
#include <utility>

#include "tools/data_task.hpp"

class MockContext
{
};

namespace
{
    void startup_routine(const std::shared_ptr<MockContext>& context, const std::string& task_name)
    {
        // Mock startup routine
        (void)context;
        (void)task_name;
    }

    void process_routine(const std::shared_ptr<MockContext>& context, const int& data, const std::string& task_name)
    {
        // Mock process routine
        (void)context;
        (void)task_name;
        (void)data;
    }
}

/**
 * @class DataTaskTest
 * @brief Unit test class for testing the data_task functionality.
 *
 * This class uses Google Test framework to set up and tear down the test environment
 * for the data_task class. It creates a mock context and a data_task instance for testing.
 */

class DataTaskTest : public ::testing::Test
{
protected:
    /**
     * @brief Sets up the test environment before each test.
     *
     * This function is called before each test case is executed. It initializes the mock context
     * and the data_task instance with the specified parameters.
     *
     */
    void SetUp() override
    {
        context = std::make_shared<MockContext>();
        task = std::make_unique<tools::data_task<MockContext, int>>(
            startup_routine, process_routine, context, 10, "TestTask", 2048);

        // Allow some time for the task to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    /**
     * @brief Tears down the test environment after each test.
     *
     * This function is called after each test case is executed. It resets the data_task instance.
     *
     */
    void TearDown() override
    {
        task.reset();
    }

    /**
     * @brief Shared pointer to the mock context used in the tests.
     */
    std::shared_ptr<MockContext> context;

    /**
     * @brief Unique pointer to the data_task instance used in the tests.
     */
    std::unique_ptr<tools::data_task<MockContext, int>> task;
};

/**
 * @brief Test case for stopping the task.
 *
 * This test submits two tasks with values 42 and 43, allows some time for the task to process,
 * and then explicitly calls the destructor to stop the task. It verifies that the task has stopped
 * by checking if the task pointer is null.
 */
TEST_F(DataTaskTest, StopTaskTest)
{
    task->submit(42);
    task->submit(43);

    // Allow some time for the task to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    task.reset(); // Explicitly call destructor to stop the task

    // Check if the task has stopped
    EXPECT_TRUE(task == nullptr);
}

/**
 * @brief Unit test for the DataTask class to verify data processing.
 *
 * This test case checks if the data processing routine is executed correctly
 * when a task is submitted. It sets up a mock context and a process routine
 * that sets a flag to true when executed. The test then submits a task and
 * waits for a short duration to allow the task to be processed. Finally, it
 * verifies that the process routine was executed by checking the flag.
 *
 * @test ProcessDataTest
 * @ingroup DataTaskTests
 */
TEST_F(DataTaskTest, ProcessDataTest)
{
    bool processed = false;
    auto process_routine
        = [&processed](const std::shared_ptr<MockContext>& context, const int& data, const std::string& task_name)
    {
        (void)context;
        (void)data;
        (void)task_name;
        processed = true;
    };

    task = std::make_unique<tools::data_task<MockContext, int>>(
        startup_routine, process_routine, context, 10, "TestTask", 2048);

    task->submit(42);

    // Allow some time for the task to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(processed);
}

/**
 * @class DualDataTaskTest
 * @brief Unit test class for testing dual data tasks using Google Test framework.
 *
 * This class sets up two data tasks and provides routines to process data for each task.
 * It uses a mock context and atomic flags to track the processing status of each task.
 */
class DataTaskDualTest : public ::testing::Test
{
protected:
    /**
     * @brief Set up the test environment.
     *
     * This method is called before each test case. It initializes the mock context and
     * creates two data tasks with their respective processing routines.
     */
    void SetUp() override
    {
        context = std::make_shared<MockContext>();

        auto process1 = std::bind(&DataTaskDualTest::process_routine1, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3);

        auto process2 = std::bind(&DataTaskDualTest::process_routine2, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3);

        task1 = std::make_unique<tools::data_task<MockContext, int>>(
            std::move(startup_routine), std::move(process1), context, 10, "Task1", 2048);

        task2 = std::make_unique<tools::data_task<MockContext, int>>(
            std::move(startup_routine), std::move(process2), context, 10, "Task2", 2048);

        // Allow some time for the task to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    /**
     * @brief Tear down the test environment.
     *
     * This method is called after each test case. It resets the data tasks.
     */
    void TearDown() override
    {
        task1.reset();
        task2.reset();
    }

    std::shared_ptr<MockContext> context;                      ///< Shared pointer to the mock context.
    std::unique_ptr<tools::data_task<MockContext, int>> task1; ///< Unique pointer to the first data task.
    std::unique_ptr<tools::data_task<MockContext, int>> task2; ///< Unique pointer to the second data task.

    std::atomic<bool> task1_processed = false; ///< Atomic flag indicating if the first task has processed data.
    std::atomic<bool> task2_processed = false; ///< Atomic flag indicating if the second task has processed data.

    /**
     * @brief Processing routine for the first data task.
     *
     * This method is called to process data for the first task. It sets the task1_processed flag
     * to true and submits incremented data to the second task.
     *
     * @param context Shared pointer to the mock context.
     * @param data The data to be processed.
     * @param task_name The name of the task.
     */
    void process_routine1(const std::shared_ptr<MockContext>& context, const int& data, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
        task1_processed = true;
        task2->submit(data + 1);
    }

    /**
     * @brief Processing routine for the second data task.
     *
     * This method is called to process data for the second task. It sets the task2_processed flag to true.
     *
     * @param context Shared pointer to the mock context.
     * @param data The data to be processed.
     * @param task_name The name of the task.
     */
    void process_routine2(const std::shared_ptr<MockContext>& context, const int& data, const std::string& task_name)
    {
        (void)context;
        (void)data;
        (void)task_name;
        task2_processed = true;
    }
};

/**
 * @brief Test case for dual task communication.
 *
 * This test submits a value to task1 and allows some time for the tasks to process.
 * It then checks if both task1 and task2 have processed the value.
 *
 * @test
 * - Submits a value to task1.
 * - Waits for 200 milliseconds to allow processing.
 * - Checks if task1 and task2 have processed the value.
 *
 * @note This test uses the Google Test framework.
 */
TEST_F(DataTaskDualTest, DualTaskCommunicationTest)
{
    task1->submit(42);

    // Allow some time for the tasks to process
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_TRUE(task1_processed);
    EXPECT_TRUE(task2_processed);
}

/**
 * @brief Unit test class for testing ping-pong data tasks using Google Test framework.
 *
 * This class sets up two data tasks that simulate a ping-pong interaction. The tasks
 * increment counters each time they process a "ping" or "pong" message.
 */

class DataTaskPingPongTest : public ::testing::Test
{
protected:
    enum class msg
    {
        ping,
        pong
    };

    /**
     * @brief Sets up the test environment by initializing the context and creating two data tasks.
     */
    void SetUp() override
    {
        context = std::make_shared<MockContext>();

        auto process_pong_bind = std::bind(&DataTaskPingPongTest::process_pong, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3);

        auto process_ping_bind = std::bind(&DataTaskPingPongTest::process_ping, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3);

        task1 = std::make_unique<tools::data_task<MockContext, msg>>(
            std::move(startup_routine), std::move(process_ping_bind), context, 10, "PingTask", 2048);

        task2 = std::make_unique<tools::data_task<MockContext, msg>>(
            std::move(startup_routine), std::move(process_pong_bind), context, 10, "PongTask", 2048);

        // Allow some time for the task to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    /**
     * @brief Cleans up the test environment by resetting the data tasks.
     */
    void TearDown() override
    {
        task1.reset();
        task2.reset();
    }

    /**
     * @brief Shared pointer to the mock context used by the data tasks.
     */
    std::shared_ptr<MockContext> context;

    /**
     * @brief Unique pointer to the first data task (PingTask).
     */
    std::unique_ptr<tools::data_task<MockContext, msg>> task1;

    /**
     * @brief Unique pointer to the second data task (PongTask).
     */
    std::unique_ptr<tools::data_task<MockContext, msg>> task2;

    /**
     * @brief Atomic counter for the number of "ping" messages processed.
     */
    std::atomic<int> ping_count = 0;

    /**
     * @brief Atomic counter for the number of "pong" messages processed.
     */
    std::atomic<int> pong_count = 0;

    /**
     * @brief Processes the "pong" message, increments the pong counter, and submits a "ping" message if the counter is
     * less than 10.
     */
    void process_pong(const std::shared_ptr<MockContext>& context, const msg& data, const std::string& task_name)
    {
        (void)context;
        (void)task_name;

        if (data == msg::pong)
        {
            pong_count.fetch_add(1);
            if (pong_count.load() < 10)
            {
                task1->submit(msg::ping);
            }
        }
    }

    /**
     * @brief Processes the "ping" message, increments the ping counter, and submits a "pong" message.
     */
    void process_ping(const std::shared_ptr<MockContext>& context, const msg& data, const std::string& task_name)
    {
        (void)context;
        (void)task_name;

        if (data == msg::ping)
        {
            ping_count.fetch_add(1);
            task2->submit(msg::pong);
        }
    }
};

/**
 * @brief Test case for PingPongDataTaskTest.
 *
 * This test verifies the communication between two tasks using a ping-pong mechanism.
 * It submits a "ping" message to task1 and allows some time for the tasks to process.
 * The test then checks if the ping and pong counts have reached the expected values.
 *
 * @test
 * - Submits a "ping" message to task1.
 * - Waits for 3000 milliseconds to allow task processing.
 * - Expects the ping_count to be 10.
 * - Expects the pong_count to be 10.
 */
TEST_F(DataTaskPingPongTest, PingPongCommunicationTest)
{
    task1->submit(msg::ping);

    // Allow some time for the tasks to process
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    EXPECT_EQ(ping_count.load(), 10);
    EXPECT_EQ(pong_count.load(), 10);
}
