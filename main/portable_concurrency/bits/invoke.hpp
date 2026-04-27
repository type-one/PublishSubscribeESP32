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

    /**
     * @brief Detects std::reference_wrapper specializations.
     * @tparam T Candidate type.
     */
    template <typename T>
    struct is_reference_wrapper : std::false_type
    {
    };

    /**
     * @brief reference_wrapper specialization marker.
     * @tparam U Wrapped type.
     */
    template <typename U>
    struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
    {
    };

    /**
     * @brief INVOKE overload for member function pointers on base/derived object references.
     * @tparam B Declaring class type.
     * @tparam T Member function type.
     * @tparam D Object reference type.
     * @tparam A Invocation argument types.
     * @param pmf Pointer to member function.
     * @param ref Object reference.
     * @param args Invocation arguments.
     * @return Result of member function invocation.
     */
    template <typename B, typename T, typename D, typename... A,
        std::enable_if_t<std::is_function<T>::value && std::is_base_of<B, std::decay_t<D>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmf, D&& ref, A&&... args) noexcept(
        noexcept((std::forward<D>(ref).*pmf)(std::forward<A>(args)...)))
    {
        return (std::forward<D>(ref).*pmf)(std::forward<A>(args)...);
    }

    /**
     * @brief INVOKE overload for member function pointers on reference_wrapper objects.
     * @tparam B Declaring class type.
     * @tparam T Member function type.
     * @tparam RefWrap Reference-wrapper type.
     * @tparam A Invocation argument types.
     * @param pmf Pointer to member function.
     * @param ref Wrapped object reference.
     * @param args Invocation arguments.
     * @return Result of member function invocation.
     */
    template <typename B, typename T, typename RefWrap, typename... A,
        std::enable_if_t<std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmf, RefWrap&& ref, A&&... args) noexcept(
        noexcept((std::forward<RefWrap>(ref).get().*pmf)(std::forward<A>(args)...)))
    {
        return (std::forward<RefWrap>(ref).get().*pmf)(std::forward<A>(args)...);
    }

    /**
     * @brief INVOKE overload for member function pointers on pointer-like objects.
     * @tparam B Declaring class type.
     * @tparam T Member function type.
     * @tparam P Pointer-like object type.
     * @tparam A Invocation argument types.
     * @param pmf Pointer to member function.
     * @param ptr Pointer-like object.
     * @param args Invocation arguments.
     * @return Result of member function invocation.
     */
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

    /**
     * @brief INVOKE overload for member data pointers on base/derived object references.
     * @tparam B Declaring class type.
     * @tparam T Member data type.
     * @tparam D Object reference type.
     * @param pmd Pointer to member data.
     * @param ref Object reference.
     * @return Reference/value produced by member access.
     */
    template <typename B, typename T, typename D,
        std::enable_if_t<!std::is_function<T>::value && std::is_base_of<B, std::decay_t<D>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmd, D&& ref) noexcept(noexcept(std::forward<D>(ref).*pmd))
    {
        return std::forward<D>(ref).*pmd;
    }

    /**
     * @brief INVOKE overload for member data pointers on reference_wrapper objects.
     * @tparam B Declaring class type.
     * @tparam T Member data type.
     * @tparam RefWrap Reference-wrapper type.
     * @param pmd Pointer to member data.
     * @param ref Wrapped object reference.
     * @return Reference/value produced by member access.
     */
    template <typename B, typename T, typename RefWrap,
        std::enable_if_t<!std::is_function<T>::value && is_reference_wrapper<std::decay_t<RefWrap>>::value, int> = 0>
    decltype(auto) invoke(T B::*pmd, RefWrap&& ref) noexcept(noexcept(std::forward<RefWrap>(ref).get().*pmd))
    {
        return std::forward<RefWrap>(ref).get().*pmd;
    }

    /**
     * @brief INVOKE overload for member data pointers on pointer-like objects.
     * @tparam B Declaring class type.
     * @tparam T Member data type.
     * @tparam P Pointer-like object type.
     * @param pmd Pointer to member data.
     * @param ptr Pointer-like object.
     * @return Reference/value produced by member access.
     */
    template <typename B, typename T, typename P,
        std::enable_if_t<!std::is_function<T>::value && !is_reference_wrapper<std::decay_t<P>>::value
                && !std::is_base_of<B, std::decay_t<P>>::value,
            int>
        = 0>
    decltype(auto) invoke(T B::*pmd, P&& ptr) noexcept(noexcept((*std::forward<P>(ptr)).*pmd))
    {
        return (*std::forward<P>(ptr)).*pmd;
    }

    /**
     * @brief INVOKE overload for regular callable objects and function pointers.
     * @tparam functor_type Callable type.
     * @tparam A Invocation argument types.
     * @param functor_arg Callable object.
     * @param args Invocation arguments.
     * @return Result of callable invocation.
     */
    template <typename functor_type, class... A,
        std::enable_if_t<!std::is_member_pointer<std::decay_t<functor_type>>::value, int> = 0>
    decltype(auto) invoke(functor_type&& functor_arg, A&&... args) noexcept(
        noexcept(std::forward<functor_type>(functor_arg)(std::forward<A>(args)...)))
    {
        return std::forward<functor_type>(functor_arg)(std::forward<A>(args)...);
    }

} // namespace pco::detail
