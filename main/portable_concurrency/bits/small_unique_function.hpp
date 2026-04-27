/**
 * @file small_unique_function.hpp
 * @brief small_unique_function implementation details.
 * @author Sergey Vidyuk
 * @date 2018-02-01
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-02-01                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <new>
#include <type_traits>

#include "invoke.hpp"
#include "small_unique_function_fwd.hpp"
#include "voidify.hpp"

namespace pco::detail
{

    /**
     * @brief Throws bad_function_call for empty callable invocation.
     * @note Marked noreturn because invocation always fails.
     */
    [[noreturn]] void throw_bad_func_call();

    // R(A...) is incomplete type so it is illegal to use sizeof(F)/alignof(F) for
    // decayed function reference to turn it into function pointer which is
    // implicitly constructible from function reference
    /**
     * @brief Helper type used to validate whether a callable can be stored in the SBO buffer.
     * @tparam F Candidate callable type.
     */
    template <typename F>
    using is_storable_helper = std::conditional_t<std::is_function<F>::value, F*, F>;

    /**
     * @brief Compile-time predicate for small-buffer storable callables.
     * @tparam F Candidate callable type.
     */
    template <typename F>
    using is_storable_t = std::integral_constant<bool,
        alignof(is_storable_helper<F>) <= small_buffer_align && sizeof(is_storable_helper<F>) <= small_buffer_size
            && std::is_nothrow_move_constructible<F>::value>;

    /**
     * @brief Convenience alias for plain function pointers.
     * @tparam R Return type.
     * @tparam A Argument pack.
     */
    template <typename R, typename... A>
    using func_ptr_t = R (*)(A...);

    /**
     * @brief Virtual-table-like dispatch table for callable lifecycle and invocation.
     * @tparam R Callable return type.
     * @tparam A Callable argument types.
     */
    template <typename R, typename... A>
    struct callable_vtbl
    {
        func_ptr_t<void, small_buffer&> destroy;
        func_ptr_t<void, small_buffer&, small_buffer*> move;
        func_ptr_t<R, small_buffer&, A...> call;
    };

    /**
     * @brief Casts a raw small buffer to the stored callable type.
     * @tparam F Stored callable type.
     * @param buf Small-buffer storage.
     * @return Reference to object stored in the buffer.
     */
    template <typename F>
    F& small_buffer_cast(small_buffer& buf)
    {
        return *std::launder(reinterpret_cast<F*>(buf.data.data()));
    }

    /**
     * @brief Builds static dispatch table for callable type F.
     * @tparam F Stored callable type.
     * @tparam R Callable return type.
     * @tparam A Callable argument types.
     * @return Reference to static callable dispatch table.
     */
    template <typename F, typename R, typename... A>
    const callable_vtbl<R, A...>& get_callable_vtbl()
    {
        static_assert(is_storable_t<F>::value, "Can't embed object into small buffer");
        static const callable_vtbl<R, A...> res
            = { [](small_buffer& buf) { small_buffer_cast<F>(buf).~F(); }, [](small_buffer& src, small_buffer* dst)
                  { new (dst->data.data()) F { std::move(small_buffer_cast<F>(src)) }; },
                  [](small_buffer& buf, A... a_arg) -> R
                  {
#if !defined(_MSC_VER)
                      // Must not perform conversions marked as explicit but must cast
                      // anything to `void` if `R` is `void`
                      return static_cast<std::conditional_t<std::is_void<R>::value, void,
                          decltype(pco::detail::invoke(small_buffer_cast<F>(buf), std::forward<A>(a_arg)...))>>(
                          pco::detail::invoke(small_buffer_cast<F>(buf), std::forward<A>(a_arg)...));
#else
                      return static_cast<R>(pco::detail::invoke(small_buffer_cast<F>(buf), std::forward<A>(a_arg)...));
#endif
                  } };
        return res;
    }

    /**
     * @brief Detects whether type T supports comparison with nullptr.
     * @tparam T Candidate type.
     */
    template <typename T, typename = void>
    struct is_null_comparable : std::false_type
    {
    };

    /**
     * @brief Specialization for types comparable with nullptr.
     * @tparam T Candidate type.
     */
    template <typename T>
    struct is_null_comparable<T, typename voidify<decltype(std::declval<T>() == nullptr)>::type> : std::true_type
    {
    };

    /**
     * @brief Null-check fallback for types that are not nullptr-comparable.
     * @tparam T Candidate type.
     * @param unused Value ignored by this overload.
     * @return Always false.
     */
    template <typename T, std::enable_if_t<!is_null_comparable<T>::value, int> = 0>
    bool is_null(const T& unused) noexcept
    {
        static_cast<void>(unused);
        return false;
    }

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 6
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
    /**
     * @brief Null-check overload for nullptr-comparable values.
     * @tparam T Candidate type.
     * @param val Value to test against nullptr.
     * @return true when val compares equal to nullptr.
     */
    template <typename T, std::enable_if_t<is_null_comparable<T>::value, int> = 0>
    bool is_null(const T& val) noexcept
    {
        return val == nullptr;
    }
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 6
#pragma GCC diagnostic pop
#endif

    /**
     * @brief Default constructor implementation.
     */
    template <typename R, typename... A>
    small_unique_function<R(A...)>::small_unique_function() noexcept = default;

    /**
     * @brief nullptr constructor implementation.
     * @param unused Null marker.
     */
    template <typename R, typename... A>
    small_unique_function<R(A...)>::small_unique_function(std::nullptr_t unused) noexcept
    {
        static_cast<void>(unused);
    }

    /**
     * @brief Callable constructor implementation.
     */
    template <typename R, typename... A>
    template <typename F, typename>
    small_unique_function<R(A...)>::small_unique_function(F&& f_arg)
    {
        static_assert(is_storable_t<std::decay_t<F>>::value, "Can't embed object into small_unique_function");
        if (detail::is_null(f_arg))
        {
            return;
        }
        new (buffer_.data.data()) std::decay_t<F> { std::forward<F>(f_arg) };
        vtbl_ = &detail::get_callable_vtbl<std::decay_t<F>, R, A...>();
    }

    /**
     * @brief Destructor implementation.
     */
    template <typename R, typename... A>
    small_unique_function<R(A...)>::~small_unique_function()
    {
        if (vtbl_)
        {
            vtbl_->destroy(buffer_);
        }
    }

    /**
     * @brief Move constructor implementation.
     */
    template <typename R, typename... A>
    small_unique_function<R(A...)>::small_unique_function(small_unique_function<R(A...)>&& rhs) noexcept
    {
        if (rhs.vtbl_)
        {
            rhs.vtbl_->move(rhs.buffer_, &buffer_);
        }
        vtbl_ = rhs.vtbl_;
    }

    /**
     * @brief Move assignment implementation.
     */
    template <typename R, typename... A>
    small_unique_function<R(A...)>& small_unique_function<R(A...)>::operator=(
        small_unique_function<R(A...)>&& rhs) noexcept
    {
        if (vtbl_)
        {
            vtbl_->destroy(buffer_);
        }
        if (rhs.vtbl_)
        {
            rhs.vtbl_->move(rhs.buffer_, &buffer_);
        }
        vtbl_ = rhs.vtbl_;
        return *this;
    }

    /**
     * @brief Invocation operator implementation.
     */
    template <typename R, typename... A>
    R small_unique_function<R(A...)>::operator()(A... args) const
    {
        if (!vtbl_)
        {
            throw_bad_func_call();
        }
        return vtbl_->call(buffer_, std::forward<A>(args)...);
    }

    /**
     * @brief Non-throwing invocation implementation.
     */
    template <typename R, typename... A>
    tools::expected<R, function_invocation_error> small_unique_function<R(A...)>::try_invoke(A... args) const noexcept
    {
        if (!vtbl_)
        {
            return tools::unexpected<function_invocation_error>(function_invocation_error::empty_target);
        }

#if defined(CPP_EXCEPTIONS_ENABLED)
        try
        {
#endif
            if constexpr (std::is_void_v<R>)
            {
                vtbl_->call(buffer_, std::forward<A>(args)...);
                return tools::expected<void, function_invocation_error> {};
            }
            else
            {
                return tools::expected<R, function_invocation_error>(vtbl_->call(buffer_, std::forward<A>(args)...));
            }
#if defined(CPP_EXCEPTIONS_ENABLED)
        }
        catch (...)
        {
            return tools::unexpected<function_invocation_error>(function_invocation_error::execution_failure);
        }
#endif
    }

} // namespace pco::detail
