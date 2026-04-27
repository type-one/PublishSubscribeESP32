/**
 * @file continuations_stack.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-02-14
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-02-14                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <utility>

#include "once_consumable_stack_fwd.hpp"
#include "small_unique_function.hpp"

namespace pco::detail
{

    /**
     * @brief Type-erased no-argument continuation task.
     */
    using continuation = small_unique_function<void()>;

    extern template struct forward_list_deleter<continuation>;
    extern template class once_consumable_stack<continuation>;

    /**
     * @brief One-shot stack of continuations executed exactly once.
     */
    class continuations_stack
    {
    public:
        /**
         * @brief Pushes continuation into the stack.
         * @param continuation_fn Continuation function to enqueue.
         */
        void push(continuation&& continuation_fn);

        /**
         * @brief Pushes continuation using a custom allocator.
         * @tparam Alloc Allocator type used for node allocation.
         * @param continuation_fn Continuation function to enqueue.
         * @param alloc Allocator instance used for push operation.
         * @note If allocation fails, continuation is executed immediately.
         */
        template <typename Alloc>
        void push(continuation&& continuation_fn, const Alloc& alloc)
        {
            auto continuation_local = std::move(continuation_fn);
            if (!stack_.push(continuation_local, alloc))
            {
                static_cast<void>(continuation_local.invoke());
            }
        }

        /**
         * @brief Executes all queued continuations and marks stack as executed.
         */
        void execute();

        /**
         * @brief Checks whether stack has already been executed.
         * @return true once execute() has run; false otherwise.
         */
        [[nodiscard]] bool executed() const;

    private:
        once_consumable_stack<continuation> stack_;
    };

} // namespace pco::detail
