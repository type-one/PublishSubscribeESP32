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

/**
 * @file example_variant_fsm.cpp
 * @brief Implements the variant-based finite state machine example.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
namespace traffic_light_state
{
    /** @brief Powered-off state where the traffic light is inactive. */
    struct off
    {
    };
    /** @brief Transitional startup state before entering normal operation. */
    struct operable_initializing
    {
    };
    /** @brief Operational red-light state; count tracks completed red entries. */
    struct operable_red
    {
        int count = 0;
    };
    /** @brief Operational orange-light state; count tracks completed orange entries. */
    struct operable_orange
    {
        int count = 0;
    };
    /** @brief Operational green-light state; count tracks completed green entries. */
    struct operable_green
    {
        int count = 0;
    };
}

namespace traffic_light_event
{
    /** @brief Event requesting transition from @c off to initialization. */
    struct power_on
    {
    };
    /** @brief Event requesting transition from an operable state back to @c off. */
    struct power_off
    {
    };
    /** @brief Event indicating startup initialization completed successfully. */
    struct init_done
    {
    };
    /** @brief Event advancing the traffic-light cycle to its next color/state. */
    struct next_state
    {
    };
}

using traffic_light_state_v = std::variant<traffic_light_state::off,
    traffic_light_state::operable_initializing,
    traffic_light_state::operable_red,
    traffic_light_state::operable_orange,
    traffic_light_state::operable_green>;

using traffic_light_event_v = std::variant<traffic_light_event::power_on,
    traffic_light_event::power_off,
    traffic_light_event::init_done,
    traffic_light_event::next_state>;

class traffic_light_fsm : tools::non_copyable
{
public:
    traffic_light_fsm() = default;
    ~traffic_light_fsm() = default;

    /** @brief Resets the FSM to the @c off state and marks it as entering that state. */
    void start();

    /**
     * @brief Dispatches an event to the current state and transitions to the returned next state.
     * @param event Variant holding the concrete event to process.
     */
    void handle_event(const traffic_light_event_v& event);

    /** @brief Runs the entry/update action for the current state. */
    void update();

private:
    traffic_light_state_v m_state = traffic_light_state::off {};
    bool m_entering_state = false;

    /** @brief Transitions from @c off to @c operable_initializing on a @c power_on event. */
    traffic_light_state_v on_event(const traffic_light_state::off& state, const traffic_light_event::power_on& event);
    /** @brief Transitions from @c operable_initializing to @c operable_red on an @c init_done event. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_initializing& state, const traffic_light_event::init_done& event);
    /** @brief Transitions from @c operable_red to @c operable_orange (or @c off after max cycles) on @c next_state. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_red& state, const traffic_light_event::next_state& event);
    /** @brief Transitions from @c operable_orange to @c operable_green on a @c next_state event. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_orange& state, const traffic_light_event::next_state& event);
    /** @brief Transitions from @c operable_green to @c operable_red on a @c next_state event. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_green& state, const traffic_light_event::next_state& event);
    /** @brief Transitions from @c operable_initializing to @c off on a @c power_off event. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_initializing& state, const traffic_light_event::power_off& event);
    /** @brief Transitions from @c operable_red to @c off on a @c power_off event. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_red& state, const traffic_light_event::power_off& event);
    /** @brief Transitions from @c operable_orange to @c off on a @c power_off event. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_orange& state, const traffic_light_event::power_off& event);
    /** @brief Transitions from @c operable_green to @c off on a @c power_off event. */
    traffic_light_state_v on_event(
        const traffic_light_state::operable_green& state, const traffic_light_event::power_off& event);

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    /** @brief Fallback for any unhandled [state, event] combination: logs an error and keeps the current state. */
    traffic_light_state_v on_event(const auto& state, const auto& event);
#else
    /**
     * @brief C++17 SFINAE trampoline: tries to call on_event(state, event); falls back to the long-overload error path.
     * @tparam State Concrete state type.
     * @tparam Event Concrete event type.
     */
    template <typename State, typename Event>
    traffic_light_state_v dispatch_event(const State& state, const Event& event)
    {
        return dispatch_event_impl(state, event, 0);
    }

    /**
     * @brief Selected when @c on_event(state, event) is well-formed; forwards the call.
     * @tparam State Concrete state type.
     * @tparam Event Concrete event type.
     */
    template <typename State, typename Event>
    auto dispatch_event_impl(const State& state, const Event& event, int priority)
        -> decltype(this->on_event(state, event))
    {
        (void)priority;
        return this->on_event(state, event);
    }

    /**
     * @brief Fallback selected when @c on_event(state, event) is ill-formed: logs an error and keeps the current state.
     * @tparam State Concrete state type.
     * @tparam Event Concrete event type.
     */
    template <typename State, typename Event>
    traffic_light_state_v dispatch_event_impl(const State& state, const Event& event, long priority)
    {
        (void)priority;
        (void)event;
        LOG_ERROR("Unsupported state transition");
        m_entering_state = false;
        return state;
    }
#endif

    /** @brief Runs the entry action for the @c off state. */
    void on_state(const traffic_light_state::off& state);
    /** @brief Runs the entry action for the @c operable_initializing state. */
    void on_state(const traffic_light_state::operable_initializing& state);
    /** @brief Runs the entry action for the @c operable_red state. */
    void on_state(const traffic_light_state::operable_red& state);
    /** @brief Runs the entry action for the @c operable_orange state. */
    void on_state(const traffic_light_state::operable_orange& state);
    /** @brief Runs the entry action for the @c operable_green state. */
    void on_state(const traffic_light_state::operable_green& state);
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    /** @brief Fallback for any unhandled state type: logs an error on state entry. */
    void on_state(const auto& state);
#else
    /**
     * @brief C++17 SFINAE trampoline: tries to call on_state(state); falls back to the long-overload error path.
     * @tparam State Concrete state type.
     */
    template <typename State>
    void dispatch_state(const State& state)
    {
        dispatch_state_impl(state, 0);
    }

    /**
     * @brief Selected when @c on_state(state) is well-formed; forwards the call.
     * @tparam State Concrete state type.
     */
    template <typename State>
    auto dispatch_state_impl(const State& state, int priority) -> decltype(this->on_state(state), void())
    {
        (void)priority;
        this->on_state(state);
    }

    /**
     * @brief Fallback selected when @c on_state(state) is ill-formed: logs an error on unhandled state entry.
     * @tparam State Concrete state type.
     */
    template <typename State>
    void dispatch_state_impl(const State& state, long priority)
    {
        (void)priority;
        (void)state;
        if (m_entering_state)
        {
            LOG_ERROR("Unsupported state");
            m_entering_state = false;
        }
    }
#endif
};

void traffic_light_fsm::start()
{
    m_state = traffic_light_state::off {};
    m_entering_state = true;
}

void traffic_light_fsm::handle_event(const traffic_light_event_v& event)
{
    // Variant visitation dispatches to the exact [state,event] overload without manual type switches.
    m_state = std::visit(tools::detail::overload {
            [&](const auto& state, const auto& evt)
            {
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
                return on_event(state, evt);
#else
                return dispatch_event(state, evt);
#endif
            } },
        m_state,
        event);
}

void traffic_light_fsm::update()
{
    std::visit(tools::detail::overload {
            [&](const auto& state)
            {
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
                on_state(state);
#else
                dispatch_state(state);
#endif
            } },
        m_state);
}

constexpr const int traffic_light_wait_ms = 1000;

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::off& state, const traffic_light_event::power_on& event)
{
    (void)state;
    (void)event;
    std::printf("switch ON traffic light\n");
    tools::sleep_for(traffic_light_wait_ms);
    m_entering_state = true;
    return traffic_light_state::operable_initializing {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::init_done& event)
{
    (void)state;
    (void)event;
    std::printf("init traffic light completed\n");
    m_entering_state = true;
    return traffic_light_state::operable_red { 0 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_red& state, const traffic_light_event::next_state& event)
{
    (void)state;
    (void)event;
    tools::sleep_for(traffic_light_wait_ms);

    traffic_light_state_v next_state;
    constexpr const int max_cycles = 2;
    constexpr const int nb_light_states = 3;
    if (state.count < max_cycles * nb_light_states)
    {
        std::printf("traffic light RED --> ORANGE\n");
        next_state = traffic_light_state::operable_orange { state.count + 1 };
    }
    else
    {
        std::printf("traffic light RED --> OFF\n");
        next_state = traffic_light_state::off {};
    }

    m_entering_state = true;

    return next_state;
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::next_state& event)
{
    (void)state;
    (void)event;
    tools::sleep_for(traffic_light_wait_ms);
    std::printf("traffic light ORANGE --> GREEN\n");
    m_entering_state = true;
    return traffic_light_state::operable_green { state.count + 1 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_green& state, const traffic_light_event::next_state& event)
{
    (void)state;
    (void)event;
    tools::sleep_for(traffic_light_wait_ms);
    std::printf("traffic light GREEN --> RED\n");
    m_entering_state = true;
    return traffic_light_state::operable_red { state.count + 1 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_red& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_green& state, const traffic_light_event::power_off& event)
{
    (void)state;
    (void)event;
    std::printf("switch OFF traffic light\n");
    m_entering_state = true;
    return traffic_light_state::off {};
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
traffic_light_state_v traffic_light_fsm::on_event(const auto& state, const auto& event)
{
    (void)event;
    LOG_ERROR("Unsupported state transition");
    m_entering_state = false;
    return state;
}
#endif

void traffic_light_fsm::on_state(const traffic_light_state::off& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light off\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_initializing& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light initializing\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_red& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light RED\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_orange& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light ORANGE\n");
        m_entering_state = false;
    }
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_green& state)
{
    (void)state;
    if (m_entering_state)
    {
        std::printf("traffic light GREEN\n");
        m_entering_state = false;
    }
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
void traffic_light_fsm::on_state(const auto& state)
{
    (void)state;
    if (m_entering_state)
    {
        LOG_ERROR("Unsupported state");
        m_entering_state = false;
    }
}
#endif

/** @brief Exercises the traffic-light FSM through a full power-on, multi-cycle, and power-off sequence. */
void test_variant_fsm()
{
    LOG_INFO("-- finite state machine (std::variant) --");
    print_stats();

    traffic_light_fsm fsm;

    fsm.start();
    fsm.update();
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::power_on {});
    // Drive initialization, then repeatedly advance through RED->ORANGE->GREEN cycles.
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::init_done {});
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.update();

    std::printf("end fsm test\n");
}
} // namespace

void run_example_variant_fsm()
{
    // Walk through the complete state machine lifecycle to mirror event-driven component design.
    test_variant_fsm();
}
