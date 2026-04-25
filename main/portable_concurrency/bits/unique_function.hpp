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
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

#include <memory>
#include <type_traits>

#include "invoke.h"
#include "small_unique_function.hpp"
#include "unique_function.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces,readability-identifier-length,readability-named-parameter,readability-braces-around-statements,cppcoreguidelines-missing-std-forward)
namespace pco {
inline namespace cxx14_v1 {

template <typename R, typename... A>
unique_function<R(A...)>::unique_function() noexcept = default;

template <typename R, typename... A>
unique_function<R(A...)>::unique_function(std::nullptr_t) noexcept {}

template <typename R, typename... A>
template <typename F, typename>
unique_function<R(A...)>::unique_function(F &&f)
    : unique_function(std::forward<F>(f),
                      detail::is_storable_t<std::decay_t<F>>{}) {}

template <typename R, typename... A>
template <typename F>
unique_function<R(A...)>::unique_function(F &&f, std::true_type)
    : func_(std::forward<F>(f)) {}

template <typename R, typename... A>
template <typename F>
unique_function<R(A...)>::unique_function(F &&f, std::false_type) {
  if (detail::is_null(f))
    return;
  func_ = [func = std::make_unique<std::decay_t<F>>(std::forward<F>(f))](
              A... a) { return detail::invoke(*func, std::forward<A>(a)...); };
}

template <typename R, typename... A>
unique_function<R(A...)>::~unique_function() = default;

template <typename R, typename... A>
unique_function<R(A...)>::unique_function(
    unique_function<R(A...)> &&) noexcept = default;

template <typename R, typename... A>
unique_function<R(A...)> &unique_function<R(A...)>::operator=(
    unique_function<R(A...)> &&) noexcept = default;

template <typename R, typename... A>
R unique_function<R(A...)>::operator()(A... args) const {
  return func_(std::forward<A>(args)...);
}

template <typename R, typename... A>
unique_function<R(A...)>::unique_function(
    detail::small_unique_function<R(A...)> &&rhs) noexcept
    : func_(std::move(rhs)) {}

template <typename R, typename... A>
unique_function<R(A...)> &unique_function<R(A...)>::operator=(
    detail::small_unique_function<R(A...)> &&rhs) noexcept {
  func_ = std::move(rhs);
  return *this;
}

template <typename R, typename... A>
unique_function<R(A...)>::operator detail::small_unique_function<R(A...)> &&()
    &&noexcept {
  return std::move(func_);
}

} // namespace cxx14_v1
} // namespace pco
// NOLINTEND(modernize-concat-nested-namespaces,readability-identifier-length,readability-named-parameter,readability-braces-around-statements,cppcoreguidelines-missing-std-forward)
