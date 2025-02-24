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

#pragma once

#if !defined(LINUX_SCHED_DEADLINE_HPP_)
#define LINUX_SCHED_DEADLINE_HPP_

#if defined(__linux__)
#include <algorithm>
#include <chrono>
#include <cstdint>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace tools
{
    static inline bool is_running_as_root()
    {
        return (getuid() == 0) || (geteuid() == 0);
    }

    // https://docs.kernel.org/scheduler/sched-deadline.html
    // https://www.kernel.org/doc/Documentation/scheduler/sched-deadline.txt
    // https://github.com/jlelli/sched-deadline-tests
    // http://retis.sssup.it/luca/TuToR/sched_dl-presentation.pdf
    // https://stackoverflow.com/questions/73431869/best-practice-for-realtime-periodic-task-1ms-with-linux-and-multi-core-syste
    // https://man7.org/linux/man-pages/man3/pthread_setschedparam.3.html
    // https://man.cx/pthread_make_periodic_np(3)
    // https://man.cx/pthread_wait_np(3)

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

/* XXX use the proper syscall numbers */
#ifdef __x86_64__
#ifndef __NR_sched_setattr
#define __NR_sched_setattr 314
#endif
#ifndef __NR_sched_getattr
#define __NR_sched_getattr 315
#endif
#endif

#ifdef __i386__
#ifndef __NR_sched_setattr
#define __NR_sched_setattr 351
#endif
#ifndef __NR_sched_getattr
#define __NR_sched_getattr 352
#endif
#endif

#ifdef __arm__
#ifndef __NR_sched_setattr
#define __NR_sched_setattr 380
#endif
#ifndef __NR_sched_getattr
#define __NR_sched_getattr 381
#endif
#endif

    namespace linux_os
    {
        struct sched_attr
        {
            __u32 size;

            __u32 sched_policy;
            __u64 sched_flags;

            /* SCHED_NORMAL, SCHED_BATCH */
            __s32 sched_nice;

            /* SCHED_FIFO, SCHED_RR */
            __u32 sched_priority;

            /* SCHED_DEADLINE (nsec) */
            __u64 sched_runtime;
            __u64 sched_deadline;
            __u64 sched_period;
        };

        inline int sched_setattr(pid_t pid, const struct tools::linux_os::sched_attr* attr, unsigned int flags)
        {
            return static_cast<int>(syscall(static_cast<long>(__NR_sched_setattr), pid, attr, flags));
        }

        inline int sched_getattr(pid_t pid, struct tools::linux_os::sched_attr* attr, unsigned int size, unsigned int flags)
        {
            return static_cast<int>(syscall(static_cast<long>(__NR_sched_getattr), pid, attr, size, flags));
        }
    }

    static bool set_earliest_deadline_scheduling(std::chrono::high_resolution_clock::time_point start_time,
        const std::chrono::duration<std::uint64_t, std::micro>& period)
    {
        // Earliest Deadline scheduling if can run as root

        bool result = false;

        if (is_running_as_root())
        {
            const auto tid = static_cast<pid_t>(syscall(static_cast<long>(SYS_gettid)));

            tools::linux_os::sched_attr attr = {};
            const unsigned int flags = 0;

            attr.size = sizeof(attr);
            attr.sched_flags = 0;
            attr.sched_nice = 0;
            attr.sched_priority = 0;
            attr.sched_policy = SCHED_DEADLINE;

            const auto deadline = start_time + period;
            const auto current_time = std::chrono::high_resolution_clock::now();
            const auto remaining_time = std::chrono::duration_cast<std::chrono::microseconds>(deadline - current_time);

            // the kernel requires that sched_runtime <= sched_deadline <= sched_period
            //
            //  arrival/wakeup                    absolute deadline
            //     |    start time                    |
            //     |        |                         |
            //     v        v                         v
            //-----x--------xooooooooooooooooo--------x--------x---
            //              |<-- Runtime ------->|
            //     |<----------- Deadline ----------->|
            //     |<-------------- Period ------------------->|

            constexpr const std::uint64_t nano_sec_coeff = 1000ULL;
            constexpr const std::uint64_t floor_value = 0ULL;
            attr.sched_runtime = static_cast<std::uint64_t>(
                std::max(floor_value, static_cast<std::uint64_t>(remaining_time.count()) * nano_sec_coeff));
            attr.sched_deadline = static_cast<std::uint64_t>(period.count()) * nano_sec_coeff;
            attr.sched_period = attr.sched_deadline;
            const int ret = sched_setattr(tid, &attr, flags); // NOLINT initialized by sched_setattr returned value

            result = (ret >= 0); // success if true
        }
        else
        {
            result = false;
        }

        return result;
    }
}

#else // end if #defined __linux__

static bool set_earliest_deadline_scheduling(std::chrono::high_resolution_clock::time_point start_time,
    const std::chrono::duration<std::uint64_t, std::micro>& period)
{
    (void)start_time;
    (void)period;

    // not implemented
    return false;
}

#endif

#endif // LINUX_SCHED_DEADLINE_HPP_
