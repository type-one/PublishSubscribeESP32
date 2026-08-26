/**
 * @file test_cjsonpp_stress.cpp
 * @brief Stress and regression tests for cjsonpp::JSONObject synthetic Wi-Fi scan / telemetry JSON builds.
 *
 * These tests reproduce, in a Linux/ASan environment, the synthetic Wi-Fi access point scan and
 * cloud-telemetry payload construction scenarios that originally surfaced a null-pointer crash on
 * cJSON allocation failure (OOM) and an unrelated std::shared_ptr lock-policy heap-pressure issue on
 * embedded targets. The JSON payloads used here are entirely synthetic (no real device or network data).
 *
 * @author Laurent Lardinois and Copilot GPT-4o
 * @date August 2026
 */

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

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "cJSON/cJSON.h"
#include "cjsonpp/cjsonpp.hpp"

namespace
{
    constexpr int max_ssid_length = 32;
    constexpr int min_json_size_bytes = 4096;

    /**
     * @brief Builds one synthetic Wi-Fi access point JSON entry.
     * @param ap_index Index used to derive deterministic synthetic ssid/rssi/channel values.
     * @param ssid_length Length of the synthetic SSID string to generate.
     * @return A cjsonpp_result holding the built entry, or the first error encountered.
     */
    cjsonpp::cjsonpp_result<cjsonpp::JSONObject> build_synthetic_ap_entry(int ap_index, int ssid_length)
    {
        cjsonpp::JSONObject entry = {};

        std::string ssid = "ap_" + std::to_string(ap_index);
        ssid.resize(static_cast<std::size_t>(ssid_length), 'x');

        if (const auto status = entry.set("ssid", ssid); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = entry.set("rssi", -30 - (ap_index % 60)); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = entry.set("channel", 1 + (ap_index % 13)); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = entry.set("secure", (ap_index % 2) == 0); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }

        return cjsonpp::cjsonpp_result<cjsonpp::JSONObject> { entry };
    }

    /**
     * @brief Builds a synthetic Wi-Fi scan JSON payload with the requested number of AP entries.
     * @param ap_count Number of synthetic AP entries to add to the scan array.
     * @param ssid_length Length of each synthetic SSID string.
     * @return A cjsonpp_result holding the scan object, or the first error encountered.
     */
    cjsonpp::cjsonpp_result<cjsonpp::JSONObject> build_synthetic_scan_json(int ap_count, int ssid_length = 8)
    {
        cjsonpp::JSONObject scan = {};
        cjsonpp::JSONObject aps = cjsonpp::arrayObject();

        for (int ap_index = 0; ap_index < ap_count; ++ap_index)
        {
            auto entry_result = build_synthetic_ap_entry(ap_index, ssid_length);
            if (!entry_result)
            {
                return tools::unexpected<cjsonpp::result_error> { entry_result.error() };
            }
            if (const auto status = aps.add(entry_result.value()); !status)
            {
                return tools::unexpected<cjsonpp::result_error> { status.error() };
            }
        }

        if (const auto status = scan.set("ap_count", ap_count); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = scan.set("access_points", aps); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }

        return cjsonpp::cjsonpp_result<cjsonpp::JSONObject> { scan };
    }

    /**
     * @brief Builds a synthetic 3-level nested telemetry payload mixing string, number, and boolean fields.
     * @param iteration Index folded into the payload to make each build slightly different.
     * @return A cjsonpp_result holding the telemetry object, or the first error encountered.
     */
    cjsonpp::cjsonpp_result<cjsonpp::JSONObject> build_synthetic_telemetry_json(int iteration)
    {
        cjsonpp::JSONObject telemetry = {};
        cjsonpp::JSONObject device = {};
        cjsonpp::JSONObject metrics = {};

        if (const auto status = device.set("device_id", "synthetic_device_" + std::to_string(iteration % 8)); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = device.set("firmware", "1.0.0"); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = metrics.set("cpu_load_percent", iteration % 100); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = metrics.set("free_heap_bytes", (iteration % 4096) + 1024); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = metrics.set("healthy", true); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = telemetry.set("device", device); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = telemetry.set("metrics", metrics); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }
        if (const auto status = telemetry.set("iteration", iteration); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }

        return cjsonpp::cjsonpp_result<cjsonpp::JSONObject> { telemetry };
    }
} // namespace

/**
 * @class pd_json_stress_fixture
 * @brief Fixture grouping the synthetic Wi-Fi scan / telemetry JSON stress and regression tests.
 */
class pd_json_stress_fixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(pd_json_stress_fixture, WifiScanSingleAp)
{
    auto scan_result = build_synthetic_scan_json(1);
    ASSERT_TRUE(scan_result.has_value());
    EXPECT_TRUE(scan_result.value().has("access_points"));
}

TEST_F(pd_json_stress_fixture, WifiScan30Ap)
{
    auto scan_result = build_synthetic_scan_json(30);
    ASSERT_TRUE(scan_result.has_value());
    const auto aps_result = scan_result.value().get("access_points");
    ASSERT_TRUE(aps_result.has_value());
    EXPECT_EQ(aps_result.value().type(), cjsonpp::JSONType::Array);
}

TEST_F(pd_json_stress_fixture, WifiScan100Ap)
{
    auto scan_result = build_synthetic_scan_json(100);
    ASSERT_TRUE(scan_result.has_value());
}

TEST_F(pd_json_stress_fixture, WifiScan500Ap)
{
    auto scan_result = build_synthetic_scan_json(500);
    ASSERT_TRUE(scan_result.has_value());
}

TEST_F(pd_json_stress_fixture, WifiScanMaxSsidLength)
{
    auto scan_result = build_synthetic_scan_json(10, max_ssid_length);
    ASSERT_TRUE(scan_result.has_value());
    const auto json_str = scan_result.value().print(false);
    EXPECT_FALSE(json_str.empty());
}

TEST_F(pd_json_stress_fixture, WifiScanRepeatedBuild100Times)
{
    constexpr int repeat_count = 100;
    for (int repeat_index = 0; repeat_index < repeat_count; ++repeat_index)
    {
        auto scan_result = build_synthetic_scan_json(30);
        ASSERT_TRUE(scan_result.has_value()) << "repeat_index=" << repeat_index;
    }
}

TEST_F(pd_json_stress_fixture, WifiScanOutputAtLeast4Kb)
{
    auto scan_result = build_synthetic_scan_json(100, max_ssid_length);
    ASSERT_TRUE(scan_result.has_value());
    const auto json_str = scan_result.value().print(false);
    EXPECT_GE(json_str.size(), static_cast<std::size_t>(min_json_size_bytes));
}

TEST_F(pd_json_stress_fixture, WifiScanRoundTripContent)
{
    auto scan_result = build_synthetic_scan_json(3, static_cast<int>(std::string("ap_0").size()));
    ASSERT_TRUE(scan_result.has_value());

    const auto json_str = scan_result.value().print(false);
    const auto parsed_result = cjsonpp::parse_result(json_str);
    ASSERT_TRUE(parsed_result.has_value());

    const auto& parsed = parsed_result.value();
    const auto ap_count_result = parsed.get<int>("ap_count");
    ASSERT_TRUE(ap_count_result.has_value());
    EXPECT_EQ(ap_count_result.value(), 3);

    const auto aps_result = parsed.get("access_points");
    ASSERT_TRUE(aps_result.has_value());
    const auto first_ap_result = aps_result.value().get(0);
    ASSERT_TRUE(first_ap_result.has_value());
    const auto ssid_result = first_ap_result.value().get<std::string>("ssid");
    ASSERT_TRUE(ssid_result.has_value());
    EXPECT_EQ(ssid_result.value(), "ap_0");
}

/**
 * @brief Regression test for the OOM null-guard: set() on a JSONObject whose node has type mismatch must fail cleanly.
 */
TEST_F(pd_json_stress_fixture, NullNodeGuardSetReturnsInternalErrorNotCrash)
{
    cjsonpp::JSONObject null_object = cjsonpp::nullObject();
    const auto status = null_object.set("field", 1);
    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, cjsonpp::result_code::invalid_type);
}

/**
 * @brief Regression test for the OOM null-guard: add() on a JSONObject whose node has type mismatch must fail cleanly.
 */
TEST_F(pd_json_stress_fixture, NullNodeGuardAddReturnsInternalErrorNotCrash)
{
    cjsonpp::JSONObject null_object = cjsonpp::nullObject();
    const auto status = null_object.add(1);
    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, cjsonpp::result_code::invalid_type);
}

/**
 * @brief Regression test: constructing a JSONObject directly around a null cJSON pointer must not crash on
 * subsequent set()/add()/print()/type() calls, and must report internal_error instead.
 */
TEST_F(pd_json_stress_fixture, NullCJsonNodeGuardedAcrossAllAccessors)
{
    cjsonpp::JSONObject wrapped_null(static_cast<cJSON*>(nullptr), true);

    EXPECT_EQ(wrapped_null.type(), cjsonpp::JSONType::Invalid);
    EXPECT_EQ(wrapped_null.print(false), std::string { "<error>" });

    const auto set_status = wrapped_null.set("field", 1);
    ASSERT_FALSE(set_status.has_value());
    EXPECT_EQ(set_status.error().code, cjsonpp::result_code::internal_error);

    const auto add_status = wrapped_null.add(1);
    ASSERT_FALSE(add_status.has_value());
    EXPECT_EQ(add_status.error().code, cjsonpp::result_code::internal_error);

    const auto get_status = wrapped_null.get<int>("field");
    ASSERT_FALSE(get_status.has_value());
    EXPECT_EQ(get_status.error().code, cjsonpp::result_code::internal_error);

    const auto as_status = wrapped_null.as<int>();
    ASSERT_FALSE(as_status.has_value());
    EXPECT_EQ(as_status.error().code, cjsonpp::result_code::internal_error);
}

TEST_F(pd_json_stress_fixture, TelemetrySingleBuild)
{
    auto telemetry_result = build_synthetic_telemetry_json(0);
    ASSERT_TRUE(telemetry_result.has_value());
    EXPECT_TRUE(telemetry_result.value().has("device"));
    EXPECT_TRUE(telemetry_result.value().has("metrics"));
}

TEST_F(pd_json_stress_fixture, TelemetryRoundTripContent)
{
    auto telemetry_result = build_synthetic_telemetry_json(7);
    ASSERT_TRUE(telemetry_result.has_value());

    const auto json_str = telemetry_result.value().print(false);
    const auto parsed_result = cjsonpp::parse_result(json_str);
    ASSERT_TRUE(parsed_result.has_value());

    const auto& parsed = parsed_result.value();
    const auto iteration_result = parsed.get<int>("iteration");
    ASSERT_TRUE(iteration_result.has_value());
    EXPECT_EQ(iteration_result.value(), 7);
}

TEST_F(pd_json_stress_fixture, Telemetry1000Iterations)
{
    constexpr int iteration_count = 1000;
    for (int iteration = 0; iteration < iteration_count; ++iteration)
    {
        auto telemetry_result = build_synthetic_telemetry_json(iteration);
        ASSERT_TRUE(telemetry_result.has_value()) << "iteration=" << iteration;
    }
}

TEST_F(pd_json_stress_fixture, WifiAndTelemetry500SequentialPairs)
{
    constexpr int pair_count = 500;
    for (int pair_index = 0; pair_index < pair_count; ++pair_index)
    {
        auto scan_result = build_synthetic_scan_json(1);
        ASSERT_TRUE(scan_result.has_value()) << "pair_index=" << pair_index;

        auto telemetry_result = build_synthetic_telemetry_json(pair_index);
        ASSERT_TRUE(telemetry_result.has_value()) << "pair_index=" << pair_index;
    }
}
