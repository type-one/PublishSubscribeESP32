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

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "CException/CException.h"
#include "bytepack/bytepack.hpp"
#define CJSONPP_NO_EXCEPTION
#include "cjsonpp/cjsonpp.hpp"

#include "tools/async_observer.hpp"
#include "tools/data_task.hpp"
#include "tools/gzip_wrapper.hpp"
#include "tools/histogram.hpp"
#include "tools/logger.hpp"
#include "tools/periodic_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/ring_buffer.hpp"
#include "tools/sync_dictionary.hpp"
#include "tools/sync_observer.hpp"
#include "tools/sync_queue.hpp"
#include "tools/sync_ring_buffer.hpp"
#include "tools/variant_overload.hpp"
#include "tools/worker_task.hpp"

#if defined(ESP_PLATFORM)
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_system.h>
#include <freertos/queue.h>
#include <hal/gpio_types.h>
#include <sdkconfig.h>
#endif

//--------------------------------------------------------------------------------------------------------------------------------

inline void print_stats()
{
#if defined(ESP_PLATFORM)
    std::printf("Current free heap size: %" PRIu32 " bytes\n", esp_get_free_heap_size());
    std::printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
#endif
#if defined(FREERTOS_PLATFORM)
    UBaseType_t ux_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
    std::printf("Minimum free stack size: %d bytes\n", ux_high_water_mark);
#endif
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer()
{
    LOG_INFO("-- ring buffer --");
    print_stats();

    auto str_queue = std::make_unique<tools::ring_buffer<std::string, 64U>>();

    str_queue->emplace("toto");

    auto item = str_queue->front();

    std::printf("%s\n", item.c_str());

    str_queue->pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer_iteration()
{
    LOG_INFO("-- ring buffer iteration --");
    print_stats();

    {
        auto str_queue = std::make_unique<tools::ring_buffer<std::string, 64U>>();

        str_queue->emplace("toto1");
        str_queue->emplace("toto2");
        str_queue->emplace("toto3");
        str_queue->emplace("toto4");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        std::for_each(str_queue->begin(), str_queue->end(), [](const auto& item) { std::printf("%s\n", item.c_str()); });

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        std::for_each(str_queue->begin(), str_queue->end(), [](const auto& item) { std::printf("%s\n", item.c_str()); });

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (const auto& item : *str_queue)
        {
            std::printf("%s\n", item.c_str());
        }

        str_queue->push("toto5");
        str_queue->push("toto6");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (const auto& item : *str_queue)
        {
            std::printf("%s\n", item.c_str());
        }

        int cnt = 0;
        while (!str_queue->full())
        {
            str_queue->emplace("tintin" + std::to_string(cnt));
            ++cnt;
        }

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (const auto& item : *str_queue)
        {
            std::printf("%s\n", item.c_str());
        }

        const std::size_t remove_count = str_queue->size() - 5U;
        for (std::size_t i = 0U; i < remove_count; ++i)
        {
            str_queue->pop();
        }

        str_queue->push("toutou1");
        str_queue->push("toutou2");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (const auto& item : *str_queue)
        {
            std::printf("%s\n", item.c_str());
        }
    }

    {
        auto val_queue = std::make_unique<tools::ring_buffer<double, 64U>>();

        val_queue->emplace(5.6);
        val_queue->emplace(7.2);
        val_queue->emplace(1.2);
        val_queue->emplace(9.1);
        val_queue->emplace(10.1);
        val_queue->emplace(7.5);

        std::size_t cnt = val_queue->size() - 1U;

        for (std::size_t i = 0U; i < cnt; ++i)
        {
            std::printf("content\n");

            for (const auto& item : *val_queue)
            {
                std::printf("%f\n", item);
            }

            const auto avg_val = std::accumulate(val_queue->begin(), val_queue->end(), 0.0, std::plus<double>()) / val_queue->size();
            std::printf("avg: %f\n", avg_val);

            const auto sqsum_val = std::transform_reduce(val_queue->begin(), val_queue->end(), 0.0, std::plus<double>(),
                [&avg_val](const auto& val)
                {
                    const auto d = (val - avg_val);
                    return d * d;
                });

            const auto variance_val = std::sqrt(sqsum_val / val_queue->size());
            std::printf("variance: %f\n", variance_val);

            auto content = std::make_unique<std::vector<double>>(val_queue->size());
            std::copy(val_queue->begin(), val_queue->end(), content->begin());

            const auto [min_val_it, max_val_it] = std::minmax_element(content->cbegin(), content->cend());

            std::printf("min: %f\n", *min_val_it);
            std::printf("max: %f\n", *max_val_it);

            std::printf("pop front\n");
            val_queue->pop();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_ring_buffer()
{
    LOG_INFO("-- sync ring buffer --");
    print_stats();

    tools::sync_ring_buffer<std::string, 64U> str_queue;

    str_queue.emplace("toto");

    auto item = str_queue.front();

    std::printf("%s\n", item.c_str());

    str_queue.pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_queue()
{
    LOG_INFO("-- sync queue --");
    print_stats();

    tools::sync_queue<std::string> str_queue;

    str_queue.emplace("toto");

    auto item = str_queue.front();

    std::printf("%s\n", item.c_str());

    str_queue.pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_dictionary()
{
    LOG_INFO("-- sync dictionary --");
    print_stats();

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
    virtual ~my_observer()
    {
    }

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

    virtual ~my_subject()
    {
    }

    virtual void publish(const my_topic& topic, const std::string& event) override
    {
        std::printf("publish: event (%s) to %s\n", event.c_str(), name().c_str());
        base_subject::publish(topic, event);
    }

private:
};

void test_publish_subscribe()
{
    LOG_INFO("-- publish subscribe --");
    print_stats();

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
    LOG_INFO("-- periodic task --");
    print_stats();

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
    virtual ~my_collector()
    {
    }

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
    LOG_INFO("-- periodic publish subscribe --");
    print_stats();

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
    LOG_INFO("-- queued commands --");
    print_stats();

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
    LOG_INFO("-- ring buffer commands --");
    print_stats();

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
    LOG_INFO("-- worker tasks --");
    print_stats();

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
    std::array<std::unique_ptr<my_worker_task>, 2> tasks = { std::move(task1), std::move(task2) };

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

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(cpu_temperature, gpu_temperature, room_temperature);
    }

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

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(vendor_name, serial_number, manufacturing_date);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(vendor_name, serial_number, manufacturing_date);
        return true; // pretend success
    }
};

struct free_text
{
    std::string text;

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(text);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(text);
        return true; // pretend success
    }
};

void test_queued_bytepack_data()
{
    LOG_INFO("-- queued bytepack data --");
    print_stats();

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

        message_type discriminant = {};
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

void test_json()
{
    LOG_INFO("-- json serialization/deserialization --");
    print_stats();

    {
        // create an empty structure (null)
        cjsonpp::JSONObject obj = {};
        cjsonpp::JSONObject obj2 = {};
        cjsonpp::JSONObject obj3 = {};
        cjsonpp::JSONObject obj4 = {};

        obj.set("pi", 3.141592);                   // add a number that is stored as double
        obj.set("happy", true);                    // add a Boolean that is stored as bool
        obj.set("name", "Niels");                  // add a string that is stored as std::string
        obj.set("nothing", cjsonpp::nullObject()); // add another null object by passing nullptr

        // add an array that is stored as std::vector (using an initializer list)
        std::vector<int> v = { 0, 1, 2 };
        obj.set("list", v);

        // add an object inside the object
        obj2.set("everything", 42);
        obj.set("answer", obj2);

        // add another object (using an initializer list of pairs)
        obj3.set("currency", "USD");
        obj4.set("value", 42.99);

        cjsonpp::JSONObject arr = cjsonpp::arrayObject();
        arr.add(obj3);
        arr.add(obj4);

        obj.set("object", arr);

        const auto s = obj.print();
        std::printf("%s\n", s.c_str());
    }

    {
        // create object from string literal
        std::string jsonstr = "{ \"happy\": true, \"pi\": 3.141 }";
        cjsonpp::JSONObject obj = cjsonpp::parse(jsonstr);

        const auto s = obj.print();
        std::printf("%s\n", s.c_str());
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_queued_json_data()
{
    LOG_INFO("-- queued json data --");
    print_stats();

    auto data_queue = std::make_unique<tools::sync_ring_buffer<std::string, 128U>>();

    {
        cjsonpp::JSONObject json = {};
        json.set("msg_type", "sensor");
        json.set("sensor_name", "indoor_temperature");
        json.set("temp", 19.47);
        json.set("activity", true);

        cjsonpp::JSONObject json_answer = {};
        json_answer.set("everything", 42);
        json.set("answer", json_answer);

        std::printf("%s\n", json.print(true).c_str());
        data_queue->emplace(json.print(false));
    }

    {
        cjsonpp::JSONObject json = {};
        json.set("msg_type", "time");
        json.set("yyyy_mm_dd", "2025/01/13");
        json.set("hh_mm_ss", "23:05:12");
        json.set("time_zone", "GMT+2");

        std::printf("%s\n", json.print(true).c_str());
        data_queue->emplace(json.print(false));
    }

    while (!data_queue->empty())
    {
        auto data = data_queue->front();
        cjsonpp::JSONObject json = cjsonpp::parse(data);

        const auto discriminant = json.get<std::string>("msg_type");

        if (discriminant == "sensor")
        {
            const auto name = json.get<std::string>("sensor_name");
            const auto temp = json.get<double>("temp");
            const auto activity = json.get<bool>("activity");
            const auto& obj = json.get("answer");
            const auto answer = obj.get<int>("everything");
            std::printf("sensor: %s - temp %f - %s - answer (%d)\n", name.c_str(), temp, activity ? "on" : "off", answer);
        }
        else if (discriminant == "time")
        {
            const auto time_date = json.get<std::string>("yyyy_mm_dd");
            const auto time_clock = json.get<std::string>("hh_mm_ss");
            const auto time_zone = json.get<std::string>("time_zone");
            std::printf("time: %s - %s - %s\n", time_date.c_str(), time_clock.c_str(), time_zone.c_str());
        }

        data_queue->pop();
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_packing_unpacking_json_data()
{
    LOG_INFO("-- packing/unpacking json data --");
    print_stats();

    // example taken from https://www.iotforall.com/10-jsonata-examples
    static const char json_str1[] = R"(
    {
      "device": "dev:5c0272356817",
      "when": 1643398446,
      "body": {
        "humid": 56.23,
        "temp": 35.52
      },
      "best_location_type": "triangulated",
      "tower_country": "US",
      "tower_lat": 44.9288392,
      "tower_lon": -84.9283943,
      "tower_location": "Grand Ledge, MI",
      "tower_id": "310,410,25878,88213007",
      "tri_country": "US",
      "tri_lat": 44.76386883,
      "tri_lon": -83.64839822,
      "tri_location": "Lansing, MI",
      "settings": [
        { "name": "power_saving", "value": false },
        { "name": "detect_motion", "value": true },
        { "name": "sample_interval", "value": 5 }
      ]
    })";

    // example taken from https://json.org/example.html
    static const char json_str2[] = R"(
    {
        "glossary": {
            "title": "example glossary",
            "GlossDiv": {
                "title": "S",
                "GlossList": {
                    "GlossEntry": {
                        "ID": "SGML",
                        "SortAs": "SGML",
                        "GlossTerm": "Standard Generalized Markup Language",
                        "Acronym": "SGML",
                        "Abbrev": "ISO 8879:1986",
                        "GlossDef": {
                            "para": "A meta-markup language, used to create markup languages such as DocBook.",
                            "GlossSeeAlso": ["GML", "XML"]
                        },
                        "GlossSee": "markup"
                    }
                }
            }
        }
    })";

    tools::gzip_wrapper gzip;

    std::vector<std::uint8_t> unpacked_buffer;
    std::vector<std::uint8_t> packed_buffer;

    // json 1

    unpacked_buffer.resize(sizeof(json_str1));
    std::memcpy(unpacked_buffer.data(), json_str1, sizeof(json_str1));

    std::printf("json file 1 of %zu bytes\n", unpacked_buffer.size());
    std::printf("packing json file 1\n");

    packed_buffer = gzip.pack(unpacked_buffer);

    std::printf("compressed to %zu bytes\n", packed_buffer.size());
    std::printf("unpacking gzip file 1\n");

    unpacked_buffer = gzip.unpack(packed_buffer);

    std::printf("unpacked to %zu bytes\n", unpacked_buffer.size());

    // json 2

    unpacked_buffer.resize(sizeof(json_str2));
    std::memcpy(unpacked_buffer.data(), json_str2, sizeof(json_str2));

    std::printf("json file 2 of %zu bytes\n", unpacked_buffer.size());
    std::printf("packing json file 2\n");

    packed_buffer = gzip.pack(unpacked_buffer);

    std::printf("compressed to %zu bytes\n", packed_buffer.size());
    std::printf("unpacking gzip file 2\n");

    unpacked_buffer = gzip.unpack(packed_buffer);

    std::printf("unpacked to %zu bytes\n", unpacked_buffer.size());
}

//--------------------------------------------------------------------------------------------------------------------------------

// clang-format off

// https://www.cppstories.com/2023/finite-state-machines-variant-cpp/

// possible states
namespace traffic_light_state
{
    struct off {};
    struct operable_initializing {};
    struct operable_red
    {
        int count = 0;
    };
    struct operable_orange
    {
        int count = 0;
    };
    struct operable_green
    {
        int count = 0;
    };
}

// possible events
namespace traffic_light_event
{
    struct power_on {};
    struct power_off {};
    struct init_done {};
    struct next_state {};
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
// clang-format on

class traffic_light_fsm
{
public:
    traffic_light_fsm() = default;
    ~traffic_light_fsm() = default;

    void start();

    void handle_event(const traffic_light_event_v& event);
    void update();

private:
    traffic_light_state_v m_state = traffic_light_state::off {}; // init state

    // states transitions methods 
    // define callbacks for [state, event]

    traffic_light_state_v on_event(const traffic_light_state::off& state, const traffic_light_event::power_on& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::init_done& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_red& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_green& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_red& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(const traffic_light_state::operable_green& state, const traffic_light_event::power_off& event);

    // fallback for undefined transitions
    traffic_light_state_v on_event(const auto&, const auto&);

    // defines callbacks for [state]
    void on_state(const traffic_light_state::off& state);
    void on_state(const traffic_light_state::operable_initializing& state);
    void on_state(const traffic_light_state::operable_red& state);
    void on_state(const traffic_light_state::operable_orange& state);
    void on_state(const traffic_light_state::operable_green& state);

    // fallback for undefined state
    void on_state(const auto&);
};

void traffic_light_fsm::start()
{
   m_state = traffic_light_state::off {}; // init state 
}

// event processing routines invoking states transitions routines through visitor
void traffic_light_fsm::handle_event(const traffic_light_event_v& event)
{
    // clang-format off
    m_state
        = std::visit(tools::detail::overload 
            { 
            [&](const auto& state, const auto& evt)
                { 
                return traffic_light_fsm::on_event(state, evt);
                }
            },
            m_state,
            event);
    // clang-format on       
}

// update cycle based on current state
void traffic_light_fsm::update()
{
    // clang-format off
    std::visit(tools::detail::overload 
        { 
        [&](const auto& state)
            { 
            return traffic_light_fsm::on_state(state);
            }
        },
        m_state);
    // clang-format on  
}

// define callbacks for [state, event]
traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::off& state, const traffic_light_event::power_on& event)
{
    std::printf("switch ON traffic light\n");
    tools::sleep_for(1000);

    return traffic_light_state::operable_initializing {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::init_done& event)
{
    std::printf("init traffic light completed\n");

    return traffic_light_state::operable_red { 0 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_red& state, const traffic_light_event::next_state& event)
{    
    tools::sleep_for(1000);

    traffic_light_state_v next_state;
    if (state.count < 2 * 3)
    {
        std::printf("traffic light RED --> ORANGE\n");
        next_state = traffic_light_state::operable_orange { state.count + 1 };
    }
    else
    {
        // finish the scenario
        std::printf("traffic light RED --> OFF\n");
        next_state = traffic_light_state::off {};
    }

    return next_state;
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::next_state& event)
{    
    tools::sleep_for(1000);
    std::printf("traffix light ORANGE --> GREEN\n");
    return traffic_light_state::operable_green { state.count + 1 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_green& state, const traffic_light_event::next_state& event)
{
    tools::sleep_for(1000);
    std::printf("traffix light GREEN --> RED\n");
    return traffic_light_state::operable_red { state.count + 1 };
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_initializing& state, const traffic_light_event::power_off& event)
{
    std::printf("switch OFF traffic light\n");
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_red& state, const traffic_light_event::power_off& event)
{
    std::printf("switch OFF traffic light\n");
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_orange& state, const traffic_light_event::power_off& event)
{
    std::printf("switch OFF traffic light\n");
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const traffic_light_state::operable_green& state, const traffic_light_event::power_off& event)
{
    std::printf("switch OFF traffic light\n");
    return traffic_light_state::off {};
}

traffic_light_state_v traffic_light_fsm::on_event(const auto&, const auto&)
{
    LOG_ERROR("Unsupported state transition");
    return traffic_light_state::off {};
}

// defines callbacks for [state]
void traffic_light_fsm::on_state(const traffic_light_state::off& state)
{
    std::printf("traffic light off\n");
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_initializing& state)
{
    std::printf("traffic light initializing\n");
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_red& state)
{
    std::printf("traffic light RED\n");
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_orange& state)
{
    std::printf("traffic light ORANGE\n");
}

void traffic_light_fsm::on_state(const traffic_light_state::operable_green& state)
{
    std::printf("traffic light GREEN\n");
}

// fallback for undefined state
void traffic_light_fsm::on_state(const auto&)
{
        LOG_ERROR("Unsupported state");
}


void test_variant_fsm()
{
    LOG_INFO("-- finite state machine (std::variant) --");
    print_stats();

    traffic_light_fsm fsm;

    fsm.start();
    fsm.update();

    // callback is invoked on state transition

    fsm.handle_event(traffic_light_event::power_on {});
    fsm.update();
    fsm.handle_event(traffic_light_event::init_done {});
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();

    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();

    std::printf("end fsm test\n");     
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_calendar_day()
{
    // https://www.modernescpp.com/index.php/c20-query-calendar-dates-and-ordinal-dates/

    LOG_INFO("calendar time and day");

    auto now = std::chrono::system_clock::now();   
    auto current_date = std::chrono::year_month_day(std::chrono::floor<std::chrono::days>(now));
    auto current_year = current_date.year();

    std::printf("The current year is %d\n", static_cast<int>(current_year));    
   
    auto moon_landing = std::chrono::year(1969)/std::chrono::month(7)/std::chrono::day(21);      

    auto anniversary_week_day = std::chrono::year_month_weekday(moon_landing);  

    current_date = std::chrono::year_month_day(
        std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));  
    current_year = current_date.year();
    
    auto elapsed_years = static_cast<int>(current_date.year()) - static_cast<int>(moon_landing.year());
    std::printf("Elapsed years since moon landing: %d\n", static_cast<int>(elapsed_years));

}

//--------------------------------------------------------------------------------------------------------------------------------


#if defined(ESP_PLATFORM)

constexpr std::size_t isr_queue_depth = 20;

struct isr_context
{
    std::queue<std::pair<std::uint32_t, std::uint32_t>> storage;
    std::shared_ptr<tools::data_task<isr_context, std::uint32_t, isr_queue_depth>> data_task;
};

using isr_data_task = tools::data_task<isr_context, std::uint32_t, isr_queue_depth>;

static bool timer_isr_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx)
{
    (void)edata;
    static std::uint32_t counter = 0U;

    isr_context* context = reinterpret_cast<isr_context*>(user_ctx);

    std::uint32_t value = counter++;

    context->data_task->isr_submit(value);

    return false; /* YIELD_FROM_ISR already handled in submit data call */
}

static void task_isr_startup(std::shared_ptr<isr_context> context, const std::string& task_name)
{
    (void)context;
    std::printf("starting %s\n", task_name.c_str());
}

static void task_isr_processing(std::shared_ptr<isr_context> context, const std::uint32_t& data, const std::string& task_name)
{
    static auto prev_timepoint = std::chrono::high_resolution_clock::now();
    auto current_timepoint = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_timepoint - prev_timepoint);
    prev_timepoint = current_timepoint;

    context->storage.emplace(std::make_pair(data, static_cast<std::uint32_t>(elapsed.count())));
}

void test_hardware_timer_interrupt()
{
    LOG_INFO("-- hardware timer interrupt & data task --");
    print_stats();

    // https://phatiphatt.wordpress.com/esp32-sampling_mode/
    // https://www.electronicwings.com/esp32/esp32-timer-interrupts
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gptimer.html

    auto my_isr_context = std::make_shared<isr_context>();

    {
        auto my_isr_task = std::make_shared<isr_data_task>(task_isr_startup, task_isr_processing, my_isr_context, "isr_task", 4096);
        my_isr_context->data_task = my_isr_task;

        std::printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

        gptimer_handle_t gptimer = nullptr;
        gptimer_config_t timer_config = {};

        timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
        timer_config.direction = GPTIMER_COUNT_UP;
        timer_config.resolution_hz = 1 * 1000 * 1000; // 1MHz, 1 tick = 1us

        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

        gptimer_alarm_config_t alarm_config = {};

        alarm_config.reload_count = 0;                  // counter will reload with 0 on alarm event
        alarm_config.alarm_count = 1000;                // period = 0.001s @resolution 1MHz
        alarm_config.flags.auto_reload_on_alarm = true; // enable auto-reload

        ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

        gptimer_event_callbacks_t cbs = {};
        cbs.on_alarm = timer_isr_handler; // register user callback

        ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, my_isr_context.get()));
        ESP_ERROR_CHECK(gptimer_enable(gptimer));
        ESP_ERROR_CHECK(gptimer_start(gptimer));

        tools::sleep_for(1000);

        ESP_ERROR_CHECK(gptimer_stop(gptimer));
        ESP_ERROR_CHECK(gptimer_disable(gptimer));

        my_isr_context->data_task.reset();
    }

    while (!my_isr_context->storage.empty())
    {
        auto [counter, elapsed] = my_isr_context->storage.front();
        my_isr_context->storage.pop();

        std::printf("ISR counter %" PRIu32 " - elapsed %" PRIu32 " us\n", counter, elapsed);
    }

    tools::sleep_for(1000);
}

#endif

//--------------------------------------------------------------------------------------------------------------------------------

void runner()
{
    test_ring_buffer();
    test_ring_buffer_iteration();

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

    test_json();
    test_queued_json_data();

    test_packing_unpacking_json_data();

    test_variant_fsm();
    test_calendar_day();

#if defined(ESP_PLATFORM)
    test_hardware_timer_interrupt();
#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
#if defined(FREERTOS_PLATFORM)
// https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/freertos.html

constexpr const std::size_t stack_size = 8192;

// Structure that will hold the TCB of the task being created.
StaticTask_t x_task_buffer = {};

// Buffer that the task being created will use as its stack.  Note this is
// an array of StackType_t variables.  The size of StackType_t is dependent on
// the RTOS port.
StackType_t x_stack[stack_size] = {};

// Function that implements the task being created.
void v_task_code(void* pv_parameters)
{
    // The parameter value is expected to be 1 as 1 is passed in the
    // pvParameters value in the call to xTaskCreateStatic().
    configASSERT((std::uint32_t)pv_parameters == 1UL);

    runner();

    vTaskDelete(nullptr); // delete itself
}

// Function that creates a task.
void launch_runner(void)
{
    TaskHandle_t x_handle = nullptr;

    // Create the task without using any dynamic memory allocation.
    x_handle = xTaskCreateStatic(v_task_code, // Function that implements the task.
        "RUNNER",                             // Text name for the task.
        stack_size,                           // Stack size in bytes, not words.
        (void*)1,                             // Parameter passed into the task.
        tskIDLE_PRIORITY,                     // Priority at which the task is created.
        x_stack,                              // Array to use as the task's stack.
        &x_task_buffer);                      // Variable to hold the task's data structure.

    (void)x_handle;

    vTaskSuspend(nullptr); // suspend main task
}
#endif
//--------------------------------------------------------------------------------------------------------------------------------

#if defined(FREERTOS_PLATFORM)
extern "C" void app_main()
#else
int main()
#endif
{

#if defined(FREERTOS_PLATFORM)
    launch_runner();
#else
    runner();
    return 0;
#endif
}
