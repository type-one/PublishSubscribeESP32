/**
 * @file functional.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-08-16
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-08-16                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

/**
 * @defgroup functional <portable_concurrency/functional>
 * @headerfile portable_concurrency/functional
 *
 * Extra functional types useful for writing concurrent code.
 *
 * `unique_function` invocation is provided via `try_invoke(...)`.
 */

#include "bits/unique_function.hpp"
