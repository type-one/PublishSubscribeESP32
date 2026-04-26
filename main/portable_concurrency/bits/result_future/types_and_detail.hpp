/**
 * @file result_future/types_and_detail.hpp
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
    enum class result_error : std::uint8_t
    {
        no_state = 1,
        broken_promise,
        execution_failure,
        continuation_failure,
    };

    // Compatibility aliases: expose tools::expected/unexpected for result-based API users
    template <typename T, typename E>
    using expected = tools::expected<T, E>;

    template <typename E>
    using unexpected = tools::unexpected<E>;

    template <typename T, typename E = result_error>
    class future_result;
    template <typename T, typename E = result_error>
    class shared_result;
    template <typename T, typename E = result_error>
    class promise_result;

    template <typename Sequence>
    struct when_any_result
    {
        std::size_t index;
        Sequence futures;
    };

    namespace detail
    {

        template <typename F, typename T, typename = void>
        struct result_then_value_type
        {
            using raw_type = std::decay_t<std::invoke_result_t<F, T>>;
            using type = raw_type;
        };

        template <typename F>
        struct result_then_value_type<F, void, std::enable_if_t<std::is_invocable_v<F>>>
        {
            using raw_type = std::decay_t<std::invoke_result_t<F>>;
            using type = raw_type;
        };

        template <typename F, typename T, typename = void>
        struct result_shared_then_value_type
        {
            using raw_type = std::decay_t<std::invoke_result_t<F, const T&>>;
            using type = raw_type;
        };

        template <typename F>
        struct result_shared_then_value_type<F, void, std::enable_if_t<std::is_invocable_v<F>>>
        {
            using raw_type = std::decay_t<std::invoke_result_t<F>>;
            using type = raw_type;
        };

        template <typename F, typename Arg>
        struct result_then_result_type
        {
            using raw_type = std::decay_t<std::invoke_result_t<F, Arg>>;
            using type = raw_type;
        };

        // Deduce result type for interruptible continuation callbacks.
        // Expected callable shape:
        //   void(promise_result<R, E>, future_result<T, E>)
        // or
        //   void(promise_result<R, E>, shared_result<T, E>)
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

        template <typename Func, typename Source, typename E, typename = void>
        struct interrupt_promise_arg
        {
        };

        template <typename Func, typename Source, typename E>
        struct interrupt_promise_arg<Func, Source, E,
            std::void_t<decltype(interrupt_promise_deducer<Source, E>::deduce(std::declval<std::decay_t<Func>>()))>>
        {
            using type = decltype(interrupt_promise_deducer<Source, E>::deduce(std::declval<std::decay_t<Func>>()));
        };

        template <typename Func, typename Source, typename E>
        using interrupt_promise_arg_t = typename interrupt_promise_arg<Func, Source, E>::type;

        template <typename T, typename E>
        struct result_shared_state
        {
            result_shared_state() = default;
            result_shared_state(const result_shared_state&) = delete;
            result_shared_state& operator=(const result_shared_state&) = delete;
            result_shared_state(result_shared_state&&) = delete;
            result_shared_state& operator=(result_shared_state&&) = delete;

            template <typename F>
            explicit result_shared_state([[maybe_unused]] canceler_arg_t canceler_tag, F&& cancel_action)
                : cancel_action_(std::forward<F>(cancel_action))
            {
            }

            ~result_shared_state()
            {
                if (!ready_ && cancel_action_)
                {
                    cancel_action_();
                }
            }

            tools::critical_section mutex_;
            tools::cond_var cv_;
            bool ready_ = false;
            std::optional<tools::expected<T, E>> result_;
            std::vector<std::function<void()>> on_ready_cbs_;
            unique_function<void()> cancel_action_;
        };

        template <typename T, typename E>
        using result_state_ptr = std::shared_ptr<result_shared_state<T, E>>;

        template <typename F, typename... A>
        using invoke_decay_t = std::decay_t<std::invoke_result_t<F, A...>>;

        template <typename T>
        struct is_result_future : std::false_type
        {
        };

        template <typename T, typename E>
        struct is_result_future<future_result<T, E>> : std::true_type
        {
        };

        template <typename T>
        struct is_shared_result : std::false_type
        {
        };

        template <typename T, typename E>
        struct is_shared_result<shared_result<T, E>> : std::true_type
        {
        };

        template <typename T>
        struct is_result_handle : std::disjunction<is_result_future<std::decay_t<T>>, is_shared_result<std::decay_t<T>>>
        {
        };

        template <typename... T>
        struct are_result_futures : std::conjunction<is_result_future<std::decay_t<T>>...>
        {
        };

        template <typename... T>
        struct are_shared_results : std::conjunction<is_shared_result<std::decay_t<T>>...>
        {
        };

        template <typename... T>
        struct are_result_handles : std::conjunction<is_result_handle<std::decay_t<T>>...>
        {
        };

        template <typename R>
        struct unwrapped_result_value
        {
            using type = R;
        };

        template <typename T, typename E>
        struct unwrapped_result_value<future_result<T, E>>
        {
            using type = T;
        };

        template <typename T, typename E>
        struct unwrapped_result_value<shared_result<T, E>>
        {
            using type = T;
        };

        template <typename Traits>
        using unwrapped_result_value_t = typename unwrapped_result_value<typename Traits::raw_type>::type;

        template <typename NextT, typename E, typename Handle>
        void resolve_nested_handle(promise_result<NextT, E>& promise, Handle&& handle);

        template <typename Alloc, typename T>
        using rebind_alloc_t = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

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
