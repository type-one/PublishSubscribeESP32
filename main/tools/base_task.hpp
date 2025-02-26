/**
 * @file base_task.hpp
 * @brief An abstract base class for creating tasks in a publish/subscribe pattern.
 *
 * This file contains the definition of the base_task class, which provides
 * a foundation for creating tasks with specific attributes such as name,
 * stack size, CPU affinity, and priority.
 *
 * @author Laurent Lardinois
 * @date February 2025
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

#pragma once

#if !defined(BASE_TASK_HPP_)
#define BASE_TASK_HPP_

#include <string>

#include "tools/non_copyable.hpp"

namespace tools
{
    /**
     * @brief A base class for creating tasks.
     */
    class base_task : public non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        static constexpr const int run_on_all_cores = -1; ///< Default constant for cpu affinity
        static constexpr const int default_priority = -1; ///< Default constant for thread priority

        base_task() = delete;

        /**
         * @brief Constructs a base_task object.
         *
         * @param task_name The name of the task.
         * @param stack_size The stack size for the task.
         * @param cpu_affinity The CPU affinity for the task or use base_task::run_on_all_cores if you don't care.
         * @param priority The priority of the task or use base_task::default_priority if you don't care.
         */
        base_task(const std::string& task_name, std::size_t stack_size, int cpu_affinity, // NOLINT keep interface
            int priority)                                                                 // NOLINT keep interface
            : m_task_name(task_name)
            , m_stack_size(stack_size)
            , m_cpu_affinity(cpu_affinity)
            , m_priority(priority)
        {
        }

        /**
         * @brief Virtual destructor for base_task.
         */
        virtual ~base_task() = default;

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        /**
         * @brief Gets the native handle of the task.
         *
         * @return A pointer to the native handle.
         */
        virtual void* native_handle() = 0;

        /**
         * @brief Gets the name of the task.
         *
         * @return A constant reference to the task name.
         */
        [[nodiscard]] const std::string& task_name() const
        {
            return m_task_name;
        }

        /**
         * @brief Gets the stack size of the task.
         *
         * @return The stack size of the task.
         */
        [[nodiscard]] std::size_t stack_size() const
        {
            return m_stack_size;
        }

        /**
         * @brief Gets the CPU affinity of the task.
         *
         * @return The CPU affinity of the task.
         */
        [[nodiscard]] int cpu_affinity() const
        {
            return m_cpu_affinity;
        }

        /**
         * @brief Gets the priority of the task.
         *
         * @return The priority of the task.
         */
        [[nodiscard]] int priority() const
        {
            return m_priority;
        }

    private:
        const std::string m_task_name;  ///< The name of the task.
        const std::size_t m_stack_size; ///< The stack size of the task.
        const int m_cpu_affinity;       ///< The CPU affinity of the task.
        const int m_priority;           ///< The priority of the task.
    };
}


#endif //  BASE_TASK_HPP_
