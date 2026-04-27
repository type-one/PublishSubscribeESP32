/**
 * @file concurrency_config.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2019-08-14
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2019-08-14                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

#if defined(_MSC_VER)
#define PC_NODISCARD _Check_return_
#elif defined(__has_cpp_attribute)
#if __has_cpp_attribute(gnu::warn_unused_result)
#define PC_NODISCARD [[gnu::warn_unused_result]]
#endif
#endif

#if !defined(PC_NODISCARD)
#define PC_NODISCARD
#endif
