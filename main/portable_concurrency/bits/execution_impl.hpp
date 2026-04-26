/**
 * @file execution_impl.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-10-27
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-10-27                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

#include <type_traits>
#include <utility>

namespace pco {

/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 * @brief Trait which can be specialized for user provided types to enable using
 * them as executors.
 *
 * Some functions in the portable_concurrency library allow to specify executor
 * to perform requested actions. Those function only participate in overload
 * resolution if this trait is specialized for the executor argument and
 * `is_executor<E>::value` is `true`.
 *
 * In addition to this trait specialization there should be @ref post function
 * provided in order to use executor type E with portable_concurrency library.
 *
 * @sa post
 */
template <typename E> struct is_executor : std::false_type {};

/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 * @brief Trivial executor which evaluates task immediately in the invocation
 * thread
 *
 * This executor is used by `future::then` and other continuation-related family
 * of functions by default when no executor is specified explicitly.
 */
class inplace_executor_t {
private:
  template <typename Task> friend void post(inplace_executor_t exec, Task &&task) {
    static_cast<void>(exec);
    std::forward<Task>(task)();
  }
};

/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 * @brief Global instance of trivial in-place executor type
 *
 * @sa inplace_executor
 */
constexpr inplace_executor_t inplace_executor;

template <> struct is_executor<inplace_executor_t> : std::true_type {};

} // namespace pco

// Documentation-only declaration for the ADL customization point used by
// user-provided executors. Doxygen sees this block when configured with the
// DOXYGEN define, while normal builds ignore it. The declaration is
// conceptual: actual overloads can take parameters by value or reference as
// long as an unqualified call to post(exec, func) is valid via ADL.
#ifdef DOXYGEN
/**
 * @headerfile portable_concurrency/execution
 * @ingroup execution
 *
 * Function which must be ADL discoverable for user-provided executor classes.
 * This function must schedule execution of the function object from the second
 * argument on the executor provided with the first argument.
 *
 * The submitted function object must be invocable with signature `void()` and
 * is typically move-constructed into the executor path. Do not require
 * copying.
 *
 * @sa pco::is_executor
 */
template <typename Executor, typename Functor>
void post(Executor&& exec, Functor&& func);
#endif
