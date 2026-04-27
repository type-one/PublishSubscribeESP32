/**
 * @file once_consumable_stack_fwd.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-07-26
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-07-26                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <atomic>
#include <memory>
#include <utility>

#include "tools/platform_detection.hpp"

namespace pco::detail
{

    /**
     * @brief Singly linked node used by lock-free stack list storage.
     * @tparam T Stored value type.
     */
    template <typename T>
    struct forward_list_node
    {
        /**
         * @brief Constructs node with value and null next pointer.
         * @param value_arg Value moved into node.
         */
        explicit forward_list_node(T&& value_arg)
            : val(std::move(value_arg))
            , next(nullptr)
        {
        }

        /**
         * @brief Constructs node with value and explicit next pointer.
         * @param value_arg Value moved into node.
         * @param next_arg Pointer to next node.
         */
        forward_list_node(T&& value_arg, forward_list_node* next_arg)
            : val(std::move(value_arg))
            , next(next_arg)
        {
        }

        forward_list_node(const forward_list_node&) = delete;
        forward_list_node& operator=(const forward_list_node&) = delete;
        forward_list_node(forward_list_node&&) = delete;
        forward_list_node& operator=(forward_list_node&&) = delete;

        /**
         * @brief Deallocates this node through the correct allocator instance.
         */
        virtual void deallocate_self() = 0;

        /** @brief Stored payload value. */
        T val;
        /** @brief Pointer to next node. */
        forward_list_node* next;

    protected:
        ~forward_list_node() = default;
    };

    /**
     * @brief Deleter for forward-list node chains.
     * @tparam T Node value type.
     */
    template <typename T>
    struct forward_list_deleter
    {
        /**
         * @brief Deletes linked list starting at head.
         * @param head First node pointer.
         */
        void operator()(forward_list_node<T>* head) noexcept;
    };

    /**
     * @brief Owning smart-pointer alias for forward-list node chains.
     * @tparam T Node value type.
     */
    template <typename T>
    using forward_list = std::unique_ptr<forward_list_node<T>, forward_list_deleter<T>>;

    /**
     * @brief Allocates and constructs a single forward-list node.
     * @tparam T Value type.
     * @tparam Alloc Allocator type.
     * @param val Value moved into node.
     * @param alloc Allocator used to allocate node storage.
     * @return Owning list handle pointing to the allocated node.
     */
    template <typename T, typename Alloc>
    forward_list<T> allocate_list_node(T&& val, const Alloc& alloc)
    {
        /**
         * @brief Allocator-aware concrete node implementation for one allocated list node.
         */
        struct node final : forward_list_node<T>
        {
            /**
             * @brief Constructs node and stores allocator copy.
             * @param value_arg Value moved into node payload.
             * @param alloc Allocator instance copied into node.
             */
            node(T&& value_arg, const Alloc& alloc)
                : forward_list_node<T>(std::move(value_arg))
                , m_allocator(alloc)
            {
            }

            /**
             * @brief Returns mutable reference to stored allocator.
             * @return Allocator reference.
             */
            Alloc& get_allocator()
            {
                return m_allocator;
            }

            /**
             * @brief Destroys and deallocates this node using rebound allocator.
             */
            void deallocate_self() override
            {
                using node_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<node>;
                node_allocator alloc { std::move(get_allocator()) };
                std::allocator_traits<node_allocator>::destroy(alloc, this);
                std::allocator_traits<node_allocator>::deallocate(alloc, this, 1);
            }

            /** @brief Allocator instance associated with this node. */
            Alloc m_allocator;
        };

        using node_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<node>;
        node_allocator nalloc { alloc };
        auto result = std::allocator_traits<node_allocator>::allocate(nalloc, 1);
#if defined(CPP_EXCEPTIONS_ENABLED)
        try
        {
            std::allocator_traits<node_allocator>::construct(nalloc, result, std::forward<T>(val), alloc);
        }
        catch (...)
        {
            std::allocator_traits<node_allocator>::deallocate(nalloc, result, 1);
            throw;
        }
#else
        std::allocator_traits<node_allocator>::construct(nalloc, result, std::forward<T>(val), alloc);
#endif
        return forward_list<T> { result, forward_list_deleter<T> {} };
    }

    /**
     * @brief Multi-producer single-consumer stack that can be consumed exactly once.
     * @tparam T Value type.
     *
     * After `consume()` is called, stack transitions to permanent consumed state.
     * Subsequent push attempts fail and preserve caller-owned value.
     */
    template <typename T>
    class once_consumable_stack
    {
    public:
        /** @brief Creates an empty non-consumed stack. */
        once_consumable_stack() noexcept = default;
        once_consumable_stack(const once_consumable_stack&) = delete;
        once_consumable_stack& operator=(const once_consumable_stack&) = delete;
        once_consumable_stack(once_consumable_stack&&) = delete;
        once_consumable_stack& operator=(once_consumable_stack&&) = delete;

        /**
         * @brief Destroys stack and frees remaining nodes when not consumed.
         */
        ~once_consumable_stack();

        /**
         * Push an object to the stack in a thread safe way. If the stack is not yet
         * @em consumed then this function moves the value from the paremeter @a val
         * and returns true. Otherwise the value of the @a val object remains untoched
         * and function returns false.
         *
         * @note Can be called from multiple threads.
         *
         * @note This function assumes that moving the value from an object and then
         * moving it back remains an object in initial state.
         */
        bool push(T& val);

        /**
         * @brief Pushes value using custom allocator for node allocation.
         * @tparam Alloc Allocator type.
         * @param val Value to move into stack node.
         * @param alloc Allocator used for node allocation.
         * @return true when push succeeds; false when stack is consumed.
         */
        template <typename Alloc>
        bool push(T& val, const Alloc& alloc)
        {
            forward_list<T> node = allocate_list_node(std::move(val), alloc);
            if (push(node))
            {
                return true;
            }
            val = std::move(node->val);
            return false;
        }

        /**
         * Checks if the stack is @em consumed.
         *
         * @note Can be called from multiple threads.
         */
        [[nodiscard]] bool is_consumed() const noexcept;

        /**
         * Consumes the stack and switch it into @em consumed state. Running this
         * function concurrently with any other function in this class (excpet
         * constructor, destructor and consume function itself) is safe.
         *
         * @note Must be called from a single thread. Must not be called twice on a
         * same queue.
         */
        forward_list<T> consume() noexcept;

    private:
        /**
         * @brief Returns sentinel pointer representing consumed state.
         * @return Sentinel marker pointer.
         */
        forward_list_node<T>* consumed_marker() noexcept;

        /**
         * @brief Returns const sentinel pointer representing consumed state.
         * @return Const sentinel marker pointer.
         */
        const forward_list_node<T>* consumed_marker() const noexcept;

        /**
         * @brief Pushes pre-allocated node list into stack.
         * @param node Node handle to push.
         * @return true on success, false when stack is consumed.
         */
        bool push(forward_list<T>& node) noexcept;

        std::atomic<forward_list_node<T>*> head_ { nullptr };
    };

} // namespace pco::detail
