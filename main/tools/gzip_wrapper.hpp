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

#include <cstdint>
#include <vector>

#include "tools/non_copyable.hpp"
#include "uzlib/uzlib.h"

namespace tools
{
    class gzip_wrapper : public non_copyable
    {
    public:
        gzip_wrapper();
        ~gzip_wrapper();

        std::vector<std::uint8_t> pack(const std::vector<std::uint8_t>& unpacked_input);
        std::vector<std::uint8_t> unpack(const std::vector<std::uint8_t>& packed_input);

    private:
        static bool m_uzlib_initialized;
        uzlib_hash_entry_t* m_hash_table = nullptr;
    };
}

#endif //  GZIP_WRAPPER_HPP_
