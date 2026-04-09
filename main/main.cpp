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

/**
 * @file main.cpp
 * @brief Application runner and platform-specific entry points.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "examples/examples.hpp"
#include "tools/platform_detection.hpp"

#if defined(CPP_EXCEPTIONS_ENABLED)
#include <exception>
#endif

#include "tools/logger.hpp"

#if defined(FREERTOS_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#if defined(USE_MEM_POOL_ALLOCATOR)
extern void init_mem_pool_allocator();
extern void destroy_mem_pool_allocator();
#endif

void runner()
{
#if defined(USE_MEM_POOL_ALLOCATOR)
    std::printf("Init mem pool allocator\n");
    init_mem_pool_allocator();
#endif

    run_example_hardware_timer_interrupt();
    run_example_ring_container();
    run_example_sync_container();
    run_example_pub_sub_and_task();
    run_example_worker_and_command();
    run_example_bytepack_and_data_task();
    run_example_json_and_fpm();
    run_example_variant_fsm();
    run_example_timer_and_date();
    run_example_smp();
    run_example_allocator_stress();

#if defined(USE_MEM_POOL_ALLOCATOR)
    std::printf("Destroy mem pool allocator\n");
    destroy_mem_pool_allocator();
#endif

    std::printf("This is The END\n");
}

#if defined(FREERTOS_PLATFORM)
constexpr const std::size_t stack_size = 8192;

StaticTask_t x_task_buffer = {};
std::array<StackType_t, stack_size> x_stack = {};

void v_task_code(void* pv_parameters)
{
    configASSERT((std::uint32_t)pv_parameters == 1UL);

    runner();

    vTaskDelete(nullptr);
}

void launch_runner() noexcept
{
    TaskHandle_t x_handle = xTaskCreateStatic(v_task_code,
        "RUNNER",
        stack_size,
        reinterpret_cast<void*>(1),
        tskIDLE_PRIORITY,
        x_stack.data(),
        &x_task_buffer);

    (void)x_handle;

    vTaskSuspend(nullptr);
}
#endif

#if !defined(FREERTOS_PLATFORM)
#if defined(CPP_EXCEPTIONS_ENABLED)
void runner_except_catch()
{
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try
    {
        runner();
    }
    catch (std::exception& exc)
    {
        LOG_ERROR("Exception catched - %s", exc.what());
    }
#else
    runner();
#endif
}
#else
void runner_no_except()
{
    runner();
}
#endif
#endif

#if defined(FREERTOS_PLATFORM)
extern "C" void app_main() noexcept
#else
int main() noexcept
#endif
{
#if defined(FREERTOS_PLATFORM)
    launch_runner();
#elif defined(CPP_EXCEPTIONS_ENABLED)
    runner_except_catch();
    return 0;
#else
    runner_no_except();
    return 0;
#endif
}
