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
 * @file example_bytepack_and_data_task.cpp
 * @brief Implements the bytepack serialization, aggregation, and data task examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/** @brief Discriminant for bytepack message variants exchanged through the synchronized queue. */
enum class message_type : std::uint8_t
{
    temperature = 1,
    manufacturing = 2,
    freetext = 3,
    aggregat = 4
};

constexpr const double sensor1_cpu_temperature_c = 45.2;
constexpr const double sensor1_gpu_temperature_c = 51.72;
constexpr const double sensor1_room_temperature_c = 21.5;
constexpr const double sensor2_cpu_temperature_c = 17.2;
constexpr const double sensor2_gpu_temperature_c = 34.7;
constexpr const double sensor2_room_temperature_c = 18.3;
constexpr const double aggregate_value_1 = 0.7;
constexpr const double aggregate_value_2 = 1.5;
constexpr const double aggregate_value_3 = 2.1;
constexpr const double aggregate_value_4 = -0.5;
/** @brief Sensor payload carrying CPU, GPU, and room temperature readings with bytepack serialization support. */
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
        return true;
    }
};

/** @brief Device manufacturing record with vendor, serial number, date, and tag, with bytepack serialization support. */
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
        return true;
    }
};

/** @brief Free-text message payload with bytepack serialization support. */
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
        return true;
    }
};

/** @brief Packs three typed messages into a synchronized ring vector and dispatches each by discriminant. */
void test_queued_bytepack_data()
{
    LOG_INFO("-- queued bytepack data --");
    print_stats();

    constexpr const std::size_t queue_depth = 128U;
    tools::sync_ring_vector<std::vector<std::uint8_t>> data_queue(queue_depth);

    free_text message1 = { "jojo rabbit" };
    manufacturing_info message2 = { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } };
    temperature_sensor message3 = { sensor1_cpu_temperature_c, sensor1_gpu_temperature_c, sensor1_room_temperature_c };

    constexpr const std::size_t buffer_size = 1024U;
    std::vector<std::uint8_t> buffer(buffer_size);
    bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer.data(), buffer.size()) };

    binary_stream.reset();
    // Prefix every payload with a type tag so receivers can decode without out-of-band metadata.
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
        // Decode queue entries exactly as a transport receiver would consume framed binary packets.
        auto data_packed = data_queue.front_pop();
        if (!data_packed.has_value())
        {
            break;
        }

        bytepack::binary_stream stream(bytepack::buffer_view(data_packed->data(), data_packed->size()));

        message_type discriminant = {};
        stream.read(discriminant);

        switch (discriminant)
        {
            // Each branch maps one wire-level discriminant to a concrete payload structure.
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
    }
}

/** @brief Compound bytepack payload aggregating a sensor dictionary, manufacturing list, status flag, and value vector. */
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

        return true;
    }
};

/** @brief Serializes an @c aggregated_info payload, deserializes it into a duplicate, and validates field values. */
void test_aggregated_bytepack_data()
{
    LOG_INFO("-- test aggregated bytepack data --");
    print_stats();

    aggregated_info aggr = {};
    aggr.m_dictionary = { { "sensor1", { sensor1_cpu_temperature_c, sensor1_gpu_temperature_c, sensor1_room_temperature_c } },
        { "sensor2", { sensor2_cpu_temperature_c, sensor2_gpu_temperature_c, sensor2_room_temperature_c } } };
    aggr.m_list = { { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } },
        { { "AMD" }, { "HTX-7788-22-5" }, { "12/05/07" }, { 'B', 'Z' } } };
    aggr.m_status = false;
    aggr.m_values = { aggregate_value_1, aggregate_value_2, aggregate_value_3, aggregate_value_4 };

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
#endif

/** @brief Empty shared context used by the bytepack data task and periodic task. */
struct data_task_context
{
};

constexpr const std::size_t binary_msg_size = 128U;
using binary_msg = std::array<std::uint8_t, binary_msg_size>;

using my_data_task = tools::data_task<data_task_context, binary_msg>;
using my_data_periodic_task = tools::periodic_task<data_task_context>;

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
namespace
{
constexpr const auto task_startup = [](const std::shared_ptr<data_task_context>& context, const std::string& task_name)
{
    (void)context;
    std::printf("starting %s\n", task_name.c_str());
};

/**
 * @brief Data task processing callback — decodes a bytepack-framed binary message and prints its contents.
 * @param context Shared data task context (unused).
 * @param data_packed Fixed-size binary buffer carrying a framed message.
 * @param task_name Task name (unused).
 */
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
}
} // namespace

/** @brief Wires a data task to a periodic sender that encodes three message types and submits them every 500 ms. */
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

    auto task_2_periodic = [&task_1, &buffer](const std::shared_ptr<data_task_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("periodic %s\n", task_name.c_str());

        temperature_sensor message1 = { sensor1_cpu_temperature_c, sensor1_gpu_temperature_c, sensor1_room_temperature_c };
        manufacturing_info message2 = { { "NVidia" }, { "HTX-4589-22-1" }, { "24/05/02" }, { 'A', 'Z' } };
        free_text message3 = { "jojo rabbit" };

        bytepack::binary_stream<> binary_stream { bytepack::buffer_view(buffer) };

        binary_stream.reset();
        binary_stream.write(message_type::temperature);
        message1.serialize(binary_stream);
        // Submit fixed-size binary frames to the data_task queue for asynchronous processing.
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
#endif

/** @brief Exercises perfect-forwarding overloads of @c tools::data_task using lvalue, rvalue, and function pointer callbacks. */
void test_data_task_perfect_forwarding()
{
    LOG_INFO("-- data task perfect forwarding --");
    print_stats();

    auto context = std::make_shared<data_task_context>();

    constexpr const std::size_t queue_depth = 4U;
    constexpr const std::size_t stack_size = 4096U;
    constexpr const int first_value = 1;
    constexpr const int second_value = 2;
    constexpr const int function_pointer_value = 42;
    constexpr const int wait_processing_ms = 100;

    using simple_data_task = tools::data_task<data_task_context, int>;

    simple_data_task::call_back startup_lvalue = [](const std::shared_ptr<data_task_context>&, const std::string& task_name)
    {
        std::printf("starting %s\n", task_name.c_str());
    };

    simple_data_task::data_call_back process_lvalue = [](const std::shared_ptr<data_task_context>&, const int& data, const std::string& task_name)
    {
        std::printf("%s received %d\n", task_name.c_str(), data);
    };

    {
        std::string name_lvalue = "data_fwd_lvalue";
        auto task = std::make_unique<simple_data_task>(
            startup_lvalue, process_lvalue, context, queue_depth, name_lvalue, stack_size);
        task->submit(first_value);
        tools::sleep_for(wait_processing_ms);
    }

    {
        auto task = std::make_unique<simple_data_task>(
            [](const std::shared_ptr<data_task_context>&, const std::string& task_name)
            {
                std::printf("starting %s\n", task_name.c_str());
            },
            [](const std::shared_ptr<data_task_context>&, const int& data, const std::string& task_name)
            {
                std::printf("%s received %d\n", task_name.c_str(), data);
            },
            context, queue_depth, std::string("data_fwd_rvalue"), stack_size);
        task->submit(second_value);
        tools::sleep_for(wait_processing_ms);
    }

    {
        void (*startup_fp)(const std::shared_ptr<data_task_context>&, const std::string&)
            = [](const std::shared_ptr<data_task_context>&, const std::string& task_name)
        {
            std::printf("starting %s\n", task_name.c_str());
        };
        void (*process_fp)(const std::shared_ptr<data_task_context>&, const int&, const std::string&)
            = [](const std::shared_ptr<data_task_context>&, const int& data, const std::string& task_name)
        {
            std::printf("%s received %d\n", task_name.c_str(), data);
        };

        auto task = std::make_unique<simple_data_task>(
            startup_fp, process_fp, context, queue_depth, "data_fwd_fp", stack_size);
        task->submit(function_pointer_value);
        tools::sleep_for(wait_processing_ms);
    }
}
} // namespace

/** @brief Example entry point: runs the bytepack serialization, data task, and aggregation examples. */
void run_example_bytepack_and_data_task()
{
    // Start with forwarding patterns for data_task callbacks and submissions.
    test_data_task_perfect_forwarding();
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
    // Queue serialized packets to see type-tag dispatch and payload reconstruction.
    test_queued_bytepack_data();
    // Aggregate multiple packed messages into one frame to model batched telemetry transport.
    test_aggregated_bytepack_data();
    // Integrate producer/consumer tasks around bytepack payloads for end-to-end task wiring.
    test_bytepack_data_task();
#endif
}
