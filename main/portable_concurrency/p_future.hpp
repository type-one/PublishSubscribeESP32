/**
 * @file p_future.hpp
 * @brief Portable concurrency unified future API entrypoint.
 * @author Laurent Lardinois
 * @date April 2026
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

#pragma once

/**
 * @defgroup future_hdr <portable_concurrency/p_future>
 * @headerfile portable_concurrency/p_future
 *
 * Unified public future API surface.
 *
 * This header intentionally forwards to the result-based v2 API and policy aliases.
 */

#include "p_future_policy.hpp"
