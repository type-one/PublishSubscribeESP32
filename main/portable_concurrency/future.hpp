/**
 * @file future.hpp
 * @brief Portable concurrency unified future API entrypoint.
 * @author Laurent Lardinois
 * @date April 2026
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

#pragma once

/**
 * @defgroup future_hdr <portable_concurrency/future>
 * @headerfile portable_concurrency/future
 *
 * Unified public future API surface.
 *
 * This header exposes a single result-based policy.
 */

#include <utility>

#include "bits/packaged_task_result.hpp"
#include "bits/result_future.hpp"

namespace pco
{
    /**
     * @brief Type alias for the default future policy.
     * @tparam T Value type.
     * @tparam E Error enum type.
     */
    template <typename T, typename E = result_error>
    using future_t = future_result<T, E>;

    /**
     * @brief Type alias for the default shared-future policy.
     * @tparam T Value type.
     * @tparam E Error enum type.
     */
    template <typename T, typename E = result_error>
    using shared_future_t = shared_result<T, E>;

    /**
     * @brief Type alias for the default promise policy.
     * @tparam T Value type.
     * @tparam E Error enum type.
     */
    template <typename T, typename E = result_error>
    using promise_t = promise_result<T, E>;

    /**
     * @brief Creates an already-ready default-policy future with value.
     * @tparam T Source value type.
     * @param value Value to store in the ready future.
     * @return Ready default-policy future.
     */
    template <typename T>
    auto make_ready_default(T&& value) -> decltype(make_ready_result<T>(std::forward<T>(value)))
    {
        return make_ready_result<T>(std::forward<T>(value));
    }

    /**
     * @brief Creates an already-ready default-policy void future.
     * @return Ready default-policy future for void value type.
     */
    inline auto make_ready_default() -> decltype(make_ready_result<>())
    {
        return make_ready_result<>();
    }

    /**
     * @brief Creates an already-failed default-policy future.
     * @tparam T Success value type.
     * @tparam E Error enum type.
     * @param error Error value to store.
     * @return Ready default-policy future containing the provided error.
     */
    template <typename T, typename E = result_error>
    auto make_error_default(E error) -> decltype(make_error_result<T, E>(std::move(error)))
    {
        return make_error_result<T, E>(std::move(error));
    }

    /**
     * @brief Unified async dispatch (result-based policy).
     *
     * Posts `callable(args...)` to executor `exec` and returns a future_result<R, E>
     * where R = std::invoke_result_t<F, A...>.
     */
    template <typename Exec, typename F, typename... A>
    auto make_async_default(Exec&& exec, F&& callable, A&&... args)
        -> decltype(async_result(std::forward<Exec>(exec), std::forward<F>(callable), std::forward<A>(args)...))
    {
        return async_result(std::forward<Exec>(exec), std::forward<F>(callable), std::forward<A>(args)...);
    }

} // namespace pco
