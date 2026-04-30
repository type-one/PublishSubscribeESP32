/**
 * @file promise.hpp
 * @brief promise_result API and nested handle resolver.
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

#include "types_and_detail.hpp"

namespace pco
{
    /**
     * @brief Producer-side handle for future_result/shared_result state.
     * @tparam T Value type delivered on success. Use void for no payload.
     * @tparam E Error enum type used on failure.
     */
    template <typename T, typename E>
    class promise_result
    {
        static_assert(!std::is_reference_v<T>,
            "portable_concurrency does not support reference return types. "
            "Use pointers or std::reference_wrapper<T> instead. "
            "This design constraint avoids move-semantics ambiguity and lifetime issues.");

    public:
        using value_type = T;
        using error_type = E;

        /**
         * @brief Creates a promise with a fresh shared state.
         */
        promise_result()
            : state_(std::make_shared<detail::result_shared_state<T, E>>())
        {
        }

        /**
         * @brief Creates a promise with cancellation callback support.
         * @tparam F Cancellation callback type.
         * @param tag Cancellation-tag selector.
         * @param cancel_action Callback invoked when abandoned before readiness.
         */
        template <typename F>
        promise_result(canceler_arg_t tag, F&& cancel_action)
            : state_(std::make_shared<detail::result_shared_state<T, E>>(tag, std::forward<F>(cancel_action)))
        {
        }

        explicit promise_result(detail::result_state_ptr<T, E> state)
            : state_(std::move(state))
        {
        }

        promise_result(const promise_result&) = delete;
        promise_result& operator=(const promise_result&) = delete;
        promise_result(promise_result&&) noexcept = default;
        promise_result& operator=(promise_result&& rhs) noexcept
        {
            if (this != &rhs)
            {
                // Use get_state() so we also abandon pending state reachable only through
                // weak_state_ (the common post-get_future() case).
                auto old_state = get_state();
                auto new_state = rhs.state_;

                if (old_state)
                {
                    std::vector<std::function<void()>> callbacks;
                    if (new_state && old_state == new_state)
                    {
                        std::scoped_lock<tools::critical_section> guard(old_state->mutex_);
                        if (!old_state->ready_)
                        {
                            old_state->result_.emplace(tools::unexpected<E>(E::broken_promise));
                            old_state->ready_ = true;
                            callbacks = std::move(old_state->on_ready_cbs_);
                            old_state->cv_.notify_all();
                        }
                    }
                    else if (new_state)
                    {
                        std::scoped_lock lock(old_state->mutex_, new_state->mutex_);
                        if (!old_state->ready_)
                        {
                            old_state->result_.emplace(tools::unexpected<E>(E::broken_promise));
                            old_state->ready_ = true;
                            callbacks = std::move(old_state->on_ready_cbs_);
                            old_state->cv_.notify_all();
                        }
                    }
                    else
                    {
                        std::scoped_lock<tools::critical_section> guard(old_state->mutex_);
                        if (!old_state->ready_)
                        {
                            old_state->result_.emplace(tools::unexpected<E>(E::broken_promise));
                            old_state->ready_ = true;
                            callbacks = std::move(old_state->on_ready_cbs_);
                            old_state->cv_.notify_all();
                        }
                    }
                    for (auto& callback : callbacks)
                    {
                        callback();
                    }
                }

                state_ = std::move(rhs.state_);
                weak_state_ = std::move(rhs.weak_state_);
                future_retrieved_ = rhs.future_retrieved_;
                rhs.future_retrieved_ = false;
            }
            return *this;
        }

        ~promise_result()
        {
            abandon_pending_state();
        }

    private:
        void abandon_pending_state()
        {
            auto state = get_state();
            if (!state)
            {
                return;
            }

            std::vector<std::function<void()>> cbs;
            {
                std::scoped_lock<tools::critical_section> guard(state->mutex_);
                if (!state->ready_)
                {
                    state->result_.emplace(tools::unexpected<E>(E::broken_promise));
                    state->ready_ = true;
                    cbs = std::move(state->on_ready_cbs_);
                    state->cv_.notify_all();
                }
            }
            for (auto& callback : cbs)
            {
                callback();
            }
        }

    public:
        /**
         * @brief Retrieves the consumer-side future handle.
         * @return Valid future_result on first call, invalid handle otherwise.
         */
        future_result<T, E> get_future()
        {
            if (future_retrieved_ || !state_)
            {
                return future_result<T, E> {};
            }
            future_retrieved_ = true;
            weak_state_ = state_;
            auto future = future_result<T, E>(state_);
            state_.reset();
            return future;
        }

        /**
         * @brief Completes the state with an expected value or error.
         * @param result Value/error payload to store.
         */
        void set_result(tools::expected<T, E> result)
        {
            auto state = get_state();
            if (!state)
            {
                return;
            }

            std::vector<std::function<void()>> cbs;
            {
                std::scoped_lock<tools::critical_section> guard(state->mutex_);
                if (state->ready_)
                {
                    return;
                }

                state->result_.emplace(std::move(result));
                state->ready_ = true;
                state->cancel_action_ = unique_function<void()> {};
                cbs = std::move(state->on_ready_cbs_);
                state->cv_.notify_all();
            }
            for (auto& callback : cbs)
            {
                callback();
            }
        }

        /**
         * @brief Completes the state with a success value.
         * @tparam U Value helper type (non-void branch).
         * @param value Success value to store.
         */
        template <typename U = T, typename std::enable_if<!std::is_void<U>::value, int>::type = 0>
        void set_value(U value)
        {
            set_result(tools::expected<T, E>(std::move(value)));
        }

        /**
         * @brief Completes the state with a void success.
         * @tparam U Value helper type (void branch).
         */
        template <typename U = T, typename std::enable_if<std::is_void<U>::value, int>::type = 0>
        void set_value()
        {
            set_result(tools::expected<void, E>());
        }

        /**
         * @brief Completes the state with an error.
         * @param error Error value to store.
         */
        void set_error(E error)
        {
            set_result(tools::unexpected<E>(std::move(error)));
        }

        /**
         * @brief Reports whether the consumer side is still awaiting completion.
         * @return true while state is still potentially observed by a consumer.
         */
        [[nodiscard]] bool is_awaiten() const noexcept
        {
            return static_cast<bool>(state_) || !weak_state_.expired();
        }

        explicit operator bool() const noexcept
        {
            return is_awaiten();
        }

#if defined(PC_HAS_COROUTINES)
        [[nodiscard]] ::pco::detail::suspend_never initial_suspend() const noexcept
        {
            return {};
        }
        [[nodiscard]] ::pco::detail::suspend_never final_suspend() const noexcept
        {
            return {};
        }
        auto get_return_object()
        {
            return get_future();
        }

        void unhandled_exception()
        {
            if constexpr (std::is_same_v<E, result_error>)
            {
                set_error(result_error::execution_failure);
            }
            else if constexpr (requires { E::execution_failure; })
            {
                set_error(E::execution_failure);
            }
            else
            {
                set_error(E {});
            }
        }

        template <typename U = T>
            requires(!std::is_void_v<U>)
        void return_value(U value)
        {
            set_value(std::move(value));
        }
#endif

    private:
        [[nodiscard]]
        detail::result_state_ptr<T, E> get_state() const
        {
            if (state_)
            {
                return state_;
            }
            return weak_state_.lock();
        }

        detail::result_state_ptr<T, E> state_;
        std::weak_ptr<detail::result_shared_state<T, E>> weak_state_;
        bool future_retrieved_ = false;
    };

    namespace detail
    {
        template <typename NextT, typename E, typename Handle>
        void resolve_nested_handle(promise_result<NextT, E>& promise, Handle&& handle)
        {
            using handle_t = std::decay_t<Handle>;
            static_assert(is_result_handle<handle_t>::value,
                "resolve_nested_handle requires future_result or shared_result handle");
            static_assert(std::is_same<E, typename handle_t::error_type>::value,
                "Nested result handle must share the same error type");

            if constexpr (is_result_future<handle_t>::value)
            {
                auto nested = std::forward<Handle>(handle).get_result();
                if (!nested.has_value())
                {
                    if (nested.error() == E::no_state)
                    {
                        // Continuation returned an invalid nested handle;
                        // surface broken_promise on the outer result.
                        promise.set_error(E::broken_promise);
                    }
                    else
                    {
                        promise.set_error(nested.error());
                    }
                    return;
                }

                if constexpr (std::is_void<NextT>::value)
                {
                    promise.set_value();
                }
                else
                {
                    promise.set_value(std::move(nested).value());
                }
            }
            else
            {
                const auto& nested = handle.get_result();
                if (!nested.has_value())
                {
                    if (nested.error() == E::no_state)
                    {
                        promise.set_error(E::broken_promise);
                    }
                    else
                    {
                        promise.set_error(nested.error());
                    }
                    return;
                }

                if constexpr (std::is_void<NextT>::value)
                {
                    promise.set_value();
                }
                else
                {
                    promise.set_value(nested.value());
                }
            }
        }
    } // namespace detail

} // namespace pco
