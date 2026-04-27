//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
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

#if !defined(PLATFORM_HELPERS_HPP_)
#define PLATFORM_HELPERS_HPP_

#include "tools/platform_detection.hpp"

#if defined(FREERTOS_PLATFORM)
#include "tools/freertos/platform_helpers_freertos.inl"
#else
#include "tools/standard/platform_helpers_std.inl"
#endif

#if defined(ESP_PLATFORM)
#include <esp_chip_info.h>
#include <esp_system.h>
#endif

namespace tools
{
    /** @brief Get the number of CPU cores available. */
    inline int get_nb_of_cpu_cores()
    {
#if defined(ESP_PLATFORM)
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        return static_cast<int>(chip_info.cores);
#elif defined(FREERTOS_PLATFORM)
        return 1; // default to single core on other FreeRTOS platform
#else
        return 2; // assume we have a PC with at least two cores
#endif
    }
}

#endif //  PLATFORM_HELPERS_HPP_
