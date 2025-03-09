/**
 * @file test_cexception.cpp
 * @brief Unit tests for simple exception handling in C framework
 *
 * CException is simple exception handling in C. It is significantly faster than full-blown
 * C++ exception handling but loses some flexibility.
 *
 * This file contains the original CException tests adapted to the Google Test framework.
 *
 * @author Laurent Lardinois
 * @date March 2025
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

#include <gtest/gtest.h>

#include <atomic>

#include "CException/CException.h"
#include "tests/test_helper.hpp"

class CExceptionTest : public ::testing::Test
{
protected:
    std::atomic<int> TestingTheFallback;
    std::atomic<int> TestingTheFallbackId;

    /**
     * @brief Sets up the test environment by initializing the bytepack::binary_stream object.
     */
    void SetUp() override
    {
        CExceptionFrames[0].pFrame = nullptr;
        TestingTheFallback.store(0);
        TestingTheFallbackId.store(0);
    }

    /**
     * @brief Tears down the test environment by resetting the bytepack::binary_stream object.
     */
    void TearDown() override
    {
    }
};

TEST_F(CExceptionTest, BasicTryDoesNothingIfNoThrow)
{
    int i = 0;
    CEXCEPTION_T e = 0x5a;

    Try
    {
        i += 1;
    }
    Catch(e)
    {
        TEST_COUT << "Should Not Enter Catch If Not Thrown" << '\n';
    }

    // verify that e was untouched
    EXPECT_EQ(0x5a, e);

    // verify that i was incremented once
    EXPECT_EQ(1, i);
}


TEST_F(CExceptionTest, BasicThrowAndCatch)
{
    CEXCEPTION_T e = 0;

    Try
    {
        Throw(0xBE);
        TEST_COUT << "Should Have Thrown An Error" << '\n';
    }
    Catch(e)
    {
        // verify that e has the right data
        EXPECT_EQ(0xBE, e);
    }

    // verify that e STILL has the right data
    EXPECT_EQ(0xBE, e);
}

TEST_F(CExceptionTest, VerifyVolatilesSurviveThrowAndCatch)
{
    std::atomic<unsigned int> VolVal { 0 };
    CEXCEPTION_T e = 0;

    Try
    {
        VolVal.store(2U);
        Throw(0xBF);
        TEST_COUT << "Should Have Thrown An Error" << '\n';
    }
    Catch(e)
    {
        VolVal.fetch_add(2U);
        EXPECT_EQ(0xBF, e);
    }

    EXPECT_EQ(4U, VolVal.load());
    EXPECT_EQ(0xBF, e);
}

namespace
{
    void HappyExceptionThrower(unsigned int ID)
    {
        if (ID != 0)
        {
            Throw(ID);
        }
    }
}

TEST_F(CExceptionTest, ThrowFromASubFunctionAndCatchInRootFunc)
{
    std::atomic<unsigned int> ID { 0 };
    CEXCEPTION_T e = 0;

    Try
    {
        HappyExceptionThrower(0xBA);
        TEST_COUT << "Should Have Thrown An Exception" << '\n';
    }
    Catch(e)
    {
        ID.store(e);
    }

    // verify that I can pass that value to something else
    EXPECT_EQ(0xBA, e);

    // verify that ID and e have the same value
    EXPECT_EQ(ID.load(), e);
}

namespace
{
    void HappyExceptionRethrower(unsigned int ID)
    {
        CEXCEPTION_T e = 0;

        Try
        {
            Throw(ID);
        }
        Catch(e)
        {
            switch (e)
            {
                case 0xBD:
                    Throw(0xBF);
                    break;
                default:
                    break;
            }
        }
    }
}

TEST_F(CExceptionTest, ThrowAndCatchFromASubFunctionAndRethrowToCatchInRootFunc)
{
    std::atomic<unsigned int> ID { 0 };
    CEXCEPTION_T e = 0;

    Try
    {
        HappyExceptionRethrower(0xBD);
        TEST_COUT << "Should Have Rethrown Exception" << '\n';
    }
    Catch(e)
    {
        ID.store(1);
    }

    EXPECT_EQ(0xBF, e);
    EXPECT_EQ(1U, ID.load());
}

TEST_F(CExceptionTest, ThrowAndCatchFromASubFunctionAndNoRethrowToCatchInRootFunc)
{
    CEXCEPTION_T e = 3;

    Try
    {
        HappyExceptionRethrower(0xBF);
    }
    Catch(e)
    {
        TEST_COUT << "Should Not Have Re-thrown Error (it should have already been caught)" << '\n';
    }

    // verify that THIS e is still untouched, even though subfunction was touched
    EXPECT_EQ(3, e);
}

TEST_F(CExceptionTest, ThrowAnErrorThenEnterATryBlockFromWithinCatch_VerifyThisDoesntCorruptExceptionId)
{
    CEXCEPTION_T e = 0;

    Try
    {
        HappyExceptionThrower(0xBF);
        TEST_COUT << "Should Have Thrown Exception" << '\n';
    }
    Catch(e)
    {
        EXPECT_EQ(0xBF, e);
        HappyExceptionRethrower(0x12);
        EXPECT_EQ(0xBF, e);
    }
    EXPECT_EQ(0xBF, e);
}

TEST_F(CExceptionTest, ThrowAnErrorThenEnterATryBlockFromWithinCatch_VerifyThatEachExceptionIdIndependent)
{
    CEXCEPTION_T e1 = 0U;
    CEXCEPTION_T e2 = 0U;

    Try
    {
        HappyExceptionThrower(0xBF);
        TEST_COUT << "Should Have Thrown Exception" << '\n';
    }
    Catch(e1)
    {
        EXPECT_EQ(0xBF, e1);
        Try
        {
            HappyExceptionThrower(0x12);
        }
        Catch(e2)
        {
            EXPECT_EQ(0x12, e2);
        }
        EXPECT_EQ(0x12, e2);
        EXPECT_EQ(0xBF, e1);
    }
    EXPECT_EQ(0x12, e2);
    EXPECT_EQ(0xBF, e1);
}

TEST_F(CExceptionTest, CanHaveMultipleTryBlocksInASingleFunction)
{
    CEXCEPTION_T e = 0;

    Try
    {
        HappyExceptionThrower(0x01);
        TEST_COUT << "Should Have Thrown Exception" << '\n';
    }
    Catch(e)
    {
        EXPECT_EQ(0x01, e);
    }

    Try
    {
        HappyExceptionThrower(0xF0);
        TEST_COUT << "Should Have Thrown Exception" << '\n';
    }
    Catch(e)
    {
        EXPECT_EQ(0xF0, e);
    }
}

TEST_F(CExceptionTest, CanHaveNestedTryBlocksInASingleFunction_ThrowInside)
{
    int i = 0;
    CEXCEPTION_T e = 0;

    Try
    {
        Try
        {
            HappyExceptionThrower(0x01);
            i = 1;
            TEST_COUT << "Should Have Rethrown Exception" << '\n';
        }
        Catch(e)
        {
            EXPECT_EQ(0x01, e);
        }
    }
    Catch(e)
    {
        TEST_COUT << "Should Have Been Caught By Inside Catch" << '\n';
    }

    // verify that i is still zero
    EXPECT_EQ(0, i);
}

TEST_F(CExceptionTest, CanHaveNestedTryBlocksInASingleFunction_ThrowOutside)
{
    std::atomic<int> i { 0 };
    CEXCEPTION_T e = 0;

    Try
    {
        Try
        {
            i.store(2);
        }
        Catch(e)
        {
            TEST_COUT << "Should Not Be Caught Here" << '\n';
        }
        HappyExceptionThrower(0x01);
        TEST_COUT << "Should Have Rethrown Exception" << '\n';
    }
    Catch(e)
    {
        EXPECT_EQ(0x01, e);
    }

    // verify that i is 2
    EXPECT_EQ(2, i.load());
}

#if 0 // TODO FIX
TEST_F(CExceptionTest, AThrowWithoutATryCatchWillUseDefaultHandlerIfSpecified)
{
    // Let the fallback handler know we're expecting it to get called this time, so don't fail
    TestingTheFallback.store(1);

    Throw(0xBE);

    // We know the fallback was run because it decrements the counter above
    EXPECT_FALSE(TestingTheFallback.load());
    EXPECT_EQ(0xBE, TestingTheFallbackId.load());
}
#endif

#if 0 // TODO FIX
TEST_F(CExceptionTest, AThrowWithoutOutsideATryCatchWillUseDefaultHandlerEvenAfterTryCatch)
{
    CEXCEPTION_T e = 0;

    Try
    {
        // It's not really important that we do anything here.
    }
    Catch(e)
    {
        // The entire purpose here is just to make sure things get set back to using the default handler when done
    }

    // Let the fallback handler know we're expecting it to get called this time, so don't fail
    TestingTheFallback.store(1);

    Throw(0xBE);

    // We know the fallback was run because it decrements the counter above
    EXPECT_FALSE(TestingTheFallback.load());
    EXPECT_EQ(0xBE, TestingTheFallbackId.load());
}
#endif

TEST_F(CExceptionTest, AbilityToExitTryWithoutThrowingAnError)
{
    std::atomic<int> i { 0 };
    CEXCEPTION_T e = 0;

    Try
    {
        ExitTry();
        i.store(1);
        TEST_COUT << "Should Have Exited Try Before This" << '\n';
    }
    Catch(e)
    {
        i.store(2);
        TEST_COUT << "Should Not Have Been Caught" << '\n';
    }

    // verify that i is still zero
    EXPECT_EQ(0, i.load());
}

TEST_F(CExceptionTest, AbilityToExitTryWillOnlyExitOneLevel)
{
    std::atomic<int> i { 0 };
    CEXCEPTION_T e = 0;
    CEXCEPTION_T e2 = 0;

    Try
    {
        Try
        {
            ExitTry();
            TEST_COUT << "Should Have Exited Try Before This" << '\n';
        }
        Catch(e)
        {
            TEST_COUT << "Should Not Have Been Caught By Inside" << '\n';
        }
        i.store(1);
    }
    Catch(e2)
    {
        TEST_COUT << "Should Not Have Been Caught By Outside" << '\n';
    }

    // verify that we picked up and ran after first Try
    EXPECT_EQ(1, i.load());
}
