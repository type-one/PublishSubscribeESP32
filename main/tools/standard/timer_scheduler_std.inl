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
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "cpptime/cpptime.hpp"
#include "tools/non_copyable.hpp"

namespace tools
{
    using timer_handle = std::size_t;

    class timer_scheduler : non_copyable
    {
    public:
        timer_scheduler() = default;
        ~timer_scheduler() = default;

        /**
         * Add a new timer.
         *
         * \param period The period of the timer in ms
         * \param handler The callable that is invoked when the timer fires.
         * \param auto_reload If true, then the timer will expire repeatedly with a frequency set by the period
         * parameter. If set to false, then the timer will be a one-shot timer.
         */
        timer_handle add(const std::string& timer_name, const std::uint64_t period,
            std::function<void(timer_handle)>&& handler, bool auto_reload = false);


        /**
         * Add a new timer.
         *
         * \param period The period of the timer as std::chrono duration
         * \param handler The callable that is invoked when the timer fires.
         * \param auto_reload If true, then the timer will expire repeatedly with a frequency set by the period
         * parameter. If set to false, then the timer will be a one-shot timer.
         */
        timer_handle add(const std::string& timer_name, const std::chrono::duration<std::uint64_t, std::micro>& period,
            std::function<void(timer_handle)>&& handler, bool auto_reload = false);


        /**
         * Removes the timer with the given id.
         */
        bool remove(timer_handle hnd);

    private:
        CppTime::Timer m_timer_scheduler;
    };
}
