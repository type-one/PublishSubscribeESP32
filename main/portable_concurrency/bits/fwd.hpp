/**
 * @file fwd.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-06-14
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-06-14                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#pragma once

#include <cstdint>
#include <memory>

namespace pco
{

    /**
     * @brief Tag type selecting canceler-aware continuation overloads.
     */
    struct canceler_arg_t
    {
    };

    /**
     * @brief Global tag value used to select canceler-aware overloads.
     */
    constexpr canceler_arg_t canceler_arg = {};

    /**
     * @brief Wait result status for time-bounded future waits.
     */
    enum class future_status : std::uint8_t
    {
        ready,
        timeout
    };

    /**
     * @brief Forward declaration of move-only future type.
     * @tparam T Future value type.
     */
    template <typename T>
    class future;

    /**
     * @brief Forward declaration of task wrapper producing futures.
     * @tparam Signature Task signature type.
     */
    template <typename Signature>
    class packaged_task;

    /**
     * @brief Forward declaration of producer-side future state handle.
     * @tparam T Future value type.
     */
    template <typename T>
    class promise;

    /**
     * @brief Forward declaration of copyable shared future type.
     * @tparam T Future value type.
     */
    template <typename T>
    class shared_future;

    namespace detail
    {
        /**
         * @brief Shared state backing `future` and `promise`.
         * @tparam T Stored value type.
         */
        template <typename T>
        struct future_state;

        /**
         * @brief Accesses shared state from lvalue `future`.
         * @tparam T Future value type.
         * @param f Future object.
         * @return Reference to shared state smart pointer.
         */
        template <typename T>
        std::shared_ptr<future_state<T>>& state_of(future<T>&);

        /**
         * @brief Extracts shared state from rvalue `future`.
         * @tparam T Future value type.
         * @param f Future object.
         * @return Shared state smart pointer.
         */
        template <typename T>
        std::shared_ptr<future_state<T>> state_of(future<T>&&);

        /**
         * @brief Accesses shared state from lvalue `shared_future`.
         * @tparam T Future value type.
         * @param f Shared future object.
         * @return Reference to shared state smart pointer.
         */
        template <typename T>
        std::shared_ptr<future_state<T>>& state_of(shared_future<T>&);

        /**
         * @brief Extracts shared state from rvalue `shared_future`.
         * @tparam T Future value type.
         * @param f Shared future object.
         * @return Shared state smart pointer.
         */
        template <typename T>
        std::shared_ptr<future_state<T>> state_of(shared_future<T>&&);
    } // namespace detail

} // namespace pco
