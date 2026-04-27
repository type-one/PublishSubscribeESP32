/**
 * @file unique_function_fwd.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-08-16
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-08-16                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <cstddef>
#include <type_traits>

#include "small_unique_function_fwd.hpp"

namespace pco
{

    /**
     * @brief Forward declaration for move-only callable wrapper.
     * @tparam S Function signature type.
     */
    template <typename S>
    class unique_function;

    /**
     * @headerfile portable_concurrency/functional
     * @ingroup functional
     * @brief Move-only type erasure for arbitrary callable object.
    * @note Invocation is performed via `invoke(...)`.
     *
     * Implementation of
     * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4543.pdf proposal.
     *
     * @a portable_concurrency/functional_fwd header provides lightweight forward
     * declarations of this class template public interface allowing to use this
     * class as parameter or return type of function declarations or as member of
     * classes.
     */
    template <typename R, typename... A>
    class unique_function<R(A...)>
    {
    public:
        /**
         * @brief Creates an empty `unique_function` object.
         */
        unique_function() noexcept;
        /**
         * @brief Creates an empty `unique_function` object.
         * @param unused Null marker.
         */
        unique_function(std::nullptr_t) noexcept;

        /**
         * @brief Creates `unique_function` holding a callable object.
         * @tparam function_type Callable type.
         * @param f_arg Callable value to store.
         *
         * Result of the expression `INVOKE(f, std::declval<A>()...)` must be
         * convertible to type `R`. Where `INVOKE` is an operation defined in the
         * section 20.9.2 of the C++14 standard with additional overload defined in
         * the section 20.9.4.4.
         *
         * If type `F` is pointer to function, pointer to member function or
         * specialization of the `std::reference_wrapper` class template this
         * constructor is guaranteed to store passed function object in a small
         * internal buffer and perform no heap allocations or deallocations.
         *
         * This implementation additionally provides the small object optimization
         * guarantees for the `packaged_task` class template instantiations and for
         * any function type which is sent to a user provided executor via
         * ADL-discovered function `post`.
         */
        template <typename function_type,
            typename = std::enable_if_t<!std::is_same<std::decay_t<function_type>, unique_function>::value>>
        unique_function(function_type&& f_arg);

        /**
         * Destroys any stored function object.
         */
        ~unique_function();

        /**
         * @brief Move-constructs from another `unique_function`.
         * @param rhs Source object.
         */
        unique_function(unique_function&& rhs) noexcept;

        /**
         * @brief unique_function is move-only and not copy constructible.
         */
        unique_function(const unique_function&) = delete;
        /**
         * @brief unique_function is move-only and not copy assignable.
         */
        unique_function& operator=(const unique_function&) = delete;

        /**
         * @brief Move-assigns from another `unique_function`.
         * @param rhs Source object.
         * @return Reference to `*this`.
         */
        unique_function& operator=(unique_function&& rhs) noexcept;

        /**
         * @brief Constructs from small-buffer callable wrapper.
         * @param rhs Source small-buffer wrapper.
         */
        unique_function(detail::small_unique_function<R(A...)>&& rhs) noexcept;

        /**
         * @brief Assigns from small-buffer callable wrapper.
         * @param rhs Source small-buffer wrapper.
         * @return Reference to `*this`.
         */
        unique_function& operator=(detail::small_unique_function<R(A...)>&& rhs) noexcept;

        /**
         * @brief Converts this object to rvalue small-buffer callable wrapper.
         */
        operator detail::small_unique_function<R(A...)>&&() && noexcept;

        /**
         * @brief Invokes the stored callable without throwing.
         * @param args Call arguments.
         * @return Success value or invocation error.
         */
        [[nodiscard]] tools::expected<R, detail::function_invocation_error> invoke(A... args) const noexcept;

        /**
         * @brief Checks whether this object currently stores a callable.
         */
        explicit operator bool() const noexcept
        {
            return static_cast<bool>(func_);
        }

        /**
         * @brief Checks whether this object is empty.
         * @param unused Null marker.
         * @return true when object is empty, false otherwise.
         */
        bool operator==(std::nullptr_t) const noexcept
        {
            return !static_cast<bool>(func_);
        }

    private:
        template <typename function_type>
        unique_function(function_type&& f_arg, std::true_type unused);

        template <typename function_type>
        unique_function(function_type f_arg, std::false_type unused);

        detail::small_unique_function<R(A...)> func_;
    };

    extern template class unique_function<void()>;

} // namespace pco
