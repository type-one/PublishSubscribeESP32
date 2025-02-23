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
    class gzip_wrapper : public non_copyable // NOLINT inherits from non copyable and non movable class
    {
    public:
        gzip_wrapper();
        ~gzip_wrapper() = default;

        std::vector<std::uint8_t> pack(const std::vector<std::uint8_t>& unpacked_input);
        std::vector<std::uint8_t> unpack(const std::vector<std::uint8_t>& packed_input);

    private:
        using hash_table = std::array<uzlib_hash_entry_t, gzip_hash_nb_entries>;

        static bool m_uzlib_initialized; // NOLINT common to all wrapper instances
        std::unique_ptr<hash_table> m_hash_table;
    };
}

#endif //  GZIP_WRAPPER_HPP_
