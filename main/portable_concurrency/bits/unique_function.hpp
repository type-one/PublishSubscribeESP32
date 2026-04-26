/**
 * @file unique_function.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-01-28
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-01-28                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <memory>
#include <type_traits>

#include "invoke.h"
#include "small_unique_function.hpp"
#include "unique_function.h"
namespace pco {

template <typename R, typename... A>
unique_function<R(A...)>::unique_function() noexcept = default;

template <typename R, typename... A>
unique_function<R(A...)>::unique_function(std::nullptr_t unused) noexcept
{
  static_cast<void>(unused);  
}

template <typename R, typename... A>
template <typename F, typename>
unique_function<R(A...)>::unique_function(F&& f_arg)
    : unique_function(std::forward<F>(f_arg), detail::is_storable_t<std::decay_t<F>>{}) {}

template <typename R, typename... A>
template <typename F>
unique_function<R(A...)>::unique_function(F&& f_arg, std::true_type unused) : func_(std::forward<F>(f_arg)) {
  static_cast<void>(unused);
}

template <typename R, typename... A>
template <typename F>
unique_function<R(A...)>::unique_function(F&& f_arg, std::false_type unused) {
  static_cast<void>(unused);
  if (detail::is_null(f_arg)) {
    return;
  }
  func_ = [func = std::make_unique<std::decay_t<F>>(std::forward<F>(f_arg))](
              A... a_arg) { return detail::invoke(*func, std::forward<A>(a_arg)...); };
}

template <typename R, typename... A>
unique_function<R(A...)>::~unique_function() = default;

template <typename R, typename... A>
unique_function<R(A...)>::unique_function(unique_function<R(A...)>&&) noexcept = default;

template <typename R, typename... A>
unique_function<R(A...)>& unique_function<R(A...)>::operator=(unique_function<R(A...)>&&) noexcept = default;

template <typename R, typename... A>
R unique_function<R(A...)>::operator()(A... args) const {
  return func_(std::forward<A>(args)...);
}

template <typename R, typename... A>
unique_function<R(A...)>::unique_function(detail::small_unique_function<R(A...)>&& rhs) noexcept
    : func_(std::move(rhs)) {}

template <typename R, typename... A>
unique_function<R(A...)>& unique_function<R(A...)>::operator=(detail::small_unique_function<R(A...)>&& rhs) noexcept {
  func_ = std::move(rhs);
  return *this;
}

template <typename R, typename... A>
unique_function<R(A...)>::operator detail::small_unique_function<R(A...)>&&() && noexcept {
  return std::move(func_);
}

} // namespace pco
