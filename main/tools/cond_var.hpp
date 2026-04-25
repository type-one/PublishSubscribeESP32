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

#if !defined(COND_VAR_HPP_)
#define COND_VAR_HPP_

// tools::cond_var — portable condition variable abstraction.
//
// Interface mirrors std::condition_variable_any so it works with any
// BasicLockable mutex type (std::unique_lock<Mutex> for any Mutex):
//
//   void notify_one()
//   void notify_all()
//   template<typename Lock, typename Pred>
//   void wait(Lock&, Pred)
//   template<typename Lock, typename Clock, typename Duration, typename Pred>
//   bool wait_until(Lock&, const std::chrono::time_point<Clock,Duration>&, Pred)
//
// On standard C++ (no FREERTOS_PLATFORM):
//   tools::cond_var = std::condition_variable_any
//
// On FreeRTOS (FREERTOS_PLATFORM defined):
//   tools::cond_var = a custom FreeRTOS-backed implementation
//
// Usage with portable_concurrency v2:
//   #include "tools/critical_section.hpp"
//   #include "tools/cond_var.hpp"
//   using future_t = pco::future_result<
//       int,
//       pco::result_error,
//       tools::critical_section,
//       tools::cond_var>;

#include "tools/platform_detection.hpp"

#if defined(FREERTOS_PLATFORM)
#include "tools/freertos/cond_var_freertos.inl"
#else
#include "tools/standard/cond_var_std.inl"
#endif

#endif // COND_VAR_HPP_
