/**
 * @file closable_queue.h
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-10-13
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-10-13                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

#include <condition_variable>
#include "tools/critical_section.hpp"
#include "tools/cond_var.hpp"
#include <mutex>
#include <queue>

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

template <typename T> class closable_queue {
public:
  bool pop(T &dest);
  void push(T &&val);
  void close();

private:
  tools::critical_section mutex_;
  tools::cond_var cv_;
  std::queue<T> queue_;
  bool closed_ = false;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace portable_concurrency