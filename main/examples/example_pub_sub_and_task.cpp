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
 * @file example_pub_sub_and_task.cpp
 * @brief Implements the publish/subscribe, observer, generic task, periodic task, and histogram examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
/** @brief Topics used by the pub/sub examples to route events to observers. */
enum class my_topic : std::uint8_t
{
    generic,
    system,
    external
};

/** @brief Event payload carrying a typed event code and a descriptive text message. */
struct my_event
{
    /** @brief Event category discriminator for the pub/sub payload. */
    enum class type : std::uint8_t
    {
        notification,
        failure
    } type = { type::notification };

    std::string description;
};

using base_observer = tools::sync_observer<my_topic, my_event>;
class my_observer : public base_observer
{
public:
    my_observer() = default;
    ~my_observer() override = default;

    my_observer(const my_observer&) = delete;
    my_observer& operator=(const my_observer&) = delete;
    my_observer(my_observer&&) = delete;
    my_observer& operator=(my_observer&&) = delete;

    /**
     * @brief Synchronously handles an incoming event by printing the topic, event description, and origin.
     * @param topic The topic on which the event was published.
     * @param event The event payload carrying the description.
     * @param origin Name of the subject that published the event.
     */
    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        std::printf("sync [topic %d] received: event (%s) from %s\n",
            static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());
    }
};

using base_async_observer = tools::async_observer<my_topic, my_event, tools::sync_ring_vector>;

constexpr const std::size_t base_async_observer_queue_depth = 256U;
class my_async_observer : public base_async_observer
{
public:
    /** @brief Constructs the observer and starts the background event-draining thread. */
    my_async_observer()
        : base_async_observer(base_async_observer_queue_depth)
        , m_task_loop([this]() { handle_events(); })
    {
    }

    my_async_observer(const my_async_observer&) = delete;
    my_async_observer& operator=(const my_async_observer&) = delete;
    my_async_observer(my_async_observer&&) = delete;
    my_async_observer& operator=(my_async_observer&&) = delete;

    /** @brief Signals the background thread to stop and waits for it to join. */
    ~my_async_observer() override
    {
        m_stop_task.store(true);
        m_task_loop.join();
    }

    /**
     * @brief Logs the incoming event immediately (push path) and enqueues it for asynchronous processing.
     * @param topic The topic on which the event was published.
     * @param event The event payload carrying the description.
     * @param origin Name of the subject that published the event.
     */
    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        std::printf("async/push [topic %d] received: event (%s) from %s\n",
            static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());

        base_async_observer::inform(topic, event, origin);
    }

private:
    /** @brief Background thread body: drains queued events with a 1 ms timeout and logs each one (pop path). */
    void handle_events()
    {
        const auto timeout = std::chrono::duration<std::uint64_t, std::micro>(1000);

        while (!m_stop_task.load())
        {
            wait_for_events(timeout);

            while (number_of_events() > 0)
            {
                auto entry = pop_first_event();
                if (entry.has_value())
                {
                    auto& [topic, event, origin] = *entry;

                    std::printf("async/pop [topic %d] received: event (%s) from %s\n",
                        static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(),
                        origin.c_str());
                }
            }
        }
    }

    std::thread m_task_loop;
    std::atomic_bool m_stop_task = false;
};

using base_subject = tools::sync_subject<my_topic, my_event>;
class my_subject : public base_subject
{
public:
    my_subject() = delete;

    /**
     * @brief Constructs the subject with the given name.
     * @param name Identifies this subject to observers receiving events.
     */
    explicit my_subject(const std::string& name)
        : base_subject(name)
    {
    }

    /**
     * @brief Logs the outgoing event and then delegates to the base-class publish mechanism.
     * @param topic Topic under which the event is dispatched to subscribers.
     * @param event The event payload to deliver.
     */
    void publish(const my_topic& topic, const my_event& event) override
    {
        std::printf("publish: event (%s) to %s\n", event.description.c_str(), name().c_str());
        base_subject::publish(topic, event);
    }
};

/** @brief Demonstrates the pub/sub pattern with synchronous and asynchronous observers, topic filtering, subscriptions, unsubscriptions, and lambda handlers. */
void test_publish_subscribe()
{
    LOG_INFO("-- publish subscribe --");
    print_stats();

    auto observer1 = std::make_shared<my_observer>();
    auto observer2 = std::make_shared<my_observer>();
    auto async_observer = std::make_shared<my_async_observer>();
    auto subject1 = std::make_shared<my_subject>("source1");
    auto subject2 = std::make_shared<my_subject>("source2");

    // Build a mixed observer graph so the same event fan-outs to sync and async consumers.
    subject1->subscribe(my_topic::generic, observer1);
    subject1->subscribe(my_topic::generic, observer2);
    subject1->subscribe(my_topic::system, observer2);
    subject1->subscribe(my_topic::generic, async_observer);

    subject2->subscribe(my_topic::generic, observer1);
    subject2->subscribe(my_topic::generic, observer2);
    subject2->subscribe(my_topic::system, observer2);
    subject2->subscribe(my_topic::generic, async_observer);

    subject1->subscribe(my_topic::generic, "loose_coupled_handler_1",
        [](const my_topic& topic, const my_event& event, const std::string& origin)
        {
            std::printf("handler [topic %d] received: event (%s) from %s\n",
                static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());
        });

    // Emit an initial event to show baseline delivery with the full subscription set.
    subject1->publish(my_topic::generic, my_event { my_event::type::notification, "toto" });

    // Remove one observer and publish again to verify runtime subscription churn.
    subject1->unsubscribe(my_topic::generic, observer1);

    subject1->publish(my_topic::generic, my_event { my_event::type::notification, "titi" });

    subject1->publish(my_topic::system, my_event { my_event::type::notification, "tata" });

    subject1->unsubscribe(my_topic::generic, "loose_coupled_handler_1");

    constexpr const int wait_time_500ms = 500;
    tools::sleep_for(wait_time_500ms);

    subject1->publish(my_topic::generic, my_event { my_event::type::notification, "tintin" });

    subject2->publish(my_topic::generic, my_event { my_event::type::notification, "tonton" });
    subject2->publish(my_topic::system, my_event { my_event::type::notification, "tantine" });
}

/** @brief Exercises perfect-forwarding subscribe/publish overloads of @c tools::sync_subject with lvalue, rvalue, and implicit-conversion topic and payload arguments. */
void test_sync_observer_perfect_forwarding()
{
    LOG_INFO("-- sync observer perfect forwarding --");
    print_stats();

    tools::sync_subject<std::string, std::string> subject("forwarding_subject");

    std::string topic_lvalue = "demo-topic";
    std::string handler_name_lvalue = "demo-handler";
    std::string payload_lvalue = "payload-lvalue";

    subject.subscribe(topic_lvalue, handler_name_lvalue,
        [](const std::string& topic, const std::string& event, const std::string& origin)
        {
            std::printf("sync-fwd [topic %s] event (%s) from %s\n", topic.c_str(), event.c_str(), origin.c_str());
        });

    subject.publish(topic_lvalue, payload_lvalue);
    subject.publish(std::string("demo-topic"), std::string("payload-rvalue"));
    subject.publish("demo-topic", "payload-conversion");
}

/** @brief Exercises perfect-forwarding @c inform overloads of @c tools::async_observer with lvalue, rvalue, and implicit-conversion arguments, then drains all queued events. */
void test_async_observer_perfect_forwarding()
{
    LOG_INFO("-- async observer perfect forwarding --");
    print_stats();

    tools::async_observer<std::string, std::string, tools::sync_queue> observer;

    std::string topic_lvalue = "demo-topic";
    std::string event_lvalue = "payload-lvalue";
    std::string origin_lvalue = "demo-origin";

    observer.inform(topic_lvalue, event_lvalue, origin_lvalue);
    observer.inform(std::string("demo-topic"), std::string("payload-rvalue"), std::string("demo-origin"));
    observer.inform("demo-topic", "payload-conversion", "demo-origin");

    auto events = observer.pop_all_events();
    // Drain queued async events in batch form; this pattern is useful for worker-loop processing.
    for (const auto& [topic, event, origin] : events)
    {
        std::printf("async-fwd [topic %s] event (%s) from %s\n", topic.c_str(), event.c_str(), origin.c_str());
    }
}

/** @brief Shared cancellation state for generic-task examples. */
struct my_generic_task_context
{
    std::atomic<bool> stop_tasks;
};

using my_generic_task = tools::generic_task<my_generic_task_context>;

void generic_function(const std::shared_ptr<my_generic_task_context>& context, const std::string& task_name)
{
    std::printf("starting generic task %s\n", task_name.c_str());

    constexpr const int sleeping_time_ms = 250;

    while (!context->stop_tasks.load())
    {
        tools::sleep_for(sleeping_time_ms);
        std::printf("loop generic task %s\n", task_name.c_str());
        tools::sleep_for(sleeping_time_ms);
    }

    std::printf("ending generic task %s\n", task_name.c_str());
}

/** @brief Demonstrates launching two generic tasks (one lambda, one function pointer) that run until a shared stop flag is set. */
void test_generic_task()
{
    LOG_INFO("-- generic task --");
    print_stats();

    auto lambda = [](const std::shared_ptr<my_generic_task_context>& context, const std::string& task_name)
    {
        std::printf("starting generic task %s\n", task_name.c_str());

        constexpr const int sleeping_time_ms = 500;

        while (!context->stop_tasks.load())
        {
            std::printf("loop generic task %s\n", task_name.c_str());
            tools::sleep_for(sleeping_time_ms);
        }

        std::printf("ending generic task %s\n", task_name.c_str());
    };

    auto context = std::make_shared<my_generic_task_context>();
    context->stop_tasks.store(false);

    constexpr const std::size_t stack_size = 2048U;

    my_generic_task task1(std::move(lambda), context, "my_generic_task1", stack_size);
    my_generic_task task2(std::move(generic_function), context, "my_generic_task2", stack_size);

    constexpr const int wait_tasks_time_ms = 2000;
    tools::sleep_for(wait_tasks_time_ms);

    context->stop_tasks.store(true);

    constexpr const int wait_join_ms = 1000;
    tools::sleep_for(wait_join_ms);

    std::printf("join tasks\n");
}

/** @brief Exercises perfect-forwarding constructor overloads of @c tools::generic_task with lvalue callback, rvalue callback, and implicit-conversion function pointer inputs. */
void test_generic_task_perfect_forwarding()
{
    LOG_INFO("-- generic task perfect forwarding --");
    print_stats();

    auto context = std::make_shared<my_generic_task_context>();
    context->stop_tasks.store(false);

    my_generic_task::call_back callback_lvalue
        = [](const std::shared_ptr<my_generic_task_context>& ctx, const std::string& task_name)
    {
        std::printf("generic forwarding start %s\n", task_name.c_str());
        ctx->stop_tasks.store(true);
    };

    constexpr const std::size_t stack_size = 2048U;

    my_generic_task task_lvalue(callback_lvalue, context, std::string("generic_forward_lvalue"), stack_size);

    auto context_conversion = std::make_shared<my_generic_task_context>();
    context_conversion->stop_tasks.store(false);
    my_generic_task task_conversion(generic_function, context_conversion, "generic_forward_conversion", stack_size);

    constexpr const int wait_join_ms = 200;
    tools::sleep_for(wait_join_ms);
    context_conversion->stop_tasks.store(true);
    tools::sleep_for(wait_join_ms);
}

/** @brief Shared state for periodic-task examples, storing loop count and timestamp samples. */
struct my_periodic_task_context
{
    std::atomic<int> loop_counter = 0;
    tools::sync_queue<std::chrono::high_resolution_clock::time_point> time_points;
};

using my_periodic_task = tools::periodic_task<my_periodic_task_context>;

/** @brief Demonstrates a periodic task firing at a fixed 20 ms interval and measures elapsed time between successive loop iterations. */
void test_periodic_task()
{
    LOG_INFO("-- periodic task --");
    print_stats();

    auto lambda = [](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter += 1;
        context->time_points.emplace(std::chrono::high_resolution_clock::now());
    };

    auto startup = [](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter = 0;
    };

    auto context = std::make_shared<my_periodic_task_context>();

    constexpr const int period_20ms = 20000;
    constexpr const auto period = std::chrono::duration<std::uint64_t, std::micro>(period_20ms);
    const auto start_timepoint = std::chrono::high_resolution_clock::now();

    constexpr const std::size_t stack_size = 2048U;
    my_periodic_task task1(startup, lambda, context, "my_periodic_task", period, stack_size);

    constexpr const int wait_task_startup_ms = 2000;
    tools::sleep_for(wait_task_startup_ms);

    std::printf("nb of periodic loops = %d\n", context->loop_counter.load());

    auto previous_timepoint = start_timepoint;
    while (!context->time_points.empty())
    {
        const auto measured_timepoint = context->time_points.front_pop();

        if (measured_timepoint.has_value())
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                measured_timepoint.value() - previous_timepoint);
            std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
            previous_timepoint = measured_timepoint.value();
        }
    }
}

/** @brief Exercises perfect-forwarding constructor overloads of @c tools::periodic_task with lvalue startup callback and rvalue periodic callback. */
void test_periodic_task_perfect_forwarding()
{
    LOG_INFO("-- periodic task perfect forwarding --");
    print_stats();

    auto context = std::make_shared<my_periodic_task_context>();

    tools::periodic_task<my_periodic_task_context>::call_back startup_lvalue
        = [](const std::shared_ptr<my_periodic_task_context>& ctx, const std::string& task_name)
    {
        (void)task_name;
        ctx->loop_counter.store(0);
    };

    auto periodic_rvalue = [](const std::shared_ptr<my_periodic_task_context>& ctx, const std::string& task_name)
    {
        (void)task_name;
        ctx->loop_counter.fetch_add(1);
    };

    constexpr std::size_t stack_size = 2048U;
    constexpr const int period_ms = 20;
    tools::periodic_task<my_periodic_task_context> task(startup_lvalue, std::move(periodic_rvalue), context,
        "my_periodic_forwarding_task", std::chrono::milliseconds(period_ms), stack_size);

    constexpr const int wait_time_ms = 200;
    tools::sleep_for(wait_time_ms);
    std::printf("periodic forwarding loop counter = %d\n", context->loop_counter.load());
}

/** @brief Exercises perfect-forwarding @c add / @c add_range overloads of @c tools::histogram with scalar lvalue, rvalue, implicit conversion, and range inputs. */
void test_histogram_perfect_forwarding()
{
    LOG_INFO("-- histogram perfect forwarding --");
    print_stats();

    tools::histogram<double> hist;

    constexpr const double value_exact = 2.5;
    constexpr const int value_int_conversion = 2;
    constexpr const float value_float_conversion = 2.5F;
    constexpr const std::array<double, 2U> repeated_values = { 3.0, 3.0 };

    double lvalue_value = value_exact;
    hist.add(lvalue_value);
    hist.add(value_exact);
    hist.add(value_int_conversion);
    hist.add(value_float_conversion);

    const std::vector<int> range_values = { 2, 3, 3 };
    hist.add_range(range_values);
    hist.add_range(repeated_values);

    std::printf("histogram total=%d top=%f occ=%d\n", hist.total_count(), hist.top(), hist.top_occurence());
}

/** @brief Demonstrates histogram statistics on @c fpm::fixed_16_16 samples without converting storage to floating-point. */
void test_histogram_fixed_16_16()
{
    LOG_INFO("-- histogram fpm::fixed_16_16 --");
    print_stats();

    using fixed_scalar = fpm::fixed_16_16;
    tools::histogram<fixed_scalar> hist;

    // Keep samples deterministic and exactly representable in 16.16 fixed-point.
    const std::array<fixed_scalar, 8U> samples = {
        fixed_scalar(1.5), fixed_scalar(0.5), fixed_scalar(1.5), fixed_scalar(-0.5),
        fixed_scalar(0.25), fixed_scalar(0.25), fixed_scalar(1.5), fixed_scalar(0.5)
    };

    hist.add_range(samples);

    const auto average = hist.average();
    const auto variance = hist.variance(average);
    const auto stddev = hist.standard_deviation(variance);
    const auto median = hist.median();
    const auto density_at_top = hist.gaussian_density(hist.top(), average, stddev);

    std::printf("histogram fixed16 total=%d top=%.6f occ=%d\n", hist.total_count(), static_cast<double>(hist.top()),
        hist.top_occurence());
    std::printf("fixed16 avg=%.6f median=%.6f var=%.6f stdev=%.6f density(top)=%.6f\n", average, median, variance,
        stddev, density_at_top);
}

class my_collector : public base_observer
{
public:
    my_collector() = default;
    ~my_collector() override = default;

    my_collector(const my_collector&) = delete;
    my_collector& operator=(const my_collector&) = delete;
    my_collector(my_collector&&) = delete;
    my_collector& operator=(my_collector&&) = delete;

    /**
     * @brief Accumulates a numerical sample from the event description into the internal histogram.
     * @param topic Topic of the incoming event (unused).
     * @param event Event whose description is parsed as a @c double sample value.
     * @param origin Origin of the event (unused).
     */
    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        (void)topic;
        (void)origin;

        m_histogram.add(std::strtod(event.description.c_str(), nullptr));
    }

    /** @brief Prints histogram statistics: top value, average, median, variance, standard deviation, and Gaussian metrics. */
    void display_stats()
    {
        constexpr const double gaussian_lower_bound_ratio = 0.5;
        constexpr const int gaussian_steps = 100;

        auto top = m_histogram.top();
        std::printf("\nvalue %f appears %d times\n", top, m_histogram.top_occurence());
        auto avg = m_histogram.average();
        std::printf("average value is %f\n", avg);
        std::printf("median value is %f\n", m_histogram.median());
        auto variance = m_histogram.variance(avg);
        std::printf("variance is %f\n", variance);
        auto std_deviation = m_histogram.standard_deviation(variance);
        std::printf("standard deviation is %f\n", std_deviation);
        std::printf("gaussian density of %f is %f\n", top, m_histogram.gaussian_density(top, avg, std_deviation));
        std::printf("gaussian probability of [%f, %f] is %f\n", gaussian_lower_bound_ratio * top, top,
            m_histogram.gaussian_probability(
                gaussian_lower_bound_ratio * top, top, avg, std_deviation, gaussian_steps));
    }

private:
    tools::histogram<double> m_histogram;
};

/** @brief Demonstrates a periodic task publishing a sine-wave signal to both an async observer and a histogram-collecting observer to illustrate integrated pub/sub with statistics. */
void test_periodic_publish_subscribe()
{
    LOG_INFO("-- periodic publish subscribe --");
    print_stats();

    auto monitoring = std::make_shared<my_async_observer>();
    auto data_source = std::make_shared<my_subject>("data_source");
    auto histogram_feeder = std::make_shared<my_collector>();

    auto sampler = [&data_source](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter += 1;

        // Model a sampled sensor waveform and publish it as a textual payload.
        double signal = std::sin(context->loop_counter.load());

        data_source->publish(my_topic::external, my_event { my_event::type::notification, std::to_string(signal) });
    };

    auto startup = [](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter = 0;
    };

    // Feed both an async monitor and an analytics sink from the same topic.
    data_source->subscribe(my_topic::external, monitoring);
    data_source->subscribe(my_topic::external, histogram_feeder);

    auto context = std::make_shared<my_periodic_task_context>();
    constexpr const std::size_t stack_size = 4096U;
    const auto period = std::chrono::duration<std::uint64_t, std::milli>(100);
    {
        my_periodic_task periodic_task(startup, sampler, context, "sampler_task", period, stack_size);

        constexpr const std::size_t wait_task = 2000;
        tools::sleep_for(wait_task);
    }

    histogram_feeder->display_stats();
}
} // namespace

void run_example_pub_sub_and_task()
{
    // Begin with histogram API forwarding so statistical sinks are easy to integrate in observers.
    test_histogram_perfect_forwarding();
    // Show histogram behavior directly on fixed-point values (fpm::fixed_16_16).
    test_histogram_fixed_16_16();
    // Validate sync observer forwarding behaviour for low-latency in-thread delivery.
    test_sync_observer_perfect_forwarding();
    // Then validate async observer forwarding and queued event draining.
    test_async_observer_perfect_forwarding();
    // Study full pub/sub wiring: topics, subscriptions, loose handlers, and unsubscriptions.
    test_publish_subscribe();
    // Move to generic task orchestration patterns (function object and function pointer callbacks).
    test_generic_task();
    test_generic_task_perfect_forwarding();
    // Add periodic scheduling examples and timing analysis.
    test_periodic_task();
    test_periodic_task_perfect_forwarding();
    // Finish with a realistic pipeline: periodic sampling -> pub/sub -> histogram-based analytics.
    test_periodic_publish_subscribe();
}
