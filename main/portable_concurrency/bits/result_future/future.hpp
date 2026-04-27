/**
 * @file future.hpp
 * @brief future_result implementation.
 * @author Laurent Lardinois, Sergey Vidyuk
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

#include "tools/platform_detection.hpp"

#include "promise.hpp"

namespace pco
{
    /**
     * @brief Forward declaration of copyable shared result handle.
     * @tparam T Value type.
     * @tparam E Error type.
     */
    template <typename T, typename E>
    class shared_result;

    /**
     * @brief Move-only asynchronous result handle.
     * @tparam T Value type delivered on success. Use void for no payload.
     * @tparam E Error enum type used on failure.
     *
     * This handle exposes waiting, continuation chaining, notification hooks,
     * and conversion to shared ownership via share().
     */
    template <typename T, typename E>
    class future_result
    {
        static_assert(!std::is_reference_v<T>,
            "portable_concurrency v2 does not support reference return types. "
            "Use pointers or std::reference_wrapper<T> instead. "
            "This design constraint avoids move-semantics ambiguity and lifetime issues.");

    public:
        using value_type = T;
        using error_type = E;
        using result_type = tools::expected<T, E>;

#if defined(PC_HAS_COROUTINES) && defined(CPP_EXCEPTIONS_ENABLED)
        using promise_type = promise_result<T, E>;
#endif

        future_result() = default;

        explicit future_result(detail::result_state_ptr<T, E> state)
            : state_(std::move(state))
        {
        }

        future_result(const future_result&) = delete;
        future_result& operator=(const future_result&) = delete;
        future_result(future_result&&) noexcept = default;
        future_result& operator=(future_result&&) noexcept = default;
        ~future_result() = default;

        /**
         * @brief Checks whether this handle currently references a shared state.
         * @return true when the handle has state, false otherwise.
         */
        [[nodiscard]] bool valid() const noexcept
        {
            return static_cast<bool>(state_);
        }

        /**
         * @brief Blocks until the asynchronous state is marked ready.
         */
        void wait() const
        {
            if (!state_)
            {
                return;
            }

            std::unique_lock<tools::critical_section> lock(state_->mutex_);
            state_->cv_.wait(lock, [this] { return state_->ready_; });
        }

        /**
         * @brief Waits for readiness until timeout duration expires.
         * @tparam Rep Duration representation type.
         * @tparam Period Duration period type.
         * @param rel_time Relative timeout duration.
         * @return future_status::ready if ready before timeout, otherwise future_status::timeout.
         */
        template <typename Rep, typename Period>
        [[nodiscard]] future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const
        {
            return wait_until(std::chrono::steady_clock::now() + rel_time);
        }

        /**
         * @brief Waits for readiness until an absolute time-point.
         * @tparam Clock Clock type.
         * @tparam Duration Duration type.
         * @param abs_time Absolute timeout time-point.
         * @return future_status::ready if ready before timeout, otherwise future_status::timeout.
         */
        template <typename Clock, typename Duration>
        [[nodiscard]] future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const
        {
            return detail::wait_until_ready(state_, abs_time);
        }

        /**
         * @brief Checks whether the state is already ready without blocking.
         * @return true when ready, false otherwise.
         */
        [[nodiscard]] bool is_ready() const
        {
            if (!state_)
            {
                return false;
            }
            std::lock_guard<tools::critical_section> guard(state_->mutex_);
            return state_->ready_;
        }

#if defined(PC_HAS_COROUTINES) && defined(CPP_EXCEPTIONS_ENABLED)
        [[nodiscard]] bool await_ready() const noexcept
        {
            return !state_ || is_ready();
        }

        auto await_resume()
        {
            auto result = get_result();
            if (!result.has_value())
            {
                throw std::runtime_error("future_result await_resume failed");
            }

            if constexpr (std::is_void_v<T>)
            {
                return;
            }
            else
            {
                return std::move(*result);
            }
        }

        void await_suspend(::pco::detail::coroutine_handle<> handle)
        {
            subscribe([handle]() mutable { handle.resume(); });
        }
#endif

        /**
         * @brief Waits for completion and consumes the stored result.
         * @return expected value or error. Returns no_state for invalid handles.
         */
        result_type get_result()
        {
            if (!state_)
            {
                return tools::unexpected<E>(E::no_state);
            }

            wait();
            return take_ready_result();
        }

        /**
         * @brief Consumes the stored result without waiting.
         * @return expected value or error. Returns no_state for invalid handles.
         * @note Intended for internal use once readiness is already guaranteed.
         */
        result_type take_ready_result()
        {
            if (!state_)
            {
                return tools::unexpected<E>(E::no_state);
            }

            result_type out = tools::unexpected<E>(E::no_state);
            {
                std::lock_guard<tools::critical_section> guard(state_->mutex_);
                out = std::move(state_->result_).value_or(result_type { tools::unexpected<E>(E::no_state) });
                state_->result_.reset();
            } // mutex released before shared_ptr drops its ref, preventing use-after-free
            state_.reset();
            return out;
        }

        /**
         * @brief Runs a continuation on successful completion.
         * @tparam F Continuation callable type.
         * @param function_arg Continuation called with T (or no arg for void T).
         * @return A new future containing the continuation result.
         */
        template <typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>, E> then_value(
            F&& function_arg) &&
        {
            using next_value_t = detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>;
            using next_raw_t = typename detail::result_then_value_type<F, T>::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
                next_promise.set_error(E::no_state);
                return next_future;
            }

            /**
             * @brief Shared continuation context for `then_value` chaining.
             */
            struct Ctx
            {
                /** @brief Source future consumed by continuation. */
                future_result self;
                /** @brief Stored continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise fulfilling next future in chain. */
                promise_result<next_value_t, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(
                Ctx { std::move(*this), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    result_type current = ctx->self.take_ready_result();
                    if (!current.has_value())
                    {
                        ctx->promise.set_error(current.error());
                        return;
                    }

#if defined(CPP_EXCEPTIONS_ENABLED)
                    try
                    {
                        if constexpr (std::is_void<T>::value)
                        {
                            if constexpr (detail::is_result_handle<next_raw_t>::value)
                            {
                                detail::resolve_nested_handle(ctx->promise, ctx->fn());
                            }
                            else if constexpr (std::is_void<next_value_t>::value)
                            {
                                ctx->fn();
                                ctx->promise.set_value();
                            }
                            else
                            {
                                ctx->promise.set_value(ctx->fn());
                            }
                        }
                        else
                        {
                            if constexpr (detail::is_result_handle<next_raw_t>::value)
                            {
                                detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current).value()));
                            }
                            else if constexpr (std::is_void<next_value_t>::value)
                            {
                                ctx->fn(std::move(current).value());
                                ctx->promise.set_value();
                            }
                            else
                            {
                                ctx->promise.set_value(ctx->fn(std::move(current).value()));
                            }
                        }
                    }
                    catch (...)
                    {
                        ctx->promise.set_error(E::continuation_failure);
                    }
#else
                    if constexpr (std::is_void<T>::value)
                    {
                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                        {
                            detail::resolve_nested_handle(ctx->promise, ctx->fn());
                        }
                        else if constexpr (std::is_void<next_value_t>::value)
                        {
                            ctx->fn();
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(ctx->fn());
                        }
                    }
                    else
                    {
                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                        {
                            detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current).value()));
                        }
                        else if constexpr (std::is_void<next_value_t>::value)
                        {
                            ctx->fn(std::move(current).value());
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(ctx->fn(std::move(current).value()));
                        }
                    }
#endif
                });

            return next_future;
        }

        /**
         * @brief Runs a continuation only when this future resolves with an error.
         * @tparam F Error continuation callable type.
         * @param function_arg Continuation called with E.
         * @return A new future with recovered or propagated result.
         */
        template <typename F>
        future_result<T, E> then_error(F&& function_arg) &&
        {
            promise_result<T, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
#if defined(CPP_EXCEPTIONS_ENABLED)
                try
                {
                    if constexpr (std::is_void<T>::value)
                    {
                        std::forward<F>(function_arg)(E::no_state);
                        next_promise.set_value();
                    }
                    else
                    {
                        next_promise.set_value(std::forward<F>(function_arg)(E::no_state));
                    }
                }
                catch (...)
                {
                    next_promise.set_error(E::continuation_failure);
                }
#else
                if constexpr (std::is_void<T>::value)
                {
                    std::forward<F>(function_arg)(E::no_state);
                    next_promise.set_value();
                }
                else
                {
                    next_promise.set_value(std::forward<F>(function_arg)(E::no_state));
                }
#endif
                return next_future;
            }

            /**
             * @brief Shared continuation context for `then_error` chaining.
             */
            struct Ctx
            {
                /** @brief Source future consumed by continuation. */
                future_result self;
                /** @brief Stored error-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
                promise_result<T, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(
                Ctx { std::move(*this), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    result_type current = ctx->self.take_ready_result();
                    if (current.has_value())
                    {
                        ctx->promise.set_result(std::move(current));
                        return;
                    }

#if defined(CPP_EXCEPTIONS_ENABLED)
                    try
                    {
                        if constexpr (std::is_void<T>::value)
                        {
                            ctx->fn(current.error());
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(ctx->fn(current.error()));
                        }
                    }
                    catch (...)
                    {
                        ctx->promise.set_error(E::continuation_failure);
                    }
#else
                    if constexpr (std::is_void<T>::value)
                    {
                        ctx->fn(current.error());
                        ctx->promise.set_value();
                    }
                    else
                    {
                        ctx->promise.set_value(ctx->fn(current.error()));
                    }
#endif
                });

            return next_future;
        }

        /**
         * @brief Runs a continuation that receives the whole expected<T,E>.
         * @tparam F Continuation callable type.
         * @param function_arg Continuation called with result_type.
         * @return A new future with transformed or flattened result.
         */
        template <typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, result_type>>, E> then_result(
            F&& function_arg) &&
        {
            using traits_t = detail::result_then_result_type<F, result_type>;
            using next_value_t = detail::unwrapped_result_value_t<traits_t>;
            using next_raw_t = typename traits_t::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
                result_type current = tools::unexpected<E>(E::no_state);
#if defined(CPP_EXCEPTIONS_ENABLED)
                try
                {
                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                    {
                        detail::resolve_nested_handle(next_promise, std::forward<F>(function_arg)(std::move(current)));
                    }
                    else if constexpr (std::is_void<next_value_t>::value)
                    {
                        std::forward<F>(function_arg)(std::move(current));
                        next_promise.set_value();
                    }
                    else
                    {
                        next_promise.set_value(std::forward<F>(function_arg)(std::move(current)));
                    }
                }
                catch (...)
                {
                    next_promise.set_error(E::continuation_failure);
                }
#else
                if constexpr (detail::is_result_handle<next_raw_t>::value)
                {
                    detail::resolve_nested_handle(next_promise, std::forward<F>(function_arg)(std::move(current)));
                }
                else if constexpr (std::is_void<next_value_t>::value)
                {
                    std::forward<F>(function_arg)(std::move(current));
                    next_promise.set_value();
                }
                else
                {
                    next_promise.set_value(std::forward<F>(function_arg)(std::move(current)));
                }
#endif
                return next_future;
            }

            /**
             * @brief Shared continuation context for `then_result` chaining.
             */
            struct Ctx
            {
                /** @brief Source future consumed by continuation. */
                future_result self;
                /** @brief Stored result-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
                promise_result<next_value_t, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(
                Ctx { std::move(*this), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    result_type current = ctx->self.take_ready_result();

#if defined(CPP_EXCEPTIONS_ENABLED)
                    try
                    {
                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                        {
                            detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
                        }
                        else if constexpr (std::is_void<next_value_t>::value)
                        {
                            ctx->fn(std::move(current));
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(ctx->fn(std::move(current)));
                        }
                    }
                    catch (...)
                    {
                        ctx->promise.set_error(E::continuation_failure);
                    }
#else
                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                    {
                        detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
                    }
                    else if constexpr (std::is_void<next_value_t>::value)
                    {
                        ctx->fn(std::move(current));
                        ctx->promise.set_value();
                    }
                    else
                    {
                        ctx->promise.set_value(ctx->fn(std::move(current)));
                    }
#endif
                });

            return next_future;
        }

        /**
         * @brief Executor-aware continuation that receives promise and source future.
         * @tparam F Continuation callable type.
         * @param function_arg Continuation invoked on the inline executor.
         * @return Future produced by the continuation contract.
         */
        template <typename F>
        future_result<detail::interrupt_promise_arg_t<F, future_result<T, E>, E>, E> then(F&& function_arg) &&
        {
            return std::move(*this).then(inplace_executor, std::forward<F>(function_arg));
        }

        /**
         * @brief Executor-aware continuation that receives promise and source future.
         * @tparam Exec Executor type satisfying is_executor.
         * @tparam F Continuation callable type.
         * @param exec Executor used to schedule continuation execution.
         * @param function_arg Continuation callable.
         * @return Future produced by the continuation contract.
         */
        template <typename Exec, typename F>
        future_result<detail::interrupt_promise_arg_t<F, future_result<T, E>, E>, E> then(
            Exec&& exec, F&& function_arg) &&
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using next_value_t = detail::interrupt_promise_arg_t<F, future_result<T, E>, E>;
            using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                std::reference_wrapper<std::remove_reference_t<Exec>>, std::decay_t<Exec>>;

            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
                next_promise.set_error(E::no_state);
                return next_future;
            }

            /**
             * @brief Shared continuation context for executor-aware `then` chaining.
             */
            struct Ctx
            {
                /** @brief Source future consumed by continuation. */
                future_result self;
                /** @brief Stored executor object/reference. */
                exec_store_t exec;
                /** @brief Stored continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
                promise_result<next_value_t, E> promise;
            };

            auto stored_exec = [&]() -> exec_store_t
            {
                if constexpr (std::is_lvalue_reference<Exec>::value)
                {
                    return std::ref(exec);
                }
                else
                {
                    return std::forward<Exec>(exec);
                }
            }();

            auto ctx = std::make_shared<Ctx>(Ctx {
                std::move(*this), std::move(stored_exec), std::forward<F>(function_arg), std::move(next_promise) });

            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    if constexpr (std::is_lvalue_reference<Exec>::value)
                    {
                        post(ctx->exec.get(),
                            [ctx]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
                                std::invoke(ctx->fn, std::move(ctx->promise), std::move(ctx->self));
                            });
                    }
                    else
                    {
                        post(ctx->exec,
                            [ctx]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
                                std::invoke(ctx->fn, std::move(ctx->promise), std::move(ctx->self));
                            });
                    }
                });

            return next_future;
        }

        /**
         * @brief Alias of then_value for v1 compatibility naming.
         * @tparam F Continuation callable type.
         * @param function_arg Continuation callable.
         * @return Same result as then_value(function_arg).
         */
        template <typename F>
        auto next(F&& function_arg) && -> decltype(std::move(*this).then_value(std::forward<F>(function_arg)))
        {
            return std::move(*this).then_value(std::forward<F>(function_arg));
        }

        /**
         * @brief Executor-aware then_value overload.
         * @tparam Exec Executor type satisfying is_executor.
         * @tparam F Continuation callable type.
         * @param exec Executor used to schedule continuation execution.
         * @param function_arg Continuation callable.
         * @return Future with transformed continuation value.
         */
        template <typename Exec, typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>, E> then_value(
            Exec&& exec, F&& function_arg) &&
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using next_value_t = detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>;
            using next_raw_t = typename detail::result_then_value_type<F, T>::raw_type;
            using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                std::reference_wrapper<std::remove_reference_t<Exec>>, std::decay_t<Exec>>;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
                next_promise.set_error(E::no_state);
                return next_future;
            }

            /**
             * @brief Shared continuation context for executor-aware `then_value` chaining.
             */
            struct Ctx
            {
                /** @brief Source future consumed by continuation. */
                future_result self;
                /** @brief Stored executor object/reference. */
                exec_store_t exec;
                /** @brief Stored value-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
                promise_result<next_value_t, E> promise;
            };

            auto stored_exec = [&]() -> exec_store_t
            {
                if constexpr (std::is_lvalue_reference<Exec>::value)
                {
                    return std::ref(exec);
                }
                else
                {
                    return std::forward<Exec>(exec);
                }
            }();

            auto ctx = std::make_shared<Ctx>(Ctx {
                std::move(*this), std::move(stored_exec), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    result_type current = ctx->self.take_ready_result();
                    if (!current.has_value())
                    {
                        ctx->promise.set_error(current.error());
                        return;
                    }

                    if constexpr (std::is_lvalue_reference<Exec>::value)
                    {
                        post(ctx->exec.get(),
                            [ctx, current = std::move(current)]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
#if defined(CPP_EXCEPTIONS_ENABLED)
                                try
                                {
                                    if constexpr (std::is_void<T>::value)
                                    {
                                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                                        {
                                            detail::resolve_nested_handle(ctx->promise, ctx->fn());
                                        }
                                        else if constexpr (std::is_void<next_value_t>::value)
                                        {
                                            ctx->fn();
                                            ctx->promise.set_value();
                                        }
                                        else
                                        {
                                            ctx->promise.set_value(ctx->fn());
                                        }
                                    }
                                    else
                                    {
                                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                                        {
                                            detail::resolve_nested_handle(
                                                ctx->promise, ctx->fn(std::move(current).value()));
                                        }
                                        else if constexpr (std::is_void<next_value_t>::value)
                                        {
                                            ctx->fn(std::move(current).value());
                                            ctx->promise.set_value();
                                        }
                                        else
                                        {
                                            ctx->promise.set_value(ctx->fn(std::move(current).value()));
                                        }
                                    }
                                }
                                catch (...)
                                {
                                    ctx->promise.set_error(E::continuation_failure);
                                }
#else
                                if constexpr (std::is_void<T>::value)
                                {
                                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                                    {
                                        detail::resolve_nested_handle(ctx->promise, ctx->fn());
                                    }
                                    else if constexpr (std::is_void<next_value_t>::value)
                                    {
                                        ctx->fn();
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn());
                                    }
                                }
                                else
                                {
                                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                                    {
                                        detail::resolve_nested_handle(
                                            ctx->promise, ctx->fn(std::move(current).value()));
                                    }
                                    else if constexpr (std::is_void<next_value_t>::value)
                                    {
                                        ctx->fn(std::move(current).value());
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn(std::move(current).value()));
                                    }
                                }
#endif
                            });
                    }
                    else
                    {
                        post(ctx->exec,
                            [ctx, current = std::move(current)]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
#if defined(CPP_EXCEPTIONS_ENABLED)
                                try
                                {
                                    if constexpr (std::is_void<T>::value)
                                    {
                                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                                        {
                                            detail::resolve_nested_handle(ctx->promise, ctx->fn());
                                        }
                                        else if constexpr (std::is_void<next_value_t>::value)
                                        {
                                            ctx->fn();
                                            ctx->promise.set_value();
                                        }
                                        else
                                        {
                                            ctx->promise.set_value(ctx->fn());
                                        }
                                    }
                                    else
                                    {
                                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                                        {
                                            detail::resolve_nested_handle(
                                                ctx->promise, ctx->fn(std::move(current).value()));
                                        }
                                        else if constexpr (std::is_void<next_value_t>::value)
                                        {
                                            ctx->fn(std::move(current).value());
                                            ctx->promise.set_value();
                                        }
                                        else
                                        {
                                            ctx->promise.set_value(ctx->fn(std::move(current).value()));
                                        }
                                    }
                                }
                                catch (...)
                                {
                                    ctx->promise.set_error(E::continuation_failure);
                                }
#else
                                if constexpr (std::is_void<T>::value)
                                {
                                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                                    {
                                        detail::resolve_nested_handle(ctx->promise, ctx->fn());
                                    }
                                    else if constexpr (std::is_void<next_value_t>::value)
                                    {
                                        ctx->fn();
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn());
                                    }
                                }
                                else
                                {
                                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                                    {
                                        detail::resolve_nested_handle(
                                            ctx->promise, ctx->fn(std::move(current).value()));
                                    }
                                    else if constexpr (std::is_void<next_value_t>::value)
                                    {
                                        ctx->fn(std::move(current).value());
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn(std::move(current).value()));
                                    }
                                }
#endif
                            });
                    }
                });

            return next_future;
        }

        /**
         * @brief Executor-aware alias of then_value.
         * @tparam Exec Executor type.
         * @tparam F Continuation callable type.
         * @param exec Executor used to schedule continuation execution.
         * @param function_arg Continuation callable.
         * @return Same result as then_value(exec, function_arg).
         */
        template <typename Exec, typename F>
        auto next(Exec&& exec, F&& function_arg) && -> decltype(std::move(*this).then_value(
                                                        std::forward<Exec>(exec), std::forward<F>(function_arg)))
        {
            return std::move(*this).then_value(std::forward<Exec>(exec), std::forward<F>(function_arg));
        }

        /**
         * @brief Executor-aware then_error overload.
         * @tparam Exec Executor type satisfying is_executor.
         * @tparam F Error continuation callable type.
         * @param exec Executor used to schedule continuation execution.
         * @param function_arg Error continuation callable.
         * @return Future with recovered or propagated result.
         */
        template <typename Exec, typename F>
        future_result<T, E> then_error(Exec&& exec, F&& function_arg) &&
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                std::reference_wrapper<std::remove_reference_t<Exec>>, std::decay_t<Exec>>;

            promise_result<T, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
                post(std::forward<Exec>(exec),
                    [function_arg = std::forward<F>(function_arg), promise = std::move(next_promise)]() mutable
                    {
                        if (!promise.is_awaiten())
                        {
                            return;
                        }
#if defined(CPP_EXCEPTIONS_ENABLED)
                        try
                        {
                            if constexpr (std::is_void<T>::value)
                            {
                                function_arg(E::no_state);
                                promise.set_value();
                            }
                            else
                            {
                                promise.set_value(function_arg(E::no_state));
                            }
                        }
                        catch (...)
                        {
                            promise.set_error(E::continuation_failure);
                        }
#else
                        if constexpr (std::is_void<T>::value)
                        {
                            function_arg(E::no_state);
                            promise.set_value();
                        }
                        else
                        {
                            promise.set_value(function_arg(E::no_state));
                        }
#endif
                    });
                return next_future;
            }

            /**
             * @brief Shared continuation context for executor-aware `then_error` chaining.
             */
            struct Ctx
            {
                /** @brief Source future consumed by continuation. */
                future_result self;
                /** @brief Stored executor object/reference. */
                exec_store_t exec;
                /** @brief Stored error-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
                promise_result<T, E> promise;
            };

            auto stored_exec = [&]() -> exec_store_t
            {
                if constexpr (std::is_lvalue_reference<Exec>::value)
                {
                    return std::ref(exec);
                }
                else
                {
                    return std::forward<Exec>(exec);
                }
            }();

            auto ctx = std::make_shared<Ctx>(Ctx {
                std::move(*this), std::move(stored_exec), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    result_type current = ctx->self.take_ready_result();
                    if (current.has_value())
                    {
                        ctx->promise.set_result(std::move(current));
                        return;
                    }

                    if constexpr (std::is_lvalue_reference<Exec>::value)
                    {
                        post(ctx->exec.get(),
                            [ctx, error = current.error()]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
#if defined(CPP_EXCEPTIONS_ENABLED)
                                try
                                {
                                    if constexpr (std::is_void<T>::value)
                                    {
                                        ctx->fn(error);
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn(error));
                                    }
                                }
                                catch (...)
                                {
                                    ctx->promise.set_error(E::continuation_failure);
                                }
#else
                                if constexpr (std::is_void<T>::value)
                                {
                                    ctx->fn(error);
                                    ctx->promise.set_value();
                                }
                                else
                                {
                                    ctx->promise.set_value(ctx->fn(error));
                                }
#endif
                            });
                    }
                    else
                    {
                        post(ctx->exec,
                            [ctx, error = current.error()]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
#if defined(CPP_EXCEPTIONS_ENABLED)
                                try
                                {
                                    if constexpr (std::is_void<T>::value)
                                    {
                                        ctx->fn(error);
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn(error));
                                    }
                                }
                                catch (...)
                                {
                                    ctx->promise.set_error(E::continuation_failure);
                                }
#else
                                if constexpr (std::is_void<T>::value)
                                {
                                    ctx->fn(error);
                                    ctx->promise.set_value();
                                }
                                else
                                {
                                    ctx->promise.set_value(ctx->fn(error));
                                }
#endif
                            });
                    }
                });

            return next_future;
        }

        /**
         * @brief Executor-aware then_result overload.
         * @tparam Exec Executor type satisfying is_executor.
         * @tparam F Continuation callable type.
         * @param exec Executor used to schedule continuation execution.
         * @param function_arg Continuation receiving result_type.
         * @return Future with transformed or flattened result.
         */
        template <typename Exec, typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, result_type>>, E> then_result(
            Exec&& exec, F&& function_arg) &&
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using traits_t = detail::result_then_result_type<F, result_type>;
            using next_value_t = detail::unwrapped_result_value_t<traits_t>;
            using next_raw_t = typename traits_t::raw_type;
            using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                std::reference_wrapper<std::remove_reference_t<Exec>>, std::decay_t<Exec>>;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
                result_type current = tools::unexpected<E>(E::no_state);
                post(std::forward<Exec>(exec),
                    [current = std::move(current), function_arg = std::forward<F>(function_arg),
                        promise = std::move(next_promise)]() mutable
                    {
                        if (!promise.is_awaiten())
                        {
                            return;
                        }
#if defined(CPP_EXCEPTIONS_ENABLED)
                        try
                        {
                            if constexpr (detail::is_result_handle<next_raw_t>::value)
                            {
                                detail::resolve_nested_handle(promise, function_arg(std::move(current)));
                            }
                            else if constexpr (std::is_void<next_value_t>::value)
                            {
                                function_arg(std::move(current));
                                promise.set_value();
                            }
                            else
                            {
                                promise.set_value(function_arg(std::move(current)));
                            }
                        }
                        catch (...)
                        {
                            promise.set_error(E::continuation_failure);
                        }
#else
                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                        {
                            detail::resolve_nested_handle(promise, function_arg(std::move(current)));
                        }
                        else if constexpr (std::is_void<next_value_t>::value)
                        {
                            function_arg(std::move(current));
                            promise.set_value();
                        }
                        else
                        {
                            promise.set_value(function_arg(std::move(current)));
                        }
#endif
                    });
                return next_future;
            }

            /**
             * @brief Shared continuation context for executor-aware `then_result` chaining.
             */
            struct Ctx
            {
                /** @brief Source future consumed by continuation. */
                future_result self;
                /** @brief Stored executor object/reference. */
                exec_store_t exec;
                /** @brief Stored result-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
                promise_result<next_value_t, E> promise;
            };

            auto stored_exec = [&]() -> exec_store_t
            {
                if constexpr (std::is_lvalue_reference<Exec>::value)
                {
                    return std::ref(exec);
                }
                else
                {
                    return std::forward<Exec>(exec);
                }
            }();

            auto ctx = std::make_shared<Ctx>(Ctx {
                std::move(*this), std::move(stored_exec), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    result_type current = ctx->self.take_ready_result();
                    if constexpr (std::is_lvalue_reference<Exec>::value)
                    {
                        post(ctx->exec.get(),
                            [ctx, current = std::move(current)]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
#if defined(CPP_EXCEPTIONS_ENABLED)
                                try
                                {
                                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                                    {
                                        detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
                                    }
                                    else if constexpr (std::is_void<next_value_t>::value)
                                    {
                                        ctx->fn(std::move(current));
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn(std::move(current)));
                                    }
                                }
                                catch (...)
                                {
                                    ctx->promise.set_error(E::continuation_failure);
                                }
#else
                                if constexpr (detail::is_result_handle<next_raw_t>::value)
                                {
                                    detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
                                }
                                else if constexpr (std::is_void<next_value_t>::value)
                                {
                                    ctx->fn(std::move(current));
                                    ctx->promise.set_value();
                                }
                                else
                                {
                                    ctx->promise.set_value(ctx->fn(std::move(current)));
                                }
#endif
                            });
                    }
                    else
                    {
                        post(ctx->exec,
                            [ctx, current = std::move(current)]() mutable
                            {
                                if (!ctx->promise.is_awaiten())
                                {
                                    return;
                                }
#if defined(CPP_EXCEPTIONS_ENABLED)
                                try
                                {
                                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                                    {
                                        detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
                                    }
                                    else if constexpr (std::is_void<next_value_t>::value)
                                    {
                                        ctx->fn(std::move(current));
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn(std::move(current)));
                                    }
                                }
                                catch (...)
                                {
                                    ctx->promise.set_error(E::continuation_failure);
                                }
#else
                                if constexpr (detail::is_result_handle<next_raw_t>::value)
                                {
                                    detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
                                }
                                else if constexpr (std::is_void<next_value_t>::value)
                                {
                                    ctx->fn(std::move(current));
                                    ctx->promise.set_value();
                                }
                                else
                                {
                                    ctx->promise.set_value(ctx->fn(std::move(current)));
                                }
#endif
                            });
                    }
                });

            return next_future;
        }

        /**
         * @brief Registers a callback invoked once this future becomes ready.
         * @param callback Callback function.
         * @note If already ready, callback is invoked inline after unlocking.
         */
        void subscribe(std::function<void()> callback)
        {
            if (!state_)
            {
                return;
            }
            std::unique_lock<tools::critical_section> lock(state_->mutex_);
            if (state_->ready_)
            {
                lock.unlock();
                callback();
                return;
            }
            state_->on_ready_cbs_.push_back(std::move(callback));
        }

        /**
         * @brief Registers a lightweight notification callback on readiness.
         * @tparam F Callback type.
         * @param notification Callback callable.
         */
        template <typename F>
        void notify(F&& notification)
        {
            auto callback = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
            subscribe([callback]() mutable { (*callback)(); });
        }

        /**
         * @brief Registers notification callback dispatched through an executor.
         * @tparam Exec Executor type satisfying is_executor.
         * @tparam F Callback type.
         * @param exec Executor used to schedule callback execution.
         * @param notification Callback callable.
         */
        template <typename Exec, typename F>
        void notify(Exec&& exec, F&& notification)
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            auto exec_obj = std::make_shared<std::decay_t<Exec>>(std::forward<Exec>(exec));
            auto callback = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
            subscribe([exec_obj, callback]() mutable { post(*exec_obj, [callback]() mutable { (*callback)(); }); });
        }

        /**
         * @brief Transfers this handle while keeping pending state alive until readiness.
         * @return Moved future_result owning the same shared state.
         */
        future_result detach()
        {
            if (state_)
            {
                auto keep_alive = state_;
                std::unique_lock<tools::critical_section> lock(state_->mutex_);
                if (!state_->ready_)
                {
                    state_->on_ready_cbs_.push_back([keep_alive]() mutable {});
                }
            }
            return std::move(*this);
        }

        /**
         * @brief Converts this move-only future into a copyable shared_result.
         * @return Shared handle referencing the same state.
         */
        shared_result<T, E> share() &&
        {
            return shared_result<T, E>(std::exchange(state_, nullptr));
        }

    private:
        detail::result_state_ptr<T, E> state_;
    };

} // namespace pco
