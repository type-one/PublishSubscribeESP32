/**
 * @file sync_object_impl_std.inl
 * @brief Implementation of a synchronization object using standard C++ constructs.
 *
 * This file contains the implementation of the sync_object class, which provides
 * a synchronization mechanism using standard C++ constructs such as mutexes and
 * condition variables.
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
#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "tools/sync_object.hpp"

namespace tools
{
    // standard implementation

    sync_object::sync_object(bool initial_state)
        : m_signaled(initial_state)
        , m_stop(false)
    {
    }

    sync_object::~sync_object()
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_signaled = true;
            m_stop = true;
        }
        m_cond.notify_all();
    }

    void sync_object::signal()
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_signaled = true;
        }
        m_cond.notify_one();
    }

    bool sync_object::is_signaled() const
    {
        return m_signaled;
    }

    void sync_object::isr_signal()
    {
        // no calls from ISRs in standard C++ platform, fallback to standard call
        sync_object::signal();
    }

    void sync_object::wait_for_signal()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [&]() { return m_signaled; });
        m_signaled = m_stop;
    }

    void sync_object::wait_for_signal(const std::chrono::duration<std::uint64_t, std::micro>& timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait_for(lock, timeout, [&]() { return m_signaled; });
        m_signaled = m_stop;
    }

} // end namespace tools
