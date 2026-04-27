/**
 * @file voidify.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-05-19
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-05-19                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

namespace pco::detail
{

    /**
     * @brief C++11/14-compatible substitute for `std::void_t`.
     * @tparam T Any type expression used for SFINAE detection.
     */
    template <typename T>
    struct voidify
    {
        using type = void;
    };

} // namespace pco::detail
