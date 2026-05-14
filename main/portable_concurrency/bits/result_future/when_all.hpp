/**
 * @file when_all.hpp
 * @brief when_all combinator for synchronizing multiple futures/shared_results.
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

#include "future.hpp"
#include "shared.hpp"
#include "types_and_detail.hpp"

namespace pco
{
    /**
     * @brief Returns an immediately-ready empty when_all aggregation.
     * @return Future holding an empty tuple.
     */
    inline future_result<std::tuple<>, result_error> when_all()
    {
        auto promise_and_future = make_result_promise<std::tuple<>, result_error>();
        auto promise = std::move(promise_and_future.first);
        auto future = std::move(promise_and_future.second);
        promise.set_value(std::tuple<> {});
        return future;
    }

    /**
     * @brief Aggregates a variadic pack of future_result handles.
     * @tparam Futures Future handle types (must share the same error type).
     * @param futures Futures to wait for.
     * @return Future containing a tuple of each input future result_type.
     */
    template <typename... Futures>
    std::enable_if_t<(sizeof...(Futures) > 0) && detail::are_result_futures<Futures...>::value,
        future_result<std::tuple<typename std::decay_t<Futures>::result_type...>,
            detail::first_result_error_type_t<Futures...>>>
    when_all(Futures&&... futures)
    {
        using error_t = detail::first_result_error_type_t<Futures...>;

        static_assert((std::is_same<error_t, typename std::decay_t<Futures>::error_type>::value && ...),
            "All futures passed to when_all must share the same error type");

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

        /**
         * @brief Shared state for variadic future_result `when_all` aggregation.
         */
        struct WhenAllCtx
        {
            /** @brief Remaining input futures not yet completed. */
            std::atomic<std::size_t> remaining_;
            /** @brief Stored input futures. */
            std::tuple<std::decay_t<Futures>...> futures;
            /** @brief Promise completing aggregated output future. */
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

    /**
     * @brief Aggregates a variadic pack of shared_result handles.
     * @tparam SharedResults Shared handle types (must share the same error type).
     * @param shared_results Shared results to wait for.
     * @return Future containing a tuple with the input shared handles.
     */
    template <typename... SharedResults>
    std::enable_if_t<(sizeof...(SharedResults) > 0) && detail::are_shared_results<SharedResults...>::value,
        future_result<std::tuple<std::decay_t<SharedResults>...>,
            detail::first_result_error_type_t<SharedResults...>>>
    when_all(SharedResults&&... shared_results)
    {
        using error_t = detail::first_result_error_type_t<SharedResults...>;
        using shared_tuple_t = std::tuple<std::decay_t<SharedResults>...>;

        static_assert((std::is_same<error_t, typename std::decay_t<SharedResults>::error_type>::value && ...),
            "All shared_results passed to when_all must share the same error type");

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

        /**
         * @brief Shared state for variadic shared_result `when_all` aggregation.
         */
        struct WhenAllSharedCtx
        {
            /** @brief Remaining input shared results not yet observed ready. */
            std::atomic<std::size_t> remaining_;
            /** @brief Stored input shared results. */
            shared_tuple_t shareds;
            /** @brief Promise completing aggregated output future. */
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

    /**
     * @brief Aggregates a variadic mixed pack of result handles.
     * @tparam Handles Result handle types (must share the same error type).
     * @param handles Handles to wait for.
     * @return Future containing a tuple with the input handles.
     */
    template <typename... Handles>
    std::enable_if_t<(sizeof...(Handles) > 0) && detail::are_result_handles<Handles...>::value
            && !detail::are_result_futures<Handles...>::value && !detail::are_shared_results<Handles...>::value,
        future_result<std::tuple<std::decay_t<Handles>...>,
            detail::first_result_error_type_t<Handles...>>>
    when_all(Handles&&... handles)
    {
        using error_t = detail::first_result_error_type_t<Handles...>;
        using handles_tuple_t = std::tuple<std::decay_t<Handles>...>;

        static_assert((std::is_same<error_t, typename std::decay_t<Handles>::error_type>::value && ...),
            "All handles passed to when_all must share the same error type");

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

        /**
         * @brief Shared state for mixed-handle variadic `when_all` aggregation.
         */
        struct WhenAllMixedCtx
        {
            /** @brief Remaining input handles not yet observed ready. */
            std::atomic<std::size_t> remaining_;
            /** @brief Stored mixed input handles. */
            handles_tuple_t handles;
            /** @brief Promise completing aggregated output future. */
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

    /**
     * @brief Aggregates a range of future_result handles.
     * @tparam InputIt Iterator to future_result-like values.
     * @param first Range begin iterator.
     * @param last Range end iterator.
     * @return Future with a vector of input result payloads.
     */
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

        /**
         * @brief Shared state for iterator-range future_result `when_all` aggregation.
         */
        struct WhenAllVectorCtx
        {
            /** @brief Remaining input futures not yet completed. */
            std::atomic<std::size_t> remaining_;
            /** @brief Stored input futures. */
            futures_vector_t futures;
            /** @brief Promise completing aggregated output future. */
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

    /**
     * @brief Aggregates a vector of future_result handles.
     * @tparam Future Future handle type.
     * @tparam Alloc Vector allocator type.
     * @param futures Vector of futures to wait for.
     * @return Future with a vector of input result payloads.
     */
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

        /**
         * @brief Shared state for vector-based future_result `when_all` aggregation.
         */
        struct WhenAllVectorCtx
        {
            /** @brief Remaining input futures not yet completed. */
            std::atomic<std::size_t> remaining_;
            /** @brief Stored input futures. */
            futures_vector_t futures;
            /** @brief Promise completing aggregated output future. */
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

    /**
     * @brief Aggregates a range of shared_result handles.
     * @tparam InputIt Iterator to shared_result-like values.
     * @param first Range begin iterator.
     * @param last Range end iterator.
     * @return Future with a vector of shared handles.
     */
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

        /**
         * @brief Shared state for iterator-range shared_result `when_all` aggregation.
         */
        struct WhenAllSharedVectorCtx
        {
            /** @brief Remaining input shared results not yet observed ready. */
            std::atomic<std::size_t> remaining_;
            /** @brief Stored input shared results. */
            shared_vector_t shareds;
            /** @brief Promise completing aggregated output future. */
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

    /**
     * @brief Aggregates a vector of shared_result handles.
     * @tparam SharedResult Shared handle type.
     * @tparam Alloc Vector allocator type.
     * @param shareds Vector of shared handles to wait for.
     * @return Future with a vector of shared handles.
     */
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

        /**
         * @brief Shared state for vector-based shared_result `when_all` aggregation.
         */
        struct WhenAllSharedVectorCtx
        {
            /** @brief Remaining input shared results not yet observed ready. */
            std::atomic<std::size_t> remaining_;
            /** @brief Stored input shared results. */
            shared_vector_t shareds;
            /** @brief Promise completing aggregated output future. */
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
