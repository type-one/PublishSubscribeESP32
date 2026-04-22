/**
 * @file alias_namespace.h
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-10-17
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-10-17                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

namespace portable_concurrency {}

#if !defined(PORTABLE_CONCURRENCY_ALIAS_NS)
#define PORTABLE_CONCURRENCY_ALIAS_NS 1
#endif

#if PORTABLE_CONCURRENCY_ALIAS_NS
/**
 * @namespace pco
 * @brief Shortcut for the library public namespace `portable_concurrency`.
 */
namespace pco = portable_concurrency;
#endif

/**
 * @namespace portable_concurrency
 * @brief The library public namespace.
 *
 * This namespace name is long and too verbose. There is an alias namespace @ref
 * pco provided which simplifies this library usage.
 */
