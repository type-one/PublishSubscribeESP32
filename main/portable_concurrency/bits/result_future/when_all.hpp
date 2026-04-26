/**
 * @file result_future/when_all.hpp
 * @brief when_all combinator for synchronizing multiple futures/shared_results.
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
#include "types_and_detail.hpp"

namespace pco
{
    inline future_result<std::tuple<>, result_error> when_all()
    {
        auto promise_and_future = make_result_promise<std::tuple<>, result_error>();
        auto promise = std::move(promise_and_future.first);
        auto future = std::move(promise_and_future.second);
        promise.set_value(std::tuple<> {});
        return future;
    }

    template <typename... Futures>
    std::enable_if_t<detail::are_result_futures<Futures...>::value,
        future_result<std::tuple<typename std::decay_t<Futures>::result_type...>,
            typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::error_type>>
    when_all(Futures&&... futures)
    {
        using first_future_t = std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>;
        using error_t = typename first_future_t::error_type;

        static_assert((std::is_same<error_t, typename std::decay_t<Futures>::error_type>::value && ...),
            "All futures passed to v2::when_all must share the same error type");

        using results_tuple_t = std::tuple<typename std::decay_t<Futures>::result_type...>;

        auto promise_and_future = make_result_promise<results_tuple_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        auto future_tuple = std::make_tuple(std::forward<Futures>(futures)...);

        bool all_valid = true;
        std::apply([&](auto&... future_item) { ((all_valid = all_valid && future_item.valid()), ...); }, future_tuple);
        if (!all_valid)
        {
            promise.set_error(error_t::no_state);
            return combined_future;
        }

        struct WhenAllCtx
        {
            std::atomic<std::size_t> remaining_;
            std::tuple<std::decay_t<Futures>...> futures;
            promise_result<results_tuple_t, error_t> promise;

            WhenAllCtx(std::size_t remaining, std::tuple<std::decay_t<Futures>...> futures_arg,
                promise_result<results_tuple_t, error_t> promise_arg)
                : remaining_(remaining)
                , futures(std::move(futures_arg))
                , promise(std::move(promise_arg))
            {
            }
        };

        auto ctx = std::make_shared<WhenAllCtx>(sizeof...(Futures), std::move(future_tuple), std::move(promise));

        auto subscribe_one = [&](auto& future_item)
        {
            future_item.subscribe(
                [ctx]() mutable
                {
                    if (ctx->remaining_.fetch_sub(1) == 1)
                    {
                        auto results = std::apply([](auto&... ready_futures)
                            { return results_tuple_t { ready_futures.take_ready_result()... }; }, ctx->futures);
                        ctx->promise.set_value(std::move(results));
                    }
                });
        };

        std::apply([&](auto&... future_item) { (subscribe_one(future_item), ...); }, ctx->futures);

        return combined_future;
    }

    template <typename... SharedResults>
    std::enable_if_t<detail::are_shared_results<SharedResults...>::value,
        future_result<std::tuple<std::decay_t<SharedResults>...>,
            typename std::decay_t<std::tuple_element_t<0, std::tuple<SharedResults...>>>::error_type>>
    when_all(SharedResults&&... shared_results)
    {
        using first_shared_t = std::decay_t<std::tuple_element_t<0, std::tuple<SharedResults...>>>;
        using error_t = typename first_shared_t::error_type;
        using shared_tuple_t = std::tuple<std::decay_t<SharedResults>...>;

        static_assert((std::is_same<error_t, typename std::decay_t<SharedResults>::error_type>::value && ...),
            "All shared_results passed to v2::when_all must share the same error type");

        auto promise_and_future = make_result_promise<shared_tuple_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        auto shared_tuple = std::make_tuple(std::forward<SharedResults>(shared_results)...);

        bool all_valid = true;
        std::apply([&](auto&... shared) { ((all_valid = all_valid && shared.valid()), ...); }, shared_tuple);
        if (!all_valid)
        {
            promise.set_error(error_t::no_state);
            return combined_future;
        }

        struct WhenAllSharedCtx
        {
            std::atomic<std::size_t> remaining_;
            shared_tuple_t shareds;
            promise_result<shared_tuple_t, error_t> promise;

            WhenAllSharedCtx(std::size_t remaining, shared_tuple_t input_shareds,
                promise_result<shared_tuple_t, error_t> input_promise)
                : remaining_(remaining)
                , shareds(std::move(input_shareds))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx
            = std::make_shared<WhenAllSharedCtx>(sizeof...(SharedResults), std::move(shared_tuple), std::move(promise));

        auto subscribe_one = [&](auto& shared)
        {
            shared.subscribe(
                [ctx]() mutable
                {
                    if (ctx->remaining_.fetch_sub(1) == 1)
                    {
                        ctx->promise.set_value(std::move(ctx->shareds));
                    }
                });
        };

        std::apply([&](auto&... shared) { (subscribe_one(shared), ...); }, ctx->shareds);

        return combined_future;
    }

    template <typename... Handles>
    std::enable_if_t<detail::are_result_handles<Handles...>::value && !detail::are_result_futures<Handles...>::value
            && !detail::are_shared_results<Handles...>::value,
        future_result<std::tuple<std::decay_t<Handles>...>,
            typename std::decay_t<std::tuple_element_t<0, std::tuple<Handles...>>>::error_type>>
    when_all(Handles&&... handles)
    {
        using first_handle_t = std::decay_t<std::tuple_element_t<0, std::tuple<Handles...>>>;
        using error_t = typename first_handle_t::error_type;
        using handles_tuple_t = std::tuple<std::decay_t<Handles>...>;

        static_assert((std::is_same<error_t, typename std::decay_t<Handles>::error_type>::value && ...),
            "All handles passed to v2::when_all must share the same error type");

        auto promise_and_future = make_result_promise<handles_tuple_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        auto handle_tuple = std::make_tuple(std::forward<Handles>(handles)...);

        bool all_valid = true;
        std::apply([&](auto&... handle_item) { ((all_valid = all_valid && handle_item.valid()), ...); }, handle_tuple);
        if (!all_valid)
        {
            promise.set_error(error_t::no_state);
            return combined_future;
        }

        struct WhenAllMixedCtx
        {
            std::atomic<std::size_t> remaining_;
            handles_tuple_t handles;
            promise_result<handles_tuple_t, error_t> promise;

            WhenAllMixedCtx(std::size_t remaining, handles_tuple_t input_handles,
                promise_result<handles_tuple_t, error_t> input_promise)
                : remaining_(remaining)
                , handles(std::move(input_handles))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx = std::make_shared<WhenAllMixedCtx>(sizeof...(Handles), std::move(handle_tuple), std::move(promise));

        auto subscribe_one = [&](auto& handle)
        {
            handle.subscribe(
                [ctx]() mutable
                {
                    if (ctx->remaining_.fetch_sub(1) == 1)
                    {
                        ctx->promise.set_value(std::move(ctx->handles));
                    }
                });
        };

        std::apply([&](auto&... handle_item) { (subscribe_one(handle_item), ...); }, ctx->handles);

        return combined_future;
    }

    template <typename InputIt>
    std::enable_if_t<detail::is_result_future<typename std::iterator_traits<InputIt>::value_type>::value,
        future_result<
            std::vector<typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::result_type>,
            typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::error_type>>
    when_all(InputIt first, InputIt last)
    {
        using future_t = std::decay_t<typename std::iterator_traits<InputIt>::value_type>;
        using result_t = typename future_t::result_type;
        using error_t = typename future_t::error_type;
        using futures_vector_t = std::vector<future_t>;
        using results_vector_t = std::vector<result_t>;

        auto promise_and_future = make_result_promise<results_vector_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        futures_vector_t futures(std::make_move_iterator(first), std::make_move_iterator(last));
        if (futures.empty())
        {
            promise.set_value(results_vector_t {});
            return combined_future;
        }

        bool all_valid = true;
        for (auto& future : futures)
        {
            all_valid = all_valid && future.valid();
        }
        if (!all_valid)
        {
            promise.set_error(error_t::no_state);
            return combined_future;
        }

        struct WhenAllVectorCtx
        {
            std::atomic<std::size_t> remaining_;
            futures_vector_t futures;
            promise_result<results_vector_t, error_t> promise;

            WhenAllVectorCtx(std::size_t remaining, futures_vector_t input_futures,
                promise_result<results_vector_t, error_t> input_promise)
                : remaining_(remaining)
                , futures(std::move(input_futures))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx = std::make_shared<WhenAllVectorCtx>(futures.size(), std::move(futures), std::move(promise));

        for (auto& future : ctx->futures)
        {
            future.subscribe(
                [ctx]() mutable
                {
                    if (ctx->remaining_.fetch_sub(1) == 1)
                    {
                        results_vector_t results;
                        results.reserve(ctx->futures.size());
                        for (auto& ready_future : ctx->futures)
                        {
                            results.push_back(ready_future.take_ready_result());
                        }
                        ctx->promise.set_value(std::move(results));
                    }
                });
        }

        return combined_future;
    }

    template <typename Future, typename Alloc>
    std::enable_if_t<detail::is_result_future<Future>::value,
        future_result<
            std::vector<typename Future::result_type, detail::rebind_alloc_t<Alloc, typename Future::result_type>>,
            typename Future::error_type>>
    when_all(std::vector<Future, Alloc> futures)
    {
        using future_t = Future;
        using result_t = typename future_t::result_type;
        using error_t = typename future_t::error_type;
        using futures_vector_t = std::vector<future_t, Alloc>;
        using results_vector_t = std::vector<result_t, detail::rebind_alloc_t<Alloc, result_t>>;

        auto promise_and_future = make_result_promise<results_vector_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        if (futures.empty())
        {
            promise.set_value(results_vector_t {});
            return combined_future;
        }

        bool all_valid = true;
        for (auto& future : futures)
        {
            all_valid = all_valid && future.valid();
        }
        if (!all_valid)
        {
            promise.set_error(error_t::no_state);
            return combined_future;
        }

        struct WhenAllVectorCtx
        {
            std::atomic<std::size_t> remaining_;
            futures_vector_t futures;
            promise_result<results_vector_t, error_t> promise;

            WhenAllVectorCtx(std::size_t remaining, futures_vector_t input_futures,
                promise_result<results_vector_t, error_t> input_promise)
                : remaining_(remaining)
                , futures(std::move(input_futures))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx = std::make_shared<WhenAllVectorCtx>(futures.size(), std::move(futures), std::move(promise));

        for (auto& future : ctx->futures)
        {
            future.subscribe(
                [ctx]() mutable
                {
                    if (ctx->remaining_.fetch_sub(1) == 1)
                    {
                        results_vector_t results;
                        results.reserve(ctx->futures.size());
                        for (auto& ready_future : ctx->futures)
                        {
                            results.push_back(ready_future.take_ready_result());
                        }
                        ctx->promise.set_value(std::move(results));
                    }
                });
        }

        return combined_future;
    }

    template <typename InputIt>
    std::enable_if_t<detail::is_shared_result<typename std::iterator_traits<InputIt>::value_type>::value,
        future_result<std::vector<std::decay_t<typename std::iterator_traits<InputIt>::value_type>>,
            typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::error_type>>
    when_all(InputIt first, InputIt last)
    {
        using shared_t = std::decay_t<typename std::iterator_traits<InputIt>::value_type>;
        using error_t = typename shared_t::error_type;
        using shared_vector_t = std::vector<shared_t>;

        auto promise_and_future = make_result_promise<shared_vector_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        shared_vector_t shareds(first, last);
        if (shareds.empty())
        {
            promise.set_value(shared_vector_t {});
            return combined_future;
        }

        bool all_valid = true;
        for (auto& shared : shareds)
        {
            all_valid = all_valid && shared.valid();
        }
        if (!all_valid)
        {
            promise.set_error(error_t::no_state);
            return combined_future;
        }

        struct WhenAllSharedVectorCtx
        {
            std::atomic<std::size_t> remaining_;
            shared_vector_t shareds;
            promise_result<shared_vector_t, error_t> promise;

            WhenAllSharedVectorCtx(std::size_t remaining, shared_vector_t input_shareds,
                promise_result<shared_vector_t, error_t> input_promise)
                : remaining_(remaining)
                , shareds(std::move(input_shareds))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx = std::make_shared<WhenAllSharedVectorCtx>(shareds.size(), std::move(shareds), std::move(promise));

        for (auto& shared : ctx->shareds)
        {
            shared.subscribe(
                [ctx]() mutable
                {
                    if (ctx->remaining_.fetch_sub(1) == 1)
                    {
                        ctx->promise.set_value(std::move(ctx->shareds));
                    }
                });
        }

        return combined_future;
    }

    template <typename SharedResult, typename Alloc>
    std::enable_if_t<detail::is_shared_result<SharedResult>::value,
        future_result<std::vector<SharedResult, Alloc>, typename SharedResult::error_type>>
    when_all(std::vector<SharedResult, Alloc> shareds)
    {
        using shared_t = SharedResult;
        using error_t = typename shared_t::error_type;
        using shared_vector_t = std::vector<shared_t, Alloc>;

        auto promise_and_future = make_result_promise<shared_vector_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        if (shareds.empty())
        {
            promise.set_value(shared_vector_t {});
            return combined_future;
        }

        bool all_valid = true;
        for (auto& shared : shareds)
        {
            all_valid = all_valid && shared.valid();
        }
        if (!all_valid)
        {
            promise.set_error(error_t::no_state);
            return combined_future;
        }

        struct WhenAllSharedVectorCtx
        {
            std::atomic<std::size_t> remaining_;
            shared_vector_t shareds;
            promise_result<shared_vector_t, error_t> promise;

            WhenAllSharedVectorCtx(std::size_t remaining, shared_vector_t input_shareds,
                promise_result<shared_vector_t, error_t> input_promise)
                : remaining_(remaining)
                , shareds(std::move(input_shareds))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx = std::make_shared<WhenAllSharedVectorCtx>(shareds.size(), std::move(shareds), std::move(promise));

        for (auto& shared : ctx->shareds)
        {
            shared.subscribe(
                [ctx]() mutable
                {
                    if (ctx->remaining_.fetch_sub(1) == 1)
                    {
                        ctx->promise.set_value(std::move(ctx->shareds));
                    }
                });
        }

        return combined_future;
    }

} // namespace pco
