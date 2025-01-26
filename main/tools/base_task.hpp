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

#if !defined(__BASE_TASK_HPP__)
#define __BASE_TASK_HPP__

#include <string>

#include "tools/non_copyable.hpp"

namespace tools
{
    class base_task : public non_copyable
    {

    public:
        base_task() = default;

        base_task(const std::string& task_name, std::size_t stack_size)
            : m_task_name(task_name)
            , m_stack_size(stack_size)
        {
        }

        virtual ~base_task() { }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        virtual void* native_handle() = 0;

        const std::string& task_name() const { return m_task_name; }

        std::size_t stack_size() const { return m_stack_size; }

    private:
        const std::string m_task_name;
        const std::size_t m_stack_size;
    };
}


#endif //  __BASE_TASK_HPP__
