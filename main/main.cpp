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
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if !defined(FREERTOS_PLATFORM)
#include <exception>
#endif

#include "CException/CException.h"
#include "bytepack/bytepack.hpp"
#define CJSONPP_NO_EXCEPTION
#include "cjsonpp/cjsonpp.hpp"

#include "tools/async_observer.hpp"
#include "tools/data_task.hpp"
#include "tools/generic_task.hpp"
#include "tools/gzip_wrapper.hpp"
#include "tools/histogram.hpp"
#include "tools/lock_free_ring_buffer.hpp"
#include "tools/logger.hpp"
#include "tools/memory_pipe.hpp"
#include "tools/non_copyable.hpp"
#include "tools/periodic_task.hpp"
#include "tools/platform_detection.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/ring_buffer.hpp"
#include "tools/ring_vector.hpp"
#include "tools/sync_dictionary.hpp"
#include "tools/sync_observer.hpp"
#include "tools/sync_queue.hpp"
#include "tools/sync_ring_buffer.hpp"
#include "tools/sync_ring_vector.hpp"
#include "tools/timer_scheduler.hpp"
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

    constexpr const std::size_t queue_size = 64U;
    auto str_queue = std::make_unique<tools::ring_buffer<std::string, queue_size>>();

    str_queue->emplace("toto1");
    str_queue->emplace("toto2");
    str_queue->emplace("toto3");

    auto item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_vector()
{
    LOG_INFO("-- ring vector --");
    print_stats();

    constexpr const std::size_t queue_size = 64U;
    auto str_queue = std::make_unique<tools::ring_vector<std::string>>(queue_size);

    str_queue->emplace("toto1");
    str_queue->emplace("toto2");
    str_queue->emplace("toto3");

    auto item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_vector_resize()
{
    LOG_INFO("-- ring vector resize --");
    print_stats();

    constexpr const std::size_t queue_size = 3U;
    auto str_queue = std::make_unique<tools::ring_vector<std::string>>(queue_size);

    str_queue->emplace("toto1");
    str_queue->emplace("toto2");
    str_queue->emplace("toto3");

    constexpr const std::size_t new_queue_size = 5U;
    str_queue->resize(new_queue_size);

    str_queue->emplace("toto4");
    str_queue->emplace("toto5");

    auto item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    str_queue->emplace("tito1");
    str_queue->emplace("tito2");
    str_queue->emplace("tito3");

    str_queue->emplace("tito4");
    str_queue->emplace("tito5");

    str_queue->resize(3);

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    item = str_queue->front();
    std::printf("%s\n", item.c_str());
    str_queue->pop();

    std::printf("expect is empty: %s\n", str_queue->empty() ? "empty" : "not empty");
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_buffer_iteration()
{
    LOG_INFO("-- ring buffer iteration --");
    print_stats();

    {
        constexpr const std::size_t queue_size = 64U;
        auto str_queue = std::make_unique<tools::ring_buffer<std::string, queue_size>>();

        str_queue->emplace("toto1");
        str_queue->emplace("toto2");
        str_queue->emplace("toto3");
        str_queue->emplace("toto4");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        str_queue->push("toto5");
        str_queue->push("toto6");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
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

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        constexpr const std::size_t nb_of_items_to_keep = 5U;
        const std::size_t remove_count = str_queue->size() - nb_of_items_to_keep;
        for (std::size_t i = 0U; i < remove_count; ++i)
        {
            str_queue->pop();
        }

        str_queue->push("toutou1");
        str_queue->push("toutou2");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }
    }

    {
        constexpr const std::size_t queue_size = 64U;
        auto val_queue = std::make_unique<tools::ring_buffer<double, queue_size>>();

        val_queue->emplace(5.6);  // NOLINT test value
        val_queue->emplace(7.2);  // NOLINT test value
        val_queue->emplace(1.2);  // NOLINT test value
        val_queue->emplace(9.1);  // NOLINT test value
        val_queue->emplace(10.1); // NOLINT test value
        val_queue->emplace(7.5);  // NOLINT test value

        std::printf("content\n");

        for (std::size_t i = 0U; i < val_queue->size(); ++i)
        {
            std::printf("%f\n", (*val_queue)[i]);
        }

        std::size_t cnt = val_queue->size() - 1U; // NOLINT

        for (std::size_t i = 0U; i < cnt; ++i)
        {
            std::printf("compute\n");

            std::vector<double> snapshot;
            snapshot.reserve(val_queue->size());
            for (std::size_t i = 0U; i < val_queue->size(); ++i)
            {
                snapshot.emplace_back((*val_queue)[i]);
            }

            for (const auto& item : snapshot)
            {
                std::printf("%f\n", item);
            }
            const auto avg_val = std::accumulate(snapshot.cbegin(), snapshot.cend(), 0.0, std::plus<>())
                / static_cast<double>(snapshot.size());
            std::printf("avg: %f\n", avg_val);

            const auto sqsum_val = std::transform_reduce(snapshot.begin(), snapshot.end(), 0.0, std::plus<>(),
                [&avg_val](const auto& val)
                {
                    const auto delta = (val - avg_val);
                    return delta * delta;
                });

            const auto variance_val = std::sqrt(sqsum_val / static_cast<double>(snapshot.size()));
            std::printf("variance: %f\n", variance_val);

            const auto [min_val_it, max_val_it] = std::minmax_element(snapshot.cbegin(), snapshot.cend());

            std::printf("min: %f\n", *min_val_it);
            std::printf("max: %f\n", *max_val_it);

            std::printf("pop front\n");
            val_queue->pop();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_ring_vector_iteration()
{
    LOG_INFO("-- ring vector iteration --");
    print_stats();

    {
        constexpr const std::size_t array_size = 64U;
        auto str_queue = std::make_unique<tools::ring_vector<std::string>>(array_size);

        str_queue->emplace("toto1");
        str_queue->emplace("toto2");
        str_queue->emplace("toto3");
        str_queue->emplace("toto4");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        std::printf("pop front\n");
        str_queue->pop();

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");
        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }

        str_queue->push("toto5");
        str_queue->push("toto6");

        std::printf("front %s\n", str_queue->front().c_str());
        std::printf("back %s\n", str_queue->back().c_str());

        std::printf("content\n");

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
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

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
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

        for (std::size_t i = 0U; i < str_queue->size(); ++i)
        {
            std::printf("%s\n", (*str_queue)[i].c_str());
        }
    }

    {
        constexpr const std::size_t array_size = 64U;
        auto val_queue = std::make_unique<tools::ring_vector<double>>(array_size);

        val_queue->emplace(5.6);  // NOLINT test value
        val_queue->emplace(7.2);  // NOLINT test value
        val_queue->emplace(1.2);  // NOLINT test value
        val_queue->emplace(9.1);  // NOLINT test value
        val_queue->emplace(10.1); // NOLINT test value
        val_queue->emplace(7.5);  // NOLINT test value

        std::printf("content\n");

        for (std::size_t i = 0U; i < val_queue->size(); ++i)
        {
            std::printf("%f\n", (*val_queue)[i]);
        }

        std::size_t cnt = val_queue->size() - 1U;

        for (std::size_t i = 0U; i < cnt; ++i)
        {
            std::printf("compute\n");

            std::vector<double> snapshot;
            snapshot.reserve(val_queue->size());
            for (std::size_t i = 0U; i < val_queue->size(); ++i)
            {
                snapshot.emplace_back((*val_queue)[i]);
            }

            for (const auto& item : snapshot)
            {
                std::printf("%f\n", item);
            }

            const auto avg_val = std::accumulate(snapshot.cbegin(), snapshot.cend(), 0.0, std::plus<>())
                / static_cast<double>(snapshot.size());
            std::printf("avg: %f\n", avg_val);

            const auto sqsum_val = std::transform_reduce(snapshot.cbegin(), snapshot.cend(), 0.0, std::plus<>(),
                [&avg_val](const auto& val)
                {
                    const auto delta = (val - avg_val);
                    return delta * delta;
                });

            const auto variance_val = std::sqrt(sqsum_val / static_cast<double>(snapshot.size()));
            std::printf("variance: %f\n", variance_val);

            const auto [min_val_it, max_val_it] = std::minmax_element(snapshot.cbegin(), snapshot.cend());

            std::printf("min: %f\n", *min_val_it);
            std::printf("max: %f\n", *max_val_it);

            std::printf("pop front\n");
            val_queue->pop();
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_lock_free_ring_buffer()
{
    LOG_INFO("-- lock free ring buffer --");
    print_stats();

    constexpr const std::size_t queue_size_pow2 = 3U;
    auto str_queue = std::make_unique<tools::lock_free_ring_buffer<std::uint8_t, queue_size_pow2>>();
    bool result = false;

    result = str_queue->push(1U);           // NOLINT test value
    result = result && str_queue->push(2U); // NOLINT test value
    result = result && str_queue->push(3U); // NOLINT test value

    std::uint8_t val = 0;

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->pop(val);
    std::printf("expect failure - %s\n", result ? "success" : "failure");

    result = str_queue->push(1U);           // NOLINT test value
    result = result && str_queue->push(2U); // NOLINT test value
    result = result && str_queue->push(3U); // NOLINT test value
    result = result && str_queue->push(4U); // NOLINT test value

    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(5U); // NOLINT test value
    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(6U); // NOLINT test value
    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(7U); // NOLINT test value
    std::printf("expect success - %s\n", result ? "success" : "failure");

    result = str_queue->push(8U); // NOLINT test value
    std::printf("expect failure - %s\n", result ? "success" : "failure");

    result = str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    result = result && str_queue->pop(val);
    std::printf("%u\n", val);

    std::printf("expect success - %s\n", result ? "success" : "failure");
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_ring_buffer()
{
    LOG_INFO("-- sync ring buffer --");
    print_stats();

    constexpr const std::size_t queue_size = 64U;
    tools::sync_ring_buffer<std::string, queue_size> str_queue;

    str_queue.emplace("toto");

    auto item = str_queue.front();

    std::printf("%s\n", item.c_str());

    str_queue.pop();
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_sync_ring_vector()
{
    LOG_INFO("-- sync ring vector --");
    print_stats();

    constexpr const std::size_t queue_size = 64U;
    tools::sync_ring_vector<std::string> str_queue(queue_size);

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

struct my_event
{
    enum class type
    {
        notification,
        failure
    } type;

    std::string description = {};
};

using base_observer = tools::sync_observer<my_topic, my_event>;
class my_observer : public base_observer // NOLINT inherits from non copyable and non movable class
{
public:
    my_observer() = default;
    ~my_observer() override = default;

    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        std::printf("sync [topic %d] received: event (%s) from %s\n",
            static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());
    }

private:
};

using base_async_observer = tools::async_observer<my_topic, my_event>;
class my_async_observer : public base_async_observer // NOLINT inherits from non copyable and non movable
{
public:
    my_async_observer()
        : m_task_loop([this]() { handle_events(); })
    {
    }

    ~my_async_observer() override
    {
        m_stop_task.store(true);
        m_task_loop.join();
    }

    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        std::printf("async/push [topic %d] received: event (%s) from %s\n",
            static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());

        base_async_observer::inform(topic, event, origin);
    }

private:
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
class my_subject : public base_subject // NOLINT inherits from non_copyable and non_movable
{
public:
    my_subject() = delete;
    my_subject(const std::string& name)
        : base_subject(name)
    {
    }

    ~my_subject() override = default;

    void publish(const my_topic& topic, const my_event& event) override
    {
        std::printf("publish: event (%s) to %s\n", event.description.c_str(), name().c_str());
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
        [](const my_topic& topic, const my_event& event, const std::string& origin)
        {
            std::printf("handler [topic %d] received: event (%s) from %s\n",
                static_cast<std::underlying_type<my_topic>::type>(topic), event.description.c_str(), origin.c_str());
        });

    subject1->publish(my_topic::generic, my_event { my_event::type::notification, "toto" });

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

//--------------------------------------------------------------------------------------------------------------------------------

struct my_generic_task_context
{
    std::atomic<bool> stop_tasks;
};

using my_generic_task = tools::generic_task<my_generic_task_context>;

static void generic_function(const std::shared_ptr<my_generic_task_context>& context, const std::string& task_name)
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

    // sleep 2 sec
    constexpr const int wait_task_startup_ms = 2000;
    tools::sleep_for(wait_task_startup_ms);

    std::printf("nb of periodic loops = %d\n", context->loop_counter.load());

    auto previous_timepoint = start_timepoint;
    while (!context->time_points.empty())
    {
        const auto measured_timepoint = context->time_points.front();
        context->time_points.pop();
        const auto elapsed
            = std::chrono::duration_cast<std::chrono::microseconds>(measured_timepoint - previous_timepoint);
        std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
        previous_timepoint = measured_timepoint;
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

class my_collector : public base_observer // NOLINT inherits from non copyable and non movable class
{
public:
    my_collector() = default;
    ~my_collector() override = default;

    void inform(const my_topic& topic, const my_event& event, const std::string& origin) override
    {
        (void)topic;
        (void)origin;

        m_histogram.add(static_cast<double>(std::strtod(event.description.c_str(), nullptr)));
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
        std::printf(
            "gaussian probability of %f occuring is %f\n", top, m_histogram.gaussian_probability(top, avg, variance));
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

    auto sampler
        = [&data_source](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter += 1;

        // mocked signal
        double signal = std::sin(context->loop_counter.load());

        // emit "signal" as a 'string' event
        data_source->publish(my_topic::external, my_event { my_event::type::notification, std::to_string(signal) });
    };

    auto startup = [](const std::shared_ptr<my_periodic_task_context>& context, const std::string& task_name)
    {
        (void)task_name;
        context->loop_counter = 0;
    };

    data_source->subscribe(my_topic::external, monitoring);
    data_source->subscribe(my_topic::external, histogram_feeder);

    // "sample" with a 100 ms period
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

    constexpr const std::size_t commands_queue_depth = 128U;
    tools::sync_ring_buffer<std::function<void()>, commands_queue_depth> commands_queue;

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

    auto startup1 = [](const std::shared_ptr<my_worker_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("welcome 1\n");
        std::printf("task %s started\n", task_name.c_str());
    };

    auto startup2 = [](const std::shared_ptr<my_worker_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("welcome 2\n");
        std::printf("task %s started\n", task_name.c_str());
    };

    auto context = std::make_shared<my_worker_task_context>();

    constexpr const std::size_t stack_size = 4096U;
    auto task1 = std::make_unique<my_worker_task>(std::move(startup1), context, "worker_1", stack_size);
    auto task2 = std::make_unique<my_worker_task>(std::move(startup2), context, "worker_2", stack_size);

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 1);
    std::array<std::unique_ptr<my_worker_task>, 2> tasks = { std::move(task1), std::move(task2) };

    constexpr const int wait_tasks_ms = 100;
    tools::sleep_for(wait_tasks_ms);

    const auto start_timepoint = std::chrono::high_resolution_clock::now();
    constexpr const int nb_loops = 20;

    for (int i = 0; i < nb_loops; ++i)
    {
        auto idx = distribution(generator);

        tasks.at(idx)->delegate(
            [](auto context, const auto& task_name)
            {
                std::printf("job %d on worker task %s\n", context->loop_counter.load(), task_name.c_str());
                context->loop_counter++;
                context->time_points.emplace(std::chrono::high_resolution_clock::now());
            });

        std::this_thread::yield();
    }

    constexpr const int wait_loops_ms = 2000;
    tools::sleep_for(wait_loops_ms);

    std::printf("nb of loops = %d\n", context->loop_counter.load());

    auto previous_timepoint = start_timepoint;
    while (!context->time_points.empty())
    {
        const auto measured_timepoint = context->time_points.front();
        context->time_points.pop();
        const auto elapsed
            = std::chrono::duration_cast<std::chrono::microseconds>(measured_timepoint - previous_timepoint);
        std::printf("timepoint: %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
        previous_timepoint = measured_timepoint;
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

enum class message_type : std::uint8_t
{
    temperature = 1,
    manufacturing = 2,
    freetext = 3,
    aggregat = 4
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
    std::string vendor_name;
    std::string serial_number;
    std::string manufacturing_date;
    std::array<char, 2> tag;

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(vendor_name, serial_number, manufacturing_date, tag);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        stream.read(vendor_name, serial_number, manufacturing_date, tag);
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

    constexpr const std::size_t queue_depth = 128U;
    tools::sync_ring_vector<std::vector<std::uint8_t>> data_queue(queue_depth);
    // tools::sync_ring_buffer<std::vector<std::uint8_t>, queue_depth> data_queue;
    // tools::sync_queue<std::vector<std::uint8_t>> data_queue;

    free_text message1 = { "jojo rabbit" };
    manufacturing_info message2 = { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } };
    temperature_sensor message3 = { 45.2, 51.72, 21.5 }; // NOLINT tests values

    constexpr const std::size_t buffer_size = 1024U;
    std::vector<std::uint8_t> buffer(buffer_size);
    bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer.data(), buffer.size()) };

    binary_stream.reset();
    binary_stream.write(message_type::freetext);
    message1.serialize(binary_stream);
    auto buffer1 = binary_stream.data();
    std::vector<std::uint8_t> encoded1 = {};
    encoded1.resize(buffer1.size());
    std::memcpy(encoded1.data(), buffer1.as<void>(), buffer1.size());
    data_queue.emplace(std::move(encoded1));

    binary_stream.reset();
    binary_stream.write(message_type::manufacturing);
    message2.serialize(binary_stream);
    auto buffer2 = binary_stream.data();
    std::vector<std::uint8_t> encoded2 = {};
    encoded2.resize(buffer2.size());
    std::memcpy(encoded2.data(), buffer2.as<void>(), buffer2.size());
    data_queue.emplace(std::move(encoded2));

    binary_stream.reset();
    binary_stream.write(message_type::temperature);
    message3.serialize(binary_stream);
    auto buffer3 = binary_stream.data();
    std::vector<std::uint8_t> encoded3 = {};
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
                free_text text = {};
                text.deserialize(stream);
                std::printf("%s\n", text.text.c_str());
            }
            break;

            case message_type::manufacturing:
            {
                manufacturing_info info = {};
                info.deserialize(stream);
                std::printf("%s\n%s\n%s\n%c %c\n", info.vendor_name.c_str(), info.serial_number.c_str(),
                    info.manufacturing_date.c_str(), info.tag[0], info.tag[1]);
            }
            break;

            case message_type::temperature:
            {
                temperature_sensor temp = {};
                temp.deserialize(stream);
                std::printf("%f\n%f\n%f\n", temp.cpu_temperature, temp.gpu_temperature, temp.room_temperature);
            }
            break;

            case message_type::aggregat:
                break;
        };

        data_queue.pop();
    }
}

//--------------------------------------------------------------------------------------------------------------------------------
struct aggregated_info
{
    std::map<std::string, temperature_sensor> m_dictionary;
    std::vector<manufacturing_info> m_list;
    bool m_status = false;
    std::vector<double> m_values;

    void serialize(bytepack::binary_stream<>& stream) const
    {
        stream.write(static_cast<std::uint32_t>(m_dictionary.size()));
        for (const auto& elt : m_dictionary)
        {
            stream.write(elt.first);
            elt.second.serialize(stream);
        }

        stream.write(static_cast<std::uint32_t>(m_list.size()));
        for (const auto& elt : m_list)
        {
            elt.serialize(stream);
        }

        stream.write(m_status, m_values);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        std::uint32_t len = 0U;
        std::string key;
        temperature_sensor sensor = {};
        manufacturing_info manuf = {};

        m_dictionary.clear();
        m_list.clear();

        stream.read(len);

        for (std::uint32_t i = 0U; i < len; ++i)
        {
            stream.read(key);
            sensor.deserialize(stream);
            m_dictionary.emplace(key, sensor);
        }

        len = 0U;
        stream.read(len);
        for (std::uint32_t i = 0U; i < len; ++i)
        {
            manuf.deserialize(stream);
            m_list.emplace_back(manuf);
        }

        stream.read(m_status, m_values);

        return true; // pretend success
    }
};

void test_aggregated_bytepack_data()
{
    LOG_INFO("-- test aggregated bytepack data --");
    print_stats();

    aggregated_info aggr = {};
    aggr.m_dictionary
        = { { "sensor1", { 45.2, 51.72, 21.5 } }, { "sensor2", { 17.2, 34.7, 18.3 } } }; // NOLINT test values
    aggr.m_list = { { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } },
        { { "AMD" }, { "HTX-7788-22-5" }, { "12/05/07" }, { 'B', 'Z' } } };
    aggr.m_status = false;
    aggr.m_values = { 0.7, 1.5, 2.1, -0.5 }; // NOLINT test values

    auto sens = aggr.m_dictionary.find("sensor2");
    std::printf(
        "%f %f %f \n", sens->second.cpu_temperature, sens->second.gpu_temperature, sens->second.room_temperature);
    std::printf("%s %s %s\n", aggr.m_list[1].manufacturing_date.c_str(), aggr.m_list[1].serial_number.c_str(),
        aggr.m_list[1].vendor_name.c_str());
    std::printf("%f %f %f\n", aggr.m_values[0], aggr.m_values[1], aggr.m_values[2]);

    constexpr const std::size_t buffer_size = 1024U;
    std::vector<std::uint8_t> buffer(buffer_size);
    bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer.data(), buffer.size()) };

    binary_stream.reset();
    binary_stream.write(message_type::aggregat);
    aggr.serialize(binary_stream);

    // deserialize

    aggregated_info aggr_dup;
    auto bin_buf = binary_stream.data();
    std::vector<std::uint8_t> data_packed(bin_buf.size());
    std::memcpy(data_packed.data(), bin_buf.as<void>(), bin_buf.size());

    bytepack::binary_stream stream(bytepack::buffer_view(data_packed.data(), data_packed.size()));
    message_type typ = {};
    stream.read(typ);
    if (message_type::aggregat == typ)
    {
        aggr_dup.deserialize(stream);

        auto sensor = aggr_dup.m_dictionary.find("sensor2");
        std::printf("%f %f %f \n", sensor->second.cpu_temperature, sensor->second.gpu_temperature,
            sensor->second.room_temperature);
        std::printf("%s %s %s %c %c \n", aggr_dup.m_list[1].manufacturing_date.c_str(),
            aggr_dup.m_list[1].serial_number.c_str(), aggr_dup.m_list[1].vendor_name.c_str(), aggr_dup.m_list[1].tag[0],
            aggr_dup.m_list[1].tag[1]);
        std::printf("%f %f %f\n", aggr_dup.m_values[0], aggr_dup.m_values[1], aggr_dup.m_values[2]);
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

struct data_task_context
{
};

constexpr const std::size_t binary_msg_size = 128U;
using binary_msg = std::array<std::uint8_t, binary_msg_size>;

using my_data_task = tools::data_task<data_task_context, binary_msg>;
using my_data_periodic_task = tools::periodic_task<data_task_context>;

namespace
{
    constexpr const auto task_startup
        = [](const std::shared_ptr<data_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("starting %s\n", task_name.c_str());
    };

    void task_1_processing(
        const std::shared_ptr<data_task_context>& context, binary_msg data_packed, const std::string& task_name)
    {
        (void)context;
        (void)task_name;

        bytepack::binary_stream stream(bytepack::buffer_view(data_packed.data(), data_packed.size()));

        message_type discriminant = {};
        stream.read(discriminant);

        switch (discriminant)
        {
            case message_type::freetext:
            {
                free_text text = {};
                text.deserialize(stream);
                std::printf("%s\n", text.text.c_str());
            }
            break;

            case message_type::manufacturing:
            {
                manufacturing_info info = {};
                info.deserialize(stream);
                std::printf("%s\n%s\n%s\n%c %c\n", info.vendor_name.c_str(), info.serial_number.c_str(),
                    info.manufacturing_date.c_str(), info.tag[0], info.tag[1]);
            }
            break;

            case message_type::temperature:
            {
                temperature_sensor temp = {};
                temp.deserialize(stream);
                std::printf("%f\n%f\n%f\n", temp.cpu_temperature, temp.gpu_temperature, temp.room_temperature);
            }
            break;

            case message_type::aggregat:
                break;
        }
    };
}

void test_bytepack_data_task()
{
    LOG_INFO("-- test bytepack data task --");
    print_stats();

    auto context = std::make_shared<data_task_context>();

    constexpr const std::size_t queue_depth = 128U;
    constexpr const std::size_t stack_size = 4096U;

    auto task_1 = std::make_unique<my_data_task>(
        std::move(task_startup), std::move(task_1_processing), context, queue_depth, "task 1", stack_size);

    binary_msg buffer;

    auto task_2_periodic
        = [&task_1, &buffer](const std::shared_ptr<data_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("periodic %s\n", task_name.c_str());

        temperature_sensor message1 = { 45.2, 51.72, 21.5 }; // NOLINT test values
        manufacturing_info message2 = { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } };
        free_text message3 = { "jojo rabbit" };

        bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer) };

        binary_stream.reset();
        binary_stream.write(message_type::temperature);
        message1.serialize(binary_stream);
        task_1->submit(buffer);

        binary_stream.reset();
        binary_stream.write(message_type::manufacturing);
        message2.serialize(binary_stream);
        task_1->submit(buffer);

        binary_stream.reset();
        binary_stream.write(message_type::freetext);
        message3.serialize(binary_stream);
        task_1->submit(buffer);
    };

    constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(500);
    auto task_2 = std::make_unique<my_data_periodic_task>(
        std::move(task_startup), std::move(task_2_periodic), context, "task 2", period, stack_size);

    constexpr const std::size_t wait_task_ms = 2500;
    tools::sleep_for(wait_task_ms);
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

        constexpr const double number_pi = 3.141592;
        obj.set("pi", number_pi);                  // add a number that is stored as double
        obj.set("happy", true);                    // add a Boolean that is stored as bool
        obj.set("name", "Niels");                  // add a string that is stored as std::string
        obj.set("nothing", cjsonpp::nullObject()); // add another null object by passing nullptr

        // add an array that is stored as std::vector (using an initializer list)
        std::vector<int> val = { 0, 1, 2 };
        obj.set("list", val);

        // add an object inside the object
        obj2.set("everything", 42); // NOLINT test value
        obj.set("answer", obj2);

        // add another object (using an initializer list of pairs)
        obj3.set("currency", "USD");
        obj4.set("value", 42.99); // NOLINT test value

        cjsonpp::JSONObject arr = cjsonpp::arrayObject();
        arr.add(obj3);
        arr.add(obj4);

        obj.set("object", arr);

        const auto str = obj.print();
        std::printf("%s\n", str.c_str());
    }

    {
        // create object from string literal
        std::string jsonstr = R"({ "happy": true, "pi": 3.141 })";
        cjsonpp::JSONObject obj = cjsonpp::parse(jsonstr);

        const auto str = obj.print();
        std::printf("%s\n", str.c_str());
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

void test_queued_json_data()
{
    LOG_INFO("-- queued json data --");
    print_stats();

    constexpr const std::size_t queue_depth = 128U;
    auto data_queue = std::make_unique<tools::sync_ring_buffer<std::string, queue_depth>>();

    {
        cjsonpp::JSONObject json = {};
        json.set("msg_type", "sensor");
        json.set("sensor_name", "indoor_temperature");
        json.set("temp", 19.47); // NOLINT test value
        json.set("activity", true);

        cjsonpp::JSONObject json_answer = {};
        json_answer.set("everything", 42); // NOLINT test value
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
            std::printf(
                "sensor: %s - temp %f - %s - answer (%d)\n", name.c_str(), temp, activity ? "on" : "off", answer);
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
    static const std::string json_str1 = R"(
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
    static const std::string json_str2 = R"(
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

    unpacked_buffer.resize(json_str1.length() + 1U);
    std::memcpy(unpacked_buffer.data(), json_str1.c_str(), json_str1.length());
    unpacked_buffer[json_str1.length()] = '\0';

    std::printf("json file 1 of %zu bytes\n", unpacked_buffer.size());
    std::printf("packing json file 1\n");

    packed_buffer = gzip.pack(unpacked_buffer);

    std::printf("compressed to %zu bytes\n", packed_buffer.size());
    std::printf("unpacking gzip file 1\n");

    unpacked_buffer = gzip.unpack(packed_buffer);

    std::printf("unpacked to %zu bytes\n", unpacked_buffer.size());

    // json 2

    unpacked_buffer.resize(json_str2.length() + 1U);
    std::memcpy(unpacked_buffer.data(), json_str2.c_str(), json_str2.length());
    unpacked_buffer[json_str2.length()] = '\0';

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

class traffic_light_fsm : tools::non_copyable // NOLINT inherits from non copyable and non movable class
{
public:
    traffic_light_fsm() = default;
    ~traffic_light_fsm() = default;

    void start();

    void handle_event(const traffic_light_event_v& event);
    void update();

private:
    traffic_light_state_v m_state = traffic_light_state::off {}; // init state
    bool m_entering_state = false;

    // states transitions methods
    // define callbacks for [state, event]

    traffic_light_state_v on_event(const traffic_light_state::off& state, const traffic_light_event::power_on& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_initializing& state, const traffic_light_event::init_done& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_red& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_orange& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_green& state, const traffic_light_event::next_state& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_initializing& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_red& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_orange& state, const traffic_light_event::power_off& event);
    traffic_light_state_v on_event(
        const traffic_light_state::operable_green& state, const traffic_light_event::power_off& event);

    // fallback for undefined transitions
    traffic_light_state_v on_event(const auto& state, const auto& event);

    // defines callbacks for [state]
    void on_state(const traffic_light_state::off& state);
    void on_state(const traffic_light_state::operable_initializing& state);
    void on_state(const traffic_light_state::operable_red& state);
    void on_state(const traffic_light_state::operable_orange& state);
    void on_state(const traffic_light_state::operable_green& state);

    // fallback for undefined state
    void on_state(const auto& state);
};

void traffic_light_fsm::start()
{
    m_state = traffic_light_state::off {}; // init state
    m_entering_state = true;
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
                traffic_light_fsm::on_state(state);
            }
        },
        m_state);
    // clang-format on  
}

constexpr const int traffic_light_wait_ms = 1000;

// define callbacks for [state, event]
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
        // finish the scenario
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

traffic_light_state_v traffic_light_fsm::on_event(const auto& state, const auto& event)
{
    (void)event;
    LOG_ERROR("Unsupported state transition");
    // don't switch to other state and do not execute entering state callback
    m_entering_state = false;
    return state;
}

// defines callbacks for [state]
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

// fallback for undefined state
void traffic_light_fsm::on_state(const auto& state)
{    
    (void)state;
    if (m_entering_state)
    {
        LOG_ERROR("Unsupported state");
        m_entering_state = false;
    }
}


void test_variant_fsm()
{
    LOG_INFO("-- finite state machine (std::variant) --");
    print_stats();

    traffic_light_fsm fsm;

    fsm.start();
    fsm.update();
    fsm.update();
    fsm.update();

    // callback is invoked on state transition

    fsm.handle_event(traffic_light_event::power_on {});
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

    //fsm.handle_event(traffic_light_event::power_on {});
    fsm.handle_event(traffic_light_event::next_state {});
    fsm.update();
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
   
    auto moon_landing = std::chrono::year(1969)/std::chrono::month(7)/std::chrono::day(21); // NOLINT test value    

    //auto anniversary_week_day = std::chrono::year_month_weekday(moon_landing);  

    current_date = std::chrono::year_month_day(
        std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));  
    current_year = current_date.year();
    
    auto elapsed_years = static_cast<int>(current_date.year()) - static_cast<int>(moon_landing.year());
    std::printf("Elapsed years since moon landing: %d\n", static_cast<int>(elapsed_years));

}

//--------------------------------------------------------------------------------------------------------------------------------

void test_timer()
{
    LOG_INFO("timer");
    
    {
        tools::timer_scheduler timer_scheduler;

        constexpr const int period_200ms = 200;
        constexpr const int period_120ms = 120;
        constexpr const int period_100ms = 100;
        constexpr const int period_75ms = 75;
        constexpr const int period_50ms = 50;
        constexpr const int period_40ms = 40;
        constexpr const int period_25ms = 25;
        constexpr const int period_20ms = 20;

        // Test one shot timer - after completion
        constexpr const int test_value = 42;
        std::atomic<int> val(0);
        timer_scheduler.add("timer1", period_100ms, [&](tools::timer_handle) { val.store(test_value); }, tools::timer_type::one_shot);
        tools::sleep_for(period_120ms);
        std::printf("Expect %d is 42\n", val.load());

        // Test one shot timer - not started yet
        val.store(0);
        timer_scheduler.add("timer2", period_100ms, [&](tools::timer_handle) { val.store(test_value); }, tools::timer_type::one_shot);
        tools::sleep_for(period_50ms);
        std::printf("Expect %d is 0\n", val.load());

        // Test periodic timer (auto-reload) - check immediately started
        std::atomic<std::size_t> count(0U);
        auto timer_id = timer_scheduler.add("timer3", period_40ms, [&](tools::timer_handle) { ++count; }, tools::timer_type::periodic);
        tools::sleep_for(period_20ms);    
        timer_scheduler.remove(timer_id);
        std::printf("Expect count %zu is 1\n", count.load());

        // Test periodic timer (auto-reload) - check after 3 cycles
        count.store(0U);
        timer_id = timer_scheduler.add("timer4", period_40ms, [&](tools::timer_handle) { ++count; }, tools::timer_type::periodic);
        tools::sleep_for(period_100ms);    
        timer_scheduler.remove(timer_id);
        std::printf("Expect count %zu is 3\n", count.load());

        // Test delete periodic timer (auto-reload) in callback
        count.store(0U);
        timer_scheduler.add("timer5", period_25ms,
                [&](tools::timer_handle timer_id) {
                    ++count;
                    timer_scheduler.remove(timer_id);
                },
                tools::timer_type::periodic);
        tools::sleep_for(period_75ms);
        std::printf("Expect count %zu is 1\n", count.load());

        // Test periodic timer delays        
        auto start_point = std::chrono::high_resolution_clock::now();
        std::queue<std::chrono::high_resolution_clock::time_point> time_points;
        timer_id = timer_scheduler.add("timer6", period_40ms, [&](tools::timer_handle)
        {
            time_points.emplace(std::chrono::high_resolution_clock::now());
        }, tools::timer_type::periodic);
        tools::sleep_for(period_200ms);    
        timer_scheduler.remove(timer_id);

        auto prev_point = start_point;
        while(!time_points.empty())
        {
            const auto cur_point = time_points.front();
            time_points.pop();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(cur_point - prev_point);
            std::printf("timepoint (periodic): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
            prev_point = cur_point;
        }

        // Test one shot timer delay        
        start_point = std::chrono::high_resolution_clock::now();
        std::chrono::high_resolution_clock::time_point time_point;
        timer_scheduler.add("timer7", period_120ms, [&](tools::timer_handle)
        {
            time_point = std::chrono::high_resolution_clock::now();
        }, tools::timer_type::one_shot);
        tools::sleep_for(period_200ms);  

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time_point - start_point);
        std::printf("timepoint (one shot): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));

        // Test one shot timer delay (variant with std::chrono input)       
        start_point = std::chrono::high_resolution_clock::now();
        constexpr const int timer_timeout_us = 120250;
        timer_scheduler.add("timer7", std::chrono::duration<std::uint64_t, std::micro>(timer_timeout_us), [&](tools::timer_handle)
        {
            time_point = std::chrono::high_resolution_clock::now();
        }, tools::timer_type::one_shot);
        tools::sleep_for(period_200ms);  

        elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time_point - start_point);
        std::printf("timepoint (one shot): %" PRId64 " us\n", static_cast<std::int64_t>(elapsed.count()));
    }
}

//--------------------------------------------------------------------------------------------------------------------------------

struct smp_task_context
{    
};

using periodic_task0 = tools::periodic_task<smp_task_context>;
using worker_task1 = tools::worker_task<smp_task_context>;

void test_smp_tasks_cpu_affinity()
{
    LOG_INFO("-- smp tasks with cpu affinity --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_task_context>();

    constexpr const std::size_t task1_stack_size = 2048U;
    constexpr const int core1 = 1;
    worker_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        static int count = 0;
        std::printf("%s (core 0): count %d\n", task_name.c_str(), count);
        ++count;

        // delegate work on core 1
        task1.delegate([](auto context, const auto& task_name)
        {
            (void)context;
            std::printf("%s (core 1): work\n", task_name.c_str());
        });
    };

    constexpr const auto period_100ms = std::chrono::duration<std::uint64_t, std::milli>(100);
    constexpr const std::size_t task0_stack_size = 4096U;
    constexpr const int core0 = 0; 

    periodic_task0 task0(startup, periodic_lambda, context, "periodic_task0", period_100ms, task0_stack_size, core0, tools::base_task::default_priority); 

    // sleep 2 sec
    constexpr const int wait_task_ms = 2000;
    tools::sleep_for(wait_task_ms);
}

//--------------------------------------------------------------------------------------------------------------------------------

constexpr const std::size_t worker_pipe_depth = 5U;
struct smp_ring_task_context
{    
    tools::lock_free_ring_buffer<std::uint8_t, worker_pipe_depth> m_to_worker_pipe;
    tools::lock_free_ring_buffer<std::uint8_t, worker_pipe_depth> m_from_worker_pipe;    
};

using periodic_ring_task0 = tools::periodic_task<smp_ring_task_context>;
using worker_ring_task1 = tools::worker_task<smp_ring_task_context>;

void test_smp_tasks_lock_free_ring_buffer()
{
    LOG_INFO("-- smp tasks with lock free ring buffer --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_ring_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_ring_task_context>();

    constexpr const std::size_t task1_stack_size = 2048U;
    constexpr const int core1 = 1;
    worker_ring_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_ring_task_context>& context, const std::string& task_name)
    {        
        static int count = 0;
        std::printf("%s (core 0): count %d\n", task_name.c_str(), count);
        ++count;

        constexpr const int mask = 0xff;
        context->m_to_worker_pipe.push(static_cast<std::uint8_t>(count & mask));

        std::uint8_t val = 0U; 
        if (context->m_from_worker_pipe.pop(val))
        {
            std::printf("(core 0): received computed (from core 1) %" PRIu8 "\n", val);
        }

        // delegate work on core 1
        task1.delegate([](const auto& context, const auto& task_name)
        {
            (void)task_name;
            std::uint8_t value = 0U; 
            if (context->m_to_worker_pipe.pop(value))
            {
                context->m_from_worker_pipe.push(static_cast<std::uint8_t>(value * value));
            }
        });
    };

    constexpr const auto period_100ms = std::chrono::duration<std::uint64_t, std::milli>(100); 
    constexpr const std::size_t task0_stack_size = 4096U;
    constexpr const int core0 = 0;
    periodic_ring_task0 task0(startup, periodic_lambda, context, "periodic_task0", period_100ms, task0_stack_size, core0, tools::base_task::default_priority); 

    constexpr const int wait_tasks_ms = 2000;
    tools::sleep_for(wait_tasks_ms);
}

//--------------------------------------------------------------------------------------------------------------------------------

// The variable used to hold the message buffer structure.
static tools::memory_pipe::static_buffer_holder static_buf_holder = {}; // NOLINT this is the purpose to have a statically allocated holder
// Used to dimension the array used to hold the messages.  The available space
// will actually be one less than this, so 999.
constexpr const std::size_t static_storage_size = 1000U;
static std::array<std::uint8_t, static_storage_size> static_storage; // NOLINT this is the purpose to have a statically allocated buffer

struct smp_mem_task_context
{
    tools::memory_pipe m_to_worker_pipe;    
    
    //smp_mem_task_context(std::size_t to_size) : m_to_worker_pipe(to_size)
    smp_mem_task_context(std::size_t to_size) : m_to_worker_pipe(to_size, static_storage.data(), &static_buf_holder)
    {
    }
};

using periodic_mem_task0 = tools::periodic_task<smp_mem_task_context>;
using worker_mem_task1 = tools::worker_task<smp_mem_task_context>;

void test_smp_tasks_memory_pipe()
{
    LOG_INFO("-- smp tasks with memory pipe --");
    print_stats();    

    auto startup = [](const std::shared_ptr<smp_mem_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    std::memset(&static_buf_holder, 0, sizeof(static_buf_holder));
    auto context = std::make_shared<smp_mem_task_context>(static_storage.size());

    {
        constexpr const std::size_t task1_stack_size = 2048U;
        constexpr const int core1 = 1;
        worker_mem_task1 task1(startup, context, "worker_task1", task1_stack_size, core1, tools::base_task::default_priority);

        static const std::string label = "this\nis\na\ntest\nto\ntransmit\nseveral\nmessages\nbetween\ntwo\ncores\n";

        std::atomic_bool stop(false);

        // delegate work on core 1
        task1.delegate([&stop](const auto& context, const auto& task_name)
        {
            std::printf("%s (core 1)\n", task_name.c_str());

            const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(20);        

            while(!stop.load())
            {
                std::vector<std::uint8_t> received;
                constexpr const std::size_t bytes_to_receive = 128U;
                const auto received_bytes = context->m_to_worker_pipe.receive(received, bytes_to_receive, timeout);
                if (received_bytes > 0U)
                {
                    for(const auto val : received)
                    {
                        std::printf("%c", val);   
                    }
                }
                
            }

            std::printf("\n");
        });

        std::size_t offset = 0U;
        auto periodic_lambda = [&offset](const std::shared_ptr<smp_mem_task_context>& context, const std::string& task_name)
        { 
            std::printf(" / %s (core 0)\n", task_name.c_str());

            constexpr const std::size_t chunk_size = 16U; 
            std::size_t to_send = std::min(chunk_size, label.size() - offset);
            
            if (offset < label.size())
            {            
                const auto timeout = std::chrono::duration<std::uint64_t, std::milli>(10);
                const auto sent = context->m_to_worker_pipe.send(
                    reinterpret_cast<const std::uint8_t*>(label.data() + offset), // NOLINT const char* to const std::uint8_t* 
                    to_send, timeout);
                offset += sent; // try again on characters left 
            }
        };

        // 50 ms period
        constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(50); 
        constexpr const std::size_t task0_stack_size = 4096U;
        constexpr const int core0 = 0;
        periodic_mem_task0 task0(startup, periodic_lambda, context, "periodic_task0", period, task0_stack_size, core0, tools::base_task::default_priority); 

        // sleep 2 sec
        constexpr const int wait_processing_ms = 2000;
        constexpr const int wait_join_ms = 250;
        tools::sleep_for(wait_processing_ms);
        stop.store(true);
        tools::sleep_for(wait_join_ms);
    }
}

//--------------------------------------------------------------------------------------------------------------------------------


void test_tasks_priority()
{
    LOG_INFO("-- tasks with priority --");
    print_stats();

    auto startup = [](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        (void)task_name;
    };

    auto context = std::make_shared<smp_task_context>();

    constexpr const auto task1_stack_size = 2048U;
    constexpr const auto task1_priority = 0; // lo prio 
    worker_task1 task1(startup, context, "worker_task1", task1_stack_size, tools::base_task::run_on_all_cores, task1_priority);

    auto periodic_lambda = [&task1](const std::shared_ptr<smp_task_context>& context, const std::string& task_name)
    {
        (void)context;
        static int count = 0;
        std::printf("%s (hi prio): count %d\n", task_name.c_str(), count);
        ++count;

        // delegate work on lower priority task
        task1.delegate([](const auto& context, const auto& task_name)
        {
            (void)context;
            std::printf("%s (lo prio): work\n", task_name.c_str());
        });
    };

    constexpr const auto period_ms = 100;
    constexpr const auto task0_stack_size = 4096U;
    constexpr const auto period = std::chrono::duration<std::uint64_t, std::milli>(period_ms); 
    periodic_task0 task0(startup, periodic_lambda, context, "periodic_task0", period, task0_stack_size, tools::base_task::run_on_all_cores, 3 /* prio + 3 */); 

    // sleep 2 sec
    constexpr const auto sleep_time_ms = 2000;
    tools::sleep_for(sleep_time_ms);
}

//--------------------------------------------------------------------------------------------------------------------------------

#if defined(ESP_PLATFORM)

constexpr std::size_t isr_queue_depth = 20;

struct isr_context
{
    tools::ring_buffer<std::pair<std::uint32_t, std::uint32_t>, 1024> storage;
    std::shared_ptr<tools::data_task<isr_context, std::uint32_t>> data_task;
    tools::ring_buffer<std::uint32_t, 1024> isr_queue;
};

using isr_data_task = tools::data_task<isr_context, std::uint32_t>;

static bool timer_isr_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx)
{
    (void)edata;
    static std::uint32_t counter = 0U;
    static auto prev_timepoint = std::chrono::high_resolution_clock::now();

    auto current_timepoint = std::chrono::high_resolution_clock::now();
    auto elapsed =  std::chrono::duration_cast<std::chrono::microseconds>(current_timepoint - prev_timepoint); 
    prev_timepoint = current_timepoint;

    isr_context* context = reinterpret_cast<isr_context*>(user_ctx);

    std::uint32_t value = counter++;

    context->data_task->isr_submit(value);

    if (counter > 1)
    {
        context->isr_queue.emplace(static_cast<std::uint32_t>(elapsed.count()));
    }    

    return false; /* YIELD_FROM_ISR already handled in submit data call */
}

static void task_isr_startup(const std::shared_ptr<isr_context>& context, const std::string& task_name)
{
    (void)context;
    std::printf("starting %s\n", task_name.c_str());
}

static void task_isr_processing(const std::shared_ptr<isr_context>& context, const std::uint32_t& data, const std::string& task_name)
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
        auto my_isr_task = std::make_shared<isr_data_task>(task_isr_startup, task_isr_processing, my_isr_context, isr_queue_depth, "isr_task", 4096);
        my_isr_context->data_task = my_isr_task;

        std::printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

        tools::sleep_for(1000);

        gptimer_handle_t gptimer = nullptr;
        gptimer_config_t timer_config = {};

        timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
        timer_config.direction = GPTIMER_COUNT_UP;
        constexpr const int one_mhz_clock = 1 * 1000 * 1000; // 1MHz, 1 tick = 1us
        timer_config.resolution_hz = one_mhz_clock; 

        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

        gptimer_alarm_config_t alarm_config = {};

        constexpr const int counter_reload_val = 0; // counter will reload with 0 on alarm event
        constexpr const int period_1ms = 1000;      // period = 0.001s @resolution 1MHz
        alarm_config.reload_count = counter_reload_val;                  
        alarm_config.alarm_count = period_1ms;                
        alarm_config.flags.auto_reload_on_alarm = true; // enable auto-reload

        ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

        gptimer_event_callbacks_t cbs = {};
        cbs.on_alarm = timer_isr_handler; // register user callback

        ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, my_isr_context.get()));
        ESP_ERROR_CHECK(gptimer_enable(gptimer));
        ESP_ERROR_CHECK(gptimer_start(gptimer));

        tools::sleep_for(500);

        ESP_ERROR_CHECK(gptimer_stop(gptimer));
        ESP_ERROR_CHECK(gptimer_disable(gptimer));

        my_isr_context->data_task.reset();
    }

    while (!my_isr_context->storage.empty())
    {
        auto [counter, elapsed] = my_isr_context->storage.front();
        my_isr_context->storage.pop();

        if (!my_isr_context->isr_queue.empty())
        {
            auto isr_elapsed = my_isr_context->isr_queue.front();
            my_isr_context->isr_queue.pop();
            std::printf("ISR produce elapsed %04" PRIu32 " us - ", isr_elapsed);
        }

        std::printf("ISR counter %" PRIu32 " - consume elapsed %" PRIu32 " us\n", counter, elapsed);
    }

    tools::sleep_for(1000);
}

#endif

//--------------------------------------------------------------------------------------------------------------------------------

void runner()
{

#if defined(ESP_PLATFORM)
    test_hardware_timer_interrupt();
#endif

    test_ring_buffer();
    test_ring_buffer_iteration();

    test_ring_vector();
    test_ring_vector_resize();
    test_ring_vector_iteration();

    test_lock_free_ring_buffer();
    test_sync_ring_buffer();
    test_sync_ring_vector();
    test_sync_queue();
    test_sync_dictionary();

    test_publish_subscribe();
    test_generic_task();
    test_periodic_task();
    test_periodic_publish_subscribe();

    test_queued_commands();
    test_ring_buffer_commands();

    test_worker_tasks();
    test_queued_bytepack_data();
    test_aggregated_bytepack_data();
    test_bytepack_data_task();

    test_json();
    test_queued_json_data();

    test_packing_unpacking_json_data();

    test_variant_fsm();
    test_calendar_day();
    test_timer();

    test_smp_tasks_cpu_affinity();
    test_smp_tasks_lock_free_ring_buffer();
    test_smp_tasks_memory_pipe();
    test_tasks_priority();

    std::printf("This is The END\n");
}

//--------------------------------------------------------------------------------------------------------------------------------
#if defined(FREERTOS_PLATFORM)
// https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/freertos.html

constexpr const std::size_t stack_size = 8192;

// Structure that will hold the TCB of the task being created.
StaticTask_t x_task_buffer = {}; // NOLINT required for xTaskCreateStatic

// Buffer that the task being created will use as its stack.  Note this is
// an array of StackType_t variables.  The size of StackType_t is dependent on
// the RTOS port.
std::array<StackType_t, stack_size> x_stack = {}; // NOLINT required for xTaskCreateStatic

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
void launch_runner() noexcept
{
    TaskHandle_t x_handle = nullptr;

    // Create the task without using any dynamic memory allocation.
    x_handle = xTaskCreateStatic(v_task_code, // Function that implements the task.
        "RUNNER",                             // Text name for the task.
        stack_size,                           // Stack size in bytes, not words.
        reinterpret_cast<void*>(1),           // Parameter passed into the task.
        tskIDLE_PRIORITY,                     // Priority at which the task is created.
        x_stack.data(),                       // Array to use as the task's stack.
        &x_task_buffer);                      // Variable to hold the task's data structure.

    (void)x_handle;

    vTaskSuspend(nullptr); // suspend main task
}
#endif
//--------------------------------------------------------------------------------------------------------------------------------

#if !defined(FREERTOS_PLATFORM)
void runner_except_catch()
{
    try
    {
        runner();
    }
    catch(std::exception& exc)
    {
        LOG_ERROR("Exception catched - %s", exc.what());
    }
}
#endif

#if defined(FREERTOS_PLATFORM)
extern "C" void app_main() noexcept
#else
int main() noexcept
#endif
{

#if defined(FREERTOS_PLATFORM)
    launch_runner();
#else
    runner_except_catch();
    return 0;
#endif
}
