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
 * @file examples.hpp
 * @brief Declares the example entry points executed by the application runner.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#pragma once

/**
 * @brief Runs the ring buffer and ring vector examples.
 */
void run_example_ring_container();

/**
 * @brief Runs the lock-free and synchronized container examples.
 */
void run_example_sync_container();

/**
 * @brief Runs the publish/subscribe, observer, generic task, and periodic task examples.
 */
void run_example_pub_sub_and_task();

/**
 * @brief Runs the queued command and worker task examples.
 */
void run_example_worker_and_command();

/**
 * @brief Runs the bytepack serialization, aggregation, and data task examples.
 */
void run_example_bytepack_and_data_task();

/**
 * @brief Runs the JSON serialization and fixed-point math examples.
 */
void run_example_json_and_fpm();

/**
 * @brief Runs the variant-based finite state machine example.
 */
void run_example_variant_fsm();

/**
 * @brief Runs the calendar and timer examples.
 */
void run_example_timer_and_date();

/**
 * @brief Runs the SMP, lock-free inter-core, memory pipe, and priority examples.
 */
void run_example_smp();

/**
 * @brief Runs the ESP32 hardware timer interrupt example when the platform supports it.
 */
void run_example_hardware_timer_interrupt();

/**
 * @brief Runs the allocator stress example.
 */
void run_example_allocator_stress();

/**
 * @brief Runs the async processing example.
 */
void run_example_async_processing();

/**
 * @brief Runs the time_list example.
 */
void run_example_time_list();
