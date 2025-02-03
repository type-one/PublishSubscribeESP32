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

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "tools/gzip_wrapper.hpp"
#include "tools/logger.hpp"
#include "tools/non_copyable.hpp"
#include "uzlib/uzlib.h"

namespace
{
    constexpr const unsigned int dict_size = 32768U;
    constexpr const unsigned int hash_bits = 12U;
    constexpr const std::size_t hash_size = sizeof(uzlib_hash_entry_t) * (1U << hash_bits);
}

namespace tools
{
    bool gzip_wrapper::m_uzlib_initialized = false;

    gzip_wrapper::gzip_wrapper()
    {
        if (!m_uzlib_initialized)
        {
            uzlib_init();
            m_uzlib_initialized = true;
        }

        m_hash_table = static_cast<uzlib_hash_entry_t*>(std::malloc(hash_size));
        if (nullptr == m_hash_table)
        {
            LOG_ERROR("could not allocate hash_table");
        }
    }

    gzip_wrapper::~gzip_wrapper()
    {
        if (nullptr != m_hash_table)
        {
            std::free(m_hash_table);
        }
    }

    std::vector<std::uint8_t> gzip_wrapper::pack(const std::vector<std::uint8_t>& unpacked_input)
    {
        std::vector<std::uint8_t> gzip_packed;

        if (nullptr != m_hash_table)
        {
            struct uzlib_comp comp = {};
            comp.dict_size = dict_size;
            comp.hash_bits = hash_bits;
            comp.hash_table = m_hash_table;
            std::memset(comp.hash_table, 0, hash_size);

            zlib_start_block(&comp.out);
            uzlib_compress(&comp, unpacked_input.data(), unpacked_input.size());
            zlib_finish_block(&comp.out);

            std::uint32_t crc32 = ~uzlib_crc32(unpacked_input.data(), unpacked_input.size(), ~0);

            // create other buffer with gzip header + footer
            const std::size_t packed_size = comp.out.outlen;
            const std::size_t gzip_buffer_sz = packed_size + 10U + 8U;
            constexpr const std::size_t gzip_header_sz = 10U;

            gzip_packed.resize(gzip_buffer_sz);

            gzip_packed[0] = 0x1f; // magic tag
            gzip_packed[1] = 0x8b;
            gzip_packed[2] = 0x08;
            gzip_packed[3] = 0x00;
            gzip_packed[4] = 0x00; // time
            gzip_packed[5] = 0x00; // time
            gzip_packed[6] = 0x00; // time
            gzip_packed[7] = 0x00; // time
            gzip_packed[8] = 0x04; // XFL
            gzip_packed[9] = 0x03; // OS

            std::memcpy(gzip_packed.data() + gzip_header_sz, comp.out.outbuf, packed_size);
            // free packed buffer
            std::free(comp.out.outbuf);

            // little endian CRC32
            gzip_packed[gzip_header_sz + packed_size + 0U] = crc32 & 0xff;
            gzip_packed[gzip_header_sz + packed_size + 1U] = (crc32 >> 8) & 0xff;
            gzip_packed[gzip_header_sz + packed_size + 2U] = (crc32 >> 16) & 0xff;
            gzip_packed[gzip_header_sz + packed_size + 3U] = (crc32 >> 24) & 0xff;

            // little endian source length
            gzip_packed[gzip_header_sz + packed_size + 4U] = unpacked_input.size() & 0xff;
            gzip_packed[gzip_header_sz + packed_size + 5U] = (unpacked_input.size() >> 8) & 0xff;
            gzip_packed[gzip_header_sz + packed_size + 6U] = (unpacked_input.size() >> 16) & 0xff;
            gzip_packed[gzip_header_sz + packed_size + 7U] = (unpacked_input.size() >> 24) & 0xff;
        } // (nullptr != m_hash_table)

        return gzip_packed;
    }

    std::vector<std::uint8_t> gzip_wrapper::unpack(const std::vector<std::uint8_t>& packed_input)
    {
        std::vector<std::uint8_t> gzip_unpacked;

        if (nullptr != m_hash_table)
        {
            const unsigned int len = static_cast<unsigned int>(packed_input.size());
            unsigned int dlen = packed_input[len - 1U];
            dlen = (dlen << 8) | packed_input[len - 2U];
            dlen = (dlen << 8) | packed_input[len - 3U];
            dlen = (dlen << 8) | packed_input[len - 4U];
            const std::size_t outlen = static_cast<std::size_t>(dlen);
            ++dlen; // reserve one extra byte

            std::uint32_t source_crc32 = packed_input[len - 5U];
            source_crc32 = (source_crc32 << 8) | packed_input[len - 6U];
            source_crc32 = (source_crc32 << 8) | packed_input[len - 7U];
            source_crc32 = (source_crc32 << 8) | packed_input[len - 8U];

            gzip_unpacked.resize(dlen);

            struct uzlib_uncomp depack_ctxt = {};
            uzlib_uncompress_init(&depack_ctxt, nullptr, 0);

            depack_ctxt.source = reinterpret_cast<const unsigned char*>(packed_input.data());
            depack_ctxt.source_limit = reinterpret_cast<const unsigned char*>(packed_input.data() + len - 4U);
            depack_ctxt.source_read_cb = nullptr;

            int res = uzlib_gzip_parse_header(&depack_ctxt);
            if (TINF_OK != res)
            {
                LOG_ERROR("error parsing header: %d", res);
                gzip_unpacked.clear();
                return gzip_unpacked;
            }

            depack_ctxt.dest = gzip_unpacked.data();
            depack_ctxt.dest_start = gzip_unpacked.data();

            // produce decompressed output in chunks of this size
            // default is to decompress byte by byte; can be any other length
            constexpr const unsigned int out_chunk_size = 1U;

            while (dlen > 0U)
            {
                unsigned int chunk_len = (dlen < out_chunk_size) ? dlen : out_chunk_size;
                depack_ctxt.dest_limit = depack_ctxt.dest + chunk_len;
                res = uzlib_uncompress_chksum(&depack_ctxt);
                dlen -= chunk_len;
                if (res != TINF_OK)
                {
                    break;
                }
            }

            if (TINF_DONE != res)
            {
                LOG_ERROR("error during decompression: %d", res);
                gzip_unpacked.clear();
                return gzip_unpacked;
            }

            const std::size_t depacked_sz = depack_ctxt.dest - reinterpret_cast<unsigned char*>(gzip_unpacked.data());

            if (depacked_sz != outlen)
            {
                LOG_ERROR("invalid decompressed length: %zu vs %zu", depacked_sz, outlen);
                gzip_unpacked.clear();
                return gzip_unpacked;
            }

            std::uint32_t check_crc32 = ~uzlib_crc32(gzip_unpacked.data(), static_cast<unsigned int>(depacked_sz), ~0);

            if (check_crc32 != source_crc32)
            {
                LOG_ERROR("invalid decompressed crc32");
                gzip_unpacked.clear();
                return gzip_unpacked;
            }

            gzip_unpacked.resize(outlen);
        } // (nullptr != m_hash_table)

        return gzip_unpacked;
    }
} // namespace tools
