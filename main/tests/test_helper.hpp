/**
* @file test_helper.hpp
* @brief Helper to display messages in GTests
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

#pragma once

#if !defined(TEST_HELPER_HPP_)
#define TEST_HELPER_HPP_

#include <iostream>

// https://stackoverflow.com/questions/16491675/how-to-send-custom-message-in-google-c-testing-framework

namespace test
{
// C++ stream interface
class print : public std::stringstream
{
public:
    print() = default;
    ~print()
    {
        std::cout << "\u001b[32m[          ] \u001b[33m" << str() << "\u001b[0m" << '\n';
    }
};
}

#define TEST_COUT test::print() // NOLINT macro for GTESTs

#endif // #if !defined(TEST_HELPER_HPP_)
