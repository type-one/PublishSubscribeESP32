/**
 * @file voidify.h
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
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

namespace pco {
inline namespace cxx14_v1 {
namespace detail {

// TODO: replace with std::void_t after migration to C++17
template <typename T> struct voidify {
  using type = void;
};

} // namespace detail
} // namespace cxx14_v1
} // namespace pco
