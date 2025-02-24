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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "tools/gzip_wrapper.hpp"
#include "tools/logger.hpp"
#include "tools/non_copyable.hpp"
#include "uzlib/uzlib.h"

namespace tools
{
    bool gzip_wrapper::m_uzlib_initialized = false; // NOLINT private variable common to all wrapper instances

    gzip_wrapper::gzip_wrapper()
    {
        if (!m_uzlib_initialized)
        {
            uzlib_init();
            m_uzlib_initialized = true;
        }

        m_hash_table = std::make_unique<hash_table>();

        if (nullptr == m_hash_table)
        {
            LOG_ERROR("could not allocate hash_table");
        }
    }

    std::vector<std::uint8_t> gzip_wrapper::pack(const std::vector<std::uint8_t>& unpacked_input)
    {
        std::vector<std::uint8_t> gzip_packed;

        if (nullptr != m_hash_table)
        {
            struct uzlib_comp comp = {};
            comp.dict_size = gzip_dict_size;
            comp.hash_bits = gzip_hash_bits;
            comp.hash_table = m_hash_table->data();
            std::memset(comp.hash_table, 0, gzip_hash_size);

            zlib_start_block(&comp.out);
            uzlib_compress(&comp, unpacked_input.data(), unpacked_input.size());
            zlib_finish_block(&comp.out);

            std::uint32_t crc32 = ~uzlib_crc32(unpacked_input.data(), unpacked_input.size(), ~0);

            // create other buffer with gzip header + footer
            const std::size_t packed_size = comp.out.outlen;
            const std::size_t gzip_buffer_sz = packed_size + 10U + 8U;
            constexpr const std::size_t gzip_header_sz = 10U;

            gzip_packed.resize(gzip_buffer_sz);

            gzip_packed.at(0) = 0x1f; // NOLINT magic tag
            gzip_packed.at(1) = 0x8b; // NOLINT magic tag
            gzip_packed.at(2) = 0x08; // NOLINT magic tag
            gzip_packed.at(3) = 0x00; // NOLINT magic tag
            gzip_packed.at(4) = 0x00; // NOLINT time
            gzip_packed.at(5) = 0x00; // NOLINT time
            gzip_packed.at(6) = 0x00; // NOLINT time
            gzip_packed.at(7) = 0x00; // NOLINT time
            gzip_packed.at(8) = 0x04; // NOLINT XFL
            gzip_packed.at(9) = 0x03; // NOLINT OS

            std::memcpy(gzip_packed.data() + gzip_header_sz, comp.out.outbuf, packed_size);
            // free packed buffer
            std::free(comp.out.outbuf); // NOLINT allocated by malloc in uzib lib

            constexpr const std::uint32_t lo_byte_mask = 0xff;

            // little endian CRC32
            gzip_packed.at(gzip_header_sz + packed_size + 0U) // NOLINT gzip footer little endian transformation
                = crc32 & lo_byte_mask;                       // NOLINT little endian transformation
            gzip_packed.at(gzip_header_sz + packed_size + 1U) // NOLINT gzip footer little endian transformation
                = (crc32 >> 8) & lo_byte_mask;                // NOLINT little endian transformation
            gzip_packed.at(gzip_header_sz + packed_size + 2U) // NOLINT gzip footer little endian transformation
                = (crc32 >> 16) & lo_byte_mask;               // NOLINT little endian transformation
            gzip_packed.at(gzip_header_sz + packed_size + 3U) // NOLINT gzip footer little endian transformation
                = (crc32 >> 24) & lo_byte_mask;               // NOLINT little endian transformation

            // little endian source length
            gzip_packed.at(gzip_header_sz + packed_size + 4U)   // NOLINT gzip footer little endian transformation
                = unpacked_input.size() & lo_byte_mask;         // NOLINT little endian transformation
            gzip_packed.at(gzip_header_sz + packed_size + 5U)   // NOLINT gzip footer little endian transformation
                = (unpacked_input.size() >> 8) & lo_byte_mask;  // NOLINT little endian transformation
            gzip_packed.at(gzip_header_sz + packed_size + 6U)   // NOLINT gzip footer little endian transformation
                = (unpacked_input.size() >> 16) & lo_byte_mask; // NOLINT little endian transformation
            gzip_packed.at(gzip_header_sz + packed_size + 7U)   // NOLINT gzip footer little endian transformation
                = (unpacked_input.size() >> 24) & lo_byte_mask; // NOLINT little endian transformation

        } // (nullptr != m_hash_table)

        return gzip_packed;
    }

    std::vector<std::uint8_t> gzip_wrapper::unpack(
        const std::vector<std::uint8_t>& packed_input) // NOLINT cognitive complexity
    {
        std::vector<std::uint8_t> gzip_unpacked;

        const auto len = static_cast<unsigned int>(packed_input.size());
        unsigned int dlen = packed_input.at(len - 1U);  // NOLINT little endian transformation
        dlen = (dlen << 8) | packed_input.at(len - 2U); // NOLINT little endian transformation
        dlen = (dlen << 8) | packed_input.at(len - 3U); // NOLINT little endian transformation
        dlen = (dlen << 8) | packed_input.at(len - 4U); // NOLINT little endian transformation
        const auto outlen = static_cast<std::size_t>(dlen);
        ++dlen; // reserve one extra byte

        std::uint32_t source_crc32 = packed_input.at(len - 5U);         // NOLINT little endian transformation
        source_crc32 = (source_crc32 << 8) | packed_input.at(len - 6U); // NOLINT little endian transformation
        source_crc32 = (source_crc32 << 8) | packed_input.at(len - 7U); // NOLINT little endian transformation
        source_crc32 = (source_crc32 << 8) | packed_input.at(len - 8U); // NOLINT little endian transformation

        gzip_unpacked.resize(dlen);

        struct uzlib_uncomp depack_ctxt = {};
        uzlib_uncompress_init(&depack_ctxt, nullptr, 0);

        depack_ctxt.source
            = reinterpret_cast<const unsigned char*>(packed_input.data()); // NOLINT std::uint8_t* to unsigned char*
        depack_ctxt.source_limit = reinterpret_cast<const unsigned char*>( // NOLINT std::uint8_t* to unsigned char*
            packed_input.data() + len - 4U); // NOLINT space for length and uint8_t* to uchar*
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

        const std::size_t depacked_sz = depack_ctxt.dest
            - reinterpret_cast<unsigned char*>(gzip_unpacked.data()); // NOLINT std::uint8_t* to unsigned char*

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

        return gzip_unpacked;
    }
} // namespace tools
