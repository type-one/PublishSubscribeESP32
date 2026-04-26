/**
 * @file concurrency_type_traits.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-06-14
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-06-14                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <type_traits>

#include "fwd.hpp"
#include "voidify.hpp"

namespace pco::detail
{

#if defined(__cpp_lib_is_invocable)
    /**
     * @brief Alias to invocability result trait (`std::invoke_result` path).
     * @tparam F Callable type.
     * @tparam A Argument types.
     */
    template <typename F, typename... A>
    using invoke_result = std::invoke_result<F, A...>;
#else
    /**
     * @brief Alias to invocability result trait (`std::result_of` fallback path).
     * @tparam F Callable type.
     * @tparam A Argument types.
     */
    template <typename F, typename... A>
    using invoke_result = std::result_of<F(A...)>;
#endif

    /**
     * @brief Convenience alias for invoke result type.
     * @tparam F Callable type.
     * @tparam A Argument types.
     */
    template <typename F, typename... A>
    using invoke_result_t = typename invoke_result<F, A...>::type;

    /**
     * @brief Detects whether type `U` is an instantiation of template `T`.
     * @tparam T Template template parameter.
     * @tparam U Candidate type.
     */
    template <template <typename...> class T, typename U>
    struct is_instantiation_of : std::false_type
    {
    };

    /**
     * @brief Positive specialization for template instantiations.
     * @tparam T Template template parameter.
     * @tparam U Template argument types.
     */
    template <template <typename...> class T, typename... U>
    struct is_instantiation_of<T, T<U...>> : std::true_type
    {
    };

    // is_future

    /** @brief Trait detecting `future<T>` instantiations. */
    template <typename T>
    using is_unique_future = std::integral_constant<bool, is_instantiation_of<future, T>::value>;

    /** @brief Trait detecting `shared_future<T>` instantiations. */
    template <typename T>
    using is_shared_future = std::integral_constant<bool, is_instantiation_of<shared_future, T>::value>;

    /** @brief Trait detecting either unique or shared future types. */
    template <typename T>
    using is_future = std::integral_constant<bool, is_unique_future<T>::value || is_shared_future<T>::value>;

    // are_futures

    /**
     * @brief Variadic trait checking that all types are future handles.
     * @tparam F Candidate types.
     */
    template <typename... F>
    struct are_futures;

    /** @brief Empty-pack specialization that evaluates to true. */
    template <>
    struct are_futures<> : std::true_type
    {
    };

    /**
     * @brief Recursive specialization requiring all head/tail types to be futures.
     * @tparam F0 Head type.
     * @tparam F Tail types.
     */
    template <typename F0, typename... F>
    struct are_futures<F0, F...> : std::integral_constant<bool, is_future<F0>::value && are_futures<F...>::value>
    {
    };

    // remove_future

    /**
     * @brief Maps future-like type to contained value type; passthrough otherwise.
     * @tparam T Input type.
     */
    template <typename T>
    struct remove_future
    {
        using type = T;
    };

    /**
     * @brief Specialization extracting value type from `future<T>`.
     * @tparam T Future value type.
     */
    template <typename T>
    struct remove_future<future<T>>
    {
        using type = T;
    };

    /**
     * @brief Specialization extracting value type from `shared_future<T>`.
     * @tparam T Shared-future value type.
     */
    template <typename T>
    struct remove_future<shared_future<T>>
    {
        using type = T;
    };

    template <typename T>
    using remove_future_t = typename remove_future<T>::type;

    // add_future

    /**
     * @brief Wraps a value type into `future<T>`; preserves future-like types.
     * @tparam T Input type.
     */
    template <typename T>
    struct add_future
    {
        using type = future<T>;
    };

    /**
     * @brief Passthrough specialization for `future<T>`.
     * @tparam T Future value type.
     */
    template <typename T>
    struct add_future<future<T>>
    {
        using type = future<T>;
    };

    /**
     * @brief Passthrough specialization for `shared_future<T>`.
     * @tparam T Shared-future value type.
     */
    template <typename T>
    struct add_future<shared_future<T>>
    {
        using type = shared_future<T>;
    };

    template <typename T>
    using add_future_t = typename add_future<T>::type;

    // cnt_future_t

    /**
     * @brief Continuation result trait for continuation taking argument `Arg`.
     * @tparam Func Continuation callable type.
     * @tparam Arg Continuation argument type.
     */
    template <typename Func, typename Arg>
    struct cnt_result : invoke_result<Func, Arg>
    {
    };

    /**
     * @brief Specialization for continuations that take no argument.
     * @tparam Func Continuation callable type.
     */
    template <typename Func>
    struct cnt_result<Func, void> : invoke_result<Func>
    {
    };

    template <typename Func, typename Arg>
    using cnt_result_t = typename cnt_result<Func, Arg>::type;

    /**
     * @brief Future-wrapped continuation result type.
     * @tparam Func Continuation callable type.
     * @tparam Arg Continuation argument type.
     */
    template <typename Func, typename Arg>
    using cnt_future_t = add_future_t<cnt_result_t<Func, Arg>>;

    // deduce result type for interruptible continuation

    /**
     * @brief Deduces `promise<R>` result type from interruptible continuation signatures.
     * @tparam Future Source future type.
     */
    template <typename Future>
    struct promise_deducer
    {
        static_assert(is_future<Future>::value, "Future parameter must be future<T> or shared_future<T>");

        template <typename R>
        static R deduce(void (*)(promise<R>, Future));

        template <typename R, typename C>
        static R deduce_method(void (C::*)(promise<R>, Future) const);

        template <typename R, typename C>
        static R deduce_method(void (C::*)(promise<R>, Future));

        template <typename F>
        static auto deduce(F) -> decltype(deduce_method(&F::operator()));
    };

    /**
     * @brief Primary template for promise argument extraction (SFINAE fallback).
     * @tparam F Callable type.
     * @tparam T Source future type.
     * @tparam Unused SFINAE hook.
     */
    template <typename F, typename T, typename = void>
    struct promise_arg
    {
    };

    /**
     * @brief Extracts promise result argument type for valid interruptible callables.
     * @tparam Func Callable type.
     * @tparam Future Source future type.
     */
    template <typename Func, typename Future>
    struct promise_arg<Func, Future,
        typename voidify<decltype(promise_deducer<Future>::deduce(std::declval<std::decay_t<Func>>()))>::type>
    {
        using type = decltype(promise_deducer<Future>::deduce(std::declval<std::decay_t<Func>>()));
    };

    template <typename Func, typename Future>
    using promise_arg_t = typename promise_arg<Func, Future>::type;

    // variadic helper swallow

    /**
     * @brief Utility sink type forcing evaluation of pack-expanded expressions.
     */
    struct swallow
    {
        /**
         * @brief Accepts arbitrary arguments and discards them.
         */
        swallow(...)
        {
        }
    };

} // namespace pco::detail
