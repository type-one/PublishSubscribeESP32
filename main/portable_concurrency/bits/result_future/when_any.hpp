/**
 * @file when_any.hpp
 * @brief when_any combinator for racing multiple futures/shared_results.
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
    /**
     * @brief Returns an immediately-ready empty when_any aggregation.
     * @return Future holding index -1 and an empty tuple.
     */
    inline future_result<when_any_result<std::tuple<>>, result_error> when_any()
    {
        auto promise_and_future = make_result_promise<when_any_result<std::tuple<>>, result_error>();
        auto promise = std::move(promise_and_future.first);
        auto future = std::move(promise_and_future.second);
        promise.set_value(when_any_result<std::tuple<>> { static_cast<std::size_t>(-1), std::tuple<> {} });
        return future;
    }

    /**
     * @brief Races a variadic pack of result handles and resolves on first ready input.
     * @tparam Futures Result handle types (must share the same error type).
     * @param futures Handles to race.
     * @return Future containing winner index and preserved handle tuple.
     */
    template <typename... Futures>
    std::enable_if_t<detail::are_result_handles<Futures...>::value,
        future_result<when_any_result<std::tuple<std::decay_t<Futures>...>>,
            typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::error_type>>
    when_any(Futures&&... futures)
    {
        using first_future_t = std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>;
        using error_t = typename first_future_t::error_type;
        using futures_tuple_t = std::tuple<std::decay_t<Futures>...>;
        using any_result_t = when_any_result<futures_tuple_t>;

        static_assert((std::is_same<error_t, typename std::decay_t<Futures>::error_type>::value && ...),
            "All futures passed to v2::when_any must share the same error type");

        auto promise_and_future = make_result_promise<any_result_t, error_t>();
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

        constexpr auto npos = static_cast<std::size_t>(-1);
        std::size_t ready_index = npos;
        {
            std::size_t idx = 0;
            auto check_ready = [&](auto& future_item)
            {
                if (ready_index == npos && future_item.is_ready())
                {
                    ready_index = idx;
                }
                ++idx;
            };
            std::apply([&](auto&... future_item) { (check_ready(future_item), ...); }, future_tuple);
        }

        if (ready_index != npos)
        {
            promise.set_value(any_result_t { ready_index, std::move(future_tuple) });
            return combined_future;
        }

        // No futures ready yet: register continuation callbacks — no polling thread.
        // Shared context keeps the futures tuple and promise alive until the first
        // callback fires and atomically claims ownership via done_.
        /**
         * @brief Shared state for variadic `when_any` race resolution.
         */
        struct WhenAnyCtx
        {
            /** @brief Atomic winner flag set by first completing input. */
            std::atomic<bool> done_ { false };
            /** @brief Stored input futures/handles. */
            futures_tuple_t futures;
            /** @brief Promise completing when_any output future. */
            promise_result<any_result_t, error_t> promise;
            WhenAnyCtx(futures_tuple_t futures_arg, promise_result<any_result_t, error_t> promise_arg)
                : futures(std::move(futures_arg))
                , promise(std::move(promise_arg))
            {
            }
        };

        auto ctx = std::make_shared<WhenAnyCtx>(std::move(future_tuple), std::move(promise));

        std::size_t reg_idx = 0;
        auto register_one = [&](auto& future_item)
        {
            const std::size_t my_idx = reg_idx++;
            future_item.subscribe(
                [ctx, my_idx]() mutable
                {
                    bool expected = false;
                    if (ctx->done_.compare_exchange_strong(expected, true))
                    {
                        ctx->promise.set_value(any_result_t { my_idx, std::move(ctx->futures) });
                    }
                });
        };
        std::apply([&](auto&... future_item) { (register_one(future_item), ...); }, ctx->futures);

        return combined_future;
    }

    /**
     * @brief Races a range of result handles and resolves on first ready input.
     * @tparam InputIt Iterator to result-handle-like values.
     * @param first Range begin iterator.
     * @param last Range end iterator.
     * @return Future containing winner index and preserved handle vector.
     */
    template <typename InputIt>
    std::enable_if_t<detail::is_result_handle<typename std::iterator_traits<InputIt>::value_type>::value,
        future_result<when_any_result<std::vector<std::decay_t<typename std::iterator_traits<InputIt>::value_type>>>,
            typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::error_type>>
    when_any(InputIt first, InputIt last)
    {
        using future_t = std::decay_t<typename std::iterator_traits<InputIt>::value_type>;
        using error_t = typename future_t::error_type;
        using futures_vector_t = std::vector<future_t>;
        using any_result_t = when_any_result<futures_vector_t>;

        auto promise_and_future = make_result_promise<any_result_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        futures_vector_t futures(std::make_move_iterator(first), std::make_move_iterator(last));
        if (futures.empty())
        {
            promise.set_value(any_result_t { static_cast<std::size_t>(-1), futures_vector_t {} });
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

        constexpr auto npos = static_cast<std::size_t>(-1);
        std::size_t ready_index = npos;
        for (std::size_t idx = 0; idx < futures.size(); ++idx)
        {
            if (ready_index == npos && futures[idx].is_ready())
            {
                ready_index = idx;
            }
        }
        if (ready_index != npos)
        {
            promise.set_value(any_result_t { ready_index, std::move(futures) });
            return combined_future;
        }

        /**
         * @brief Shared state for iterator-range `when_any` race resolution.
         */
        struct WhenAnyVectorCtx
        {
            /** @brief Atomic winner flag set by first completing input. */
            std::atomic<bool> done_ { false };
            /** @brief Stored input futures/handles. */
            futures_vector_t futures;
            /** @brief Promise completing when_any output future. */
            promise_result<any_result_t, error_t> promise;

            WhenAnyVectorCtx(futures_vector_t input_futures, promise_result<any_result_t, error_t> input_promise)
                : futures(std::move(input_futures))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx = std::make_shared<WhenAnyVectorCtx>(std::move(futures), std::move(promise));

        for (std::size_t idx = 0; idx < ctx->futures.size(); ++idx)
        {
            ctx->futures[idx].subscribe(
                [ctx, idx]() mutable
                {
                    bool expected = false;
                    if (ctx->done_.compare_exchange_strong(expected, true))
                    {
                        ctx->promise.set_value(any_result_t { idx, std::move(ctx->futures) });
                    }
                });
        }

        return combined_future;
    }

    /**
     * @brief Races a vector of result handles and resolves on first ready input.
     * @tparam Future Result handle type.
     * @tparam Alloc Vector allocator type.
     * @param futures Vector of handles to race.
     * @return Future containing winner index and preserved handle vector.
     */
    template <typename Future, typename Alloc>
    std::enable_if_t<detail::is_result_handle<Future>::value,
        future_result<when_any_result<std::vector<Future, Alloc>>, typename Future::error_type>>
    when_any(std::vector<Future, Alloc> futures)
    {
        using future_t = Future;
        using error_t = typename future_t::error_type;
        using futures_vector_t = std::vector<future_t, Alloc>;
        using any_result_t = when_any_result<futures_vector_t>;

        auto promise_and_future = make_result_promise<any_result_t, error_t>();
        auto promise = std::move(promise_and_future.first);
        auto combined_future = std::move(promise_and_future.second);

        if (futures.empty())
        {
            promise.set_value(any_result_t { static_cast<std::size_t>(-1), futures_vector_t {} });
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

        constexpr auto npos = static_cast<std::size_t>(-1);
        std::size_t ready_index = npos;
        for (std::size_t idx = 0; idx < futures.size(); ++idx)
        {
            if (ready_index == npos && futures[idx].is_ready())
            {
                ready_index = idx;
            }
        }
        if (ready_index != npos)
        {
            promise.set_value(any_result_t { ready_index, std::move(futures) });
            return combined_future;
        }

        /**
         * @brief Shared state for vector-based `when_any` race resolution.
         */
        struct WhenAnyVectorCtx
        {
            /** @brief Atomic winner flag set by first completing input. */
            std::atomic<bool> done_ { false };
            /** @brief Stored input futures/handles. */
            futures_vector_t futures;
            /** @brief Promise completing when_any output future. */
            promise_result<any_result_t, error_t> promise;

            WhenAnyVectorCtx(futures_vector_t input_futures, promise_result<any_result_t, error_t> input_promise)
                : futures(std::move(input_futures))
                , promise(std::move(input_promise))
            {
            }
        };

        auto ctx = std::make_shared<WhenAnyVectorCtx>(std::move(futures), std::move(promise));

        for (std::size_t idx = 0; idx < ctx->futures.size(); ++idx)
        {
            ctx->futures[idx].subscribe(
                [ctx, idx]() mutable
                {
                    bool expected = false;
                    if (ctx->done_.compare_exchange_strong(expected, true))
                    {
                        ctx->promise.set_value(any_result_t { idx, std::move(ctx->futures) });
                    }
                });
        }

        return combined_future;
    }

} // namespace pco
