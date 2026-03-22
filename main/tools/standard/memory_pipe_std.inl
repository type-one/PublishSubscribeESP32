/**
 * @file memory_pipe_std.inl
 * @brief A memory pipe implementation for inter-thread communication.
 *
 * This file contains the implementation of a lock-free ring buffer for sending and receiving data between threads.
 * It supports both standard and ISR (Interrupt Service Routine) contexts.
 *
 * @author Laurent Lardinois
 * @date February 2025
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

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#include <span>
#endif

#include "tools/non_copyable.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/sync_object.hpp"

namespace tools
{
    /**
     * @brief A class representing a memory pipe for inter-thread communication.
     *
     * This class provides a lock-free ring buffer for sending and receiving data between threads.
     * It supports both standard and ISR (Interrupt Service Routine) contexts.
     */
    class memory_pipe : public non_copyable // NOLINT inherits from non copyable/non movable class
    {
    public:
        struct static_buffer_holder
        {
            int dummy;
        };

        memory_pipe() = delete;
        ~memory_pipe() = default;

        /**
         * @brief Constructs a memory_pipe object.
         *
         * @param buffer_size The size of the buffer.
         * @param buffer_addr Optional pointer to an external buffer. If nullptr, an internal buffer will be used.
         * @param static_holder Optional pointer to a static buffer holder. Currently unused.
         */
        memory_pipe(std::size_t buffer_size, std::uint8_t* buffer_addr, static_buffer_holder* static_holder)
            : m_capacity(buffer_size)
            , m_active_buffer(buffer_addr)
        {
            (void)static_holder;
            m_push_index.store(0U);
            m_pop_index.store(0U);
            m_reading.store(false);
            m_writing.store(false);

            if (nullptr == m_active_buffer)
            {
                m_internal_buffer.resize(m_capacity);
                m_active_buffer = m_internal_buffer.data();
            }
        }

        /**
         * @brief Constructs a memory_pipe object using an internal buffer.
         *
         * @param buffer_size The size of the buffer.
         */
        memory_pipe(std::size_t buffer_size)
            : memory_pipe(buffer_size, nullptr, nullptr)
        {
        }

        /**
         * @brief Get the capacity of the memory pipe.
         *
         * @return std::size_t The capacity of the memory pipe.
         */
        [[nodiscard]] std::size_t capacity() const
        {
            return m_capacity;
        }

        /**
         * @brief Sends data through the memory pipe with a specified timeout.
         *
         * This function attempts to send the specified number of bytes from the given data buffer
         * through the memory pipe. If the pipe is full, it will retry until the timeout expires.
         *
         * @param data Pointer to the data buffer to be sent.
         * @param send_bytes Number of bytes to send from the data buffer.
         * @param timeout Maximum duration to keep trying to send the data before giving up.
         * @return The number of bytes successfully sent.
         */
        [[nodiscard]] std::size_t send(const std::uint8_t* data, std::size_t send_bytes,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::size_t sent = 0U;

            if (nullptr == data)
            {
                return sent;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            auto deadline = start_time + timeout;

            for (; sent < send_bytes;)
            {
                bool success = push(data[sent]);

                if (success)
                {
                    ++sent;
                }
                else
                {
                    // wait and retry
                    auto current_time = std::chrono::high_resolution_clock::now();

                    if (current_time >= deadline)
                    {
                        // exit because timeout expired
                        break;
                    }

                    tools::sleep_for(1);
                }
            } // end sending loop

            if (sent > 0U)
            {
                m_sync.signal();
            }

            return sent;
        }

        /**
         * @brief Sends data through the memory pipe with a specified timeout.
         *
         * This function attempts to send the specified number of bytes from the given data buffer
         * through the memory pipe. If the pipe is full, it will retry until the timeout expires.
         *
         * @param data data buffer to be sent as a std::vector.
         * @param timeout Maximum duration to keep trying to send the data before giving up.
         * @return The number of bytes successfully sent.
         */
        [[nodiscard]] std::size_t send(
            const std::vector<std::uint8_t>& data, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            return send(data.data(), data.size(), timeout);
        }

        /**
         * @brief Sends data through the memory pipe from an rvalue vector.
         */
        [[nodiscard]] std::size_t send(
            std::vector<std::uint8_t>&& data, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            return send(data.data(), data.size(), timeout);
        }

        /**
         * @brief Sends data through the memory pipe using perfect forwarding.
         */
        template <typename UData
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<std::vector<std::uint8_t>, UData>::value
                && !std::is_integral<typename std::decay<UData>::type>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<std::vector<std::uint8_t>, UData>
                         && (!std::is_integral_v<std::decay_t<UData>>)
#endif
        [[nodiscard]] std::size_t send(UData&& data, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            auto forwarding_data = std::vector<std::uint8_t>(std::forward<UData>(data));
            return send(forwarding_data.data(), forwarding_data.size(), timeout);
        }

        /**
         * @brief Sends bytes from a generic range source.
         *
         * In C++20, accepts any std::ranges::input_range whose value type is
         * constructible to std::uint8_t. In C++17, accepts any iterable source
         * whose dereferenced element is constructible to std::uint8_t.
         *
         * @return The number of bytes successfully sent.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<
                std::is_convertible<
                    typename std::decay<
                        decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))
                    >::type,
                    std::uint8_t
                >::value
            >::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange>
                  && std::is_convertible_v<std::ranges::range_value_t<TRange>, std::uint8_t>
#endif
        [[nodiscard]] std::size_t send_range(
            TRange&& range, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::vector<std::uint8_t> forwarding_data;
            for (auto&& value : std::forward<TRange>(range))
            {
                forwarding_data.push_back(std::uint8_t(std::forward<decltype(value)>(value)));
            }

            return send(forwarding_data.data(), forwarding_data.size(), timeout);
        }

        /**
         * @brief Sends bytes from an initializer-list source.
         *
         * @return The number of bytes successfully sent.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_convertible<const U&, std::uint8_t>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_convertible_v<const U&, std::uint8_t>
#endif
        [[nodiscard]] std::size_t send_range(
            std::initializer_list<U> range, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::vector<std::uint8_t> forwarding_data;
            for (const auto& value : range)
            {
                forwarding_data.push_back(std::uint8_t(value));
            }

            return send(forwarding_data.data(), forwarding_data.size(), timeout);
        }

        /**
         * @brief Receives data from the memory pipe with a specified timeout.
         *
         * This function attempts to receive a specified number of bytes from the memory pipe.
         * If the data is not available immediately, it will wait and retry until the timeout expires.
         *
         * @param data Pointer to the buffer where the received data will be stored.
         * @param rcv_bytes The number of bytes to receive.
         * @param timeout The maximum duration to wait for the data.
         * @return The number of bytes successfully received.
         */
        [[nodiscard]] std::size_t receive(
            std::uint8_t* data, std::size_t rcv_bytes, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::size_t received = 0U;

            if (nullptr == data)
            {
                return received;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            auto deadline = start_time + timeout;

            for (; received < rcv_bytes;)
            {
                bool success = pop(data[received]);

                if (success)
                {
                    ++received;
                }
                else
                {
                    // wait and retry
                    auto current_time = std::chrono::high_resolution_clock::now();

                    if (current_time >= deadline)
                    {
                        // exit because timeout expired
                        break;
                    }

                    m_sync.wait_for_signal(std::chrono::duration<std::uint64_t, std::milli>(1));
                }
            } // end receiving loop

            return received;
        }

        /**
         * @brief Receives data from the memory pipe with a specified timeout.
         *
         * This function attempts to receive a specified number of bytes from the memory pipe.
         * If the data is not available immediately, it will wait and retry until the timeout expires.
         *
         * @param data std::vector buffer where the received data will be stored.
         * @param rcv_bytes The number of bytes to receive.
         * @param timeout The maximum duration to wait for the data.
         * @return The number of bytes successfully received.
         */
        [[nodiscard]] std::size_t receive(std::vector<std::uint8_t>& data, std::size_t rcv_bytes,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            data.resize(rcv_bytes);
            std::size_t effective_size = receive(data.data(), rcv_bytes, timeout);
            data.resize(effective_size);
            return effective_size;
        }

        /**
         * @brief Receives bytes into an output iterator range.
         *
         * Extracts up to the destination capacity and returns the effective
         * number of bytes received.
         */
        template <typename OutputIt>
        [[nodiscard]] std::size_t receive_range(OutputIt first, OutputIt last,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            const auto destination_size = static_cast<std::size_t>(std::distance(first, last));
            std::vector<std::uint8_t> buffer(destination_size);
            const std::size_t received_count = receive(buffer.data(), buffer.size(), timeout);

            for (std::size_t idx = 0U; idx < received_count; ++idx)
            {
                *first = buffer[idx];
                ++first;
            }

            return received_count;
        }

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        /**
         * @brief Receives bytes into contiguous storage exposed as std::span.
         */
        [[nodiscard]] std::size_t receive_range(std::span<std::uint8_t> destination,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            return receive(destination.data(), destination.size(), timeout);
        }
#endif

        /**
         * @brief Sends data from an ISR context.
         *
         * This function attempts to send the specified number of bytes from the provided data buffer.
         * Since standard C++ platforms do not support ISR calls, it falls back to a standard send call.
         *
         * @param data Pointer to the data buffer to be sent.
         * @param send_bytes Number of bytes to send from the data buffer.
         * @return The number of bytes successfully sent.
         */
        [[nodiscard]] std::size_t isr_send(const std::uint8_t* data, std::size_t send_bytes)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            return send(data, send_bytes, std::chrono::duration<std::uint64_t, std::milli>(0));
        }

        /**
         * @brief Sends data from an ISR context.
         *
         * This function attempts to send the specified number of bytes from the provided data buffer.
         * Since standard C++ platforms do not support ISR calls, it falls back to a standard send call.
         *
         * @param data std::vector data buffer to be sent.
         * @return The number of bytes successfully sent.
         */
        [[nodiscard]] std::size_t isr_send(const std::vector<std::uint8_t>& data)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            return isr_send(data.data(), data.size());
        }

        /**
         * @brief Sends data from an ISR context from an rvalue vector.
         */
        [[nodiscard]] std::size_t isr_send(std::vector<std::uint8_t>&& data)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            return isr_send(data.data(), data.size());
        }

        /**
         * @brief Sends data from an ISR context using perfect forwarding.
         */
        template <typename UData
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            ,
            typename = typename std::enable_if<std::is_constructible<std::vector<std::uint8_t>, UData>::value
                && !std::is_integral<typename std::decay<UData>::type>::value>::type
#endif
            >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_constructible_v<std::vector<std::uint8_t>, UData>
                         && (!std::is_integral_v<std::decay_t<UData>>)
#endif
        [[nodiscard]] std::size_t isr_send(UData&& data)
        {
            auto forwarding_data = std::vector<std::uint8_t>(std::forward<UData>(data));
            return isr_send(forwarding_data.data(), forwarding_data.size());
        }

        /**
         * @brief Sends bytes from a generic range source in ISR context.
         *
         * On standard C++ platform, ISR variants fallback to standard semantics.
         *
         * @return The number of bytes successfully sent.
         */
        template <typename TRange
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<
                std::is_convertible<
                    typename std::decay<
                        decltype(*std::begin(std::declval<typename std::decay<TRange>::type&>()))
                    >::type,
                    std::uint8_t
                >::value
            >::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::ranges::input_range<TRange>
                  && std::is_convertible_v<std::ranges::range_value_t<TRange>, std::uint8_t>
#endif
        [[nodiscard]] std::size_t isr_send_range(TRange&& range)
        {
            std::vector<std::uint8_t> forwarding_data;
            for (auto&& value : std::forward<TRange>(range))
            {
                forwarding_data.push_back(std::uint8_t(std::forward<decltype(value)>(value)));
            }

            return isr_send(forwarding_data.data(), forwarding_data.size());
        }

        /**
         * @brief Sends bytes from an initializer-list source in ISR context.
         */
        template <typename U
#if !((__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)))
            , typename = typename std::enable_if<std::is_convertible<const U&, std::uint8_t>::value>::type
#endif
        >
#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
            requires std::is_convertible_v<const U&, std::uint8_t>
#endif
        [[nodiscard]] std::size_t isr_send_range(std::initializer_list<U> range)
        {
            std::vector<std::uint8_t> forwarding_data;
            for (const auto& value : range)
            {
                forwarding_data.push_back(std::uint8_t(value));
            }

            return isr_send(forwarding_data.data(), forwarding_data.size());
        }

        /**
         * @brief Receives data in an interrupt service routine (ISR) context.
         *
         * This function is intended to be called from an ISR. However, since standard C++ platforms
         * do not support ISR calls, it falls back to a standard receive call with a zero timeout.
         *
         * @param data Pointer to the buffer where the received data will be stored.
         * @param rcv_bytes Number of bytes to receive.
         * @return The number of bytes actually received.
         */
        [[nodiscard]] std::size_t isr_receive(std::uint8_t* data, std::size_t rcv_bytes)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            return receive(data, rcv_bytes, std::chrono::duration<std::uint64_t, std::milli>(0));
        }

        /**
         * @brief Receives data in an interrupt service routine (ISR) context.
         *
         * This function is intended to be called from an ISR. However, since standard C++ platforms
         * do not support ISR calls, it falls back to a standard receive call with a zero timeout.
         *
         * @param data std::vector buffer where the received data will be stored.
         * @param rcv_bytes Number of bytes to receive.
         * @return The number of bytes actually received.
         */
        [[nodiscard]] std::size_t isr_receive(std::vector<std::uint8_t>& data, std::size_t rcv_bytes)
        {
            // no calls from ISRs in standard C++ platform, fallback to standard call
            data.resize(rcv_bytes);
            std::size_t effective_size = isr_receive(data.data(), rcv_bytes);
            data.resize(effective_size);
            return effective_size;
        }

        /**
         * @brief Receives bytes into an output iterator range in ISR context.
         */
        template <typename OutputIt>
        [[nodiscard]] std::size_t isr_receive_range(OutputIt first, OutputIt last)
        {
            const auto destination_size = static_cast<std::size_t>(std::distance(first, last));
            std::vector<std::uint8_t> buffer(destination_size);
            const std::size_t received_count = isr_receive(buffer.data(), buffer.size());

            for (std::size_t idx = 0U; idx < received_count; ++idx)
            {
                *first = buffer[idx];
                ++first;
            }

            return received_count;
        }

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
        /**
         * @brief Receives bytes into contiguous storage exposed as std::span in ISR context.
         */
        [[nodiscard]] std::size_t isr_receive_range(std::span<std::uint8_t> destination)
        {
            return isr_receive(destination.data(), destination.size());
        }
#endif

    private:
        /**
         * @brief Pushes a value into the internal lock-free ring buffer.
         *
         * This function attempts to push a given value into the ring buffer. It first checks if the buffer is full.
         * If the buffer is not full, it handles potential race conditions by checking the distance between the write
         * and read indices. If necessary, it waits until the reading operation is complete before proceeding.
         * Finally, it stores the value in the buffer and updates the write index.
         *
         * @param value The value to be pushed into the buffer.
         * @return true if the value was successfully pushed into the buffer, false if the buffer is full.
         */
        bool push(std::uint8_t value)
        {
            // push on an internal lock free ring buffer
            const std::size_t snap_write_idx = m_push_index.load();
            const std::size_t snap_read_idx = m_pop_index.load();

            // is full ?
            if ((snap_read_idx % m_capacity) == ((snap_write_idx + 1U) % m_capacity))
            {
                return false;
            }

            // getting close or wrap around, risk of race condition
            if (((snap_write_idx - snap_read_idx) <= 2U) || (snap_write_idx < snap_read_idx))
            {
                do
                {
                } while (m_reading.load());
            }

            m_writing.store(true);
            const std::size_t write_idx = m_push_index.fetch_add(1U);
            m_active_buffer[write_idx % m_capacity] = value;
            m_writing.store(false);

            return true;
        }

        /**
         * @brief Pops an element from the internal lock-free ring buffer.
         *
         * This function attempts to pop an element from the ring buffer and store it in the provided variable.
         * It handles potential race conditions by checking the indices and waiting if necessary.
         *
         * @param variable Reference to a std::uint8_t where the popped element will be stored.
         * @return true if an element was successfully popped, false if the buffer is empty.
         */
        bool pop(std::uint8_t& variable)
        {
            // pop from an internal lock free ring buffer

            const std::size_t snap_write_idx = m_push_index.load();
            const std::size_t snap_read_idx = m_pop_index.load();

            // is empty ?
            if ((snap_read_idx % m_capacity) == (snap_write_idx % m_capacity))
            {
                return false;
            }

            // getting close or wrap around, risk of race condition
            if (((snap_write_idx - snap_read_idx) <= 2U) || (snap_write_idx < snap_read_idx))
            {
                do
                {
                } while (m_writing.load());
            }

            m_reading.store(true);
            const std::size_t read_idx = m_pop_index.fetch_add(1U);
            variable = m_active_buffer[read_idx % m_capacity];
            m_reading.store(false);

            return true;
        }

        std::size_t m_capacity = 0;
        std::uint8_t* m_active_buffer = nullptr;
        std::vector<std::uint8_t> m_internal_buffer;

        std::atomic<std::size_t> m_push_index;
        std::atomic<std::size_t> m_pop_index;
        std::atomic_bool m_reading;
        std::atomic_bool m_writing;

        tools::sync_object m_sync;
    };
}
