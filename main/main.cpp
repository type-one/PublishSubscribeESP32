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
#include <vector>

#include "bytepack/bytepack.hpp"
#define CJSONPP_NO_EXCEPTION
#include "cjsonpp/cjsonpp.hpp"

#include "tools/async_observer.hpp"
#include "tools/data_task.hpp"
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

#if defined(ESP_PLATFORM)
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_system.h>
#include <freertos/queue.h>
#include <hal/gpio_types.h>
#include <sdkconfig.h>
#endif

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer()
{
    std::printf("-- ring buffer --\n");
    auto str_queue = std::make_unique<tools::ring_buffer<std::string, 64U>>();

    str_queue->emplace("toto");

    auto item = str_queue->front();

    std::printf("%s\n", item.c_str());

    str_queue->pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer_iteration()
{
    std::printf("-- ring buffer iteration --\n");

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

        std::string s = obj.print();
        std::printf("%s\n", s.c_str());
    }

    {
        // create object from string literal
        std::string jsonstr = "{ \"happy\": true, \"pi\": 3.141 }";
        cjsonpp::JSONObject obj = cjsonpp::parse(jsonstr);

        std::string s = obj.print();
        std::printf("%s\n", s.c_str());
    }  
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_queued_json_data()
{
    std::printf("-- queued json data --\n");
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

        data_queue->emplace(json.print());
    }

    {
        cjsonpp::JSONObject json = {};
        json.set("msg_type", "time");
        json.set("yyyy_mm_dd", "2025/01/13");
        json.set("hh_mm_ss", "23:05:12");
        json.set("time_zone", "GMT+2");

        data_queue->emplace(json.print());
    }

    while (!data_queue->empty())
    {
        auto data = data_queue->front();
        cjsonpp::JSONObject json = cjsonpp::parse(data);

        std::string discriminant = json.get<std::string>("msg_type");

        if (discriminant == "sensor")
        {
            std::printf("sensor / %s\n", json.print().c_str());
            //std::string name = (*json)["sensor_name"];
            //double temp = (*json)["temp"];
            //bool activity = (*json)["activity"];
            //int answer = (*json)["answer"]["everything"];
            //std::printf("sensor: %s - temp %f - %s - answer (%d)\n",
            //            name.c_str(),
            //            temp,
            //            activity ? "on":"off",
            //            answer);
        }
        else if (discriminant == "time")
        {
            std::printf("time / %s\n", json.print().c_str());
            //std::string time_date = (*json)["yyyy_mm_dd"];
            //std::string time_clock = (*json)["hh_mm_ss"];
            //std::string time_zone = (*json)["time_zone"];
            //std::printf("time: %s - %s - %s\n", time_date.c_str(), time_clock.c_str(), time_zone.c_str());
        }

        data_queue->pop();
    }
    
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


#if defined(FREERTOS_PLATFORM)
extern "C" void app_main()
#else
int main()
#endif
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

#if defined(ESP_PLATFORM)
    test_hardware_timer_interrupt();
#endif

#if !defined(FREERTOS_PLATFORM)
    return 0;
#endif
}
