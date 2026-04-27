/**
 * @file either.hpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-10-25
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-10-25                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <portable_concurrency/bits/concurrency_config.hpp>

#include "concurrency_type_traits.hpp"

namespace pco::detail
{

#if defined(__has_builtin)
#if __has_builtin(__type_pack_element)
#define USE_TYPE_PACK_BUILTIN
#endif
#endif

#if defined(USE_TYPE_PACK_BUILTIN)
    /**
     * @brief Alias selecting the I-th type from a variadic pack.
     * @tparam I Zero-based type index.
     * @tparam T Type pack.
     */
    template <std::size_t I, typename... T>
    using at_t = __type_pack_element<I, T...>;
#else
    /**
     * @brief Recursive type selector for variadic type packs.
     * @tparam I Zero-based type index.
     * @tparam T Type pack.
     */
    template <std::size_t I, typename... T>
    struct at;

    /**
     * @brief Base specialization selecting first type in pack.
     * @tparam H Head type.
     * @tparam T Tail types.
     */
    template <typename H, typename... T>
    struct at<0u, H, T...>
    {
        using type = H;
    };

    /**
     * @brief Recursive specialization selecting type at index I.
     * @tparam I Target index.
     * @tparam H Head type.
     * @tparam T Tail types.
     */
    template <std::size_t I, typename H, typename... T>
    struct at<I, H, T...> : at<I - 1, T...>
    {
    };

    /**
     * @brief Alias selecting the I-th type from a variadic pack.
     * @tparam I Zero-based type index.
     * @tparam T Type pack.
     */
    template <std::size_t I, typename... T>
    using at_t = typename at<I, T...>::type;
#endif

    /**
     * @brief Type-level tag used to dispatch by active alternative index.
     * @tparam I Zero-based index value.
     */
    template <std::size_t I>
    using in_place_index_t = std::integral_constant<std::size_t, I>;

    /**
     * @brief Sentinel state representing an empty either value.
     */
    struct monostate
    {
    };

    /**
     * @brief Visitor that destroys active alternative in place.
     */
    struct destroy
    {
        /**
         * @brief Destroys non-monostate object.
         * @tparam T Active object type.
         * @param t Object to destroy.
         */
        template <typename T>
        void operator()(T& t) const
        {
            t.~T();
        }

        /**
         * @brief No-op overload for monostate sentinel.
         * @param unused Monostate value.
         */
        void operator()(monostate unused) const
        {
            static_cast<void>(unused);
        }
    };

    /**
     * @brief Forward declaration for either storage type.
     * @tparam T Alternative types.
     */
    template <typename... T>
    class either;

    // Minimalistic discriminated union: std::variant<std::monostate, T...>.
    // MoveConstructible && !CopyConstructible.
    //
    // Move operation is explicitly marked as noexcept(true) and only used by library
    // for instantiations on types which are nothrow move constructible.
    //
    // First state (monostate) is used as the valueless-by-exception sentinel.
    template <typename... T>
    class either<monostate, T...>
    {
    public:
        /**
         * @brief Index value representing empty state.
         */
        constexpr static std::size_t empty_state = 0;

        /**
         * @brief Creates empty either value.
         */
        either() noexcept = default;

        /**
         * @brief Destroys active alternative and resets to empty state.
         */
        ~either()
        {
            clean();
        }

        either(const either&) = delete;
        either& operator=(const either&) = delete;

        /**
         * @brief In-place constructs alternative at index I.
         * @tparam I Active alternative index.
         * @tparam A Constructor argument types.
         * @param tag In-place index tag.
         * @param a Constructor arguments.
         */
        template <std::size_t I, typename... A>
        either(in_place_index_t<I> tag, A&&... a)
        {
            emplace(tag, std::forward<A>(a)...);
        }

        /**
         * @brief Move-constructs from another either.
         * @param rhs Source either value.
         */
        either(either&& rhs) noexcept
        {
            move_from(std::move(rhs), std::make_index_sequence<sizeof...(T)> {});
        }

        /**
         * @brief Move-assigns from another either.
         * @param rhs Source either value.
         * @return Reference to this object.
         */
        either& operator=(either&& rhs) noexcept
        {
            clean();
            move_from(std::move(rhs), std::make_index_sequence<sizeof...(T)> {});
            return *this;
        }

        /**
         * @brief Replaces current value with alternative at index I.
         * @tparam I Active alternative index.
         * @tparam A Constructor argument types.
         * @param unused In-place index tag.
         * @param a Constructor arguments.
         */
        template <std::size_t I, typename... A>
        void emplace(in_place_index_t<I> unused, A&&... a)
        {
            static_cast<void>(unused);
            static_assert(I != empty_state, "Can't emplace construct monostate");
            clean();
            new (&storage_) at_t<I - 1, T...>(std::forward<A>(a)...);
            state_ = I;
        }

        /**
         * @brief Returns active alternative index.
         * @return Current state index.
         */
        std::size_t state() const noexcept
        {
            return state_;
        }

        /**
         * @brief Checks whether object is in empty state.
         * @return true when empty, false otherwise.
         */
        bool empty() const noexcept
        {
            return state_ == empty_state;
        }

        /**
         * @brief Accesses active alternative by index.
         * @tparam I Active alternative index.
         * @param unused In-place index tag.
         * @return Reference to active value.
         */
        template <std::size_t I>
        auto& get(in_place_index_t<I> unused) noexcept
        {
            static_cast<void>(unused);
            assert(state_ == I);
            return reinterpret_cast<at_t<I - 1, T...>&>(storage_);
        }

        /**
         * @brief Accesses active alternative by index (const overload).
         * @tparam I Active alternative index.
         * @param unused In-place index tag.
         * @return Const reference to active value.
         */
        template <std::size_t I>
        const auto& get(in_place_index_t<I> unused) const noexcept
        {
            static_cast<void>(unused);
            assert(state_ == I);
            return reinterpret_cast<const at_t<I - 1, T...>&>(storage_);
        }

        /**
         * @brief Applies visitor to active alternative.
         * @tparam F Visitor type.
         * @param f Visitor callable.
         * @return Visitor result.
         */
        template <typename F>
        decltype(auto) visit(F&& f)
        {
            return visit_backward(in_place_index_t<sizeof...(T)> {}, std::forward<F>(f));
        }

        /**
         * @brief Applies visitor to active alternative (const overload).
         * @tparam F Visitor type.
         * @param f Visitor callable.
         * @return Visitor result.
         */
        template <typename F>
        decltype(auto) visit(F&& f) const
        {
            return visit_backward(in_place_index_t<sizeof...(T)> {}, std::forward<F>(f));
        }

        /**
         * @brief Destroys active value and resets to empty state.
         */
        void clean() noexcept
        {
            visit(destroy {});
            state_ = empty_state;
        }

    private:
        /**
         * @brief Moves active alternative from source either using index-pack dispatch.
         * @tparam I Index sequence values for alternative dispatch.
         * @param src Source either object.
         * @param unused Index sequence tag.
         */
        template <std::size_t... I>
        void move_from(either&& src, std::index_sequence<I...> unused) noexcept
        {
            static_cast<void>(unused);
            swallow { (src.state_ == I + 1
                    ? (emplace(in_place_index_t<I + 1> {}, std::move(reinterpret_cast<T&>(src.storage_))), false)
                    : false)... };
            src.clean();
        }

        /**
         * @brief Visits active alternative by descending index search.
         * @tparam F Visitor type.
         * @tparam I Current index checked in recursion.
         * @param unused In-place index tag.
         * @param visitor Visitor callable.
         * @return Visitor result.
         */
        template <typename F, std::size_t I>
        decltype(auto) visit_backward(in_place_index_t<I> unused, F&& visitor)
        {
            static_cast<void>(unused);
            if (state_ == I)
            {
                return visitor(reinterpret_cast<at_t<I - 1, T...>&>(storage_));
            }
            return visit_backward(in_place_index_t<I - 1> {}, std::forward<F>(visitor));
        }

        /**
         * @brief Const overload of descending-index visitor dispatch.
         * @tparam F Visitor type.
         * @tparam I Current index checked in recursion.
         * @param unused In-place index tag.
         * @param visitor Visitor callable.
         * @return Visitor result.
         */
        template <typename F, std::size_t I>
        decltype(auto) visit_backward(in_place_index_t<I> unused, F&& visitor) const
        {
            static_cast<void>(unused);
            if (state_ == I)
            {
                return visitor(reinterpret_cast<const at_t<I - 1, T...>&>(storage_));
            }
            return visit_backward(in_place_index_t<I - 1> {}, std::forward<F>(visitor));
        }

        /**
         * @brief Terminal visitor dispatch for empty-state in const context.
         * @tparam F Visitor type.
         * @param unused Empty-state index tag.
         * @param visitor Visitor callable.
         * @return Visitor result.
         */
        template <typename F>
        decltype(auto) visit_backward(in_place_index_t<empty_state> unused, F&& visitor) const
        {
            static_cast<void>(unused);
            return visitor(monostate {});
        }

        /**
         * @brief Terminal visitor dispatch for empty-state in mutable context.
         * @tparam F Visitor type.
         * @param unused Empty-state index tag.
         * @param visitor Visitor callable.
         * @return Visitor result.
         */
        template <typename F>
        decltype(auto) visit_backward(in_place_index_t<empty_state> unused, F&& visitor)
        {
            static_cast<void>(unused);
            return visitor(monostate {});
        }

    private:
        /** @brief Raw storage holding active alternative object. */
        std::aligned_union_t<1, T...> storage_;
        /** @brief Index of active alternative (`empty_state` when valueless). */
        std::size_t state_ = empty_state;
    };

    /**
     * @brief Forward declaration of scope guard cleaning an either instance.
     * @tparam T either type specialization.
     */
    template <typename T>
    struct scope_either_cleaner;

    /**
     * @brief Scope guard that resets either to empty state on destruction.
     * @tparam T Alternative types used by either.
     */
    template <typename... T>
    struct scope_either_cleaner<either<monostate, T...>>
    {
        either<monostate, T...>& target;

        /**
         * @brief Cleans target either on scope exit.
         */
        ~scope_either_cleaner()
        {
            target.clean();
        }
    };

} // namespace pco::detail
