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

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include "tools/base_task.hpp"
#include "tools/linux/linux_sched_deadline.hpp"

namespace tools
{
    template <typename Context>
    class periodic_task : public base_task
    {

    public:
        periodic_task() = delete;

        using call_back = std::function<void(std::shared_ptr<Context>, const std::string& task_name)>;

        periodic_task(call_back&& startup_routine, call_back&& periodic_routine, std::shared_ptr<Context> context,
            const std::string& task_name, const std::chrono::duration<int, std::micro>& period, std::size_t stack_size,
            int cpu_affinity = -1)
            : base_task(task_name, stack_size, cpu_affinity)
            , m_startup_routine(std::move(startup_routine))
            , m_periodic_routine(std::move(periodic_routine))
            , m_context(context)
            , m_period(period)
        {
            m_task = std::make_unique<std::thread>(
                [this]()
                {
#if defined(__linux__)
                    pthread_setname_np(pthread_self(), this->task_name().c_str());
                    int cpu_affinity = this->cpu_affinity();

                    if (cpu_affinity >= 0)
                    {
                        cpu_set_t cpuset = {};
                        pthread_t thread_id = pthread_self();
                        CPU_ZERO(&cpuset);
                        CPU_SET(cpu_affinity, &cpuset);

                        int ret = pthread_setaffinity_np(thread_id, sizeof(cpuset), &cpuset);
                        if (0 != ret)
                        {
                            LOG_ERROR("Could not set cpu affinity %d to thread %s", cpu_affinity, this->task_name().c_str());
                        }
                    }
#elif defined(_WIN32)
                    if (cpu_affinity >= 0)
                    {
                        HANDLE thread_id = GetCurrentThread();
                        auto mask = (static_cast<DWORD_PTR>(1) << cpu_affinity);
                        auto ret = SetThreadAffinityMask(thread_id, mask);

                        if (0 == ret)
                        {
                            LOG_ERROR("Could not set cpu affinity %d to thread  %s", cpu_affinity, this->task_name().c_str());
                        }
                    }
#endif
                    periodic_call();
                });
        }

        ~periodic_task()
        {
            m_stop_task.store(true);
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        virtual void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle());
        }

    private:
        void periodic_call()
        {
            auto start_time = std::chrono::high_resolution_clock::now();
            auto deadline = start_time + m_period;

            bool earliest_deadline_enabled = set_earliest_deadline_scheduling(start_time, m_period);

            // execute given startup function
            m_startup_routine(m_context, this->task_name());

            while (!m_stop_task.load())
            {
                // active wait loop
                std::chrono::high_resolution_clock::time_point current_time;
                do
                {
                    current_time = std::chrono::high_resolution_clock::now();
                } while (deadline > current_time);

                // execute given periodic function
                m_periodic_routine(m_context, this->task_name());

                // compute next deadline
                deadline += m_period;

                current_time = std::chrono::high_resolution_clock::now();

                // wait period
                if (deadline > current_time)
                {
                    const auto remaining_time = std::chrono::duration_cast<std::chrono::microseconds>(deadline - current_time);
                    // wait between 90% and 96% of the remaining time depending on scheduling mode
                    const double ratio = (earliest_deadline_enabled) ? 0.96 : 0.9;

                    // sleep until we are close to the deadline
                    const auto sleep_time = std::chrono::duration<int, std::micro>(static_cast<int>(ratio * remaining_time.count()));
                    std::this_thread::sleep_for(sleep_time);

                } // end if wait period needed
            }     // periodic task loop
        }

        call_back m_startup_routine;
        call_back m_periodic_routine;
        std::shared_ptr<Context> m_context;
        std::chrono::duration<int, std::micro> m_period;
        std::atomic_bool m_stop_task = false;
        std::unique_ptr<std::thread> m_task;
    };
}
