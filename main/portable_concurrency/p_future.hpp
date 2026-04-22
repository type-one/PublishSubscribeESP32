/**
 * @file p_future.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-06-14
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-06-14                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

/**
 * @defgroup future_hdr <portable_concurrency/future>
 * @headerfile portable_concurrency/future
 *
 * `future` API imlementation with continuations and executors support
 */

#include "bits/algo_adapters.h"
#include "bits/alias_namespace.h"
#include "bits/async.h"
#include "bits/future.hpp"
#include "bits/make_future.h"
#include "bits/packaged_task.h"
#include "bits/promise.h"
#include "bits/shared_future.hpp"
#include "bits/when_all.h"
#include "bits/when_any.h"