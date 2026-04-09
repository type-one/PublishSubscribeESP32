//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

// Standard C++ implementation of tools::cond_var.
// Uses std::condition_variable_any which accepts any BasicLockable mutex type,
// including std::mutex and tools::critical_section (on the host build).

#pragma once

#include <condition_variable>

namespace tools
{
    // On standard C++, cond_var is std::condition_variable_any.
    // It satisfies the tools::cond_var interface:
    //   notify_one(), notify_all()
    //   wait(unique_lock<Mutex>&, Predicate)
    //   wait_until(unique_lock<Mutex>&, time_point, Predicate) -> bool
    using cond_var = std::condition_variable_any;

} // namespace tools
