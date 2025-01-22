//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois      //
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

#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>


#include "bytepack/bytepack.hpp"

#include "tools/async_observer.hpp"
#include "tools/histogram.hpp"
#include "tools/periodic_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/ring_buffer.hpp"
#include "tools/sync_dictionary.hpp"
#include "tools/sync_observer.hpp"
#include "tools/sync_queue.hpp"
#include "tools/sync_ring_buffer.hpp"
#include "tools/worker_task.hpp"

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer()
{
    std::printf("-- ring buffer --\n");
    tools::ring_buffer<std::string, 64U> str_queue;

    str_queue.emplace("toto");

    auto item = str_queue.front();

    std::printf("%s\n", item.c_str());

    str_queue.pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_ring_buffer()
{
    std::printf("-- sync ring buffer --\n");
    tools::sync_ring_buffer<std::string, 64U> str_queue;

    str_queue.emplace("toto");

    auto item = str_queue.front();

    std::printf("%s\n", item.c_str());

    str_queue.pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_queue()
{
    std::printf("-- sync queue --\n");
    tools::sync_queue<std::string> str_queue;

    str_queue.emplace("toto");

    auto item = str_queue.front();

    std::printf("%s\n", item.c_str());

    str_queue.pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_dictionary()
{
    std::printf("-- sync dictionary --\n");
    tools::sync_dictionary<std::string, std::string> str_dict;

    str_dict.add("toto", "blob");

    auto result = str_dict.find("toto");

    if (result.has_value())
    {
        std::printf("%s\n", (*result).c_str());
        str_dict.remove("toto");
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

enum class my_topic
{
    generic,
    system,
    external
};

using base_observer = tools::sync_observer<my_topic, std::string>;
class my_observer : public base_observer
{
public:
    my_observer() = default;
    virtual ~my_observer() { }

    virtual void inform(const my_topic& topic, const std::string& event, const std::string& origin) override
    {
        std::printf("sync [topic %d] received: event (%s) from %s\n", static_cast<std::underlying_type<my_topic>::type>(topic),
            event.c_str(), origin.c_str());
    }

private:
};

using base_async_observer = tools::async_observer<my_topic, std::string>;
class my_async_observer : public base_async_observer
{
public:
    my_async_observer()
        : m_task_loop([this]() { handle_events(); })
    {
    }

    virtual ~my_async_observer()
    {
        m_stop_task.store(true);
        m_task_loop.join();
    }

    virtual void inform(const my_topic& topic, const std::string& event, const std::string& origin) override
    {
        std::printf("async/push [topic %d] received: event (%s) from %s\n", static_cast<std::underlying_type<my_topic>::type>(topic),
            event.c_str(), origin.c_str());

        base_async_observer::inform(topic, event, origin);
    }

private:
    void handle_events()
    {
        const auto timeout = std::chrono::duration<int, std::micro>(1000);

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
                        static_cast<std::underlying_type<my_topic>::type>(topic), event.c_str(), origin.c_str());
                }
            }
        }
    }

    std::thread m_task_loop;
    std::atomic_bool m_stop_task = false;
};

using base_subject = tools::sync_subject<my_topic, std::string>;
class my_subject : public base_subject
{
public:
    my_subject() = delete;
    my_subject(const std::string name)
        : base_subject(name)
    {
    }

    virtual ~my_subject() { }

    virtual void publish(const my_topic& topic, const std::string& event) override
    {
        std::printf("publish: event (%s) to %s\n", event.c_str(), name().c_str());
        base_subject::publish(topic, event);
    }

private:
};

void test_publish_subscribe()
{
    std::printf("-- publish subscribe --\n");
    auto observer1 = std::make_shared<my_observer>();
    auto observer2 = std::make_shared<my_observer>();
    auto async_observer = std::make_shared<my_async_observer>();
    auto subject1 = std::make_shared<my_subject>("source1");
    auto subject2 = std::make_shared<my_subject>("source2");

    subject1->subscribe(my_topic::generic, observer1);
    subject1->subscribe(my_topic::generic, observer2);
    subject1->subscribe(my_topic::system, observer2);
    subject1->subscribe(my_topic::generic, async_observer);

    subject2->subscribe(my_topic::generic, observer1);
    subject2->subscribe(my_topic::generic, observer2);
    subject2->subscribe(my_topic::system, observer2);
    subject2->subscribe(my_topic::generic, async_observer);

    subject1->subscribe(my_topic::generic, "loose_coupled_handler_1",
        [](const my_topic& topic, const std::string& event, const std::string& origin)
        {
            std::printf("handler [topic %d] received: event (%s) from %s\n", static_cast<std::underlying_type<my_topic>::type>(topic),
                event.c_str(), origin.c_str());
        });

    subject1->publish(my_topic::generic, "toto");

    subject1->unsubscribe(my_topic::generic, observer1);

    subject1->publish(my_topic::generic, "titi");

    subject1->publish(my_topic::system, "tata");

    subject1->unsubscribe(my_topic::generic, "loose_coupled_handler_1");

    tools::sleep_for(500);

    subject1->publish(my_topic::generic, "tintin");

    subject2->publish(my_topic::generic, "tonton");
    subject2->publish(my_topic::system, "tantine");
}

//--------------------------------------------------------------------------------------------------------------------------------

struct my_periodic_task_context
{
    std::atomic<int> loop_counter = 0;
    tools::sync_queue<std::chrono::high_resolution_clock::time_point> time_points;
};

using my_periodic_task = tools::periodic_task<my_periodic_task_context>;

void test_periodic_task()
{
    std::printf("-- periodic task --\n");
    auto lambda = [](std::shared_ptr<my_periodic_task_context> context, const std::string& task_name) -> void
    {
        (void)task_name;
        context->loop_counter += 1;
        context->time_points.emplace(std::chrono::high_resolution_clock::now());
    };

    auto startup = [](std::shared_ptr<my_periodic_task_context> context, const std::string& task_name) -> void
    {
        (void)task_name;
        context->loop_counter = 0;
    };

    auto context = std::make_shared<my_periodic_task_context>();
    // 20 ms period
    constexpr const auto period = std::chrono::duration<int, std::micro>(20000);
    const auto start_timepoint = std::chrono::high_resolution_clock::now();
    my_periodic_task task1(startup, lambda, context, "my_periodic_task", period, 2048U);

    // sleep 2 sec
    tools::sleep_for(2000);

    std::printf("nb of periodic loops = %d\n", context->loop_counter.load());

    auto previous_timepoint = start_timepoint;
    while (!context->time_points.empty())
    {
        const auto measured_timepoint = context->time_points.front();
        context->time_points.pop();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(measured_timepoint - previous_timepoint);
        std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
        previous_timepoint = measured_timepoint;
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

class my_collector : public base_observer
{
public:
    my_collector() = default;
    virtual ~my_collector() { }

    virtual void inform(const my_topic& topic, const std::string& event, const std::string& origin) override
    {
        (void)topic;
        (void)origin;

        m_histogram.add(static_cast<double>(std::strtod(event.c_str(), nullptr)));
    }

    void display_stats()
    {
        auto top = m_histogram.top();
        std::printf("\nvalue %f appears %d times\n", top, m_histogram.top_occurence());
        auto avg = m_histogram.average();
        std::printf("average value is %f\n", avg);
        std::printf("median value is %f\n", m_histogram.median());
        auto variance = m_histogram.variance(avg);
        std::printf("variance is %f\n", variance);
        std::printf("gaussian probability of %f occuring is %f\n", top, m_histogram.gaussian_probability(top, avg, variance));
    }

private:
    tools::histogram<double> m_histogram;
};

//--------------------------------------------------------------------------------------------------------------------------------

void test_periodic_publish_subscribe()
{
    std::printf("-- periodic publish subscribe --\n");
    auto monitoring = std::make_shared<my_async_observer>();
    auto data_source = std::make_shared<my_subject>("data_source");
    auto histogram_feeder = std::make_shared<my_collector>();

    auto sampler = [&data_source](std::shared_ptr<my_periodic_task_context> context, const std::string& task_name) -> void
    {
        (void)task_name;
        context->loop_counter += 1;

        // mocked signal
        double signal = std::sin(context->loop_counter.load());

        // emit "signal" as a 'string' event
        data_source->publish(my_topic::external, std::to_string(signal));
    };

    auto startup = [](std::shared_ptr<my_periodic_task_context> context, const std::string& task_name) -> void
    {
        (void)task_name;
        context->loop_counter = 0;
    };

    data_source->subscribe(my_topic::external, monitoring);
    data_source->subscribe(my_topic::external, histogram_feeder);

    // "sample" with a 100 ms period
    auto context = std::make_shared<my_periodic_task_context>();
    const auto period = std::chrono::duration<int, std::milli>(100);
    {
        my_periodic_task periodic_task(startup, sampler, context, "sampler_task", period, 4096U);

        tools::sleep_for(2000);
    }

    histogram_feeder->display_stats();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_queued_commands()
{
    std::printf("-- queued commands --\n");
    tools::sync_queue<std::function<void()>> commands_queue;

    commands_queue.emplace([]() { std::printf("hello\n"); });

    commands_queue.emplace([]() { std::printf("world\n"); });

    while (!commands_queue.empty())
    {
        auto call = commands_queue.front();
        call();
        commands_queue.pop();
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer_commands()
{
    std::printf("-- ring buffer commands --\n");
    tools::sync_ring_buffer<std::function<void()>, 128U> commands_queue;

    commands_queue.emplace([]() { std::printf("hello\n"); });

    commands_queue.emplace([]() { std::printf("world\n"); });

    while (!commands_queue.empty())
    {
        auto call = commands_queue.front();
        call();
        commands_queue.pop();
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

struct my_worker_task_context
{
    std::atomic<int> loop_counter = 0;
    tools::sync_queue<std::chrono::high_resolution_clock::time_point> time_points;
};

using my_worker_task = tools::worker_task<my_worker_task_context>;

void test_worker_tasks()
{
    std::printf("-- worker tasks --\n");

    auto startup1 = [](std::shared_ptr<my_worker_task_context> context, const std::string& task_name) -> void
    {
        (void)context;
        std::printf("welcome 1\n");
        std::printf("task %s started\n", task_name.c_str());
    };

    auto startup2 = [](std::shared_ptr<my_worker_task_context> context, const std::string& task_name) -> void
    {
        (void)context;
        std::printf("welcome 2\n");
        std::printf("task %s started\n", task_name.c_str());
    };

    auto context = std::make_shared<my_worker_task_context>();

    auto task1 = std::make_unique<my_worker_task>(std::move(startup1), context, "worker_1", 4096U);
    auto task2 = std::make_unique<my_worker_task>(std::move(startup2), context, "worker_2", 4096U);

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 1);
    std::array<std::unique_ptr<my_worker_task>, 2> tasks = {std::move(task1), std::move(task2)};

    tools::sleep_for(100); // 100 ms

    const auto start_timepoint = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 20; ++i)
    {
        auto idx = distribution(generator);

        tasks[idx]->delegate(
            [](auto context, const auto& task_name) -> void
            {
                std::printf("job %d on worker task %s\n", context->loop_counter.load(), task_name.c_str());
                context->loop_counter++;
                context->time_points.emplace(std::chrono::high_resolution_clock::now());
            });

        std::this_thread::yield();    
    }

    // sleep 2 sec
    tools::sleep_for(2000);

    std::printf("nb of loops = %d\n", context->loop_counter.load());

    auto previous_timepoint = start_timepoint;
    while (!context->time_points.empty())
    {
        const auto measured_timepoint = context->time_points.front();
        context->time_points.pop();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(measured_timepoint - previous_timepoint);
        std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
        previous_timepoint = measured_timepoint;
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

enum class message_type : std::uint8_t
{
    temperature = 1,
    manufacturing = 2,
    freetext = 3
};

struct temperature_sensor
{
    double cpu_temperature;
    double gpu_temperature;
    double room_temperature;

    void serialize(bytepack::binary_stream<>& stream) const { stream.write(cpu_temperature, gpu_temperature, room_temperature); }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(cpu_temperature, gpu_temperature, room_temperature);
        return true; // pretend success
    }
};

struct manufacturing_info
{
    char vendor_name[32];
    char serial_number[16];
    char manufacturing_date[10];

    void serialize(bytepack::binary_stream<>& stream) const { stream.write(vendor_name, serial_number, manufacturing_date); }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(vendor_name, serial_number, manufacturing_date);
        return true; // pretend success
    }
};

struct free_text
{
    std::string text;

    void serialize(bytepack::binary_stream<>& stream) const { stream.write(text); }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(text);
        return true; // pretend success
    }
};

void test_queued_bytepack_data()
{
    std::printf("-- queued bytepack data --\n");
    tools::sync_ring_buffer<std::vector<std::uint8_t>, 128U> data_queue;
    // tools::sync_queue<std::vector<std::uint8_t>> data_queue;

    free_text message1 = { "jojo rabbit" };
    manufacturing_info message2 = { "NVidia", "HTX-4589-22-1", "24/05/02" };
    temperature_sensor message3 = { 45.2, 51.72, 21.5 };

    std::array<std::uint8_t, 1024> buffer;
    bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer) };

    binary_stream.reset();
    binary_stream.write(message_type::freetext);
    message1.serialize(binary_stream);
    auto buffer1 = binary_stream.data();
    std::vector<std::uint8_t> encoded1;
    encoded1.resize(buffer1.size());
    std::memcpy(encoded1.data(), buffer1.as<void>(), buffer1.size());
    data_queue.emplace(std::move(encoded1));

    binary_stream.reset();
    binary_stream.write(message_type::manufacturing);
    message2.serialize(binary_stream);
    auto buffer2 = binary_stream.data();
    std::vector<std::uint8_t> encoded2;
    encoded2.resize(buffer2.size());
    std::memcpy(encoded2.data(), buffer2.as<void>(), buffer2.size());
    data_queue.emplace(std::move(encoded2));

    binary_stream.reset();
    binary_stream.write(message_type::temperature);
    message3.serialize(binary_stream);
    auto buffer3 = binary_stream.data();
    std::vector<std::uint8_t> encoded3;
    encoded3.resize(buffer3.size());
    std::memcpy(encoded3.data(), buffer3.as<void>(), buffer3.size());
    data_queue.emplace(std::move(encoded3));

    while (!data_queue.empty())
    {
        auto data_packed = data_queue.front();

        bytepack::binary_stream stream(bytepack::buffer_view(data_packed.data(), data_packed.size()));

        message_type discriminant;
        stream.read(discriminant);

        switch (discriminant)
        {
            case message_type::freetext:
            {
                free_text text;
                text.deserialize(stream);
                std::printf("%s\n", text.text.c_str());
            }
            break;

            case message_type::manufacturing:
            {
                manufacturing_info info;
                info.deserialize(stream);
                std::printf("%s\n%s\n%s\n", info.vendor_name, info.serial_number, info.manufacturing_date);
            }
            break;

            case message_type::temperature:
            {
                temperature_sensor temp;
                temp.deserialize(stream);
                std::printf("%f\n%f\n%f\n", temp.cpu_temperature, temp.gpu_temperature, temp.room_temperature);
            }
            break;
        };

        data_queue.pop();
    }
}

//--------------------------------------------------------------------------------------------------------------------------------


#if defined(FREERTOS_PLATFORM)
extern "C" void app_main()
#else
int main()
#endif
{
    test_ring_buffer();
    test_sync_ring_buffer();
    test_sync_queue();
    test_sync_dictionary();

    test_publish_subscribe();
    test_periodic_task();
    test_periodic_publish_subscribe();

    test_queued_commands();
    test_ring_buffer_commands();
    test_worker_tasks();
    test_queued_bytepack_data();

#if !defined(FREERTOS_PLATFORM)
    return 0;
#endif
}
