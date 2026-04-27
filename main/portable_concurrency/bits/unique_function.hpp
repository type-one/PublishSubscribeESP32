/**
 * @file unique_function.hpp
 * @brief unique_function implementation details.
 * @author Sergey Vidyuk
 * @date 2018-01-28
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-01-28                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <memory>
#include <type_traits>

#include "invoke.hpp"
#include "small_unique_function.hpp"
#include "unique_function_fwd.hpp"
namespace pco
{

    /**
     * @brief Default constructor implementation.
     */
    template <typename R, typename... A>
    unique_function<R(A...)>::unique_function() noexcept = default;

    /**
     * @brief nullptr constructor implementation.
     * @param unused Null marker.
     */
    template <typename R, typename... A>
    unique_function<R(A...)>::unique_function(std::nullptr_t unused) noexcept
    {
        static_cast<void>(unused);
    }

    template <typename R, typename... A>
    template <typename function_type, typename>
    unique_function<R(A...)>::unique_function(function_type&& f_arg)
        : unique_function(std::forward<function_type>(f_arg), detail::is_storable_t<std::decay_t<function_type>> {})
    {
    }

    template <typename R, typename... A>
    template <typename function_type>
    unique_function<R(A...)>::unique_function(function_type&& f_arg, std::true_type unused)
        : func_(std::forward<function_type>(f_arg))
    {
        static_cast<void>(unused);
    }

    template <typename R, typename... A>
    template <typename function_type>
    unique_function<R(A...)>::unique_function(function_type f_arg, std::false_type unused)
    {
        static_cast<void>(unused);
        if (detail::is_null(f_arg))
        {
            return;
        }
        using stored_function_type = std::decay_t<function_type>;
        auto stored_function = std::make_unique<stored_function_type>(std::move(f_arg));
        func_ = [func = std::move(stored_function)](A... a_arg)
        { return detail::invoke(*func, std::forward<A>(a_arg)...); };
    }

    template <typename R, typename... A>
    unique_function<R(A...)>::~unique_function() = default;

    template <typename R, typename... A>
    unique_function<R(A...)>::unique_function(unique_function<R(A...)>&&) noexcept = default;

    template <typename R, typename... A>
    unique_function<R(A...)>& unique_function<R(A...)>::operator=(unique_function<R(A...)>&&) noexcept = default;

    template <typename R, typename... A>
    tools::expected<R, detail::function_invocation_error> unique_function<R(A...)>::try_invoke(A... args) const noexcept
    {
        return func_.try_invoke(std::forward<A>(args)...);
    }

    template <typename R, typename... A>
    unique_function<R(A...)>::unique_function(detail::small_unique_function<R(A...)>&& rhs) noexcept
        : func_(std::move(rhs))
    {
    }

    template <typename R, typename... A>
    unique_function<R(A...)>& unique_function<R(A...)>::operator=(detail::small_unique_function<R(A...)>&& rhs) noexcept
    {
        func_ = std::move(rhs);
        return *this;
    }

    template <typename R, typename... A>
    unique_function<R(A...)>::operator detail::small_unique_function<R(A...)>&&() && noexcept
    {
        return std::move(func_);
    }

} // namespace pco
