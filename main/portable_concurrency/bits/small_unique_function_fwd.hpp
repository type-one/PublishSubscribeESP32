/**
 * @file small_unique_function_fwd.hpp
 * @brief Portable concurrency component.
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "tools/expected.hpp"

#include "tools/platform_detection.hpp"

namespace pco::detail
{

    /**
     * @brief Error codes produced by non-throwing callable invocation helpers.
     */
    enum class function_invocation_error : std::uint8_t
    {
        empty_target = 1,
        execution_failure,
    };

    /**
     * @brief Inline storage size used by small_unique_function.
     */
    constexpr size_t small_buffer_size = 5 * sizeof(void*);

    /**
     * @brief Inline storage alignment used by small_unique_function.
     */
    constexpr size_t small_buffer_align = alignof(void*);

    /**
     * @brief Small-buffer storage block for in-place callable objects.
     */
    struct small_buffer
    {
        alignas(small_buffer_align) std::array<std::byte, small_buffer_size> data;
    };

    /**
     * @brief Forward declaration for callable dispatch table.
     * @tparam R Callable return type.
     * @tparam A Callable argument types.
     */
    template <typename R, typename... A>
    struct callable_vtbl;

    /**
     * @brief Forward declaration for small-buffer move-only callable wrapper.
     * @tparam S Function signature type.
     */
    template <typename S>
    class small_unique_function;

    /**
     * @brief Move-only type-erased wrapper for small nothrow-movable callables.
     * @tparam R Callable return type.
     * @tparam A Callable argument types.
     */
    template <typename R, typename... A>
    class small_unique_function<R(A...)>
    {
    public:
        /**
         * @brief Creates an empty callable wrapper.
         */
        small_unique_function() noexcept;

        /**
         * @brief Creates an empty callable wrapper.
         * @param unused Null marker.
         */
        small_unique_function(std::nullptr_t) noexcept;

        /**
         * @brief Stores a callable object in the inline buffer.
         * @tparam function_type Callable type.
         * @param f_arg Callable object to store.
         */
        template <typename function_type,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<function_type>, small_unique_function>>>
        small_unique_function(function_type&& f_arg);

        /**
         * @brief Destroys the stored callable, if present.
         */
        ~small_unique_function();

        small_unique_function(const small_unique_function&) = delete;
        small_unique_function& operator=(const small_unique_function&) = delete;

        /**
         * @brief Move-constructs from another wrapper.
         * @param rhs Source wrapper.
         */
        small_unique_function(small_unique_function&& rhs) noexcept;

        /**
         * @brief Move-assigns from another wrapper.
         * @param rhs Source wrapper.
         * @return Reference to this object.
         */
        small_unique_function& operator=(small_unique_function&& rhs) noexcept;

        /**
         * @brief Invokes the stored callable.
         * @param args Invocation arguments.
         * @return Callable return value.
         */
        R operator()(A... args) const;

        /**
         * @brief Invokes the stored callable without throwing.
         * @param args Invocation arguments.
         * @return Success value or invocation error.
         */
        [[nodiscard]] tools::expected<R, function_invocation_error> try_invoke(A... args) const noexcept;

        /**
         * @brief Checks whether a callable is currently stored.
         * @return true when callable storage is non-empty.
         */
        explicit operator bool() const noexcept
        {
            return vtbl_ != nullptr;
        }

    private:
        mutable small_buffer buffer_ {};
        const callable_vtbl<R, A...>* vtbl_ = nullptr;
    };

    extern template class small_unique_function<void()>;

} // namespace pco::detail
