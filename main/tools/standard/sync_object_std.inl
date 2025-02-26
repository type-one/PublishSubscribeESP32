/**
 * @file sync_object_std.inl
 * @brief Synchronization object using standard C++ constructs.
 *
 * This file contains the definition of the sync_object class, which provides
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
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "tools/non_copyable.hpp"

namespace tools
{
    class sync_object : public non_copyable // NOLINT inherits from non copyable/non movable class
    {
    public:
        /**
         * @brief Constructs a sync_object with an initial state.
         *
         * @param initial_state The initial state of the sync_object. If true, the object is signaled.
         */
        sync_object(bool initial_state);

        /**
         * @brief Constructs a default sync_object with the object not signaled.
         */
        sync_object()
            : sync_object(false)
        {
        }

        /**
         * @brief Destructor for the sync_object class.
         *
         * This destructor ensures that the sync_object is properly cleaned up.
         * It sets the m_signaled and m_stop flags to true and notifies all
         * waiting threads.
         */
        ~sync_object();

        /**
         * @brief Signals the sync_object, setting its state to signaled and notifying one waiting thread.
         */
        void signal();

        /**
         * @brief Waits for the signal to be set.
         *
         * This function will block until the signal is set. It uses a condition variable
         * to wait for the signal and a mutex to protect the shared state.
         */
        void wait_for_signal();

        /**
         * @brief Waits for the signal with a specified timeout.
         *
         * This function waits for the signal to be set, or until the specified timeout duration has elapsed.
         *
         * @param timeout The maximum duration to wait for the signal, specified as a std::chrono::duration.
         */
        void wait_for_signal(const std::chrono::duration<std::uint64_t, std::micro>& timeout);

        /**
         * @brief Signals the synchronization object from an ISR context.
         *
         * This function is intended to be called from an Interrupt Service Routine (ISR).
         * Since standard C++ does not support ISR-specific calls, this function falls back
         * to the standard signal call.
         */
        void isr_signal();

    private:
        bool m_signaled;
        bool m_stop;
        std::mutex m_mutex;
        std::condition_variable m_cond;
    };
}
