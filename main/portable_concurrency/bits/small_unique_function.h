/**
 * @file small_unique_function.h
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-02-01
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-02-01                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

namespace pco::detail {

constexpr size_t small_buffer_size = 5 * sizeof(void *);
constexpr size_t small_buffer_align = alignof(void *);

struct small_buffer {
  alignas(small_buffer_align) std::array<std::byte, small_buffer_size> data;
};

template <typename R, typename... A> struct callable_vtbl;

template <typename S> class small_unique_function;

// Move-only type erasure for small NothrowMoveConstructible Callable object.
template <typename R, typename... A> class small_unique_function<R(A...)> {
public:
  small_unique_function() noexcept;
  small_unique_function(std::nullptr_t) noexcept;

  template <typename F,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>,
                                                        small_unique_function>>>
  small_unique_function(F &&f);

  ~small_unique_function();

  small_unique_function(const small_unique_function &) = delete;
  small_unique_function &operator=(const small_unique_function &) = delete;

  small_unique_function(small_unique_function &&rhs) noexcept;

  small_unique_function &operator=(small_unique_function &&rhs) noexcept;

  R operator()(A... args) const;

  explicit operator bool() const noexcept { return vtbl_ != nullptr; }

private:
  mutable small_buffer buffer_{};
  const callable_vtbl<R, A...> *vtbl_ = nullptr;
};

extern template class small_unique_function<void()>;

} // namespace pco::detail
