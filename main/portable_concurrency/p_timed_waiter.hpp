/**
 * @file p_timed_waiter.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2019-03-16
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2019-03-16                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

#if defined(PORTABLE_CONCURRENCY_V1_API_FROZEN)
#error "portable_concurrency timed_waiter is deprecated with v1 freeze. Migrate to future_result/shared_result wait_for or wait_until via portable_concurrency/p_future.hpp."
#endif

/**
 * @defgroup timed_waiter_hdr <portable_concurrency/timed_waiter>
 * @headerfile portable_concurrency/timed_waiter
 *
 * Class allowing to wait for futures with timeout
 */

#include "bits/timed_waiter.h"
