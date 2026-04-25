/**
 * @file worker_task_std.inl
 * @brief A worker task class template.
 *
 * This file contains the implementation of the worker_task class template, which
 * provides functionality to delegate tasks and manage their execution.
 *
 * @author Laurent Lardinois
 * @date January 2025
 */

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


#include <atomic>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#endif

#include "tools/base_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"
#include "portable_concurrency/p_future.hpp"
#include "portable_concurrency/bits/coro.h"
#include "tools/sync_object.hpp"
#include "tools/sync_queue.hpp"

namespace tools
{
    template <typename Context>
    class worker_task;

    template <typename Context>
    class worker_task_executor
    {
    public:
        explicit worker_task_executor(worker_task<Context>* owner)
            : m_owner(owner)
        {
        }

    private:
        worker_task<Context>* m_owner = nullptr;

        template <typename Ctx, typename Task>
        friend void post(worker_task_executor<Ctx> exec, Task&& task);
    };

    template <typename Context, typename Task>
    void post(worker_task_executor<Context> exec, Task&& task)
    {
        auto shared_task = std::make_shared<std::decay_t<Task>>(std::forward<Task>(task));
        exec.m_owner->delegate(
            [shared_task](const std::shared_ptr<Context>&, const std::string&) mutable { (*shared_task)(); });
    }

    /**
     * @brief A worker task class template.
     *
     * This class represents a worker task that inherits from the base_task class.
     * It provides functionality to delegate tasks and manage their execution.
     *
     * @tparam Context The type of the context object.
     */
    template <typename Context>
    class worker_task : public base_task // NOLINT base_task is non copyable and non movable
    {

    public:
        worker_task() = delete;

        /**
         * @brief Type alias for a callback function.
         *
         * This callback function takes a shared pointer to a Context object and a task name string as parameters.
         *
         * @param context A shared pointer to a Context object.
         * @param task_name The name of the task as a string.
         */
        using call_back = std::function<void(const std::shared_ptr<Context>& context, const std::string& task_name)>;
        using executor_type = worker_task_executor<Context>;

        /**
         * @brief Constructs a worker_task object.
         *
         * @param startup_routine A callable object that represents the startup routine.
         * @param context A shared pointer to the Context object.
         * @param task_name The name of the task.
         * @param stack_size The size of the stack for the task.
         * @param cpu_affinity The CPU affinity for the task.
         * @param priority The priority of the task.
         */
        worker_task(call_back&& startup_routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size, int cpu_affinity, int priority)
            : base_task(task_name, stack_size, cpu_affinity, priority)
            , m_startup_routine(std::move(startup_routine))
            , m_context(context)
            , m_task(std::make_unique<std::thread>(
                  [this]()
                  {
                      set_current_thread_params(this->task_name(), this->cpu_affinity(), this->priority());

                      run_loop();
                  }))
        {
        }

        /**
         * @brief Constructs a worker_task object using perfect forwarding.
         *
         * This overload supports conversion-based arguments beyond exact-type overloads.
         * In C++20, this constructor is constrained to constructible argument types.
         */
        template <typename UStartup,
            typename UContext,
            typename UName
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<call_back, UStartup>::value
                && std::is_constructible<std::shared_ptr<Context>, UContext>::value
                && std::is_constructible<std::string, UName>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<call_back, UStartup>
                         && std::is_constructible_v<std::shared_ptr<Context>, UContext>
                         && std::is_constructible_v<std::string, UName>
#endif
        worker_task(UStartup&& startup_routine, UContext&& context, UName&& task_name, std::size_t stack_size,
            int cpu_affinity, int priority)
            : base_task(std::string(std::forward<UName>(task_name)), stack_size, cpu_affinity, priority)
            , m_startup_routine(call_back(std::forward<UStartup>(startup_routine)))
            , m_context(std::shared_ptr<Context>(std::forward<UContext>(context)))
            , m_task(std::make_unique<std::thread>(
                  [this]()
                  {
                      set_current_thread_params(this->task_name(), this->cpu_affinity(), this->priority());

                      run_loop();
                  }))
        {
        }

        /**
         * @brief Constructs a worker_task object with default priority and default cpu affinity.
         *
         * @param startup_routine A callable object that represents the startup routine.
         * @param context A shared pointer to the Context object.
         * @param task_name The name of the task.
         * @param stack_size The size of the stack for the task.
         */
        worker_task(call_back&& startup_routine, const std::shared_ptr<Context>& context, const std::string& task_name,
            std::size_t stack_size)
            : worker_task(std::move(startup_routine), context, task_name, stack_size, base_task::run_on_all_cores,
                  base_task::default_priority)
        {
        }

        /**
         * @brief Constructs a worker_task object with default priority and default cpu affinity using perfect
         * forwarding.
         */
        template <typename UStartup,
            typename UContext,
            typename UName
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<call_back, UStartup>::value
                && std::is_constructible<std::shared_ptr<Context>, UContext>::value
                && std::is_constructible<std::string, UName>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<call_back, UStartup>
                         && std::is_constructible_v<std::shared_ptr<Context>, UContext>
                         && std::is_constructible_v<std::string, UName>
#endif
        worker_task(UStartup&& startup_routine, UContext&& context, UName&& task_name, std::size_t stack_size)
            : worker_task(std::forward<UStartup>(startup_routine), std::forward<UContext>(context),
                std::forward<UName>(task_name), stack_size, base_task::run_on_all_cores, base_task::default_priority)
        {
        }

        /**
         * @brief Destructor for the worker_task class.
         *
         * This destructor sets the stop flag to true, signals the work synchronization
         * object, and waits for the task to complete by joining the task thread.
         */
        ~worker_task() override
        {
            m_stop_task.store(true);
            m_work_sync.signal();
            m_task->join();
        }

        // note: native handle allows specific OS calls like setting scheduling policy or setting priority
        /**
         * @brief Retrieves the native handle of the task.
         *
         * This function overrides the base class implementation to return the native handle
         * of the task, wrapped as a void pointer.
         *
         * @return void* The native handle of the task, cast to a void pointer.
         */
        void* native_handle() override
        {
            return reinterpret_cast<void*>(m_task->native_handle()); // NOLINT native handler wrapping as a void*
        }

        /**
         * @brief Delegates a task to the worker by adding it to the work queue.
         *
         * @param work A callable object (e.g., lambda, function object) to be executed by the worker.
         */
        void delegate(call_back&& work)
        {
            do_delegate(std::move(work));
        }

        void delegate(const call_back& work)
        {
            do_delegate(call_back(work));
        }

        /**
         * @brief Delegates a task using perfect forwarding.
         *
         * This template supports conversion-based callable arguments beyond exact-type overloads.
         */
        template <typename UWork>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<call_back, UWork>
#endif
        auto delegate(UWork&& work)
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            -> typename std::enable_if<std::is_constructible<call_back, UWork>::value, void>::type
#endif
        {
            do_delegate(call_back(std::forward<UWork>(work)));
        }

        /**
         * @brief Delegates a batch of work callbacks from a generic range.
         *
         * In C++20, accepts any std::ranges::input_range whose value type can
         * construct call_back. In C++17, accepts any iterable source whose
         * dereferenced element can construct call_back.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<
                std::is_constructible<
                    call_back,
                    decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))
                >::value
            >::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange>
                  && std::is_constructible_v<call_back, std::ranges::range_value_t<TRange>>
#endif
        void delegate_range(TRange&& range)
        {
            for (auto&& work : std::forward<TRange>(range))
            {
                do_delegate(call_back(std::forward<decltype(work)>(work)));
            }
        }

        template <typename InputIt>
        void delegate_range(InputIt first, InputIt last)
        {
            m_work_queue.push_range(first, last);
            m_work_sync.signal();
        }

        /**
         * @brief Delegates a batch of work callbacks from an initializer-list.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_constructible<call_back, const U&>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<call_back, const U&>
#endif
        void delegate_range(std::initializer_list<U> range)
        {
            for (const auto& work : range)
            {
                do_delegate(call_back(work));
            }
        }

        [[nodiscard]] executor_type as_executor()
        {
            return executor_type { this };
        }

        template <typename Callable, typename... Args>
        auto delegate_async(Callable&& work, Args&&... args)
            -> decltype(pco::v2::async_result(std::declval<executor_type>(),
                std::forward<Callable>(work), std::declval<std::shared_ptr<Context>>(), std::declval<std::string>(),
                std::forward<Args>(args)...))
        {
            return pco::v2::async_result(
                as_executor(), std::forward<Callable>(work), m_context, this->task_name(), std::forward<Args>(args)...);
        }

#if defined(PC_HAS_COROUTINES) || defined(__cpp_impl_coroutine) || defined(__cpp_coroutines) || (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        class schedule_awaitable
        {
        public:
            explicit schedule_awaitable(worker_task* owner)
                : m_owner(owner)
            {
            }

            [[nodiscard]] bool await_ready() const noexcept
            {
                return false;
            }

            void await_suspend(pco::detail::coroutine_handle<> handle)
            {
                m_owner->delegate(
                    [handle](const std::shared_ptr<Context>&, const std::string&) mutable { handle.resume(); });
            }

            void await_resume() const noexcept
            {
            }

        private:
            worker_task* m_owner = nullptr;
        };

        [[nodiscard]] schedule_awaitable schedule()
        {
            return schedule_awaitable { this };
        }
#endif // coroutine support

    private:
        void do_delegate(call_back&& work)
        {
            m_work_queue.emplace(std::move(work));
            m_work_sync.signal();
        }

        /**
         * @brief Executes the main loop of the worker task.
         *
         * This function runs the startup routine and then enters a loop where it waits for a signal
         * to process work items from the work queue. The loop continues until the stop task flag is set.
         */
        void run_loop()
        {
            // execute given startup function
            m_startup_routine(m_context, this->task_name());

            while (!m_stop_task.load())
            {
                m_work_sync.wait_for_signal();

                while (!m_work_queue.empty())
                {
                    auto work = m_work_queue.front_pop();

                    if (work.has_value())
                    {
                        work.value()(m_context, this->task_name());
                    }
                }
            } // run loop
        }

        call_back m_startup_routine;
        tools::sync_object m_work_sync;
        tools::sync_queue<call_back> m_work_queue;
        std::shared_ptr<Context> m_context;
        std::atomic_bool m_stop_task = false;
        std::unique_ptr<std::thread> m_task;
    };
}

namespace pco
{
    template <typename Context>
    struct is_executor<tools::worker_task_executor<Context>> : std::true_type
    {
    };
}
