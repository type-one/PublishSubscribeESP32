/**
 * @file then.hpp
 * @brief Continuation method support.
 * @author Laurent Lardinois (with the help of Github Copilot), Sergey Vidyuk
 * @date April 2026
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

#pragma once

// Continuation methods (then_value, then_error, then_result, then, next) are
// implemented as member methods in future_result<T,E> and shared_result<T,E>.
// They are included transitively through those classes.
//
// This file serves as a semantic facade for continuation-related functionality.
