/**
 * @file cjsonpp_result.hpp
 * @brief Result and error types for exception-less cjsonpp APIs.
 */

#pragma once

#ifndef CJSONPP_RESULT_HPP_
#define CJSONPP_RESULT_HPP_

#include <cstdint>
#include <string>

#include "tools/expected.hpp"

namespace cjsonpp
{

    /**
     * @brief Error categories for exception-less cjsonpp operations.
     */
    enum class result_code : std::uint8_t
    {
        /** @brief Operation succeeded. */
        ok = 0,
        /** @brief JSON node type does not match requested operation/type. */
        invalid_type,
        /** @brief Requested object key or array index does not exist. */
        missing_item,
        /** @brief Parse operation failed due to invalid JSON input. */
        parse_error,
        /** @brief Invalid function argument supplied by caller. */
        invalid_argument,
        /** @brief Unexpected internal failure. */
        internal_error
    };

    /**
     * @brief Rich error payload returned by cjsonpp result APIs.
     */
    struct result_error
    {
        /** @brief Broad error category. */
        result_code code = result_code::ok;
        /** @brief Optional numeric detail (for example cJSON type code or index). */
        int detail = 0;
        /** @brief Human-readable context message. */
        std::string message;
    };

    /**
     * @brief Result alias used by cjsonpp APIs.
     *
     * This resolves to `std::expected<T, result_error>` on C++23 toolchains with
     * `<expected>` support, otherwise to the fallback `tools::expected` type.
     *
     * @tparam T Success payload type.
     */
    template <typename T>
    using cjsonpp_result = tools::expected<T, result_error>;

    /** @brief Convenience status alias for operations returning no payload. */
    using cjsonpp_status = cjsonpp_result<void>;

} // namespace cjsonpp

#endif // CJSONPP_RESULT_HPP_
