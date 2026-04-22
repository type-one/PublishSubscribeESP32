/**
 * @file p_future_v2.hpp
 * @brief Portable concurrency component.
 * @author Laurent Lardinois
 * @date April 2026
 * @note Refactored with the help of Copilot.
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
// Refactored with the help of Copilot.                                        //
//-----------------------------------------------------------------------------//

#pragma once

/**
 * @defgroup future_v2_hdr <portable_concurrency/p_future_v2>
 * @headerfile portable_concurrency/p_future_v2
 *
 * Result-oriented future API that avoids exception-based error transport.
 */

#include "bits/alias_namespace.h"
#include "bits/packaged_task_result.h"
#include "bits/result_future.h"