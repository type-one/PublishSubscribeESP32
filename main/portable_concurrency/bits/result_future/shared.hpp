/**
 * @file result_future/shared.hpp
 * @brief shared_result handle for shared async values.
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

#include "result_shared_state_impl.hpp"
#include "promise.hpp"

namespace pco
{
    template <typename T, typename E>
    class shared_result
    {
        static_assert(!std::is_reference_v<T>,
            "portable_concurrency v2 does not support reference return types. "
            "Use pointers or std::reference_wrapper<T> instead. "
            "This design constraint avoids move-semantics ambiguity and lifetime issues.");

    public:
        using value_type = T;
        using error_type = E;
        using result_type = tools::expected<T, E>;

        shared_result() = default;

        explicit shared_result(detail::result_state_ptr<T, E> state)
            : state_(std::move(state))
        {
        }

        shared_result(const shared_result& other)
            : state_(other.state_)
        {
        }

        shared_result& operator=(const shared_result& other)
        {
            if (this != &other)
            {
                state_ = other.state_;
                invalid_result_.reset();
            }
            return *this;
        }

        shared_result(shared_result&&) noexcept = default;
        shared_result& operator=(shared_result&&) noexcept = default;
        ~shared_result() = default;

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

        const result_type& get_result() const
        {
            if (!state_)
            {
                if (!invalid_result_)
                {
                    invalid_result_ = std::make_unique<result_type>(tools::unexpected<E>(E::no_state));
                }
                return *invalid_result_;
            }

            wait();
            std::lock_guard<tools::critical_section> guard(state_->mutex_);
            return *state_->result_;
        }

        void subscribe(std::function<void()> callback) const
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
        void notify(F&& notification) const
        {
            auto callback = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
            subscribe([callback]() mutable { (*callback)(); });
        }

        template <typename Exec, typename F>
        void notify(Exec&& exec, F&& notification) const
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            auto exec_obj = std::make_shared<std::decay_t<Exec>>(std::forward<Exec>(exec));
            auto callback = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
            subscribe([exec_obj, callback]() mutable { post(*exec_obj, [callback]() mutable { (*callback)(); }); });
        }

        // Keeps the shared state alive until readiness and transfers this handle
        // out, leaving the source invalid.
        shared_result detach()
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

        template <typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>, E> then_value(
            F&& function_arg) const
        {
            using next_value_t = detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>;
            using next_raw_t = typename detail::result_shared_then_value_type<F, T>::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            struct Ctx
            {
                shared_result self;
                std::decay_t<F> fn;
                promise_result<next_value_t, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(Ctx { *this, std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }
                    const auto& current = ctx->self.get_result();
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
                                detail::resolve_nested_handle(ctx->promise, ctx->fn(current.value()));
                            }
                            else if constexpr (std::is_void<next_value_t>::value)
                            {
                                ctx->fn(current.value());
                                ctx->promise.set_value();
                            }
                            else
                            {
                                ctx->promise.set_value(ctx->fn(current.value()));
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
                            detail::resolve_nested_handle(ctx->promise, ctx->fn(current.value()));
                        }
                        else if constexpr (std::is_void<next_value_t>::value)
                        {
                            ctx->fn(current.value());
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(ctx->fn(current.value()));
                        }
                    }
#endif
                });

            return next_future;
        }

        template <typename Exec, typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>, E> then_value(
            Exec&& exec, F&& function_arg) const
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using next_value_t = detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>;
            using next_raw_t = typename detail::result_shared_then_value_type<F, T>::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            struct Ctx
            {
                shared_result self;
                std::decay_t<Exec> exec;
                std::decay_t<F> fn;
                promise_result<next_value_t, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(
                Ctx { *this, std::forward<Exec>(exec), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }
                    const auto& current = ctx->self.get_result();
                    if (!current.has_value())
                    {
                        ctx->promise.set_error(current.error());
                        return;
                    }

                    post(ctx->exec,
                        [ctx]() mutable
                        {
                            if (!ctx->promise.is_awaiten())
                            {
                                return;
                            }
                            const auto& ready = ctx->self.get_result();
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
                                        detail::resolve_nested_handle(ctx->promise, ctx->fn(ready.value()));
                                    }
                                    else if constexpr (std::is_void<next_value_t>::value)
                                    {
                                        ctx->fn(ready.value());
                                        ctx->promise.set_value();
                                    }
                                    else
                                    {
                                        ctx->promise.set_value(ctx->fn(ready.value()));
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
                                    detail::resolve_nested_handle(ctx->promise, ctx->fn(ready.value()));
                                }
                                else if constexpr (std::is_void<next_value_t>::value)
                                {
                                    ctx->fn(ready.value());
                                    ctx->promise.set_value();
                                }
                                else
                                {
                                    ctx->promise.set_value(ctx->fn(ready.value()));
                                }
                            }
#endif
                        });
                });

            return next_future;
        }

        template <typename F>
        future_result<detail::interrupt_promise_arg_t<F, shared_result<T, E>, E>, E> then(F&& function_arg) const
        {
            return this->then(inplace_executor, std::forward<F>(function_arg));
        }

        template <typename Exec, typename F>
        future_result<detail::interrupt_promise_arg_t<F, shared_result<T, E>, E>, E> then(
            Exec&& exec, F&& function_arg) const
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using next_value_t = detail::interrupt_promise_arg_t<F, shared_result<T, E>, E>;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            if (!state_)
            {
                next_promise.set_error(E::no_state);
                return next_future;
            }

            struct Ctx
            {
                shared_result self;
                std::decay_t<Exec> exec;
                std::decay_t<F> fn;
                promise_result<next_value_t, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(
                Ctx { *this, std::forward<Exec>(exec), std::forward<F>(function_arg), std::move(next_promise) });

            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }

                    post(ctx->exec,
                        [ctx]() mutable
                        {
                            if (!ctx->promise.is_awaiten())
                            {
                                return;
                            }
                            std::invoke(ctx->fn, std::move(ctx->promise), ctx->self);
                        });
                });

            return next_future;
        }

        template <typename F>
        auto next(F&& function_arg) const -> decltype(this->then_value(std::forward<F>(function_arg)))
        {
            return this->then_value(std::forward<F>(function_arg));
        }

        template <typename Exec, typename F>
        auto next(Exec&& exec, F&& function_arg) const -> decltype(this->then_value(
                                                           std::forward<Exec>(exec), std::forward<F>(function_arg)))
        {
            return this->then_value(std::forward<Exec>(exec), std::forward<F>(function_arg));
        }

        template <typename F, typename U = T,
            typename std::enable_if<std::is_void<U>::value || std::is_copy_constructible<U>::value, int>::type = 0>
        future_result<T, E> then_error(F&& function_arg) const
        {
            promise_result<T, E> next_promise;
            auto next_future = next_promise.get_future();

            struct Ctx
            {
                shared_result self;
                std::decay_t<F> fn;
                promise_result<T, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(Ctx { *this, std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }
                    const auto& current = ctx->self.get_result();
                    if (current.has_value())
                    {
                        if constexpr (std::is_void<T>::value)
                        {
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(current.value());
                        }
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

        template <typename Exec, typename F, typename U = T,
            typename std::enable_if<std::is_void<U>::value || std::is_copy_constructible<U>::value, int>::type = 0>
        future_result<T, E> then_error(Exec&& exec, F&& function_arg) const
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            promise_result<T, E> next_promise;
            auto next_future = next_promise.get_future();

            struct Ctx
            {
                shared_result self;
                std::decay_t<Exec> exec;
                std::decay_t<F> fn;
                promise_result<T, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(
                Ctx { *this, std::forward<Exec>(exec), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }
                    const auto& current = ctx->self.get_result();
                    if (current.has_value())
                    {
                        if constexpr (std::is_void<T>::value)
                        {
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(current.value());
                        }
                        return;
                    }

                    post(ctx->exec,
                        [ctx]() mutable
                        {
                            if (!ctx->promise.is_awaiten())
                            {
                                return;
                            }
                            const auto& ready = ctx->self.get_result();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
                            try
                            {
                                if constexpr (std::is_void<T>::value)
                                {
                                    ctx->fn(ready.error());
                                    ctx->promise.set_value();
                                }
                                else
                                {
                                    ctx->promise.set_value(ctx->fn(ready.error()));
                                }
                            }
                            catch (...)
                            {
                                ctx->promise.set_error(E::continuation_failure);
                            }
#else
                            if constexpr (std::is_void<T>::value)
                            {
                                ctx->fn(ready.error());
                                ctx->promise.set_value();
                            }
                            else
                            {
                                ctx->promise.set_value(ctx->fn(ready.error()));
                            }
#endif
                        });
                });

            return next_future;
        }

        template <typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, const result_type&>>, E>
        then_result(F&& function_arg) const
        {
            using traits_t = detail::result_then_result_type<F, const result_type&>;
            using next_value_t = detail::unwrapped_result_value_t<traits_t>;
            using next_raw_t = typename traits_t::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            struct Ctx
            {
                shared_result self;
                std::decay_t<F> fn;
                promise_result<next_value_t, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(Ctx { *this, std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }
                    const auto& current = ctx->self.get_result();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
                    try
                    {
                        if constexpr (detail::is_result_handle<next_raw_t>::value)
                        {
                            detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
                        }
                        else if constexpr (std::is_void<next_value_t>::value)
                        {
                            ctx->fn(current);
                            ctx->promise.set_value();
                        }
                        else
                        {
                            ctx->promise.set_value(ctx->fn(current));
                        }
                    }
                    catch (...)
                    {
                        ctx->promise.set_error(E::continuation_failure);
                    }
#else
                    if constexpr (detail::is_result_handle<next_raw_t>::value)
                    {
                        detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
                    }
                    else if constexpr (std::is_void<next_value_t>::value)
                    {
                        ctx->fn(current);
                        ctx->promise.set_value();
                    }
                    else
                    {
                        ctx->promise.set_value(ctx->fn(current));
                    }
#endif
                });

            return next_future;
        }

        template <typename Exec, typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, const result_type&>>, E>
        then_result(Exec&& exec, F&& function_arg) const
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using traits_t = detail::result_then_result_type<F, const result_type&>;
            using next_value_t = detail::unwrapped_result_value_t<traits_t>;
            using next_raw_t = typename traits_t::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            struct Ctx
            {
                shared_result self;
                std::decay_t<Exec> exec;
                std::decay_t<F> fn;
                promise_result<next_value_t, E> promise;
            };

            auto ctx = std::make_shared<Ctx>(
                Ctx { *this, std::forward<Exec>(exec), std::forward<F>(function_arg), std::move(next_promise) });
            ctx->self.subscribe(
                [ctx]() mutable
                {
                    if (!ctx->promise.is_awaiten())
                    {
                        return;
                    }
                    post(ctx->exec,
                        [ctx]() mutable
                        {
                            if (!ctx->promise.is_awaiten())
                            {
                                return;
                            }
                            const auto& current = ctx->self.get_result();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
                            try
                            {
                                if constexpr (detail::is_result_handle<next_raw_t>::value)
                                {
                                    detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
                                }
                                else if constexpr (std::is_void<next_value_t>::value)
                                {
                                    ctx->fn(current);
                                    ctx->promise.set_value();
                                }
                                else
                                {
                                    ctx->promise.set_value(ctx->fn(current));
                                }
                            }
                            catch (...)
                            {
                                ctx->promise.set_error(E::continuation_failure);
                            }
#else
                            if constexpr (detail::is_result_handle<next_raw_t>::value)
                            {
                                detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
                            }
                            else if constexpr (std::is_void<next_value_t>::value)
                            {
                                ctx->fn(current);
                                ctx->promise.set_value();
                            }
                            else
                            {
                                ctx->promise.set_value(ctx->fn(current));
                            }
#endif
                        });
                });

            return next_future;
        }

    private:
        detail::result_state_ptr<T, E> state_;
        mutable std::unique_ptr<result_type> invalid_result_;
    };

    template <typename T, typename E = result_error>
    std::pair<promise_result<T, E>, future_result<T, E>> make_result_promise()
    {
        promise_result<T, E> promise;
        auto future = promise.get_future();
        return { std::move(promise), std::move(future) };
    }

    template <typename T, typename E = result_error, typename F>
    std::pair<promise_result<T, E>, future_result<T, E>> make_result_promise(canceler_arg_t tag, F&& cancel_action)
    {
        promise_result<T, E> promise(tag, std::forward<F>(cancel_action));
        auto future = promise.get_future();
        return { std::move(promise), std::move(future) };
    }

} // namespace pco
