/**
 * @file coro.h
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2022-06-28
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2022-06-28                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#if defined(__cpp_impl_coroutine)
#include <coroutine>
namespace pco::detail {
using suspend_never = std::suspend_never;
template <typename Promise = void>
using coroutine_handle = std::coroutine_handle<Promise>;
} // namespace pco::detail
#define PC_HAS_COROUTINES
#elif defined(__cpp_coroutines)
#include <experimental/coroutine>
namespace pco::detail {
using suspend_never = std::experimental::suspend_never;
template <typename Promise = void>
using coroutine_handle = std::experimental::coroutine_handle<Promise>;
#define PC_HAS_COROUTINES
} // namespace pco::detail
#endif
