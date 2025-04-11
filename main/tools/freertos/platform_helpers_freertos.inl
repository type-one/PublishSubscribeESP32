/**
 * @file platform_helpers_freertos.inl
 * @brief Helper functions for FreeRTOS tasks in a C++ Publish/Subscribe pattern.
 *
 * This file contains helper functions for creating and managing FreeRTOS tasks,
 * including functions for sleeping and creating tasks with optional CPU affinity.
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

#include <algorithm>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "tools/platform_detection.hpp"

#if defined(ESP_PLATFORM)
#include <freertos/idf_additions.h>
#endif

#include "tools/logger.hpp"

namespace tools
{
    /**
     * @brief Puts the current task to sleep for a specified duration.
     *
     * This function converts the given duration in milliseconds to FreeRTOS ticks
     * and then calls vTaskDelay to put the current task to sleep for that duration.
     *
     * @param ms The duration in milliseconds for which the task should sleep.
     */
    inline void sleep_for(int ms)
    {
        const TickType_t delay = pdMS_TO_TICKS(ms);
        vTaskDelay(delay);
    }

    /**
     * @brief Yields the current task to allow other tasks to run.
     *
     * This function calls taskYIELD to yield the current task, allowing other
     * tasks of equal or higher priority to run.
     */
    inline void yield()
    {
        taskYIELD();
    }

    // -- specific FreeRTOS task helper --

    /**
     * @brief Type alias for a FreeRTOS task function handler.
     *
     * This type represents a function pointer to a FreeRTOS task function,
     * which takes a single void pointer argument.
     */
    using task_func_handler = void(void*);

    /**
     * @brief Creates a FreeRTOS task with optional CPU affinity.
     *
     * This function creates a FreeRTOS task with the specified parameters. If the ESP_PLATFORM is defined,
     * it attempts to create the task pinned to a specific core. If the task creation fails, it logs an error.
     * If the task is successfully created, it optionally sets the core affinity if configUSE_CORE_AFFINITY is defined
     * and ESP_PLATFORM is not defined.
     *
     * @param task_handle Pointer to the handle of the created task.
     * @param task_name Name of the task.
     * @param task_function Function to be executed by the task.
     * @param task_param Parameter to be passed to the task function.
     * @param stack_size Stack size for the task.
     * @param cpu_affinity CPU core to which the task should be pinned. If negative, no affinity is set.
     * @param priority Priority of the task.
     * @return true if the task was successfully created, false otherwise.
     */
    inline bool task_create(TaskHandle_t* task_handle, const std::string& task_name, task_func_handler task_function,
        void* task_param, std::size_t stack_size, int cpu_affinity, int priority)
    {
        BaseType_t ret = 0;
        bool task_created = false;

        UBaseType_t freertos_priority = tskIDLE_PRIORITY;

        if (priority >= 0)
        {
            // https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/01-xTaskCreate
            freertos_priority = std::clamp(static_cast<UBaseType_t>(priority + tskIDLE_PRIORITY),
                static_cast<UBaseType_t>(tskIDLE_PRIORITY), static_cast<UBaseType_t>(configMAX_PRIORITIES - 1));
        }

#if defined(ESP_PLATFORM)
        // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_idf.html
        if (cpu_affinity >= 0)
        {
            ret = xTaskCreatePinnedToCore(task_function, task_name.c_str(), stack_size, task_param, freertos_priority,
                task_handle, static_cast<BaseType_t>(cpu_affinity));

            if (pdPASS != ret)
            {
                LOG_ERROR("xTaskCreatePinnedToCore() failed for task %s on core %d", task_name.c_str(), cpu_affinity);
            }
        }
#else
        (void)cpu_affinity; // potentially unused
#endif

        if (pdPASS != ret)
        {
            ret = xTaskCreate(task_function, task_name.c_str(), stack_size, task_param, freertos_priority, task_handle);
        }

        if (pdPASS == ret)
        {
#if defined(configUSE_CORE_AFFINITY) && !defined(ESP_PLATFORM)
            if (cpu_affinity >= 0)
            {
                // https://github.com/dogusyuksel/rtos_hal_stm32
                // https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/13-Symmetric-multiprocessing-introduction
                UBaseType_t mask = (1 << cpu_affinity);
                vTaskCoreAffinitySet(task_handle, mask);
            }
#endif
            task_created = true;
        }
        else
        {
            LOG_ERROR("FATAL error: xTaskCreate() failed for task %s", task_name.c_str());
        }

        return task_created;
    }
}
