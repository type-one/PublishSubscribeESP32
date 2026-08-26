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
 * @file example_json_stress.cpp
 * @brief Implements synthetic Wi-Fi scan / telemetry JSON build stress scenarios via cjsonpp.
 * @author Laurent Lardinois
 * @date 2026-08-26
 */

#include "example_common.hpp"
#include "examples.hpp"

namespace
{
    // The 100/500-AP and 1000-iteration cases allocate several hundred KB of transient cJSON nodes.
    // On ESP32/ESP32-S3/ESP32-C5 without external PSRAM, the internal SRAM heap cannot sustain that
    // peak and the custom pool allocator throws std::bad_alloc, aborting the firmware. Once SPIRAM is
    // enabled and wired as an allocation backend, lift this cap to exercise the full stress range again.
#if defined(ESP_PLATFORM) && !defined(CONFIG_SPIRAM)
    constexpr int synthetic_ap_count = 30;
    constexpr int telemetry_iterations = 100;
#else
    constexpr int synthetic_ap_count = 500;
    constexpr int telemetry_iterations = 1000;
#endif
    constexpr int synthetic_rssi_base = -90;
    constexpr int synthetic_rssi_span = 60;
    constexpr int synthetic_channel_span = 13;

    /**
     * @brief Builds one synthetic (non real-world) Wi-Fi access point entry as a JSON object.
     * @param ap_index Index used to derive deterministic synthetic ssid/rssi/channel values.
     * @return A cjsonpp_result holding the built entry, or the first error encountered.
     */
    cjsonpp::cjsonpp_result<cjsonpp::JSONObject> build_synthetic_ap_entry(int ap_index)
    {
        cjsonpp::JSONObject entry = {};

        const std::string ssid = "synthetic_ssid_" + std::to_string(ap_index);
        if (const auto status = entry.set("ssid", ssid); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }

        const int rssi = synthetic_rssi_base + (ap_index % synthetic_rssi_span);
        if (const auto status = entry.set("rssi", rssi); !status)
        {
            return tools::unexpected<cjsonpp::result_error> { status.error() };
        }

        const int channel = 1 + (ap_index % synthetic_channel_span);
        if (const auto status = entry.set("channel", channel); !status)
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
     * @return A cjsonpp_result holding the scan object, or the first error encountered.
     */
    cjsonpp::cjsonpp_result<cjsonpp::JSONObject> build_synthetic_scan_json(int ap_count)
    {
        cjsonpp::JSONObject scan = {};
        cjsonpp::JSONObject aps = cjsonpp::arrayObject();

        for (int ap_index = 0; ap_index < ap_count; ++ap_index)
        {
            auto entry_result = build_synthetic_ap_entry(ap_index);
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
     * @brief Builds a synthetic 3-level nested telemetry payload mixing string, number and boolean fields.
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

    /** @brief Exercises progressively larger synthetic Wi-Fi scan JSON builds, logging size and timing. */
    void test_synthetic_scan_stress()
    {
        LOG_INFO("-- synthetic wi-fi scan json stress --");
        print_stats();

#if defined(ESP_PLATFORM) && !defined(CONFIG_SPIRAM)
        // Skip the 100/500-AP cases until SPIRAM is available; see synthetic_ap_count comment above.
        for (const int ap_count : { 1, synthetic_ap_count })
#else
        for (const int ap_count : { 1, 30, 100, synthetic_ap_count })
#endif
        {
            const auto start = std::chrono::high_resolution_clock::now();
            auto scan_result = build_synthetic_scan_json(ap_count);
            const auto end = std::chrono::high_resolution_clock::now();

            if (!scan_result)
            {
                LOG_ERROR("synthetic scan build (ap_count=%d) failed: %s", ap_count, scan_result.error().message.c_str());
                continue;
            }

            const auto json_str = scan_result.value().print(false);
            const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            LOG_INFO("synthetic scan ap_count=%d json_len=%zu bytes elapsed=%lld us", ap_count, json_str.size(),
                static_cast<long long>(elapsed_us));
        }
    }

    /** @brief Repeatedly builds a synthetic telemetry payload to check for stable size/timing over many iterations. */
    void test_synthetic_telemetry_stress()
    {
        LOG_INFO("-- synthetic telemetry json stress --");
        print_stats();

        std::size_t last_json_len = 0;
        for (int iteration = 0; iteration < telemetry_iterations; ++iteration)
        {
            auto telemetry_result = build_synthetic_telemetry_json(iteration);
            if (!telemetry_result)
            {
                LOG_ERROR(
                    "synthetic telemetry build (iteration=%d) failed: %s", iteration, telemetry_result.error().message.c_str());
                break;
            }
            last_json_len = telemetry_result.value().print(false).size();
        }

        LOG_INFO("synthetic telemetry iterations=%d last_json_len=%zu bytes", telemetry_iterations, last_json_len);
        print_stats();
    }

    /** @brief Regression check: set()/add() on a JSONObject whose underlying node is null must fail cleanly. */
    void test_null_node_guard()
    {
        LOG_INFO("-- cjsonpp null-node guard regression --");

        cjsonpp::JSONObject null_object = cjsonpp::nullObject();

        const auto set_status = null_object.set("field", 1);
        if (set_status)
        {
            LOG_ERROR("expected set() on a non-object node to fail, but it succeeded");
        }
        else
        {
            LOG_INFO("set() on a non-object node correctly returned an error: %s", set_status.error().message.c_str());
        }
    }
} // namespace

/** @brief Example entry point: runs synthetic Wi-Fi scan and telemetry JSON stress scenarios via cjsonpp. */
void run_example_json_stress()
{
    test_synthetic_scan_stress();
    test_synthetic_telemetry_stress();
    test_null_node_guard();
}
