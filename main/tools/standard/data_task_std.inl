/**
 * @file data_task_std.inl
 * @brief A header file defining the data_task class for handling tasks that process data of a specified type.
 *
 * This file contains the implementation of the data_task class, which is derived from the base_task class.
 * It provides functionality for handling tasks that process data of a specified type, using a publish/subscribe
 * pattern.
 *
 * @author Laurent Lardinois
 * @date January 2025
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


#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "tools/base_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/sync_object.hpp"
#include "tools/sync_ring_vector.hpp"

namespace tools
{
    /**
     * @brief A class representing a data task.
     *
     * This class is derived from the base_task class and provides functionality
     * for handling tasks that process data of a specified type.
     *
     * @tparam Context The type of the context object.
     * @tparam DataType The type of the data to be processed.
     */
    template <typename Context, typename DataType>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    requires std::is_standard_layout_v<DataType> && std::is_trivial_v<DataType>
#endif
    class data_task : public base_task // NOLINT base_task is non copyable and non movable
    {
        static_assert(std::is_standard_layout<DataType>::value, "DataType has to provide standard layout");
        static_assert(std::is_trivial<DataType>::value, "DataType has to be trivial type");

    public:
        data_task() = delete;

        /**
         * @brief Alias for a callback function that is invoked at startup.
         *
         * This callback function is used to initialize data or other parameters within a given context.
         *
         * @param context A shared pointer to the Context object.
         * @param task_name The name of the task associated with this callback.
         */
        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;

        /**
         * @brief Alias for a callback function that processes data.
         *
         * This callback function is used to process data within a given context.
         *
         * @param context A shared pointer to the Context object.
         * @param data The data to be processed.
         * @param task_name The name of the task associated with this callback.
         */
        using data_call_back = std::function<void(
            const std::shared_ptr<Context>& context, const DataType& data, const std::string& task_name)>;

        /**
         * @brief Constructs a data_task object.
         *
         * @param startup_routine The routine to be called during startup.
         * @param process_routine The routine to process data.
         * @param context Shared pointer to the context object.
         * @param data_queue_depth The depth of the data queue.
         * @param task_name The name of the task.
         * @param stack_size The stack size for the task.
         * @param cpu_affinity The CPU affinity for the task.
         * @param priority The priority of the task.
         */
        data_task(call_back&& startup_routine, data_call_back&& process_routine,
            const std::shared_ptr<Context>& context, std::size_t data_queue_depth, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_process_routine(std::move(process_routine))
            , m_data_queue(data_queue_depth)
            , m_context(context)
        {
            m_task = std::make_unique<std::thread>(
                [this]()
                {
                    set_current_thread_params(this->task_name(), this->cpu_affinity(), this->priority());

                    run_loop();
                });
        }

        /**
         * @brief Constructs a data_task object with the specified parameters, with default priority and default cpu
         * affinity.
         *
         * @param startup_routine The callback function to be executed during startup.
         * @param process_routine The callback function to process data.
         * @param context Shared pointer to the Context object.
         * @param data_queue_depth The depth of the data queue.
         * @param task_name The name of the task.
         * @param stack_size The size of the stack for the task.
         */
        data_task(call_back&& startup_routine, data_call_back&& process_routine,
            const std::shared_ptr<Context>& context, std::size_t data_queue_depth, const std::string& task_name,
            std::size_t stack_size)
            : data_task(std::move(startup_routine), std::move(process_routine), context, data_queue_depth, task_name,
                stack_size, base_task::run_on_all_cores, base_task::default_priority)
        {
        }

        /**
         * @brief Destructor for the data_task class.
         *
         * This destructor sets the m_stop_task flag to true, signals the m_data_sync condition,
         * and waits for the task thread to complete by calling join on m_task.
         */
        ~data_task() override
        {
            m_stop_task.store(true);
            m_data_sync.signal();
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        /**
         * @brief Retrieves the native handle of the task.
         *
         * This function overrides the base class implementation to return the native handle
         * of the task, wrapped as a void pointer.
         *
         * @return void* The native handle of the task, cast to a void pointer.
         */
        void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle()); // NOLINT native handler wrapping as a void*
        }

        /**
         * @brief Submits data to the queue and signals the data synchronization.
         *
         * This method pushes the provided data into the data queue and then signals
         * the data synchronization mechanism to indicate that new data is available.
         *
         * @param data The data to be submitted to the queue.
         */
        void submit(const DataType& data)
        {
            m_data_queue.push(data);
            m_data_sync.signal();
        }

        /**
         * @brief Submits data from an ISR context.
         *
         * This function is intended to be called from an Interrupt Service Routine (ISR).
         * Since standard C++ does not support direct calls from ISRs, this function falls
         * back to a standard call to submit the data.
         *
         * @param data The data to be submitted.
         */
        void isr_submit(const DataType& data)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            submit(data);
        }

    private:
        /**
         * @brief Executes the main loop for the task.
         *
         * This function runs the startup routine and then enters a loop where it waits for a signal
         * to process data from the queue. The loop continues until the stop task flag is set.
         */
        void run_loop()
        {
            // execute given startup function
            m_startup_routine(m_context, this->task_name());

            while (!m_stop_task.load())
            {
                m_data_sync.wait_for_signal();

                while (!m_data_queue.empty())
                {
                    auto data = m_data_queue.front();

                    if (data.has_value())
                    {
                        m_data_queue.pop();

                        m_process_routine(m_context, data.value(), this->task_name());
                    }
                }
            } // run loop
        }

        call_back m_startup_routine;
        data_call_back m_process_routine;
        tools::sync_object m_data_sync;
        tools::sync_ring_vector<DataType> m_data_queue;
        std::shared_ptr<Context> m_context;
        std::atomic_bool m_stop_task = false;
        std::unique_ptr<std::thread> m_task;
    };
}
