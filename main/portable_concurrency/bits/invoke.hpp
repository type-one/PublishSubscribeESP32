/**
 * @file invoke.hpp
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

#include <functional>
#include <type_traits>
#include <utility>

namespace pco::detail
{

    template <typename T>
    struct is_reference_wrapper : std::false_type
    {
    };
    template <typename U>
    struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
    {
    };

    template <typename B, typename T, typename D, typename... A,
        std::enable_if_t<std::is_function<T>::value && std::is_base_of<B, std::decay_t<D>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmf, D&& ref, A&&... args) noexcept(
        noexcept((std::forward<D>(ref).*pmf)(std::forward<A>(args)...)))
    {
        return (std::forward<D>(ref).*pmf)(std::forward<A>(args)...);
    }

    template <typename B, typename T, typename RefWrap, typename... A,
        std::enable_if_t<std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmf, RefWrap&& ref, A&&... args) noexcept(
        noexcept((std::forward<RefWrap>(ref).get().*pmf)(std::forward<A>(args)...)))
    {
        return (std::forward<RefWrap>(ref).get().*pmf)(std::forward<A>(args)...);
    }

    template <typename B, typename T, typename P, typename... A,
        std::enable_if_t<std::is_function<T>::value && !is_reference_wrapper<std::decay_t<P>>::value
                && !std::is_base_of<B, std::decay_t<P>>::value,
            int>
        = 0>
    decltype(auto) invoke(T B::*pmf, P&& ptr, A&&... args) noexcept(
        noexcept(((*std::forward<P>(ptr)).*pmf)(std::forward<A>(args)...)))
    {
        return ((*std::forward<P>(ptr)).*pmf)(std::forward<A>(args)...);
    }

    template <typename B, typename T, typename D,
        std::enable_if_t<!std::is_function<T>::value && std::is_base_of<B, std::decay_t<D>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmd, D&& ref) noexcept(noexcept(std::forward<D>(ref).*pmd))
    {
        return std::forward<D>(ref).*pmd;
    }

    template <typename B, typename T, typename RefWrap,
        std::enable_if_t<!std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmd, RefWrap&& ref) noexcept(noexcept(std::forward<RefWrap>(ref).get().*pmd))
    {
        return std::forward<RefWrap>(ref).get().*pmd;
    }

    template <typename B, typename T, typename P,
        std::enable_if_t<!std::is_function<T>::value && !is_reference_wrapper<std::decay_t<P>>::value
                && !std::is_base_of<B, std::decay_t<P>>::value,
            int>
        = 0>
    decltype(auto) invoke(T B::*pmd, P&& ptr) noexcept(noexcept((*std::forward<P>(ptr)).*pmd))
    {
        return (*std::forward<P>(ptr)).*pmd;
    }

    template <typename functor_type, class... A,
        std::enable_if_t<!std::is_member_pointer<std::decay_t<functor_type>>::value, int> = 0>
    decltype(auto) invoke(functor_type&& functor_arg, A&&... args) noexcept(
        noexcept(std::forward<functor_type>(functor_arg)(std::forward<A>(args)...)))
    {
        return std::forward<functor_type>(functor_arg)(std::forward<A>(args)...);
    }

} // namespace pco::detail
