/**
 * @file test_gzip_wrapper.cpp
 * @brief Unit tests for the GzipWrapper class.
 *
 * This file contains a series of unit tests for the GzipWrapper class, which
 * provides functionality for packing and unpacking data using gzip compression.
 * The tests are implemented using the Google Test framework.
 *
 * The tests cover various scenarios including packing and unpacking small, large,
 * empty, random, invalid, corrupted, truncated data, and data
 *
 * @author Laurent Lardinois and Copilot GPT-4o
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

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "tools/gzip_wrapper.hpp"

/**
 * @brief Test fixture for gzip_wrapper tests.
 *
 * This class sets up and tears down the test environment for gzip_wrapper tests.
 */
class GzipWrapperTest : public ::testing::Test
{
protected:
    tools::gzip_wrapper gzip;

    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Teardown code if needed
    }
};

/**
 * @brief Test case for packing and unpacking small data using GzipWrapper.
 *
 * This test verifies that the GzipWrapper can correctly pack and unpack a small
 * vector of uint8_t data. The original data is packed into a compressed format
 * and then unpacked back to its original form. The test asserts that the unpacked
 * data matches the original data.
 *
 * @test PackUnpackSmallData
 * @ingroup GzipWrapperTest
 */
TEST_F(GzipWrapperTest, PackUnpackSmallData)
{
    std::vector<std::uint8_t> original_data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    std::vector<std::uint8_t> packed_data = gzip.pack(original_data);
    std::vector<std::uint8_t> unpacked_data = gzip.unpack(packed_data);

    ASSERT_EQ(original_data, unpacked_data);
}

/**
 * @brief Test case for packing and unpacking empty data using GzipWrapper.
 *
 * This test verifies that the GzipWrapper can correctly handle empty data.
 * The original empty data is packed into a compressed format and then unpacked
 * back to its original form. The test asserts that the unpacked data matches
 * the original empty data.
 *
 * @test PackUnpackEmptyData
 * @ingroup GzipWrapperTest
 */
TEST_F(GzipWrapperTest, PackUnpackEmptyData)
{
    std::vector<std::uint8_t> original_data = {};

    std::vector<std::uint8_t> packed_data = gzip.pack(original_data);
    std::vector<std::uint8_t> unpacked_data = gzip.unpack(packed_data);

    ASSERT_EQ(original_data, unpacked_data);
}

/**
 * @brief Test case for packing and unpacking large data using GzipWrapper.
 *
 * This test verifies that the GzipWrapper can correctly pack and unpack a large
 * vector of uint8_t data. The original data is packed into a compressed format
 * and then unpacked back to its original form. The test asserts that the unpacked
 * data matches the original data.
 *
 * @test PackUnpackLargeData
 * @ingroup GzipWrapperTest
 */
TEST_F(GzipWrapperTest, PackUnpackLargeData)
{
    std::vector<std::uint8_t> original_data(1000, 0xAB); // 1000 bytes of 0xAB

    std::vector<std::uint8_t> packed_data = gzip.pack(original_data);
    std::vector<std::uint8_t> unpacked_data = gzip.unpack(packed_data);

    ASSERT_EQ(original_data, unpacked_data);
}

/**
 * @brief Test case for packing and unpacking random data using GzipWrapper.
 *
 * This test verifies that the GzipWrapper can correctly pack and unpack a vector of random data.
 * It compares the original data with the unpacked data to ensure they are equal.
 *
 * @test This test case creates a vector of random data, packs it using the GzipWrapper,
 *       unpacks it, and then asserts that the original data and the unpacked data are equal.
 */
TEST_F(GzipWrapperTest, PackUnpackRandomData)
{
    std::vector<std::uint8_t> original_data = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };

    std::vector<std::uint8_t> packed_data = gzip.pack(original_data);
    std::vector<std::uint8_t> unpacked_data = gzip.unpack(packed_data);

    ASSERT_EQ(original_data, unpacked_data);
}

/**
 * @brief Test case for unpacking invalid gzip data.
 *
 * This test verifies that the GzipWrapper correctly handles invalid data
 * by returning an empty vector when attempting to unpack it.
 *
 * @test UnpackInvalidData
 * - Given: A vector of invalid gzip data {0x00, 0x01, 0x02, 0x03}.
 * - When: The data is unpacked using the GzipWrapper.
 * - Then: The resulting unpacked data should be an empty vector.
 */
TEST_F(GzipWrapperTest, UnpackInvalidData)
{
    std::vector<std::uint8_t> invalid_data = { 0x00, 0x01, 0x02, 0x03 };

    std::vector<std::uint8_t> unpacked_data = gzip.unpack(invalid_data);

    ASSERT_TRUE(unpacked_data.empty());
}

/**
 * @brief Test case for unpacking corrupted data using GzipWrapper.
 *
 * This test verifies that the GzipWrapper correctly handles corrupted data
 * by returning an empty vector when attempting to unpack it.
 *
 * @details
 * - Original data is packed using the GzipWrapper.
 * - The packed data is intentionally corrupted by modifying a byte.
 * - The corrupted packed data is then unpacked.
 * - The test asserts that the unpacked data is empty, indicating that the
 *   GzipWrapper correctly identified the corruption and failed to unpack it.
 */
TEST_F(GzipWrapperTest, UnpackCorruptedData)
{
    std::vector<std::uint8_t> original_data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    std::vector<std::uint8_t> packed_data = gzip.pack(original_data);

    // Corrupt the packed data
    packed_data[5] = 0xFF;

    std::vector<std::uint8_t> unpacked_data = gzip.unpack(packed_data);

    ASSERT_TRUE(unpacked_data.empty());
}

/**
 * @brief Test case for unpacking truncated gzip data.
 *
 * This test verifies that the GzipWrapper correctly handles the case where
 * the packed data is truncated. It ensures that unpacking truncated data
 * results in an empty vector.
 *
 * @test This test packs a vector of original data, truncates the packed data
 * to half its size, and then attempts to unpack it. The test asserts that
 * the unpacked data is empty.
 */
TEST_F(GzipWrapperTest, UnpackTruncatedData)
{
    std::vector<std::uint8_t> original_data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    std::vector<std::uint8_t> packed_data = gzip.pack(original_data);

    // Truncate the packed data
    packed_data.resize(packed_data.size() / 2);

    std::vector<std::uint8_t> unpacked_data = gzip.unpack(packed_data);

    ASSERT_TRUE(unpacked_data.empty());
}

/**
 * @brief Test case for unpacking data with an invalid CRC.
 *
 * This test verifies that the GzipWrapper correctly handles the case where the CRC
 * of the packed data is corrupted. It expects the unpacked data to be empty when
 * the CRC is invalid.
 *
 * @test This test packs a vector of bytes, corrupts the CRC of the packed data,
 * and then attempts to unpack it. The test asserts that the unpacked data is empty.
 */
TEST_F(GzipWrapperTest, UnpackDataWithInvalidCRC)
{
    std::vector<std::uint8_t> original_data = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    std::vector<std::uint8_t> packed_data = gzip.pack(original_data);

    // Corrupt the CRC
    packed_data[packed_data.size() - 1] = 0xFF;

    std::vector<std::uint8_t> unpacked_data = gzip.unpack(packed_data);

    ASSERT_TRUE(unpacked_data.empty());
}
