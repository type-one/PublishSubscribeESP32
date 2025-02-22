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

#if !defined(VARIANT_OVERLOAD_HPP_)
#define VARIANT_OVERLOAD_HPP_

namespace tools::detail
{
    // overload pattern using variadic template
    // Rainer Grimm
    // Typically, you use the overload pattern for a std::variant.
    // A std::variant (C++17) has one value from one of its types. std::visit allows you to apply a visitor to it.
    // Typically, the overload pattern is used for visiting the value held by a std::variant.
    // https://www.modernescpp.com/index.php/visiting-a-std-variant-with-the-overload-pattern/
    // https://www.cppstories.com/2023/finite-state-machines-variant-cpp/

    template <typename... Ts>
    struct overload : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    overload(Ts...) -> overload<Ts...>;
}

#endif //  VARIANT_OVERLOAD_HPP_
