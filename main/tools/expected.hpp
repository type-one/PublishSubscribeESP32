//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//                                                                             //
// This software is provided 'as-is', without any express or implied           //
// warranty.In no event will the authors be held liable for any damages        //
// arising from the use of this software.                                      //
//                                                                             //
// Permission is granted to anyone to use this software for any purpose,       //
// including commercial applications, and to alter itand redistribute it       //
// freely, subject to the following restrictions :                             //
//                                                                             //
// 1. The origin of this software must not be misrepresented; you must not     //
// claim that you wrote the original software.If you use this software         //
// in a product, an acknowledgment in the product documentation would be       //
// appreciated but is not required.                                            //
// 2. Altered source versions must be plainly marked as such, and must not be  //
// misrepresented as being the original software.                              //
// 3. This notice may not be removed or altered from any source distribution.  //
//-----------------------------------------------------------------------------//

/**
 * @file expected.hpp
 * @brief Expected/unexpected compatibility layer.
 *
 * This header exposes `tools::expected`, `tools::unexpected`, and `tools::unexpect`
 * with an API aligned to `std::expected`.
 *
 * - On C++23 (and when `<expected>` is available), these names are aliases to the
 *   standard library implementation.
 * - Otherwise, a lightweight fallback implementation is provided.
 */

#pragma once

#if !defined(TOOLS_EXPECTED_HPP_)
#define TOOLS_EXPECTED_HPP_

#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>

#if (((__cplusplus >= 202302L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202302L))) && defined(__has_include))
#if __has_include(<expected>)
#include <expected>
#define TOOLS_HAS_STD_EXPECTED 1
#endif
#endif

namespace tools
{

#if defined(TOOLS_HAS_STD_EXPECTED)

    /**
     * @brief Alias to `std::expected` when available.
     *
     * @tparam T Value type.
     * @tparam E Error type.
     */
    template <typename T, typename E>
    using expected = std::expected<T, E>;

    /**
     * @brief Alias to `std::unexpected` when available.
     *
     * @tparam E Error type.
     */
    template <typename E>
    using unexpected = std::unexpected<E>;

    /** @brief Alias to `std::unexpect_t` when available. */
    using unexpect_t = std::unexpect_t;
    /** @brief Alias to `std::unexpect` when available. */
    inline constexpr unexpect_t unexpect = std::unexpect;

#else

    /**
     * @brief Container used to explicitly construct an error state.
     *
     * @tparam E Error type.
     */
    template <typename E>
    class unexpected
    {
    private:
        E m_error;

    public:
        /**
         * @brief Construct from a copy of the error.
         * @param e Error value.
         */
        explicit unexpected(const E& error_value)
            : m_error(error_value)
        {
        }

        /**
         * @brief Construct from a moved error.
         * @param e Error value.
         */
        explicit unexpected(E&& error_value)
            : m_error(std::move(error_value))
        {
        }

        /** @brief Mutable lvalue access to the stored error. */
        E& error() &
        {
            return m_error;
        }

        /** @brief Const lvalue access to the stored error. */
        [[nodiscard]] const E& error() const&
        {
            return m_error;
        }

        /** @brief Rvalue access to the stored error. */
        E&& error() &&
        {
            return std::move(m_error);
        }

        /** @brief Const rvalue access to the stored error. */
        [[nodiscard]] const E&& error() const&&
        {
            return std::move(m_error);
        }
    };

    /** @brief CTAD guide for `unexpected`. */
    template <typename E>
    unexpected(E) -> unexpected<std::decay_t<E>>;

    /** @brief Tag type used to construct `expected` in error state. */
    struct unexpect_t
    {
        /** @brief Default constructor. */
        explicit constexpr unexpect_t() = default;
    };

    /** @brief Tag value used to construct `expected` in error state. */
    inline constexpr unexpect_t unexpect {};

    /**
     * @brief Fallback `expected` implementation for non-C++23 toolchains.
     *
     * @tparam T Value type.
     * @tparam E Error type.
     */
    template <typename T, typename E>
    class expected
    {
    private:
        static constexpr std::size_t kValueIndex = 0U;
        static constexpr std::size_t kErrorIndex = 1U;
        std::variant<T, E> m_data;

    public:
        /**
         * @brief Construct a success result from a copied value.
         * @param value Value to store.
         */
        expected(const T& value)
            : m_data(value)
        {
        }

        /**
         * @brief Construct a success result from a moved value.
         * @param value Value to store.
         */
        expected(T&& value)
            : m_data(std::move(value))
        {
        }

        /**
         * @brief Construct an error result from `unexpected`.
         * @param err Error wrapper.
         */
        expected(const unexpected<E>& error_result)
            : m_data(error_result.error())
        {
        }

        /**
         * @brief Construct an error result from moved `unexpected`.
         * @param err Error wrapper.
         */
        expected(unexpected<E>&& error_result)
            : m_data(std::move(error_result).error())
        {
        }

        /**
         * @brief Construct an error result in-place.
         * @param args Arguments forwarded to `E` constructor.
         */
        template <typename... Args>
        explicit expected(unexpect_t unexpect_tag, Args&&... error_args)
            : m_data(std::in_place_index<kErrorIndex>, std::forward<Args>(error_args)...)
        {
            static_cast<void>(unexpect_tag);
        }

        /** @brief Check whether a success value is stored. */
        [[nodiscard]] bool has_value() const noexcept
        {
            return std::holds_alternative<T>(m_data);
        }

        /** @brief Equivalent to `has_value()`. */
        explicit operator bool() const noexcept
        {
            return has_value();
        }

        /** @brief Mutable lvalue access to the success value. */
        T& value() &
        {
            assert(has_value());
            return std::get<T>(m_data);
        }

        /** @brief Const lvalue access to the success value. */
        [[nodiscard]] const T& value() const&
        {
            assert(has_value());
            return std::get<T>(m_data);
        }

        /** @brief Rvalue access to the success value. */
        T&& value() &&
        {
            assert(has_value());
            return std::move(std::get<T>(m_data));
        }

        /** @brief Const rvalue access to the success value. */
        [[nodiscard]] const T&& value() const&&
        {
            assert(has_value());
            return std::move(std::get<T>(m_data));
        }

        /** @brief Mutable lvalue access to the error. */
        E& error() &
        {
            assert(!has_value());
            return std::get<E>(m_data);
        }

        /** @brief Const lvalue access to the error. */
        [[nodiscard]] const E& error() const&
        {
            assert(!has_value());
            return std::get<E>(m_data);
        }

        /** @brief Rvalue access to the error. */
        E&& error() &&
        {
            assert(!has_value());
            return std::move(std::get<E>(m_data));
        }

        /** @brief Const rvalue access to the error. */
        [[nodiscard]] const E&& error() const&&
        {
            assert(!has_value());
            return std::move(std::get<E>(m_data));
        }

        /** @brief Pointer-style access to the success value. */
        T* operator->()
        {
            return &value();
        }

        /** @brief Const pointer-style access to the success value. */
        const T* operator->() const
        {
            return &value();
        }

        /** @brief Dereference access to the success value. */
        T& operator*() &
        {
            return value();
        }

        /** @brief Const dereference access to the success value. */
        const T& operator*() const&
        {
            return value();
        }

        /** @brief Rvalue dereference access to the success value. */
        T&& operator*() &&
        {
            return std::move(value());
        }

        /** @brief Const rvalue dereference access to the success value. */
        const T&& operator*() const&&
        {
            return std::move(value());
        }

        /**
         * @brief Return the value or a provided default.
         * @param default_value Value used when in error state.
         */
        template <typename U>
        T value_or(U&& default_value) const&
        {
            return has_value() ? std::get<T>(m_data) : static_cast<T>(std::forward<U>(default_value));
        }

        /**
         * @brief Return the value (moving when possible) or a provided default.
         * @param default_value Value used when in error state.
         */
        template <typename U>
        T value_or(U&& default_value) &&
        {
            return has_value() ? std::move(std::get<T>(m_data)) : static_cast<T>(std::forward<U>(default_value));
        }
    };

    /**
     * @brief `expected` specialization for operations that return no value.
     *
     * @tparam E Error type.
     */
    template <typename E>
    class expected<void, E>
    {
    private:
        static constexpr std::size_t kStatusIndex = 0U;
        static constexpr std::size_t kErrorIndex = 1U;
        std::variant<std::monostate, E> m_data;

    public:
        /** @brief Construct a success status. */
        expected()
            : m_data(std::monostate {})
        {
        }

        /**
         * @brief Construct an error status from `unexpected`.
         * @param err Error wrapper.
         */
        expected(const unexpected<E>& error_result)
            : m_data(error_result.error())
        {
        }

        /**
         * @brief Construct an error status from moved `unexpected`.
         * @param err Error wrapper.
         */
        expected(unexpected<E>&& error_result)
            : m_data(std::move(error_result).error())
        {
        }

        /**
         * @brief Construct an error status in-place.
         * @param args Arguments forwarded to `E` constructor.
         */
        template <typename... Args>
        explicit expected(unexpect_t unexpect_tag, Args&&... error_args)
            : m_data(std::in_place_index<kErrorIndex>, std::forward<Args>(error_args)...)
        {
            static_cast<void>(unexpect_tag);
        }

        /** @brief Check whether the status is success. */
        [[nodiscard]] bool has_value() const noexcept
        {
            return std::holds_alternative<std::monostate>(m_data);
        }

        /** @brief Equivalent to `has_value()`. */
        explicit operator bool() const noexcept
        {
            return has_value();
        }

        /**
         * @brief Validate success state (debug assert only).
         */
        void value() const
        {
            assert(has_value());
        }

        /** @brief Mutable lvalue access to the error. */
        E& error() &
        {
            assert(!has_value());
            return std::get<E>(m_data);
        }

        /** @brief Const lvalue access to the error. */
        [[nodiscard]] const E& error() const&
        {
            assert(!has_value());
            return std::get<E>(m_data);
        }

        /** @brief Rvalue access to the error. */
        E&& error() &&
        {
            assert(!has_value());
            return std::move(std::get<E>(m_data));
        }

        /** @brief Const rvalue access to the error. */
        [[nodiscard]] const E&& error() const&&
        {
            assert(!has_value());
            return std::move(std::get<E>(m_data));
        }
    };

#endif

} // namespace tools

#endif // TOOLS_EXPECTED_HPP_
