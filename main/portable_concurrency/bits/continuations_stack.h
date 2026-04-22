/**
 * @file continuations_stack.h
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-02-14
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-02-14                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

#include "once_consumable_stack.h"
#include "small_unique_function.hpp"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

using continuation = small_unique_function<void()>;
extern template struct forward_list_deleter<continuation>;
extern template class once_consumable_stack<continuation>;

class continuations_stack {
public:
  void push(continuation &&cnt);
  template <typename Alloc> void push(continuation &&cnt, const Alloc &alloc) {
    if (!stack_.push(cnt, alloc))
      cnt();
  }

  void execute();
  bool executed() const;

private:
  once_consumable_stack<continuation> stack_;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency