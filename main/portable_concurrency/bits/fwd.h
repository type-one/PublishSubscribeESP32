/**
 * @file fwd.h
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

#include <memory>

namespace pco {

struct canceler_arg_t {};
constexpr canceler_arg_t canceler_arg = {};

enum class future_status { ready, timeout };

template <typename T> class future;
template <typename Signature> class packaged_task;
template <typename T> class promise;
template <typename T> class shared_future;

namespace detail {
template <typename T> struct future_state;

template <typename T> std::shared_ptr<future_state<T>> &state_of(future<T> &);
template <typename T> std::shared_ptr<future_state<T>> state_of(future<T> &&);

template <typename T>
std::shared_ptr<future_state<T>> &state_of(shared_future<T> &);
template <typename T>
std::shared_ptr<future_state<T>> state_of(shared_future<T> &&);
} // namespace detail

} // namespace pco
