/**
 * @file result_future/future.hpp
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

#include "promise.hpp"

namespace pco
{
    // Forward declaration for share()
    template <typename T, typename E>
    class shared_result;

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

#if defined(PC_HAS_COROUTINES)
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

        [[nodiscard]] bool valid() const noexcept
        {
            return static_cast<bool>(state_);
        }

        void wait() const
        {
            if (!state_)
            {
                return;
            }

            std::unique_lock<tools::critical_section> lock(state_->mutex_);
            state_->cv_.wait(lock, [this] { return state_->ready_; });
        }

        template <typename Rep, typename Period>
        [[nodiscard]] future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) const
        {
            return wait_until(std::chrono::steady_clock::now() + rel_time);
        }

        template <typename Clock, typename Duration>
        [[nodiscard]] future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const
        {
            return detail::wait_until_ready(state_, abs_time);
        }

        [[nodiscard]] bool is_ready() const
        {
            if (!state_)
            {
                return false;
            }
            std::lock_guard<tools::critical_section> guard(state_->mutex_);
            return state_->ready_;
        }

#if defined(PC_HAS_COROUTINES)
        [[nodiscard]] bool await_ready() const noexcept
        {
            return !state_ || is_ready();
        }

        auto await_resume()
        {
            auto result = get_result();
            if (!result.has_value())
            {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
                throw std::runtime_error("future_result await_resume failed");
#else
                std::terminate();
#endif
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

        result_type get_result()
        {
            if (!state_)
            {
                return tools::unexpected<E>(E::no_state);
            }

            wait();
            return take_ready_result();
        }

        // Internal helper for combinators that are invoked only after readiness.
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

            struct Ctx
            {
                future_result self;
                std::decay_t<F> fn;
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

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

        template <typename F>
        future_result<T, E> then_error(F&& function_arg) &&
        {
            promise_result<T, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

            struct Ctx
            {
                future_result self;
                std::decay_t<F> fn;
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

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

            struct Ctx
            {
                future_result self;
                std::decay_t<F> fn;
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

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

        template <typename F>
        future_result<detail::interrupt_promise_arg_t<F, future_result<T, E>, E>, E> then(F&& function_arg) &&
        {
            return std::move(*this).then(inplace_executor, std::forward<F>(function_arg));
        }

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

            struct Ctx
            {
                future_result self;
                exec_store_t exec;
                std::decay_t<F> fn;
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

        template <typename F>
        auto next(F&& function_arg) && -> decltype(std::move(*this).then_value(std::forward<F>(function_arg)))
        {
            return std::move(*this).then_value(std::forward<F>(function_arg));
        }

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

            struct Ctx
            {
                future_result self;
                exec_store_t exec;
                std::decay_t<F> fn;
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

        template <typename Exec, typename F>
        auto next(Exec&& exec, F&& function_arg) && -> decltype(std::move(*this).then_value(
                                                        std::forward<Exec>(exec), std::forward<F>(function_arg)))
        {
            return std::move(*this).then_value(std::forward<Exec>(exec), std::forward<F>(function_arg));
        }

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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

            struct Ctx
            {
                future_result self;
                exec_store_t exec;
                std::decay_t<F> fn;
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

            struct Ctx
            {
                future_result self;
                exec_store_t exec;
                std::decay_t<F> fn;
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
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

        // Register a callback invoked when this future becomes ready.
        // If already ready the callback is invoked inline (lock released first).
        // Silently dropped if the future has no state.
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

        template <typename F>
        void notify(F&& notification)
        {
            auto callback = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
            subscribe([callback]() mutable { (*callback)(); });
        }

        template <typename Exec, typename F>
        void notify(Exec&& exec, F&& notification)
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            auto exec_obj = std::make_shared<std::decay_t<Exec>>(std::forward<Exec>(exec));
            auto callback = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
            subscribe([exec_obj, callback]() mutable { post(*exec_obj, [callback]() mutable { (*callback)(); }); });
        }

        // Keeps the shared state alive until readiness and transfers this handle
        // out, leaving the source invalid.
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

        shared_result<T, E> share() &&
        {
            return shared_result<T, E>(std::exchange(state_, nullptr));
        }

    private:
        detail::result_state_ptr<T, E> state_;
    };

} // namespace pco
