/**
 * @file result_future/factories.hpp
 * @brief result_future ready/error factories and async_result helper.
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

#include "future.hpp"
#include "shared.hpp"

namespace pco
{
    template <typename T, typename E = result_error>
    future_result<std::decay_t<T>, E> make_ready_result(T&& value)
    {
        auto promise_and_future = make_result_promise<std::decay_t<T>, E>();
        promise_and_future.first.set_value(std::forward<T>(value));
        return std::move(promise_and_future.second);
    }

    template <typename E = result_error>
    future_result<void, E> make_ready_result()
    {
        auto promise_and_future = make_result_promise<void, E>();
        promise_and_future.first.set_value();
        return std::move(promise_and_future.second);
    }

    template <typename T, typename E = result_error>
    future_result<T, E> make_error_result(E error)
    {
        auto promise_and_future = make_result_promise<T, E>();
        promise_and_future.first.set_error(std::move(error));
        return std::move(promise_and_future.second);
    }

    template <typename Exec, typename F, typename... A>
    future_result<typename detail::unwrapped_result_value<typename detail::invoke_decay_t<F, A...>>::type, result_error>
    async_result(Exec&& exec, F&& function_arg, A&&... args)
    {
        static_assert(is_executor<std::decay_t<Exec>>::value, "Exec must satisfy pco::is_executor");

        using raw_value_t = typename detail::invoke_decay_t<F, A...>;
        using value_t = typename detail::unwrapped_result_value<raw_value_t>::type;

        auto promise_and_future = make_result_promise<value_t, result_error>();
        auto promise = std::move(promise_and_future.first);
        auto future = std::move(promise_and_future.second);

        if constexpr (detail::is_result_handle<raw_value_t>::value)
        {
            struct UnwrapCtx
            {
                raw_value_t inner;
                promise_result<value_t, result_error> outer;
            };

            auto task = [promise = std::move(promise), function_arg = std::forward<F>(function_arg),
                            params = std::make_tuple(std::forward<A>(args)...)]() mutable
            {
                raw_value_t inner = [&]()
                {
                    auto function_local = std::move(function_arg);
                    auto params_local = std::move(params);
                    return std::apply(std::move(function_local), std::move(params_local));
                }();

                auto ctx = std::make_shared<UnwrapCtx>(UnwrapCtx { std::move(inner), std::move(promise) });

                if constexpr (detail::is_result_future<raw_value_t>::value)
                {
                    ctx->inner.notify(
                        [ctx]() mutable
                        {
                            auto result_holder = ctx->inner.get_result();
                            if (result_holder.has_value())
                            {
                                if constexpr (std::is_void_v<value_t>)
                                {
                                    ctx->outer.set_value();
                                }
                                else
                                {
                                    ctx->outer.set_value(std::move(result_holder).value());
                                }
                            }
                            else
                            {
                                ctx->outer.set_error(result_holder.error());
                            }
                        });
                }
                else
                {
                    ctx->inner.notify(
                        [ctx]() mutable
                        {
                            const auto& result_holder = ctx->inner.get_result();
                            if (result_holder.has_value())
                            {
                                if constexpr (std::is_void_v<value_t>)
                                {
                                    ctx->outer.set_value();
                                }
                                else
                                {
                                    ctx->outer.set_value(result_holder.value());
                                }
                            }
                            else
                            {
                                ctx->outer.set_error(result_holder.error());
                            }
                        });
                }
            };

            post(std::forward<Exec>(exec), std::move(task));
        }
        else
        {
            auto task = [promise = std::move(promise), function_arg = std::forward<F>(function_arg),
                            params = std::make_tuple(std::forward<A>(args)...)]() mutable
            {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
                try
                {
                    if constexpr (std::is_void_v<value_t>)
                    {
                        {
                            auto function_local = std::move(function_arg);
                            auto params_local = std::move(params);
                            std::apply(function_local, std::move(params_local));
                        }
                        promise.set_value();
                    }
                    else
                    {
                        auto val = [&]()
                        {
                            auto function_local = std::move(function_arg);
                            auto params_local = std::move(params);
                            return std::apply(function_local, std::move(params_local));
                        }();
                        promise.set_value(std::move(val));
                    }
                }
                catch (...)
                {
                    promise.set_error(result_error::execution_failure);
                }
#else
                if constexpr (std::is_void_v<value_t>)
                {
                    {
                        auto function_local = std::move(function_arg);
                        auto params_local = std::move(params);
                        std::apply(function_local, std::move(params_local));
                    }
                    promise.set_value();
                }
                else
                {
                    auto val = [&]()
                    {
                        auto function_local = std::move(function_arg);
                        auto params_local = std::move(params);
                        return std::apply(function_local, std::move(params_local));
                    }();
                    promise.set_value(std::move(val));
                }
#endif
            };

            post(std::forward<Exec>(exec), std::move(task));
        }

        return future;
    }

} // namespace pco
