//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois      //
//                                                                             //
// https://github.com/type-one/PublishSubscribe                                //
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

#if !defined(__PLATFORM_HELPERS_HPP__)
#define __PLATFORM_HELPERS_HPP__

#include "platform_detection.hpp"

#if defined(FREERTOS_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <chrono>
#include <thread>
#endif

namespace tools
{
#if defined(FREERTOS_PLATFORM)
    inline void sleep_for(int ms)
    {
        const TickType_t delay = pdMS_TO_TICKS(ms);
        vTaskDelay(delay);
    }
#else
    inline void sleep_for(int ms) { std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(ms)); }
#endif
}

#endif //  __PLATFORM_HELPERS_HPP__
