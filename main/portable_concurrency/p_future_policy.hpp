/**
 * @file p_future_policy.hpp
 * @brief Portable concurrency component.
 * @author Laurent Lardinois
 * @date April 2026
 * @note Refactored with the help of Copilot.
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
// Refactored with the help of Copilot.                                        //
//-----------------------------------------------------------------------------//

#pragma once

#include <utility>

/**
 * @defgroup future_policy_hdr <portable_concurrency/p_future_policy>
 * @headerfile portable_concurrency/p_future_policy
 *
 * Build-time policy selector: routes future creation to v1 (exception-based)
 * or v2 (result-based) API depending on the PORTABLE_CONCURRENCY_V2_DEFAULT
 * compile-time definition.
 *
 * Usage:
 *   #include "portable_concurrency/p_future_policy.hpp"
 *
 *   // Create a typed future in whichever mode is active:
 *   pc::future_t<int> fut = pc::make_async_default(exec, []{ return 42; });
 *
 *   // Inspect the active policy at compile time:
 *   static_assert(pc::uses_v2_policy == true);  // when v2 is selected
 *
 * When PORTABLE_CONCURRENCY_V2_DEFAULT is defined:
 *   - pc::future_t<T>               -> pc::v2::future_result<T, pc::v2::result_error>
 *   - pc::shared_future_t<T>        -> pc::v2::shared_result<T, pc::v2::result_error>
 *   - pc::promise_t<T>              -> pc::v2::promise_result<T, pc::v2::result_error>
 *   - pc::make_async_default(...)   -> pc::v2::async_result(...)
 *   - pc::make_ready_default(...)   -> pc::v2::make_ready_result(...)
 *   - pc::make_error_default<T>(e)  -> pc::v2::make_error_result<T>(e)
 *   - pc::active_async_policy       -> pc::async_policy_v2_tag
 *   - pc::uses_v2_policy            == true
 *
 * When PORTABLE_CONCURRENCY_V2_DEFAULT is NOT defined (default):
 *   - pc::future_t<T>               -> pc::future<T>
 *   - pc::shared_future_t<T>        -> pc::shared_future<T>
 *   - pc::promise_t<T>              -> pc::promise<T>
 *   - pc::make_async_default(...)   -> pc::async(...)
 *   - pc::make_ready_default(...)   -> pc::make_ready_future(...)
 *   - pc::make_error_default<T>(e)  -> pc::make_exceptional_future<T>(e)
 *   - pc::active_async_policy       -> pc::async_policy_v1_tag
 *   - pc::uses_v2_policy            == false
 *   - Requires exception support (__EXCEPTIONS / _CPPUNWIND)
 */

#ifdef PORTABLE_CONCURRENCY_V2_DEFAULT

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

#else // ----------------------------------------------------------------- v1

#include "p_future.hpp"

namespace portable_concurrency {

/// Type alias for the default future: resolves to future<T> in v1 mode.
template <typename T>
using future_t = future<T>;

/// Type alias for the default shared future: resolves to shared_future<T> in v1 mode.
template <typename T>
using shared_future_t = shared_future<T>;

/// Type alias for the default promise: resolves to promise<T> in v1 mode.
template <typename T>
using promise_t = promise<T>;

/// Unified ready-future factory — v1 policy.
template <typename T>
auto make_ready_default(T &&value) -> decltype(make_ready_future(std::forward<T>(value))) {
    return make_ready_future(std::forward<T>(value));
}

/// Unified ready-future factory for void — v1 policy.
inline auto make_ready_default() -> decltype(make_ready_future()) {
    return make_ready_future();
}

/// Unified error-future factory — v1 policy.
template <typename T>
future_t<T> make_error_default(std::exception_ptr error) {
    return make_exceptional_future<T>(std::move(error));
}

/// Unified error-future factory — v1 policy.
template <typename T, typename Err>
future_t<T> make_error_default(Err error) {
    return make_exceptional_future<T>(std::move(error));
}

/**
 * @brief Unified async dispatch — v1 (exception-based) policy.
 *
 * Posts `callable(args...)` to executor `exec` and returns a future<R>
 * where R = std::invoke_result_t<F, A...>.  Requires exception support.
 */
template <typename Exec, typename F, typename... A>
auto make_async_default(Exec&& exec, F&& callable, A&&... args)
    -> decltype(async(std::forward<Exec>(exec), std::forward<F>(callable), std::forward<A>(args)...))
{
    return async(std::forward<Exec>(exec), std::forward<F>(callable), std::forward<A>(args)...);
}

/// Tag type identifying that the v1 (exception-based) policy is active.
struct async_policy_v1_tag {};

/// Resolves to the tag type of the currently active async policy.
using active_async_policy = async_policy_v1_tag;

/// True when the v2 result-based policy is active; false when the v1 exception-based policy is active.
static constexpr bool uses_v2_policy = false;

} // namespace portable_concurrency

#endif // PORTABLE_CONCURRENCY_V2_DEFAULT