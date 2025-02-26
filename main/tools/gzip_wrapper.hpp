/**
 * @file gzip_wrapper.hpp
 * @brief A wrapper class for gzip compression and decompression using the uzlib library.
 *
 * This file contains the definition of the gzip_wrapper class, which provides methods to
 * compress and decompress data using the gzip format. It utilizes the uzlib library for
 * compression and decompression.
 *
 * @author Laurent Lardinois
 * @date January 2025
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

#pragma once

#if !defined(GZIP_WRAPPER_HPP_)
#define GZIP_WRAPPER_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "tools/non_copyable.hpp"
#include "uzlib/uzlib.h"
namespace tools
{
    constexpr const unsigned int gzip_dict_size = 32768U;
    constexpr const unsigned int gzip_hash_bits = 12U;
    constexpr const std::size_t gzip_hash_nb_entries = (1U << gzip_hash_bits);
    constexpr const std::size_t gzip_hash_size = sizeof(uzlib_hash_entry_t) * gzip_hash_nb_entries;

    /**
     * @brief A wrapper class for gzip compression and decompression.
     *
     * This class provides methods to compress and decompress data using the gzip format.
     * It utilizes the uzlib library for compression and decompression.
     */
    class gzip_wrapper : public non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        /**
         * @brief Constructor for the gzip_wrapper class.
         *
         * This constructor initializes the uzlib library if it has not been initialized yet.
         */
        gzip_wrapper();
        ~gzip_wrapper() = default;

        /**
         * @brief Compresses the input data using gzip compression.
         *
         * This function takes a vector of uncompressed input data and compresses it using the gzip format.
         * It utilizes the uzlib library for compression and constructs the gzip header and footer manually.
         *
         * @param unpacked_input A vector of uncompressed input data.
         * @return A vector containing the gzip compressed data.
         */
        std::vector<std::uint8_t> pack(const std::vector<std::uint8_t>& unpacked_input); // use the hash table instance

        /**
         * @brief Unpacks a gzip compressed input vector.
         *
         * This function decompresses a gzip compressed input vector of bytes and returns the decompressed data.
         * It also verifies the integrity of the decompressed data using CRC32 checksum.
         *
         * @param packed_input The input vector containing gzip compressed data.
         * @return A vector containing the decompressed data. If decompression fails, an empty vector is returned.
         */
        std::vector<std::uint8_t> unpack(const std::vector<std::uint8_t>& packed_input); // doesn't use the hash table

    private:
        using hash_table = std::array<uzlib_hash_entry_t, gzip_hash_nb_entries>;

        static bool m_uzlib_initialized; // NOLINT common to all wrapper instances
        std::unique_ptr<hash_table> m_hash_table;
    };
}

#endif //  GZIP_WRAPPER_HPP_
