/**
 * @file packaged_task_result.hpp
 * @brief Portable concurrency component.
 * @author Laurent Lardinois (with the help of Github Copilot), Sergey Vidyuk
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
// Derived from Sergey Vidyuk's work with the help of Copilot.                 //
//-----------------------------------------------------------------------------//

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "result_future.hpp"

namespace pco
{

    /**
     * @brief Move-only task wrapper producing result-based futures.
     * @tparam Signature Callable signature, e.g. R(A...).
     */
    template <typename Signature>
    class packaged_task_result;

    namespace detail
    {

        /**
         * @brief Maps callable return type to packaged_task_result error type.
         * @tparam R Callable return type.
         */
        template <typename R>
        struct packaged_task_result_error
        {
            using type = result_error;
        };

        /**
         * @brief Error-type mapping for nested future_result return values.
         * @tparam T Nested value type.
         * @tparam E Nested error type.
         */
        template <typename T, typename E>
        struct packaged_task_result_error<future_result<T, E>>
        {
            using type = E;
        };

        /**
         * @brief Error-type mapping for nested shared_result return values.
         * @tparam T Nested value type.
         * @tparam E Nested error type.
         */
        template <typename T, typename E>
        struct packaged_task_result_error<shared_result<T, E>>
        {
            using type = E;
        };

        /**
         * @brief Maps callable return type to externally visible future type.
         * @tparam R Callable return type.
         * @tparam E Error type.
         */
        template <typename R, typename E>
        struct packaged_task_result_future
        {
            using type = future_result<R, E>;
        };

        /**
         * @brief Future-type mapping passthrough for nested future_result return values.
         * @tparam T Nested value type.
         * @tparam E Nested error type.
         */
        template <typename T, typename E>
        struct packaged_task_result_future<future_result<T, E>, E>
        {
            using type = future_result<T, E>;
        };

        /**
         * @brief Future-type mapping passthrough for nested shared_result return values.
         * @tparam T Nested value type.
         * @tparam E Nested error type.
         */
        template <typename T, typename E>
        struct packaged_task_result_future<shared_result<T, E>, E>
        {
            using type = shared_result<T, E>;
        };

        /**
         * @brief Maps callable return type to the produced value type.
         * @tparam R Callable return type.
         */
        template <typename R>
        struct packaged_task_result_value
        {
            using type = R;
        };

        /**
         * @brief Value-type mapping for nested future_result return values.
         * @tparam T Nested value type.
         * @tparam E Nested error type.
         */
        template <typename T, typename E>
        struct packaged_task_result_value<future_result<T, E>>
        {
            using type = T;
        };

        /**
         * @brief Value-type mapping for nested shared_result return values.
         * @tparam T Nested value type.
         * @tparam E Nested error type.
         */
        template <typename T, typename E>
        struct packaged_task_result_value<shared_result<T, E>>
        {
            using type = T;
        };

    } // namespace detail

    /**
     * @brief Result-based packaged task specialization for signature R(A...).
     * @tparam R Callable return type.
     * @tparam A Callable argument types.
     */
    template <typename R, typename... A>
    class packaged_task_result<R(A...)>
    {
    private:
        using error_type = typename detail::packaged_task_result_error<R>::type;
        using value_type = typename detail::packaged_task_result_value<R>::type;
        using future_type = typename detail::packaged_task_result_future<R, error_type>::type;

        static_assert(!std::is_reference_v<value_type>,
            "portable_concurrency does not support reference return types. "
            "Use pointers or std::reference_wrapper<T> instead. "
            "This design constraint avoids move-semantics ambiguity and lifetime issues.");

        /**
         * @brief Polymorphic state interface for task storage and invocation.
         */
        struct state_base
        {
            state_base() = default;
            state_base(const state_base&) = delete;
            state_base& operator=(const state_base&) = delete;
            state_base(state_base&&) = delete;
            state_base& operator=(state_base&&) = delete;
            virtual ~state_base() = default;
            virtual future_type get_future() = 0;
            virtual void run(A... args) = 0;
        };

        /**
         * @brief State implementation for callable type F.
         * @tparam F Stored callable type.
         */
        template <typename F>
        struct state_impl final : state_base
        {
            /**
             * @brief Stores callable and initializes associated promise state.
             * @param function_arg Callable to execute once.
             */
            explicit state_impl(F&& function_arg)
                : stored_function(std::move(function_arg))
            {
            }

            /**
             * @brief Obtains consumer future for the stored promise.
             * @return Future or shared_result depending on R mapping.
             */
            future_type get_future() override
            {
                auto future_value = promise.get_future();
                if constexpr (detail::is_shared_result<std::decay_t<R>>::value)
                {
                    return std::move(future_value).share();
                }
                else
                {
                    return future_value;
                }
            }

            /**
             * @brief Executes stored callable once and fulfills the promise.
             * @param args Arguments forwarded to callable.
             */
            void run(A... args) override
            {
                if (!stored_function.has_value())
                {
                    return;
                }

                auto local_function = std::move(*stored_function);
                stored_function.reset();

                invoke_and_set(local_function, std::forward<A>(args)...);
            }

            /**
             * @brief Invokes callable and resolves promise with value, error, or nested handle.
             * @tparam Fn Callable type.
             * @tparam Args Callable argument types.
             * @param callable Callable instance.
             * @param call_args Arguments forwarded to callable.
             */
            template <typename Fn, typename... Args>
            void invoke_and_set(Fn& callable, Args&&... call_args)
            {
                using raw_result_t = std::decay_t<std::invoke_result_t<Fn&, Args...>>;
                if constexpr (detail::is_result_handle<raw_result_t>::value)
                {
                    detail::resolve_nested_handle(promise, std::invoke(callable, std::forward<Args>(call_args)...));
                }
                else if constexpr (std::is_void<value_type>::value)
                {
                    std::invoke(callable, std::forward<Args>(call_args)...);
                    promise.set_value();
                }
                else
                {
                    promise.set_value(std::invoke(callable, std::forward<Args>(call_args)...));
                }
            }

            promise_result<value_type, error_type> promise;
            std::optional<std::decay_t<F>> stored_function;
        };

    public:
        /**
         * @brief Creates an empty packaged task.
         */
        packaged_task_result() noexcept = default;

        /**
         * @brief Destroys the packaged task state.
         */
        ~packaged_task_result() = default;

        /**
         * @brief Constructs packaged task from callable object.
         * @tparam F Callable type.
         * @param function_arg Callable target.
         */
        template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, packaged_task_result>>>
        explicit packaged_task_result(F&& function_arg)
            : state_(std::make_shared<state_impl<F>>(std::forward<F>(function_arg)))
        {
            static_assert(std::is_convertible<std::invoke_result_t<std::decay_t<F>&, A...>, R>::value,
                "F must be callable with signature R(A...)");
        }

        packaged_task_result(const packaged_task_result&) = delete;
        packaged_task_result(packaged_task_result&&) noexcept = default;
        packaged_task_result& operator=(const packaged_task_result&) = delete;
        packaged_task_result& operator=(packaged_task_result&&) noexcept = default;

        /**
         * @brief Reports whether the packaged task has executable state.
         * @return true if state is present, false otherwise.
         */
        [[nodiscard]] bool valid() const noexcept
        {
            return static_cast<bool>(state_);
        }

        /**
         * @brief Swaps underlying state with another packaged task.
         * @param other Task to swap with.
         */
        void swap(packaged_task_result& other) noexcept
        {
            std::swap(state_, other.state_);
        }

        /**
         * @brief Obtains consumer future for this task result.
         * @return Future handle corresponding to task signature.
         */
        future_type get_future()
        {
            if (!state_)
            {
                return future_type {};
            }
            return state_->get_future();
        }

        /**
         * @brief Executes task with provided arguments when state is valid.
         * @param args Arguments forwarded to wrapped callable.
         */
        void operator()(A... args)
        {
            if (!state_)
            {
                return;
            }
            state_->run(std::forward<A>(args)...);
        }

    private:
        std::shared_ptr<state_base> state_;
    };

} // namespace pco
