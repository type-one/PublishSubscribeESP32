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
 * @file example_json_and_fpm.cpp
 * @brief Implements the JSON serialization, compression, and fixed-point math examples.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
    constexpr const int meaning_of_life_answer = 42;
    constexpr const double sample_object_value = 42.99;
    constexpr const double indoor_temperature_c = 19.47;

    void test_json()
    {
        LOG_INFO("-- json serialization/deserialization --");
        print_stats();

        {
            cjsonpp::JSONObject obj = {};
            cjsonpp::JSONObject obj2 = {};
            cjsonpp::JSONObject obj3 = {};
            cjsonpp::JSONObject obj4 = {};

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            constexpr const double number_pi = std::numbers::pi;
#else
            constexpr const double number_pi = 3.141592;
#endif
            obj.set("pi", number_pi);
            obj.set("happy", true);
            obj.set("name", "Niels");
            obj.set("nothing", cjsonpp::nullObject());

            std::vector<int> val = { 0, 1, 2 };
            obj.set("list", val);

            obj2.set("everything", meaning_of_life_answer);
            obj.set("answer", obj2);

            obj3.set("currency", "USD");
            obj4.set("value", sample_object_value);

            cjsonpp::JSONObject arr = cjsonpp::arrayObject();
            arr.add(obj3);
            arr.add(obj4);

            obj.set("object", arr);

            const auto str = obj.print();
            std::printf("%s\n", str.c_str());
        }

        {
            std::string jsonstr = R"({ "happy": true, "pi": 3.141 })";
            auto parse_result = cjsonpp::parse_result(jsonstr);
            if (!parse_result)
            {
                LOG_ERROR("json parse failed: %s", parse_result.error().message.c_str());
                return;
            }

            cjsonpp::JSONObject obj = parse_result.value();

            const auto str = obj.print();
            std::printf("%s\n", str.c_str());
        }
    }

    /** @brief Demonstrates serialising typed JSON messages into a ring-buffer queue and dispatching them by a
     * discriminant field. */
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
            json.set("temp", indoor_temperature_c);
            json.set("activity", true);

            cjsonpp::JSONObject json_answer = {};
            json_answer.set("everything", meaning_of_life_answer);
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
            // Pop one serialized envelope at a time so downstream handlers stay stateless.
            auto data = data_queue->front_pop();
            if (!data.has_value())
            {
                break;
            }

            auto parse_result = cjsonpp::parse_result(data.value());
            if (!parse_result)
            {
                LOG_ERROR("queue json parse failed: %s", parse_result.error().message.c_str());
                continue;
            }

            cjsonpp::JSONObject json = parse_result.value();

            const auto discriminant_result = json.get<std::string>("msg_type");
            if (!discriminant_result)
            {
                LOG_ERROR("missing discriminant: %s", discriminant_result.error().message.c_str());
                continue;
            }

            const auto discriminant = discriminant_result.value();

            // Route by discriminant just like a production message bus consumer.
            if (discriminant == "sensor")
            {
                const auto name_result = json.get<std::string>("sensor_name");
                const auto temp_result = json.get<double>("temp");
                const auto activity_result = json.get<bool>("activity");
                const auto answer_obj_result = json.get<cjsonpp::JSONObject>("answer");
                if (!name_result || !temp_result || !activity_result || !answer_obj_result)
                {
                    LOG_ERROR("sensor payload invalid");
                    continue;
                }

                const auto answer_result = answer_obj_result.value().get<int>("everything");
                if (!answer_result)
                {
                    LOG_ERROR("sensor answer invalid: %s", answer_result.error().message.c_str());
                    continue;
                }

                const auto name = name_result.value();
                const auto temp = temp_result.value();
                const auto activity = activity_result.value();
                const auto answer = answer_result.value();
                std::printf(
                    "sensor: %s - temp %f - %s - answer (%d)\n", name.c_str(), temp, activity ? "on" : "off", answer);
            }
            else if (discriminant == "time")
            {
                const auto time_date_result = json.get<std::string>("yyyy_mm_dd");
                const auto time_clock_result = json.get<std::string>("hh_mm_ss");
                const auto time_zone_result = json.get<std::string>("time_zone");
                if (!time_date_result || !time_clock_result || !time_zone_result)
                {
                    LOG_ERROR("time payload invalid");
                    continue;
                }

                const auto time_date = time_date_result.value();
                const auto time_clock = time_clock_result.value();
                const auto time_zone = time_zone_result.value();
                std::printf("time: %s - %s - %s\n", time_date.c_str(), time_clock.c_str(), time_zone.c_str());
            }
        }
    }

    /** @brief Demonstrates gzip compression and decompression of JSON payloads using @c tools::gzip_wrapper. */
    void test_packing_unpacking_json_data()
    {
        LOG_INFO("-- packing/unpacking json data --");
        print_stats();

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

        unpacked_buffer.resize(json_str1.length() + 1U);
        std::memcpy(unpacked_buffer.data(), json_str1.c_str(), json_str1.length());
        unpacked_buffer[json_str1.length()] = '\0';

        std::printf("json file 1 of %zu bytes\n", unpacked_buffer.size());
        std::printf("packing json file 1\n");

        // Compress before transport/storage to reduce payload footprint on constrained links.
        packed_buffer = gzip.pack(unpacked_buffer);

        std::printf("compressed to %zu bytes\n", packed_buffer.size());
        std::printf("unpacking gzip file 1\n");

        // Decompress at the receiver to reconstruct the original JSON document bytes.
        unpacked_buffer = gzip.unpack(packed_buffer);

        std::printf("unpacked to %zu bytes\n", unpacked_buffer.size());

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

    /** @brief Demonstrates fixed-point arithmetic, conversion, and mathematical functions using the fpm library. */
    void test_fpm()
    {
        LOG_INFO("-- fpm fixed-point math --");
        print_stats();

        using fixed_distance = fpm::fixed_24_8;
        using fixed_angle = fpm::fixed_16_16;
        static constexpr unsigned int fixed_math_fraction_bits = 12U;
        using fixed_math = fpm::fixed<std::int32_t, std::int64_t, fixed_math_fraction_bits>;

        const fixed_distance width(3.5);
        const fixed_distance height(7.25);
        const auto perimeter = fixed_distance(2) * (width + height);
        const auto area = width * height;
        const auto ratio = width / height;

        std::printf("arithmetic: width=%.6f height=%.6f perimeter=%.6f area=%.6f ratio=%.6f\n",
            static_cast<double>(width), static_cast<double>(height), static_cast<double>(perimeter),
            static_cast<double>(area), static_cast<double>(ratio));

        const auto explicit_fixed = fixed_angle::from_fixed_point<4>(18);
        const auto raw_fixed = fixed_angle::from_raw_value(0x00018000);

        std::printf("conversion: from_fixed_point<4>(18)=%.6f raw=0x%08x from_raw_value(0x00018000)=%.6f\n",
            static_cast<double>(explicit_fixed), static_cast<unsigned int>(explicit_fixed.raw_value()),
            static_cast<double>(raw_fixed));

        std::printf("constants: pi=%.6f half_pi=%.6f two_pi=%.6f e=%.6f\n", static_cast<double>(fixed_angle::pi()),
            static_cast<double>(fixed_angle::half_pi()), static_cast<double>(fixed_angle::two_pi()),
            static_cast<double>(fixed_angle::e()));

        const fixed_math angle = fixed_math::pi() / fixed_math(6);
        // Run deterministic fixed-point math operations that mirror embedded control calculations.
        const auto sine = fpm::sin(angle);
        const auto cosine = fpm::cos(angle);
        const auto hypotenuse = fpm::sqrt(fixed_math(3) * fixed_math(3) + fixed_math(4) * fixed_math(4));
        const auto wrapped = fpm::remainder(fixed_distance(9.5), fixed_distance(2));
        const auto exponent = fpm::pow(fixed_math(2), fixed_math(3.5));

        std::printf("math: sin(pi/6)=%.6f cos(pi/6)=%.6f sqrt(3^2+4^2)=%.6f remainder(9.5,2)=%.6f pow(2,3.5)=%.6f\n",
            static_cast<double>(sine), static_cast<double>(cosine), static_cast<double>(hypotenuse),
            static_cast<double>(wrapped), static_cast<double>(exponent));
    }
} // namespace

void run_example_json_and_fpm()
{
    // Start with basic JSON creation/parsing primitives used by the later transport examples.
    test_json();
    // Queue typed JSON payloads to demonstrate message routing by discriminant field.
    test_queued_json_data();
    // Exercise fixed-point arithmetic when deterministic numeric behaviour is preferred over float.
    test_fpm();
    // Conclude with gzip pack/unpack to reduce payload size for constrained links.
    test_packing_unpacking_json_data();
}
