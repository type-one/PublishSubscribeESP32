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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/message_buffer.h>

#include "tools/logger.hpp"
#include "tools/non_copyable.hpp"
#include "tools/platform_helpers.hpp"

namespace tools
{
    class memory_pipe : public non_copyable // NOLINT inherits from non copyable/non movable class
    {
    public:
        using static_buffer_holder = StaticMessageBuffer_t;

        memory_pipe() = delete;

        memory_pipe(
            std::size_t buffer_size, std::uint8_t* buffer_addr = nullptr, static_buffer_holder* static_holder = nullptr)
            : m_capacity(buffer_size)
        {
            // https://www.freertos.org/Community/Blogs/2020/simple-multicore-core-to-core-communication-using-freertos-message-buffers
            // https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/04-Stream-and-message-buffers/03-Message-buffer-example
            // https://github.com/MaJerle/stm32h7-dual-core-inter-cpu-async-communication/tree/main

            // When a message is written to
            // the message buffer an additional sizeof( size_t ) bytes are also written to
            // store the message's length.  sizeof( size_t ) is typically 4 bytes on a
            // 32-bit architecture, so on most 32-bit architectures a 10 byte message will
            // take up 14 bytes of message buffer space.

            if ((nullptr == buffer_addr) || (nullptr == static_holder))
            {
                // https://freertos.org/Documentation/02-Kernel/04-API-references/09-Message-buffers/01-xMessageBufferCreate
                m_message_buffer_hnd = xMessageBufferCreate(m_capacity);

                if (nullptr == m_message_buffer_hnd)
                {
                    LOG_ERROR("FATAL error: xMessageBufferCreate() failed");
                }
            }
            else
            {
                // https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/freertos.html
                // https://www.freertos.org/Documentation/02-Kernel/04-API-references/09-Message-buffers/02-xMessageBufferCreateStatic

                m_message_buffer_hnd = xMessageBufferCreateStatic(m_capacity, buffer_addr, static_holder);

                if (nullptr == m_message_buffer_hnd)
                {
                    LOG_ERROR("FATAL error: xMessageBufferCreateStatic() failed");
                }
                else
                {
                    m_static_msg_buffer = static_holder;
                }
            }
        }

        ~memory_pipe()
        {
            if (nullptr != m_message_buffer_hnd)
            {
                vMessageBufferDelete(m_message_buffer_hnd);
            }
        }

        std::size_t capacity() const
        {
            return m_capacity;
        }

        std::size_t send(const std::uint8_t* data, std::size_t send_bytes,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::size_t sent = 0U;

            if ((nullptr != m_message_buffer_hnd) && (nullptr != data))
            {
                if (pdFALSE == xMessageBufferIsFull(m_message_buffer_hnd))
                {
                    const TickType_t ticks_to_wait = static_cast<TickType_t>(timeout.count() * portTICK_PERIOD_MS);
                    sent = xMessageBufferSend(m_message_buffer_hnd, data, send_bytes, ticks_to_wait);
                }
            }

            return sent;
        }

        std::size_t send(
            const std::vector<std::uint8_t>& data, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            return send(data.data(), data.size(), timeout);
        }

        std::size_t receive(
            std::uint8_t* data, std::size_t rcv_bytes, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::size_t received = 0U;

            if ((nullptr != m_message_buffer_hnd) && (nullptr != data))
            {
                if (pdFALSE == xMessageBufferIsEmpty(m_message_buffer_hnd))
                {
                    const TickType_t ticks_to_wait = static_cast<TickType_t>(timeout.count() * portTICK_PERIOD_MS);
                    received = xMessageBufferReceive(m_message_buffer_hnd, data, rcv_bytes, ticks_to_wait);
                }
            }

            return received;
        }

        std::size_t receive(std::vector<std::uint8_t>& data, std::size_t rcv_bytes,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            data.resize(rcv_bytes);
            std::size_t effective_size = receive(data.data(), rcv_bytes, timeout);
            data.resize(effective_size);
            return effective_size;
        }

        std::size_t isr_send(const std::uint8_t* data, std::size_t send_bytes)
        {
            std::size_t sent = 0U;

            if ((nullptr != m_message_buffer_hnd) && (nullptr != data))
            {
                if (pdFALSE == xMessageBufferIsFull(m_message_buffer_hnd))
                {
                    BaseType_t px_higher_priority_task_woken = pdFALSE;
                    sent = xMessageBufferSendFromISR(
                        m_message_buffer_hnd, data, send_bytes, &px_higher_priority_task_woken);
                    portYIELD_FROM_ISR(px_higher_priority_task_woken);
                }
            }

            return sent;
        }

        std::size_t isr_send(const std::vector<std::uint8_t>& data)
        {
            return isr_send(data.data(), data.size());
        }

        std::size_t isr_receive(std::uint8_t* data, std::size_t rcv_bytes)
        {
            std::size_t received = 0U;

            if ((nullptr != m_message_buffer_hnd) && (nullptr != data))
            {
                if (pdFALSE == xMessageBufferIsEmpty(m_message_buffer_hnd))
                {
                    BaseType_t px_higher_priority_task_woken = pdFALSE;
                    received = xMessageBufferReceiveFromISR(
                        m_message_buffer_hnd, data, rcv_bytes, &px_higher_priority_task_woken);
                    portYIELD_FROM_ISR(px_higher_priority_task_woken);
                }
            }

            return received;
        }

        std::size_t isr_receive(std::vector<std::uint8_t>& data, std::size_t rcv_bytes)
        {
            data.resize(rcv_bytes);
            std::size_t effective_size = isr_receive(data.data(), rcv_bytes);
            data.resize(effective_size);
            return effective_size;
        }

    private:
        std::size_t m_capacity = 0;
        MessageBufferHandle_t m_message_buffer_hnd = nullptr;
        static_buffer_holder* m_static_msg_buffer = nullptr;
    };
}
