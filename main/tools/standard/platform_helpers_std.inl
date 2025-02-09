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

#include <algorithm>
#include <chrono>
#include <thread>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include "tools/logger.hpp"

namespace tools
{
    inline void sleep_for(int ms)
    {
        std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(ms));
    }

    // -- specific posix and win32 task helper --

    inline void set_current_thread_params(const std::string& task_name, int cpu_affinity, int priority)
    {

#if defined(__linux__)
        pthread_setname_np(pthread_self(), task_name.c_str());

        if (cpu_affinity >= 0)
        {
            cpu_set_t cpuset = {};
            pthread_t thread_id = pthread_self();
            CPU_ZERO(&cpuset);
            CPU_SET(cpu_affinity, &cpuset);

            int ret = pthread_setaffinity_np(thread_id, sizeof(cpuset), &cpuset);
            if (0 != ret)
            {
                LOG_ERROR("Could not set cpu affinity %d to thread %s", cpu_affinity, task_name.c_str());
            }
        }

        if (priority >= 0)
        {
            // https://man7.org/linux/man-pages/man2/sched_get_priority_min.2.html
            struct sched_param param = {};
            int policy = 0;
            pthread_getschedparam(pthread_self(), &policy, &param);
            const auto min_prio = sched_get_priority_min(SCHED_RR);
            const auto max_prio = sched_get_priority_max(SCHED_RR);
            param.sched_priority = std::clamp(min_prio + priority, min_prio, max_prio);
            pthread_setschedparam(pthread_self(), SCHED_RR, &param);
        }
#elif defined(_WIN32)
        if (cpu_affinity >= 0)
        {
            HANDLE thread_id = GetCurrentThread();
            auto mask = (static_cast<DWORD_PTR>(1) << cpu_affinity);
            auto ret = SetThreadAffinityMask(thread_id, mask);

            if (0 == ret)
            {
                LOG_ERROR("Could not set cpu affinity %d to thread  %s", cpu_affinity, task_name.c_str());
            }
        }

        if (priority >= 0)
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
            HANDLE thread_id = GetCurrentThread();
            auto prio = THREAD_PRIORITY_LOWEST;
            prio = std::clamp(THREAD_PRIORITY_LOWEST + priority, THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_HIGHEST);
            auto ret = SetThreadPriority(thread_id, prio);

            if (!ret)
            {
                LOG_ERROR("Could not set priority %d to thread  %s", priority, task_name.c_str());
            }
        }
#endif
    }
}
