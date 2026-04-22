/**
 * @file expected_compat.h
 * @brief Portable concurrency component.
 * @author Laurent Lardinois
 * @date April 2026
 * @note Refactored with the help of Copilot.
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
// Refactored with the help of Copilot.                                        //
//-----------------------------------------------------------------------------//

#pragma once

// Reuse the first-party tools::expected / tools::unexpected implementation.
// tools/expected.hpp internally selects std::expected on C++23 and provides
// a std::variant-backed fallback on earlier standards.
#include "tools/expected.hpp"

namespace portable_concurrency {
inline namespace cxx14_v1 {
    namespace v2 {

        template <typename T, typename E>
        using expected = tools::expected<T, E>;

        template <typename E>
        using unexpected = tools::unexpected<E>;

        template <typename T, typename E>
        expected<T, E> make_expected_error(E error)
        {
            return tools::unexpected<E>(std::move(error));
        }

        template <typename E>
        expected<void, E> make_expected_error_void(E error)
        {
            return tools::unexpected<E>(std::move(error));
        }

    } // namespace v2
} // namespace cxx14_v1
} // namespace portable_concurrency