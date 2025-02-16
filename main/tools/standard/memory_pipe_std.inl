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

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "tools/non_copyable.hpp"
#include "tools/platform_helpers.hpp"
#include "tools/sync_object.hpp"

namespace tools
{
    class memory_pipe : public non_copyable
    {
    public:
        memory_pipe() = delete;
        ~memory_pipe() = default;

        memory_pipe(std::size_t buffer_size, std::uint8_t* buffer_addr = nullptr)
            : m_capacity(buffer_size)
            , m_active_buffer(buffer_addr)
        {
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

        std::size_t capacity() const
        {
            return m_capacity;
        }

        std::size_t send(const std::uint8_t* data, std::size_t send_bytes,
            const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::size_t sent = 0U;

            if (nullptr == data)
            {
                return sent;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            auto deadline = start_time + timeout;

            for (; sent <= send_bytes;)
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

        std::size_t send(
            const std::vector<std::uint8_t>& data, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            return send(data.data(), data.size(), timeout);
        }

        std::size_t receive(
            std::uint8_t* data, std::size_t rcv_bytes, const std::chrono::duration<std::uint64_t, std::milli>& timeout)
        {
            std::size_t received = 0U;

            if (nullptr == data)
            {
                return received;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            auto deadline = start_time + timeout;

            for (; received <= rcv_bytes;)
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
            return send(data, send_bytes, std::chrono::duration<std::uint64_t, std::milli>(0));
        }

        std::size_t isr_send(const std::vector<std::uint8_t>& data)
        {
            return isr_send(data.data(), data.size());
        }

        std::size_t isr_receive(std::uint8_t* data, std::size_t rcv_bytes)
        {
            return receive(data, rcv_bytes, std::chrono::duration<std::uint64_t, std::milli>(0));
        }

        std::size_t isr_receive(std::vector<std::uint8_t>& data, std::size_t rcv_bytes)
        {
            data.resize(rcv_bytes);
            std::size_t effective_size = isr_receive(data.data(), rcv_bytes);
            data.resize(effective_size);
            return effective_size;
        }

    private:
        bool push(std::uint8_t value)
        {
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

        bool pop(std::uint8_t& variable)
        {
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
