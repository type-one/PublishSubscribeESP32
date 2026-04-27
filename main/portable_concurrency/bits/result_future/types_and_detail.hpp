/**
 * @file types_and_detail.hpp
 * @brief Result-future shared types and detail utilities.
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

namespace pco
{
    /**
     * @brief Canonical error codes for result-based future APIs.
     */
    enum class result_error : std::uint8_t
    {
        no_state = 1,
        broken_promise,
        execution_failure,
        continuation_failure,
    };

    /**
     * @brief Compatibility alias exposing `tools::expected`.
     * @tparam T Value type.
     * @tparam E Error type.
     */
    template <typename T, typename E>
    using expected = tools::expected<T, E>;

    /**
     * @brief Compatibility alias exposing `tools::unexpected`.
     * @tparam E Error type.
     */
    template <typename E>
    using unexpected = tools::unexpected<E>;

    /**
     * @brief Forward declaration of move-only result future.
     * @tparam T Value type.
     * @tparam E Error type.
     */
    template <typename T, typename E = result_error>
    class future_result;

    /**
     * @brief Forward declaration of copyable shared result handle.
     * @tparam T Value type.
     * @tparam E Error type.
     */
    template <typename T, typename E = result_error>
    class shared_result;

    /**
     * @brief Forward declaration of producer-side result promise.
     * @tparam T Value type.
     * @tparam E Error type.
     */
    template <typename T, typename E = result_error>
    class promise_result;

    /**
     * @brief Return type of `when_any` combinators.
     * @tparam Sequence Future sequence container type.
     */
    template <typename Sequence>
    struct when_any_result
    {
        /** @brief Index of the first completed input future. */
        std::size_t index;
        /** @brief Original sequence containing all input futures. */
        Sequence futures;
    };

    namespace detail
    {

        /**
         * @brief Deduces continuation value type for `future_result<T, E>::then_value`.
         * @tparam F Continuation type.
         * @tparam T Source value type.
         */
        template <typename F, typename T, typename = void>
        struct result_then_value_type
        {
            using raw_type = std::decay_t<std::invoke_result_t<F, T>>;
            using type = raw_type;
        };

        /**
         * @brief Void-source specialization for `then_value` type deduction.
         * @tparam F Continuation type.
         */
        template <typename F>
        struct result_then_value_type<F, void, std::enable_if_t<std::is_invocable_v<F>>>
        {
            using raw_type = std::decay_t<std::invoke_result_t<F>>;
            using type = raw_type;
        };

        /**
         * @brief Deduces continuation value type for `shared_result<T, E>::then_value`.
         * @tparam F Continuation type.
         * @tparam T Source value type.
         */
        template <typename F, typename T, typename = void>
        struct result_shared_then_value_type
        {
            using raw_type = std::decay_t<std::invoke_result_t<F, const T&>>;
            using type = raw_type;
        };

        /**
         * @brief Void-source specialization for shared `then_value` type deduction.
         * @tparam F Continuation type.
         */
        template <typename F>
        struct result_shared_then_value_type<F, void, std::enable_if_t<std::is_invocable_v<F>>>
        {
            using raw_type = std::decay_t<std::invoke_result_t<F>>;
            using type = raw_type;
        };

        /**
         * @brief Deduces type returned by result-aware continuations.
         * @tparam F Continuation type.
         * @tparam Arg Continuation argument type.
         */
        template <typename F, typename Arg>
        struct result_then_result_type
        {
            using raw_type = std::decay_t<std::invoke_result_t<F, Arg>>;
            using type = raw_type;
        };

        /**
         * @brief Deduces result type for interruptible continuation callbacks.
         * @tparam Source Source handle type passed to continuation.
         * @tparam E Error type.
         */
        template <typename Source, typename E>
        struct interrupt_promise_deducer
        {
            template <typename R>
            static R deduce(void (*)(promise_result<R, E>, Source));

            template <typename R, typename C>
            static R deduce_method(void (C::*)(promise_result<R, E>, Source) const);

            template <typename R, typename C>
            static R deduce_method(void (C::*)(promise_result<R, E>, Source));

            template <typename F>
            static auto deduce(F function_object) -> decltype(deduce_method(&F::operator()));
        };

        /**
         * @brief Primary template for interrupt-promise extraction (SFINAE fallback).
         * @tparam Func Callback type.
         * @tparam Source Source handle type.
         * @tparam E Error type.
         * @tparam Unused SFINAE hook.
         */
        template <typename Func, typename Source, typename E, typename = void>
        struct interrupt_promise_arg
        {
        };

        /**
         * @brief Extracts interrupt-promise result type from compatible callback.
         * @tparam Func Callback type.
         * @tparam Source Source handle type.
         * @tparam E Error type.
         */
        template <typename Func, typename Source, typename E>
        struct interrupt_promise_arg<Func, Source, E,
            std::void_t<decltype(interrupt_promise_deducer<Source, E>::deduce(std::declval<std::decay_t<Func>>()))>>
        {
            using type = decltype(interrupt_promise_deducer<Source, E>::deduce(std::declval<std::decay_t<Func>>()));
        };

        /**
         * @brief Convenience alias for interrupt-promise argument extraction.
         * @tparam Func Callback type.
         * @tparam Source Source handle type.
         * @tparam E Error type.
         */
        template <typename Func, typename Source, typename E>
        using interrupt_promise_arg_t = typename interrupt_promise_arg<Func, Source, E>::type;

        /**
         * @brief Shared state backing promise/future result handles.
         * @tparam T Value type.
         * @tparam E Error type.
         */
        template <typename T, typename E>
        struct result_shared_state
        {
            /** @brief Constructs empty state. */
            result_shared_state() = default;
            result_shared_state(const result_shared_state&) = delete;
            result_shared_state& operator=(const result_shared_state&) = delete;
            result_shared_state(result_shared_state&&) = delete;
            result_shared_state& operator=(result_shared_state&&) = delete;

            /**
             * @brief Constructs state with cancellation action.
             * @tparam F Callable cancellation action type.
             * @param canceler_tag Tag selecting cancel-action constructor.
             * @param cancel_action Cancellation callback invoked on abandonment.
             */
            template <typename F>
            explicit result_shared_state([[maybe_unused]] canceler_arg_t canceler_tag, F&& cancel_action)
                : cancel_action_(std::forward<F>(cancel_action))
            {
            }

            /**
             * @brief Invokes cancellation action when state is abandoned before ready.
             */
            ~result_shared_state()
            {
                if (!ready_ && cancel_action_)
                {
                    static_cast<void>(cancel_action_.try_invoke());
                }
            }

            /** @brief Mutex protecting shared state access. */
            tools::critical_section mutex_;
            /** @brief Condition variable signaled when readiness changes. */
            tools::cond_var cv_;
            /** @brief Ready flag set once a terminal result is available. */
            bool ready_ = false;
            /** @brief Stored terminal expected result. */
            std::optional<tools::expected<T, E>> result_;
            /** @brief Deferred callbacks executed when state becomes ready. */
            std::vector<std::function<void()>> on_ready_cbs_;
            /** @brief Optional cancellation action invoked on abandonment. */
            unique_function<void()> cancel_action_;
        };

        /**
         * @brief Shared-pointer alias to result shared state.
         * @tparam T Value type.
         * @tparam E Error type.
         */
        template <typename T, typename E>
        using result_state_ptr = std::shared_ptr<result_shared_state<T, E>>;

        /**
         * @brief Decayed invoke result alias.
         * @tparam F Callable type.
         * @tparam A Argument types.
         */
        template <typename F, typename... A>
        using invoke_decay_t = std::decay_t<std::invoke_result_t<F, A...>>;

        /** @brief Trait detecting `future_result` handles. */
        template <typename T>
        struct is_result_future : std::false_type
        {
        };

        /**
         * @brief Positive specialization for `future_result<T, E>` handles.
         * @tparam T Value type.
         * @tparam E Error type.
         */
        template <typename T, typename E>
        struct is_result_future<future_result<T, E>> : std::true_type
        {
        };

        /** @brief Trait detecting `shared_result` handles. */
        template <typename T>
        struct is_shared_result : std::false_type
        {
        };

        /**
         * @brief Positive specialization for `shared_result<T, E>` handles.
         * @tparam T Value type.
         * @tparam E Error type.
         */
        template <typename T, typename E>
        struct is_shared_result<shared_result<T, E>> : std::true_type
        {
        };

        /** @brief Trait detecting either `future_result` or `shared_result` handles. */
        template <typename T>
        struct is_result_handle : std::disjunction<is_result_future<std::decay_t<T>>, is_shared_result<std::decay_t<T>>>
        {
        };

        /** @brief Variadic trait requiring all arguments to be `future_result`. */
        template <typename... T>
        struct are_result_futures : std::conjunction<is_result_future<std::decay_t<T>>...>
        {
        };

        /** @brief Variadic trait requiring all arguments to be `shared_result`. */
        template <typename... T>
        struct are_shared_results : std::conjunction<is_shared_result<std::decay_t<T>>...>
        {
        };

        /** @brief Variadic trait requiring all arguments to be result handles. */
        template <typename... T>
        struct are_result_handles : std::conjunction<is_result_handle<std::decay_t<T>>...>
        {
        };

        /**
         * @brief Maps result handle type to underlying value type.
         * @tparam R Raw or wrapped result type.
         */
        template <typename R>
        struct unwrapped_result_value
        {
            using type = R;
        };

        /**
         * @brief Unwrap specialization for `future_result<T, E>`.
         * @tparam T Value type.
         * @tparam E Error type.
         */
        template <typename T, typename E>
        struct unwrapped_result_value<future_result<T, E>>
        {
            using type = T;
        };

        /**
         * @brief Unwrap specialization for `shared_result<T, E>`.
         * @tparam T Value type.
         * @tparam E Error type.
         */
        template <typename T, typename E>
        struct unwrapped_result_value<shared_result<T, E>>
        {
            using type = T;
        };

        template <typename Traits>
        using unwrapped_result_value_t = typename unwrapped_result_value<typename Traits::raw_type>::type;

        /**
         * @brief Resolves nested result handles into a destination promise.
         * @tparam NextT Destination value type.
         * @tparam E Error type.
         * @tparam Handle Source nested handle type.
         * @param promise Destination promise.
         * @param handle Nested source handle.
         */
        template <typename NextT, typename E, typename Handle>
        void resolve_nested_handle(promise_result<NextT, E>& promise, Handle&& handle);

        /**
         * @brief Rebinds allocator `Alloc` to element type `T`.
         * @tparam Alloc Source allocator type.
         * @tparam T Target element type.
         */
        template <typename Alloc, typename T>
        using rebind_alloc_t = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

        /**
         * @brief Waits until result state is ready or timeout point is reached.
         * @tparam T Value type.
         * @tparam E Error type.
         * @tparam Clock Clock type.
         * @tparam Duration Duration type.
         * @param state Shared result state.
         * @param abs_time Timeout deadline.
         * @return `future_status::ready` when state becomes ready, otherwise `future_status::timeout`.
         */
        template <typename T, typename E, typename Clock, typename Duration>
        future_status wait_until_ready(
            const result_state_ptr<T, E>& state, const std::chrono::time_point<Clock, Duration>& abs_time)
        {
            if (!state)
            {
                return future_status::timeout;
            }

            std::unique_lock<tools::critical_section> lock(state->mutex_);
            if (state->ready_)
            {
                return future_status::ready;
            }

            return state->cv_.wait_until(lock, abs_time, [&state] { return state->ready_; }) ? future_status::ready
                                                                                             : future_status::timeout;
        }

    } // namespace detail

} // namespace pco
