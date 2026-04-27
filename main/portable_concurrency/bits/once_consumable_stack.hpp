/**
 * @file once_consumable_stack.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2018-01-30
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2018-01-30                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <cassert>
#include <type_traits>
#include <utility>

#include "once_consumable_stack_fwd.hpp"

namespace pco::detail
{

    /**
     * @brief Deletes all nodes in a singly linked forward list.
     * @tparam T Node value type.
     * @param head Pointer to first list node.
     */
    template <typename T>
    void forward_list_deleter<T>::operator()(forward_list_node<T>* head) noexcept
    {
        if (!head)
        {
            return;
        }

        for (auto* ptr = head; ptr != nullptr;)
        {
            auto* to_delete = ptr;
            ptr = ptr->next;
            to_delete->deallocate_self();
        }
    }

    /**
     * @brief Lightweight forward-list iterator over node values.
     * @tparam T Value type.
     */
    template <typename T>
    class forward_list_iterator
    {
    public:
        /** @brief Constructs end iterator. */
        forward_list_iterator() noexcept = default;

        /**
         * @brief Constructs begin iterator from list head.
         * @param list Source list.
         */
        forward_list_iterator(forward_list<T>& list) noexcept
            : node_(list.get())
        {
        }

        /**
         * @brief Advances iterator to next node.
         * @return Updated iterator.
         */
        forward_list_iterator operator++() noexcept
        {
            if (!node_)
            {
                return *this;
            }
            node_ = node_->next;
            return *this;
        }

        /**
         * @brief Dereferences iterator.
         * @return Reference to current node value.
         */
        T& operator*() noexcept
        {
            return node_->val;
        }

        /**
         * @brief Equality comparison.
         * @param rhs Iterator to compare with.
         * @return true when iterators point to the same node.
         */
        bool operator==(const forward_list_iterator& rhs) const noexcept
        {
            return node_ == rhs.node_;
        }

        /**
         * @brief Inequality comparison.
         * @param rhs Iterator to compare with.
         * @return true when iterators point to different nodes.
         */
        bool operator!=(const forward_list_iterator& rhs) const noexcept
        {
            return node_ != rhs.node_;
        }

    private:
        forward_list_node<T>* node_ = nullptr;
    };

    /**
     * @brief Returns begin iterator for forward list.
     * @tparam T Value type.
     * @param list Source list.
     * @return Iterator pointing to first node.
     */
    template <typename T>
    forward_list_iterator<T> begin(forward_list<T>& list) noexcept
    {
        return { list };
    }

    /**
     * @brief Returns end iterator sentinel for forward list.
     * @tparam T Value type.
     * @param unused Unused list reference.
     * @return End iterator sentinel.
     */
    template <typename T>
    forward_list_iterator<T> end(forward_list<T>& unused) noexcept
    {
        static_cast<void>(unused);
        return {};
    }

    /**
     * @brief Destroys stack and frees all non-consumed nodes.
     * @tparam T Value type.
     */
    template <typename T>
    once_consumable_stack<T>::~once_consumable_stack()
    {
        // Create temporary forward_list which can destroy all nodes properly. Nobody
        // else should access this object anymore at the moment of destruction so
        // relaxed memory order is enough.
        auto* head = head_.load(std::memory_order_relaxed);
        if (head && head != consumed_marker())
        {
            forward_list<T> { head };
        }
    }

    /**
     * @brief Pushes value using default allocator.
     * @tparam T Value type.
     * @param val Value to push.
     * @return true on success, false when stack is already consumed.
     */
    template <typename T>
    bool once_consumable_stack<T>::push(T& val)
    {
        return push(val, std::allocator<T> {});
    }

    /**
     * @brief Pushes preallocated node to stack.
     * @tparam T Value type.
     * @param node Node list handle with one element.
     * @return true on success, false when stack is consumed.
     */
    template <typename T>
    bool once_consumable_stack<T>::push(forward_list<T>& node) noexcept
    {
        assert(!node->next);
        auto* curr_head = head_.load(std::memory_order_acquire);
        if (curr_head == consumed_marker())
        {
            return false;
        }
        node->next = curr_head;
        while (!head_.compare_exchange_strong(node->next, node.get(), std::memory_order_acq_rel))
        {
            if (node->next == consumed_marker())
            {
                node->next = nullptr;
                return false;
            }
        }
        node.release();
        return true;
    }

    /**
     * @brief Checks whether stack has been consumed.
     * @tparam T Value type.
     * @return true when consumed marker is set.
     */
    template <typename T>
    bool once_consumable_stack<T>::is_consumed() const noexcept
    {
        return head_.load(std::memory_order_acquire) == consumed_marker();
    }

    /**
     * @brief Atomically consumes stack contents and marks stack as consumed.
     * @tparam T Value type.
     * @return Forward-list handle containing previous stack nodes.
     */
    template <typename T>
    forward_list<T> once_consumable_stack<T>::consume() noexcept
    {
        auto* curr_head = head_.exchange(consumed_marker(), std::memory_order_acq_rel);
        if (curr_head == consumed_marker())
        {
            return { nullptr, forward_list_deleter<T> {} };
        }
        return forward_list<T> { curr_head, forward_list_deleter<T> {} };
    }

    /**
     * @brief Returns unique consumed-state marker pointer.
     * @tparam T Value type.
     * @return Marker pointer value.
     */
    template <typename T>
    forward_list_node<T>* once_consumable_stack<T>::consumed_marker() noexcept
    {
        return reinterpret_cast<forward_list_node<T>*>(this);
    }

    /**
     * @brief Returns const consumed-state marker pointer.
     * @tparam T Value type.
     * @return Const marker pointer value.
     */
    template <typename T>
    const forward_list_node<T>* once_consumable_stack<T>::consumed_marker() const noexcept
    {
        return reinterpret_cast<const forward_list_node<T>*>(this);
    }

} // namespace pco::detail
