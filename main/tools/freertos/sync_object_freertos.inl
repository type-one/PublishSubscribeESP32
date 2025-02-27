/**
 * @file sync_object_freertos.inl
 * @brief Definition of the sync_object class using FreeRTOS event groups.
 *
 * This file contains the definition of the sync_object class, which provides
 * synchronization mechanisms using FreeRTOS event groups. The class allows for
 * signaling and waiting for events, both from task and ISR contexts.
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
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "tools/non_copyable.hpp"

namespace tools
{
    /**
     * @brief A synchronization object class using FreeRTOS event groups.
     *
     * This class provides a synchronization mechanism using FreeRTOS event groups.
     * It allows signaling and waiting for events in a thread-safe manner.
     */
    class sync_object : public non_copyable // NOLINT inherits from non copyable/non movable class
    {
    public:
        /**
         * @brief Constructor for the sync_object class.
         *
         * This constructor initializes the sync_object with an initial state and creates an event group using FreeRTOS.
         *
         * @param initial_state The initial state of the sync_object.
         */
        sync_object(bool initial_state);

        /**
         * @brief Constructor for the sync_object class with no signaled object.
         *
         * This constructor initializes the sync_object with an initial state as false and creates an event group using
         * FreeRTOS.
         *
         */
        sync_object()
            : sync_object(false)
        {
        }

        /**
         * @brief Destructor for the sync_object class.
         *
         * This destructor is responsible for cleaning up the FreeRTOS event group
         * associated with the sync_object instance. If the event group is not null,
         * it will be deleted using the FreeRTOS vEventGroupDelete function.
         */
        ~sync_object();

        /**
         * @brief Signals the event group to set the specified bit.
         *
         * This function sets the BIT0 in the event group if the event group is not null.
         */
        void signal();

        /**
         * @brief Signals the event group from an ISR context.
         *
         * This function sets a bit in the event group to signal that an event has occurred.
         * It is designed to be called from an ISR (Interrupt Service Routine) context.
         *
         * If the event group is not null, it sets the specified bit (BIT0) in the event group
         * and yields from the ISR if a higher priority task was woken.
         */
        void isr_signal();

        /**
         * @brief Waits for a signal from the event group.
         *
         * This function blocks indefinitely until the specified bit (BIT0) in the event group is set.
         * It clears the bit upon exit.
         */
        void wait_for_signal();

        /**
         * @brief Waits for a signal on the sync_object.
         *
         * This function waits for a signal on the sync_object for a specified timeout duration.
         * It uses FreeRTOS event groups to wait for the signal.
         *
         * @param timeout The maximum duration to wait for the signal, specified as a std::chrono::duration in
         * microseconds.
         */
        void wait_for_signal(const std::chrono::duration<std::uint64_t, std::micro>& timeout);

    private:
        EventGroupHandle_t m_event_group;
    };
}
