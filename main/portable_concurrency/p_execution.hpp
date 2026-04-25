/**
 * @file p_execution.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-10-27
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-10-27                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

/**
 * @defgroup execution <portable_concurrency/execution>
 * @headerfile portable_concurrency/execution
 *
 * Set of primitives to establish future API interaction with user provided
 * executor types.
 */

#include "bits/execution.h"

#if !defined(PCO_NAMESPACE_ALIAS_DEFINED)
#define PCO_NAMESPACE_ALIAS_DEFINED
namespace pco = portable_concurrency;
#endif
