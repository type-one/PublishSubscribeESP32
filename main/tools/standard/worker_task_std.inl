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
#include <thread>

#if defined(__linux__)
#include <pthread.h>
#endif

#include "tools/non_copyable.hpp"
#include "tools/sync_object.hpp"
#include "tools/sync_queue.hpp"

namespace tools
{
    template <typename Context>
    class worker_task : public non_copyable
    {

    public:
        worker_task() = delete;

        worker_task(std::function<void(std::shared_ptr<Context>)>&& startup_routine, std::shared_ptr<Context> context,
            const std::string& task_name, std::size_t stack_size = 2048U)
            : m_startup_routine(std::move(startup_routine))
            , m_context(context)
            , m_task_name(task_name)
            , m_stack_size(stack_size)
        {
            m_task = std::make_unique<std::thread>(
                [this]()
                {
#if defined(__linux__)
                    pthread_setname_np(pthread_self(), m_task_name.c_str());
#endif
                    run_loop();
                });
        }

        ~worker_task()
        {
            m_stop_task.store(true);
            m_work_sync.signal();
            m_task->join();
        }

        void* native_handle() const { return reinterpret_cast<void*>(m_task->native_handle()); }

        void delegate(std::function<void(std::shared_ptr<Context>)>&& work)
        {
            m_work_queue.emplace(std::move(work));
            m_work_sync.signal();
        }

    private:
        void run_loop()
        {
            // execute given startup function
            m_startup_routine(m_context);

            while (!m_stop_task.load())
            {
                m_work_sync.wait_for_signal();

                while (!m_work_queue.empty())
                {
                    auto work = m_work_queue.front();
                    m_work_queue.pop();

                    work(m_context);
                }
            } // run loop
        }

        std::function<void(std::shared_ptr<Context>)> m_startup_routine;
        tools::sync_object m_work_sync;
        tools::sync_queue<std::function<void(std::shared_ptr<Context>)>> m_work_queue;
        std::shared_ptr<Context> m_context;
        std::string m_task_name;
        const std::size_t m_stack_size;
        std::atomic_bool m_stop_task = false;
        std::unique_ptr<std::thread> m_task;
    };
}
