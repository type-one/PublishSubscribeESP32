/**
 * @file memory_pipe_freertos.inl
 * @brief Implementation of a memory pipe for inter-task communication using FreeRTOS message buffers.
 *
 * This file contains the implementation of the memory_pipe class, which provides
 * a mechanism for inter-task communication using FreeRTOS message buffers. The class
 * supports dynamic and static buffer allocation, and provides methods for sending
 * and receiving data both in normal and ISR contexts.
 *
 * @author Laurent Lardinois
 * @date February 2025
 */
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
    /**
     * @brief Class representing a memory pipe for inter-task communication using FreeRTOS message buffers.
     *
     * This class provides a mechanism for inter-task communication using FreeRTOS message buffers. The class
     * supports dynamic and static buffer allocation, and provides methods for sending and receiving data both in normal
     * and ISR contexts.
     */
    class memory_pipe : public non_copyable // NOLINT inherits from non copyable/non movable class
    {
    public:
        /**
         * @brief Alias for StaticMessageBuffer_t used for holding static message buffers.
         */
        using static_buffer_holder = StaticMessageBuffer_t;

        memory_pipe() = delete;

        /**
         * @brief Constructor for the memory_pipe class.
         *
         * This constructor initializes a memory pipe with a specified buffer size.
         * It can optionally use a provided buffer address and static buffer holder.
         *
         * @param buffer_size The size of the buffer to be used.
         * @param buffer_addr Optional pointer to a pre-allocated buffer. If nullptr, a buffer will be created
         * dynamically.
         * @param static_holder Optional pointer to a static buffer holder. If nullptr, a buffer will be created
         * dynamically.
         */
        memory_pipe(
            std::size_t buffer_size, std::uint8_t* buffer_addr = nullptr, static_buffer_holder* static_holder = nullptr)
            : m_capacity(buffer_size)
        {
            // FreeRTOS platform

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

        /**
         * @brief Destructor for the memory_pipe class.
         *
         * This destructor is responsible for cleaning up the resources allocated
         * for the memory_pipe instance. Specifically, it deletes the FreeRTOS
         * message buffer if it has been initialized.
         */
        ~memory_pipe()
        {
            // FreeRTOS platform

            if (nullptr != m_message_buffer_hnd)
            {
                vMessageBufferDelete(m_message_buffer_hnd);
            }
        }

        /**
         * @brief Returns the capacity of the memory pipe.
         *
         * @return The capacity of the memory pipe.
         */
        std::size_t capacity() const
        {
            return m_capacity;
        }

        /**
         * @brief Sends data to the message pipe (internally FreeRTOS message buffer).
         *
         * This function attempts to send the specified number of bytes from the provided data buffer
         * to the message buffer, within the given timeout duration.
         *
         * @param data Pointer to the data buffer to be sent.
         * @param send_bytes Number of bytes to send from the data buffer.
         * @param timeout Maximum duration to wait for the send operation to complete.
         * @return The number of bytes successfully sent to the message buffer.
         */
        std::size_t send(const std::uint8_t* data, std::size_t send_bytes,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            // FreeRTOS platform

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

        /**
         * @brief Sends data to the message pipe (internally FreeRTOS message buffer).
         *
         * This function attempts to send the specified number of bytes from the provided data buffer
         * to the message buffer, within the given timeout duration.
         *
         * @param data  std::vector data buffer to be sent.
         * @param timeout Maximum duration to wait for the send operation to complete.
         * @return The number of bytes successfully sent to the message buffer.
         */
        std::size_t send(
            const std::vector<std::uint8_t>& data, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            return send(data.data(), data.size(), timeout);
        }

        /**
         * @brief Receives data from the message pipe (internally FreeRTOS message buffer).
         *
         * This function attempts to receive a specified number of bytes from the message buffer
         * within a given timeout period.
         *
         * @param data Pointer to the buffer where the received data will be stored.
         * @param rcv_bytes The number of bytes to receive.
         * @param timeout The maximum duration to wait for the data, specified as a std::chrono::duration.
         * @return The number of bytes actually received from the message buffer.
         */
        std::size_t receive(
            std::uint8_t* data, std::size_t rcv_bytes, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            // FreeRTOS platform

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

        /**
         * @brief Receives data from the message pipe (internally FreeRTOS message buffer).
         *
         * This function attempts to receive a specified number of bytes from the message buffer
         * within a given timeout period.
         *
         * @param data std::vector buffer where the received data will be stored.
         * @param rcv_bytes The number of bytes to receive.
         * @param timeout The maximum duration to wait for the data, specified as a std::chrono::duration.
         * @return The number of bytes actually received from the message buffer.
         */
        std::size_t receive(std::vector<std::uint8_t>& data, std::size_t rcv_bytes,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            data.resize(rcv_bytes);
            std::size_t effective_size = receive(data.data(), rcv_bytes, timeout);
            data.resize(effective_size);
            return effective_size;
        }

        /**
         * @brief Sends data from an ISR (Interrupt Service Routine) using a FreeRTOS message buffer.
         *
         * This function attempts to send the specified number of bytes from the provided data buffer
         * to the FreeRTOS message buffer, if it is not full. It is designed to be called from an ISR.
         *
         * @param data Pointer to the data buffer to be sent.
         * @param send_bytes Number of bytes to send from the data buffer.
         * @return The number of bytes successfully sent to the message buffer.
         */
        std::size_t isr_send(const std::uint8_t* data, std::size_t send_bytes)
        {
            // FreeRTOS platform

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

        /**
         * @brief Sends data from an ISR (Interrupt Service Routine) using a FreeRTOS message buffer.
         *
         * This function attempts to send the specified number of bytes from the provided data buffer
         * to the FreeRTOS message buffer, if it is not full. It is designed to be called from an ISR.
         *
         * @param data std::vector data buffer to be sent.
         * @return The number of bytes successfully sent to the message buffer.
         */
        std::size_t isr_send(const std::vector<std::uint8_t>& data)
        {
            return isr_send(data.data(), data.size());
        }

        /**
         * @brief Receives data from a message buffer in an ISR context.
         *
         * This function attempts to receive a specified number of bytes from a FreeRTOS
         * message buffer in an interrupt service routine (ISR) context. If the message
         * buffer is not empty, it retrieves the data and yields from the ISR if a higher
         * priority task was woken.
         *
         * @param data Pointer to the buffer where the received data will be stored.
         * @param rcv_bytes The number of bytes to receive from the message buffer.
         * @return The number of bytes actually received from the message buffer.
         */
        std::size_t isr_receive(std::uint8_t* data, std::size_t rcv_bytes)
        {
            // FreeRTOS platform

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

        /**
         * @brief Receives data from a message buffer in an ISR context.
         *
         * This function attempts to receive a specified number of bytes from a FreeRTOS
         * message buffer in an interrupt service routine (ISR) context. If the message
         * buffer is not empty, it retrieves the data and yields from the ISR if a higher
         * priority task was woken.
         *
         * @param data std::vector buffer where the received data will be stored.
         * @param rcv_bytes The number of bytes to receive from the message buffer.
         * @return The number of bytes actually received from the message buffer.
         */
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
