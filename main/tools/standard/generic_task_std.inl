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
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "tools/base_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"

namespace tools
{
    template <typename Context>
    class generic_task : public base_task
    {

    public:
        generic_task() = delete;

        using call_back = std::function<void(std::shared_ptr<Context>, const std::string& task_name)>;

        generic_task(call_back&& routine, std::shared_ptr<Context> context, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity = base_task::run_on_all_cores,
            int priority = base_task::default_priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_routine(std::move(routine))
            , m_context(context)
        {
            m_task = std::make_unique<std::thread>(
                [this]()
                {
                    set_current_thread_params(this->task_name(), this->cpu_affinity(), this->priority());

                    single_call();
                });
        }

        ~generic_task()
        {
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        virtual void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle());
        }

    private:
        void single_call()
        {
            // execute given function
            m_routine(m_context, this->task_name());
        }

        call_back m_routine;
        std::shared_ptr<Context> m_context;
        std::unique_ptr<std::thread> m_task;
    };
}
