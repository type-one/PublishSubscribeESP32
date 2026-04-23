/**
 * @file p_future_policy.hpp
 * @brief Portable concurrency component.
 * @author Laurent Lardinois, Sergey Vidyuk
 * @date April 2026
 * @note Refactored from Sergey Vidyuk's work with the help of Copilot.
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
// Derived from Sergey Vidyuk's work with the help of Copilot.                 //
//-----------------------------------------------------------------------------//

#pragma once

#include <utility>

/**
 * @defgroup future_policy_hdr <portable_concurrency/p_future_policy>
 * @headerfile portable_concurrency/p_future_policy
 *
 * Result-oriented future API (v2): routes all future creation to the v2 API.
 * Exception support is not required.
 *
 * Preferred usage:
 *   #include "portable_concurrency/p_future.hpp"
 *
 * Advanced/policy-only usage:
 *   #include "portable_concurrency/p_future_policy.hpp"
 *
 *   // Create a typed future using the v2 result-based API:
 *   pco::future_t<int> fut = pco::make_async_default(exec, []{ return 42; });
 *
 * Type aliases (all use v2 result-based API):
 *   - pco::future_t<T>               -> pco::v2::future_result<T, pco::v2::result_error>
 *   - pco::shared_future_t<T>        -> pco::v2::shared_result<T, pco::v2::result_error>
 *   - pco::promise_t<T>              -> pco::v2::promise_result<T, pco::v2::result_error>
 *   - pco::make_async_default(...)   -> pco::v2::async_result(...)
 *   - pco::make_ready_default(...)   -> pco::v2::make_ready_result(...)
 *   - pco::make_error_default<T>(e)  -> pco::v2::make_error_result<T>(e)
 *   - pco::active_async_policy       -> pco::async_policy_v2_tag
 *   - pco::uses_v2_policy            == true (always)
 */

#include "p_future_v2.hpp"

namespace portable_concurrency {

/// Type alias for the default future: resolves to future_result<T, E> in v2 mode.
template <typename T, typename E = v2::result_error>
using future_t = v2::future_result<T, E>;

/// Type alias for the default shared future: resolves to shared_result<T, E> in v2 mode.
template <typename T, typename E = v2::result_error>
using shared_future_t = v2::shared_result<T, E>;

/// Type alias for the default promise: resolves to promise_result<T, E> in v2 mode.
template <typename T, typename E = v2::result_error>
using promise_t = v2::promise_result<T, E>;

/// Unified ready-future factory — v2 policy.
template <typename T>
auto make_ready_default(T &&value)
    -> decltype(v2::make_ready_result<T>(std::forward<T>(value))) {
    return v2::make_ready_result<T>(std::forward<T>(value));
}

/// Unified ready-future factory for void — v2 policy.
inline auto make_ready_default() -> decltype(v2::make_ready_result<>()) {
    return v2::make_ready_result<>();
}

/// Unified error-future factory — v2 policy.
template <typename T, typename E = v2::result_error>
auto make_error_default(E error)
    -> decltype(v2::make_error_result<T, E>(std::move(error))) {
    return v2::make_error_result<T, E>(std::move(error));
}

/**
 * @brief Unified async dispatch — v2 (result-based) policy.
 *
 * Posts `callable(args...)` to executor `exec` and returns a future_result<R, result_error>
 * where R = std::invoke_result_t<F, A...>.
 */
template <typename Exec, typename F, typename... A>
auto make_async_default(Exec&& exec, F&& callable, A&&... args)
    -> decltype(v2::async_result(
        std::forward<Exec>(exec), std::forward<F>(callable), std::forward<A>(args)...))
{
    return v2::async_result(
        std::forward<Exec>(exec), std::forward<F>(callable), std::forward<A>(args)...);
}

/// Tag type identifying that the v2 (result-based) policy is active.
struct async_policy_v2_tag {};

/// Resolves to the tag type of the currently active async policy.
using active_async_policy = async_policy_v2_tag;

/// True when the v2 result-based policy is active; false when the v1 exception-based policy is active.
static constexpr bool uses_v2_policy = true;

} // namespace portable_concurrency
