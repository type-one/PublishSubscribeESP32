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

#include "tools/base_task.hpp"
#include "tools/logger.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/sync_object.hpp"
#include "tools/sync_ring_vector.hpp"

namespace tools
{
    template <typename Context, typename DataType>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        requires std::is_standard_layout_v<DataType> && std::is_trivial_v<DataType>
#endif
    class data_task : public base_task
    {
        static_assert(std::is_standard_layout<DataType>::value, "DataType has to provide standard layout");
        static_assert(std::is_trivial<DataType>::value, "DataType has to be trivial type");

    public:
        data_task() = delete;

        using call_back = std::function<void(std::shared_ptr<Context>, const std::string& task_name)>;
        using data_call_back = std::function<void(std::shared_ptr<Context>, const DataType& data, const std::string& task_name)>;

        data_task(call_back&& startup_routine, data_call_back&& process_routine, std::shared_ptr<Context> context,
            std::size_t data_queue_depth, const std::string& task_name, std::size_t stack_size, 
            int cpu_affinity = base_task::run_on_all_cores, int priority = base_task::default_priority)
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

        ~data_task()
        {
            m_stop_task.store(true);
            m_data_sync.signal();
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        virtual void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle());
        }

        void submit(const DataType& data)
        {
            m_data_queue.push(data);
            m_data_sync.signal();
        }

        void isr_submit(const DataType& data)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            submit(data);
        }

    private:
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
                    m_data_queue.pop();

                    m_process_routine(m_context, data, this->task_name());
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
