/**
 * @file p_latch.hpp
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
 * @defgroup latch <portable_concurrency/latch>
 * @headerfile portable_concurrency/latch
 *
 * Simple non reusable barrier class.
 */

#include "bits/latch.h"

#if !defined(PCO_NAMESPACE_ALIAS_DEFINED)
#define PCO_NAMESPACE_ALIAS_DEFINED
namespace pco = portable_concurrency;
#endif
