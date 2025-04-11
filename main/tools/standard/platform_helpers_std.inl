/**
 * @file platform_helpers_std.inl
 * @brief Platform-specific helper functions for thread management.
 *
 * This file contains platform-specific helper functions for managing threads,
 * including setting thread parameters such as name, CPU affinity, and priority,
 * as well as putting the current thread to sleep for a specified duration.
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

#include <algorithm>
#include <chrono>
#include <cstdint>
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
    /**
     * @brief Puts the current thread to sleep for the specified duration.
     *
     * This function causes the current thread to sleep for the specified number of milliseconds.
     *
     * @param ms The duration in milliseconds for which the thread should sleep.
     */
    inline void sleep_for(std::uint64_t ms)
    {
        std::this_thread::sleep_for(std::chrono::duration<std::uint64_t, std::milli>(ms));
    }

    /**
     * @brief Yields the execution of the current thread to allow other threads to run.
     * 
     * This function is a wrapper around `std::this_thread::yield()` and is used to 
     * signal to the operating system that the current thread is willing to yield 
     * its remaining time slice to other threads. This can be useful in multi-threaded 
     * applications to improve responsiveness or to avoid busy-waiting.
     * 
     * @note The behavior of this function depends on the operating system's thread 
     * scheduler and may vary between platforms.
     */
    inline void yield()
    {
        std::this_thread::yield();
    }

    // -- specific posix and win32 task helper --

    /**
     * @brief Sets the current thread's parameters such as name, CPU affinity, and priority.
     *
     * This function sets the name, CPU affinity, and priority of the current thread.
     * It supports both Linux and Windows platforms.
     *
     * @param task_name The name to set for the current thread.
     * @param cpu_affinity The CPU affinity to set for the current thread. If negative, affinity is not set.
     * @param priority The priority to set for the current thread. If negative, priority is not set.
     */
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
