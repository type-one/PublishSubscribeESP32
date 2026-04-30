/**
 * @file shared.hpp
 * @brief shared_result handle for shared async values.
 * @author Laurent Lardinois (with the help of Github Copilot), Sergey Vidyuk
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
#include "result_shared_state_impl.hpp"

namespace pco
{
    /**
     * @brief Copyable asynchronous result handle.
     * @tparam T Value type delivered on success. Use void for no payload.
     * @tparam E Error enum type used on failure.
     *
     * Unlike future_result, shared_result can be copied and queried repeatedly.
     */
    template <typename T, typename E>
    class shared_result
    {
        static_assert(!std::is_reference_v<T>,
            "portable_concurrency does not support reference return types. "
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
            std::scoped_lock<tools::critical_section> guard(state_->mutex_);
            return state_->ready_;
        }

        /**
         * @brief Waits and returns a stable reference to the stored expected result.
         * @return Reference to the shared expected payload.
         */
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
            std::scoped_lock<tools::critical_section> guard(state_->mutex_);
            return *state_->result_;
        }

        /**
         * @brief Registers a callback invoked once this shared_result becomes ready.
         * @param callback Callback function.
         */
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

        /**
         * @brief Registers a lightweight notification callback on readiness.
         * @tparam F Callback type.
         * @param notification Callback callable.
         */
        template <typename F>
        void notify(F&& notification) const
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
        void notify(Exec&& exec, F&& notification) const
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            auto exec_obj = std::make_shared<std::decay_t<Exec>>(std::forward<Exec>(exec));
            auto callback = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
            subscribe([exec_obj, callback]() mutable { post(*exec_obj, [callback]() mutable { (*callback)(); }); });
        }

        /**
         * @brief Transfers this handle while keeping pending state alive until readiness.
         * @return Moved shared_result owning the same shared state.
         */
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

        /**
         * @brief Runs a continuation on successful completion.
         * @tparam F Continuation callable type.
         * @param function_arg Continuation called with T (or no arg for void T).
         * @return A new future containing the continuation result.
         */
        template <typename F>
        future_result<detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>, E> then_value(
            F&& function_arg) const
        {
            using next_value_t = detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>;
            using next_raw_t = typename detail::result_shared_then_value_type<F, T>::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            /**
             * @brief Shared continuation context for `shared_result::then_value` chaining.
             */
            struct Ctx
            {
                /** @brief Source shared_result observed by continuation. */
                shared_result self;
                /** @brief Stored continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise fulfilling next future in chain. */
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
                });

            return next_future;
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
        future_result<detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>, E> then_value(
            Exec&& exec, F&& function_arg) const
        {
            static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

            using next_value_t = detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>;
            using next_raw_t = typename detail::result_shared_then_value_type<F, T>::raw_type;
            promise_result<next_value_t, E> next_promise;
            auto next_future = next_promise.get_future();

            /**
             * @brief Shared continuation context for executor-aware `then_value` chaining.
             */
            struct Ctx
            {
                /** @brief Source shared_result observed by continuation. */
                shared_result self;
                /** @brief Stored executor object. */
                std::decay_t<Exec> exec;
                /** @brief Stored value-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
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
                        });
                });

            return next_future;
        }

        /**
         * @brief Executor-aware continuation that receives promise and source shared_result.
         * @tparam F Continuation callable type.
         * @param function_arg Continuation invoked on the inline executor.
         * @return Future produced by the continuation contract.
         */
        template <typename F>
        future_result<detail::interrupt_promise_arg_t<F, shared_result<T, E>, E>, E> then(F&& function_arg) const
        {
            return this->then(inplace_executor, std::forward<F>(function_arg));
        }

        /**
         * @brief Executor-aware continuation that receives promise and source shared_result.
         * @tparam Exec Executor type satisfying is_executor.
         * @tparam F Continuation callable type.
         * @param exec Executor used to schedule continuation execution.
         * @param function_arg Continuation callable.
         * @return Future produced by the continuation contract.
         */
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

            /**
             * @brief Shared continuation context for executor-aware `then` chaining.
             */
            struct Ctx
            {
                /** @brief Source shared_result observed by continuation. */
                shared_result self;
                /** @brief Stored executor object. */
                std::decay_t<Exec> exec;
                /** @brief Stored continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
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

        /**
         * @brief Alias of then_value.
         * @tparam F Continuation callable type.
         * @param function_arg Continuation callable.
         * @return Same result as then_value(function_arg).
         */
        template <typename F>
        auto next(F&& function_arg) const -> decltype(this->then_value(std::forward<F>(function_arg)))
        {
            return this->then_value(std::forward<F>(function_arg));
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

            /**
             * @brief Shared continuation context for `then_error` chaining on shared_result.
             */
            struct Ctx
            {
                /** @brief Source shared_result observed by continuation. */
                shared_result self;
                /** @brief Stored error-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
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

                    if constexpr (std::is_void<T>::value)
                    {
                        ctx->fn(current.error());
                        ctx->promise.set_value();
                    }
                    else
                    {
                        ctx->promise.set_value(ctx->fn(current.error()));
                    }
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

            /**
             * @brief Shared continuation context for executor-aware `then_error` chaining.
             */
            struct Ctx
            {
                /** @brief Source shared_result observed by continuation. */
                shared_result self;
                /** @brief Stored executor object. */
                std::decay_t<Exec> exec;
                /** @brief Stored error-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
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
                            if constexpr (std::is_void<T>::value)
                            {
                                ctx->fn(ready.error());
                                ctx->promise.set_value();
                            }
                            else
                            {
                                ctx->promise.set_value(ctx->fn(ready.error()));
                            }
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

            /**
             * @brief Shared continuation context for `then_result` chaining on shared_result.
             */
            struct Ctx
            {
                /** @brief Source shared_result observed by continuation. */
                shared_result self;
                /** @brief Stored result-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
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

            /**
             * @brief Shared continuation context for executor-aware `then_result` chaining.
             */
            struct Ctx
            {
                /** @brief Source shared_result observed by continuation. */
                shared_result self;
                /** @brief Stored executor object. */
                std::decay_t<Exec> exec;
                /** @brief Stored result-continuation callable. */
                std::decay_t<F> fn;
                /** @brief Promise completing the chained future. */
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
                        });
                });

            return next_future;
        }

    private:
        detail::result_state_ptr<T, E> state_;
        mutable std::unique_ptr<result_type> invalid_result_;
    };

    /**
     * @brief Creates a promise/future pair sharing the same state.
     * @tparam T Value type delivered on success.
     * @tparam E Error enum type.
     * @return Pair of {promise_result, future_result}.
     */
    template <typename T, typename E = result_error>
    std::pair<promise_result<T, E>, future_result<T, E>> make_result_promise()
    {
        promise_result<T, E> promise;
        auto future = promise.get_future();
        return { std::move(promise), std::move(future) };
    }

    /**
     * @brief Creates a promise/future pair with cancellation callback support.
     * @tparam T Value type delivered on success.
     * @tparam E Error enum type.
     * @tparam F Cancellation callback type.
     * @param tag Cancellation-tag selector.
     * @param cancel_action Callback invoked when abandoned before readiness.
     * @return Pair of {promise_result, future_result}.
     */
    template <typename T, typename E = result_error, typename F>
    std::pair<promise_result<T, E>, future_result<T, E>> make_result_promise(canceler_arg_t tag, F&& cancel_action)
    {
        promise_result<T, E> promise(tag, std::forward<F>(cancel_action));
        auto future = promise.get_future();
        return { std::move(promise), std::move(future) };
    }

} // namespace pco
