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
 * @file example_hardware_timer_interrupt.cpp
 * @brief Implements the ESP32 hardware timer interrupt and ISR-backed data task example.
 * @author Laurent Lardinois
 * @date 2026-04-21
 */

#include "example_common.hpp"
#include "examples.hpp"

#if defined(ESP_PLATFORM)
namespace
{
    constexpr std::size_t isr_queue_depth = 20;

    /** @brief Shared context for the ISR-driven data task, holding the data task handle, storage ring, and ISR timing
     * queue. */
    struct isr_context
    {
        tools::ring_buffer<std::pair<std::uint32_t, std::uint32_t>, 1024> storage;
        std::shared_ptr<tools::data_task<isr_context, std::uint32_t>> data_task;
        tools::ring_buffer<std::uint32_t, 1024> isr_queue;
    };

    using isr_data_task = tools::data_task<isr_context, std::uint32_t>;

    /**
     * @brief Hardware timer ISR callback — fires at each alarm event and submits a counter value to the data task.
     * @param timer GP timer handle (unused).
     * @param edata Alarm event data (unused).
     * @param user_ctx Pointer to the @c isr_context registered as user data.
     * @return False to indicate no higher-priority task was woken.
     */
    static bool timer_isr_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx)
    {
        (void)timer;
        (void)edata;
        static std::uint32_t counter = 0U;
        static auto prev_timepoint = std::chrono::high_resolution_clock::now();

        auto current_timepoint = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_timepoint - prev_timepoint);
        prev_timepoint = current_timepoint;

        auto* context = reinterpret_cast<isr_context*>(user_ctx);

        std::uint32_t value = counter++;

        // ISR must stay minimal: push work to data_task and return quickly.
        context->data_task->isr_submit(value);

        if (counter > 1)
        {
            context->isr_queue.emplace(static_cast<std::uint32_t>(elapsed.count()));
        }

        return false;
    }

    /**
     * @brief Data task startup callback — prints a ready message when the task begins.
     * @param context Shared ISR context (unused at startup).
     * @param task_name Name of the data task.
     */
    static void task_isr_startup(const std::shared_ptr<isr_context>& context, const std::string& task_name)
    {
        (void)context;
        std::printf("starting %s\n", task_name.c_str());
    }

    /**
     * @brief Data task processing callback — records elapsed time between consecutive ISR submissions.
     * @param context Shared ISR context used to store the (counter, elapsed_us) pair.
     * @param data Counter value submitted from the ISR.
     * @param task_name Task name (unused).
     */
    static void task_isr_processing(
        const std::shared_ptr<isr_context>& context, const std::uint32_t& data, const std::string& task_name)
    {
        (void)task_name;
        static auto prev_timepoint = std::chrono::high_resolution_clock::now();
        auto current_timepoint = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_timepoint - prev_timepoint);
        prev_timepoint = current_timepoint;

        context->storage.emplace(std::make_pair(data, static_cast<std::uint32_t>(elapsed.count())));
    }

    /** @brief Configures an ESP32 GP timer, fires it at 1 kHz for 500 ms, and logs ISR-driven counter data. */
    void test_hardware_timer_interrupt()
    {
        LOG_INFO("-- hardware timer interrupt & data task --");
        print_stats();

        auto my_isr_context = std::make_shared<isr_context>();

        {
            auto my_isr_task = std::make_shared<isr_data_task>(
                task_isr_startup, task_isr_processing, my_isr_context, isr_queue_depth, "isr_task", 4096);
            my_isr_context->data_task = my_isr_task;

            std::printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

            tools::sleep_for(1000);

            gptimer_handle_t gptimer = nullptr;
            gptimer_config_t timer_config = {};

            timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
            timer_config.direction = GPTIMER_COUNT_UP;
            constexpr const int one_mhz_clock = 1 * 1000 * 1000;
            timer_config.resolution_hz = one_mhz_clock;

            ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

            gptimer_alarm_config_t alarm_config = {};

            constexpr const int counter_reload_val = 0;
            constexpr const int period_1ms = 1000;
            alarm_config.reload_count = counter_reload_val;
            alarm_config.alarm_count = period_1ms;
            alarm_config.flags.auto_reload_on_alarm = true;

            ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

            gptimer_event_callbacks_t cbs = {};
            cbs.on_alarm = timer_isr_handler;

            ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, my_isr_context.get()));
            ESP_ERROR_CHECK(gptimer_enable(gptimer));
            ESP_ERROR_CHECK(gptimer_start(gptimer));

            tools::sleep_for(500);

            ESP_ERROR_CHECK(gptimer_stop(gptimer));
            ESP_ERROR_CHECK(gptimer_disable(gptimer));

            my_isr_context->data_task.reset();
        }

        // Drain captured pairs to compare ISR production interval vs task consumption interval.
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
} // namespace
#endif

/** @brief Example entry point: runs the hardware timer interrupt and ISR-backed data task example. */
void run_example_hardware_timer_interrupt()
{
#if defined(ESP_PLATFORM)
    // ESP32-only reference for ISR handoff into task-level processing via framework ISR-safe APIs.
    test_hardware_timer_interrupt();
#endif
}
