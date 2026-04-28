/**
 * @file test_cjsonpp.cpp
 * @brief Unit tests for cjsonpp::JSONObject functionality using Google Test framework.
 *
 * This file contains a series of unit tests for the cjsonpp::JSONObject class,
 * verifying various functionalities such as creating JSON objects of different types,
 * parsing JSON strings, setting and removing items, and serializing/deserializing JSON objects.
 *
 * The tests are organized into a test fixture class JSONObjectTest, which provides setup
 * and teardown methods to initialize and clean up the test environment.
 *
 * Each test case is documented with a brief description and detailed steps of the test.
 *
 * @author Laurent Lardinois and Copilot GPT-4o
 * @date February 2025
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
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

#include <string>
#include <vector>

#include "cJSON/cJSON.h"
#include "cjsonpp/cjsonpp.hpp"


/**
 * @class JSONObjectTest
 * @brief Unit test class for testing cjsonpp::JSONObject functionality.
 *
 * This class uses Google Test framework to perform unit tests on the cjsonpp::JSONObject class.
 * It provides setup and teardown methods to initialize and clean up the test environment.
 *
 */
class JSONObjectTest : public ::testing::Test
{
protected:
    /**
     * @brief Sets up the test environment by creating a new cjsonpp::JSONObject instance.
     */
    void SetUp() override
    {
    }

    /**
     * @brief Cleans up the test environment by resetting the cjsonpp::JSONObject instance.
     */
    void TearDown() override
    {
    }
};

/**
 * @brief Test case for creating an empty JSON object.
 *
 * This test verifies that the type of the created JSON object is cjsonpp::JSONType::Object.
 */
TEST_F(JSONObjectTest, CreateEmptyObject)
{
    auto obj = cjsonpp::JSONObject();
    const auto type = obj.type();
    EXPECT_EQ(type, cjsonpp::JSONType::Object);
}

/**
 * @brief Test case for creating a boolean JSON object.
 *
 * This test verifies that the type of the created JSON object is cjsonpp::JSONType::Bool.
 */
TEST_F(JSONObjectTest, CreateBooleanObject)
{
    {
        auto obj = cjsonpp::JSONObject(true);
        auto type = obj.type();
        EXPECT_EQ(type, cjsonpp::JSONType::Bool);
    }
    {
        auto obj = cjsonpp::JSONObject(false);
        auto type = obj.type();
        EXPECT_EQ(type, cjsonpp::JSONType::Bool);
    }
}

/**
 * @brief Test case for creating a JSON number object.
 *
 * This test verifies that a JSON object created with an integer or a floating-point number
 * is correctly identified as a number type.
 *
 * @test
 * - Create a JSON object with an integer value (42) and check its type.
 * - Create a JSON object with a floating-point value (3.14) and check its type.
 */
TEST_F(JSONObjectTest, CreateNumberObject)
{
    {
        auto obj = cjsonpp::JSONObject(42);
        EXPECT_EQ(obj.type(), cjsonpp::JSONType::Number);
    }

    {
        auto obj = cjsonpp::JSONObject(3.14);
        EXPECT_EQ(obj.type(), cjsonpp::JSONType::Number);
    }
}

/**
 * @brief Test case for creating a string JSON object.
 *
 * This test verifies that the type of the created JSON object is cjsonpp::JSONType::String.
 */
TEST_F(JSONObjectTest, CreateStringObject)
{
    auto obj = cjsonpp::JSONObject("Hello, World!");
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::String);
}

/**
 * @brief Test case for parsing a JSON string.
 *
 * This test verifies that a JSON object can be correctly parsed from a JSON string.
 * It checks that the parsed object has the pco::expected keys and values.
 *
 * @test
 * - Parse a JSON string and check the type of the resulting object.
 * - Verify that the parsed object contains the pco::expected keys.
 */
TEST_F(JSONObjectTest, ParseJSONString)
{
    std::string jsonString = R"({"key": "value", "number": 42})";
    const auto parse_res = cjsonpp::parse_result(jsonString);
    ASSERT_TRUE(parse_res.has_value());
    const auto& obj = parse_res.value();
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::Object);
    EXPECT_TRUE(obj.has("key"));
    EXPECT_TRUE(obj.has("number"));
}

/**
 * @brief Test case for parsing a valid JSON string using the result API.
 */
TEST_F(JSONObjectTest, ParseJSONStringResultApi)
{
    std::string jsonString = R"({"key": "value", "number": 42})";
    auto parse_result = cjsonpp::parse_result(jsonString);
    ASSERT_TRUE(parse_result.has_value());
    const auto& obj = parse_result.value();
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::Object);
    EXPECT_TRUE(obj.has("key"));
    EXPECT_TRUE(obj.has("number"));
}

/**
 * @brief Test case for parsing an invalid JSON string using the result API.
 */
TEST_F(JSONObjectTest, ParseInvalidJSONStringResultApi)
{
    std::string invalid_json = R"({"key": "value", "number": 42)";
    auto parse_result = cjsonpp::parse_result(invalid_json);
    ASSERT_FALSE(parse_result.has_value());
    EXPECT_EQ(parse_result.error().code, cjsonpp::result_code::parse_error);
}

/**
 * @brief Test case for setting and removing an object item in JSONObject.
 *
 * This test case verifies the functionality of setting an object item with a key
 * and then removing it. It ensures that the item is correctly added and then removed
 * from the JSONObject.
 *
 * @test
 * - Create a JSONObject with a key "test".
 * - Set the JSONObject with key "key".
 * - Check if the key "key" exists in the JSONObject.
 * - Remove the key "key" from the JSONObject.
 * - Verify that the key "key" no longer exists in the JSONObject.
 */
TEST_F(JSONObjectTest, SetAndRemoveObjectItem)
{
    auto obj = cjsonpp::JSONObject();
    cjsonpp::JSONObject value("test");
    ASSERT_TRUE(obj.set("key", value));
    EXPECT_TRUE(obj.has("key"));
    ASSERT_TRUE(obj.remove("key"));
    EXPECT_FALSE(obj.has("key"));
}

/**
 * @brief Test case for adding an array item in JSONObject.
 *
 * This test case verifies the functionality of adding an array item
 * It ensures that the item is correctly added to the JSONObject.
 *
 * @test
 * - Create a JSONObject as an array.
 * - Add an item in the array.
 * - Check if the JSONObject is of array type.
 * - Get the item from the array and verify its type.
 */
TEST_F(JSONObjectTest, AddArrayItem)
{
    auto obj = cjsonpp::arrayObject();
    cjsonpp::JSONObject value("test");
    ASSERT_TRUE(obj.add(value));
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::Array);
    const auto item_res = obj.get(0);
    ASSERT_TRUE(item_res.has_value());
    EXPECT_EQ(item_res.value().type(), cjsonpp::JSONType::String);
}

/**
 * @brief Test case for serializing and deserializing a boolean value in a JSONObject.
 *
 * This test creates a JSONObject with a boolean value, serializes it to a string,
 * deserializes it back to a JSONObject, and then checks if the type is correctly
 * identified as a boolean and the value is true.
 *
 * @test
 * - Create a JSONObject with a boolean value (true).
 * - Serialize the JSONObject to a string.
 * - Deserialize the string back to a JSONObject.
 * - Verify that the type of the deserialized JSONObject is boolean.
 * - Verify that the value of the deserialized JSONObject is true.
 */
TEST_F(JSONObjectTest, SerializeAndDeserializeBoolean)
{
    auto obj = cjsonpp::JSONObject(true);
    std::string serialized = obj.print();
    const auto parse_res = cjsonpp::parse_result(serialized);
    ASSERT_TRUE(parse_res.has_value());
    const auto& de_obj = parse_res.value();
    EXPECT_EQ(de_obj.type(), cjsonpp::JSONType::Bool);
    EXPECT_TRUE(de_obj.obj()->valueint);
}

/**
 * @brief Test case for serializing and deserializing a number in a JSONObject.
 *
 * This test creates a JSONObject with a number value, serializes it to a string,
 * deserializes it back to a JSONObject, and then checks if the type is correctly
 * identified as a number and the value is 123.456.
 *
 * @test
 * - Create a JSONObject with a number value (123.456).
 * - Serialize the JSONObject to a string.
 * - Deserialize the string back to a JSONObject.
 * - Verify that the type of the deserialized JSONObject is number.
 * - Verify that the value of the deserialized JSONObject is 123.456.
 */
TEST_F(JSONObjectTest, SerializeAndDeserializeNumber)
{
    auto obj = cjsonpp::JSONObject(123.456);
    std::string serialized = obj.print();
    const auto parse_res = cjsonpp::parse_result(serialized);
    ASSERT_TRUE(parse_res.has_value());
    const auto& de_obj = parse_res.value();
    EXPECT_EQ(de_obj.type(), cjsonpp::JSONType::Number);
    EXPECT_DOUBLE_EQ(de_obj.obj()->valuedouble, 123.456);
}

/**
 * @brief Test case for serializing and deserializing a JSON string.
 *
 * This test creates a JSON object with a string value, serializes it to a string,
 * deserializes it back to a JSON object, and verifies that the type and value
 * of the JSON object are correct.
 *
 * @test
 * - Create a JSON object with the string "Test String".
 * - Serialize the JSON object to a string.
 * - Deserialize the string back to a JSON object.
 * - Verify that the type of the JSON object is cjsonpp::JSONType::String.
 * - Verify that the value of the JSON object is "Test String".
 */
TEST_F(JSONObjectTest, SerializeAndDeserializeString)
{
    auto obj = cjsonpp::JSONObject("Test String");
    std::string serialized = obj.print();
    const auto parse_res = cjsonpp::parse_result(serialized);
    ASSERT_TRUE(parse_res.has_value());
    const auto& de_obj = parse_res.value();
    EXPECT_EQ(de_obj.type(), cjsonpp::JSONType::String);
    EXPECT_STREQ(de_obj.obj()->valuestring, "Test String");
}

/**
 * @brief Test case for creating and checking a null JSON object.
 *
 * This test case creates a JSON object using the cjsonpp::nullObject() method
 * and verifies that the type of the created object is cjsonpp::JSONType::Null.
 */
TEST_F(JSONObjectTest, CreateAndCheckNullObject)
{
    auto obj = cjsonpp::nullObject();
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::Null);
}

/**
 * @brief Test case for creating and checking an array JSON object.
 *
 * This test verifies that the type of the created JSON object is cjsonpp::JSONType::Array.
 */
TEST_F(JSONObjectTest, CreateAndCheckArrayObject)
{
    auto obj = cjsonpp::arrayObject();
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::Array);
}

/**
 * @brief Test case for parsing and checking nested JSON objects.
 *
 * This test verifies that a nested JSON string is correctly parsed into a
 * cjsonpp::JSONObject and that the nested structure can be accessed and
 * validated. Specifically, it checks:
 * - The top-level object is of type cjsonpp::JSONType::Object.
 * - The top-level object contains a key "outer".
 * - The "outer" object contains a key "inner".
 * - The "inner" object contains a key "key".
 * - The value associated with "key" is of type cjsonpp::JSONType::String.
 * - The value associated with "key" is "value".
 */
TEST_F(JSONObjectTest, ParseAndCheckNestedJSON)
{
    std::string jsonString = R"({"outer": {"inner": {"key": "value"}}})";
    const auto parse_res = cjsonpp::parse_result(jsonString);
    ASSERT_TRUE(parse_res.has_value());
    const auto& obj = parse_res.value();
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::Object);
    EXPECT_TRUE(obj.has("outer"));
    const auto outer_res = obj.get("outer");
    ASSERT_TRUE(outer_res.has_value());
    const auto& outer = outer_res.value();
    EXPECT_TRUE(outer.has("inner"));
    const auto inner_res = outer.get("inner");
    ASSERT_TRUE(inner_res.has_value());
    const auto& inner = inner_res.value();
    EXPECT_TRUE(inner.has("key"));
    const auto key_res = inner.get("key");
    ASSERT_TRUE(key_res.has_value());
    EXPECT_EQ(key_res.value().type(), cjsonpp::JSONType::String);
    EXPECT_STREQ(key_res.value().obj()->valuestring, "value");
}

/**
 * @brief Test case for get result behavior on missing keys.
 */
TEST_F(JSONObjectTest, TryGetMissingItemResultApi)
{
    auto obj = cjsonpp::JSONObject();
    auto key_result = obj.get<int>("missing");
    ASSERT_FALSE(key_result.has_value());
    EXPECT_EQ(key_result.error().code, cjsonpp::result_code::missing_item);
}

/**
 * @brief Test case for parsing a JSON string and checking the array in the JSON object.
 *
 * This test verifies that a JSON string containing an array is correctly parsed into a cjsonpp::JSONObject.
 * It checks that the object type is correct, the array exists, and the array elements are of the pco::expected type and
 * value.
 *
 * @test
 * - Parses a JSON string with an array.
 * - Checks that the parsed object is of type cjsonpp::JSONType::Object.
 * - Verifies the existence of the "array" key in the JSON object.
 * - Retrieves the array and checks its type and size.
 * - Iterates through the array and verifies each element's type and value.
 */
TEST_F(JSONObjectTest, ParseAndCheckArrayInJSON)
{
    std::string jsonString = R"({"array": [1, 2, 3, 4, 5]})";
    const auto parse_res = cjsonpp::parse_result(jsonString);
    ASSERT_TRUE(parse_res.has_value());
    const auto& obj = parse_res.value();
    EXPECT_EQ(obj.type(), cjsonpp::JSONType::Object);
    EXPECT_TRUE(obj.has("array"));
    const auto array_res = obj.get("array");
    ASSERT_TRUE(array_res.has_value());
    const auto& array = array_res.value();
    EXPECT_EQ(array.type(), cjsonpp::JSONType::Array);

    constexpr int expected_size = 5;
    EXPECT_EQ(cJSON_GetArraySize(array.obj()), expected_size);
    for (int idx = 0; idx < expected_size; ++idx)
    {
        const auto item_res = array.get<int>(idx);
        ASSERT_TRUE(item_res.has_value());
        EXPECT_EQ(item_res.value(), idx + 1);
    }
}

/**
 * @brief Test case for setting and getting a nested JSON object.
 *
 * This test verifies that a nested JSON object can be correctly set and retrieved.
 * It checks that the outer object contains the inner object and that the inner object
 * has the pco::expected key and value.
 *
 * @test
 * - Create an inner JSON object with a string value.
 * - Create an outer JSON object and set the inner object as a value.
 * - Set the outer object in the main JSON object.
 * - Verify that the main JSON object contains the outer object.
 * - Retrieve the outer object and verify it contains the inner object.
 * - Retrieve the inner object and verify its type and value.
 */
TEST_F(JSONObjectTest, SetAndGetNestedObject)
{
    cjsonpp::JSONObject inner("inner_value");
    auto outer = cjsonpp::JSONObject();
    ASSERT_TRUE(outer.set("inner_key", inner));
    auto obj = cjsonpp::JSONObject();
    ASSERT_TRUE(obj.set("outer_key", outer));
    EXPECT_TRUE(obj.has("outer_key"));
    const auto outer_res = obj.get("outer_key");
    ASSERT_TRUE(outer_res.has_value());
    const auto& retrieved_outer = outer_res.value();
    EXPECT_TRUE(retrieved_outer.has("inner_key"));
    const auto inner_res = retrieved_outer.get("inner_key");
    ASSERT_TRUE(inner_res.has_value());
    EXPECT_EQ(inner_res.value().type(), cjsonpp::JSONType::String);
    EXPECT_STREQ(inner_res.value().obj()->valuestring, "inner_value");
}

/**
 * @brief Test case for serializing and deserializing a nested JSON object.
 *
 * This test creates a nested JSON object, serializes it to a string,
 * deserializes it back to a JSON object, and verifies that the nested structure
 * is correctly preserved.
 *
 * @test
 * - Create an inner JSON object with a string value.
 * - Create an outer JSON object and set the inner object as a value.
 * - Set the outer object in the main JSON object.
 * - Serialize the main JSON object to a string.
 * - Deserialize the string back to a JSON object.
 * - Verify that the main JSON object contains the outer object.
 * - Retrieve the outer object and verify it contains the inner object.
 * - Retrieve the inner object and verify its type and value.
 */
TEST_F(JSONObjectTest, SerializeAndDeserializeNestedObject)
{
    cjsonpp::JSONObject inner("inner_value");
    auto outer = cjsonpp::JSONObject();
    ASSERT_TRUE(outer.set("inner_key", inner));
    auto obj = cjsonpp::JSONObject();
    ASSERT_TRUE(obj.set("outer_key", outer));
    std::string serialized = obj.print();
    const auto parse_res = cjsonpp::parse_result(serialized);
    ASSERT_TRUE(parse_res.has_value());
    const auto& de_obj = parse_res.value();
    EXPECT_TRUE(de_obj.has("outer_key"));
    const auto outer_res = de_obj.get("outer_key");
    ASSERT_TRUE(outer_res.has_value());
    const auto& retrieved_outer = outer_res.value();
    EXPECT_TRUE(retrieved_outer.has("inner_key"));
    const auto inner_res = retrieved_outer.get("inner_key");
    ASSERT_TRUE(inner_res.has_value());
    EXPECT_EQ(inner_res.value().type(), cjsonpp::JSONType::String);
    EXPECT_STREQ(inner_res.value().obj()->valuestring, "inner_value");
}

/**
 * @brief Test case for removing a non-existent object item.
 *
 * This test verifies that attempting to remove a non-existent item from a JSONObject
 * throws an exception with the message "No such item".
 *
 * @test
 * - Attempt to remove a non-existent item from the JSONObject.
 * - Verify that an exception is thrown with the message "No such item".
 */
TEST_F(JSONObjectTest, RemoveNonExistentObjectItem)
{
    auto obj = cjsonpp::JSONObject();
    const auto remove_status = obj.remove("nonexistent");
    ASSERT_FALSE(remove_status.has_value());
    EXPECT_EQ(remove_status.error().code, cjsonpp::result_code::missing_item);
}

/**
 * @brief Test case for removing a non-existent array item from a JSONObject.
 *
 * This test case verifies that attempting to remove an item from an empty
 * JSONObject array throws an exception with the pco::expected error message.
 *
 * @throws std::exception if the item does not exist in the array.
 */
TEST_F(JSONObjectTest, RemoveNonExistentArrayItem)
{
    auto obj = cjsonpp::arrayObject();
    const auto remove_status = obj.remove(0);
    ASSERT_FALSE(remove_status.has_value());
    EXPECT_EQ(remove_status.error().code, cjsonpp::result_code::missing_item);
}

/**
 * @brief Test case for setting an invalid type in a JSONObject.
 *
 * This test verifies that attempting to set a value of an invalid type
 * (in this case, a JSONObject with an array type) in a JSONObject throws
 * an exception with the appropriate error message.
 *
 * @details
 * - Creates a JSONObject with an array type.
 * - Attempts to set a key-value pair in the JSONObject where the value is another JSONObject.
 * - Expects an exception to be thrown with the message "Not an object type".
 *
 * @throws std::exception if the value type is invalid for the JSONObject.
 */
TEST_F(JSONObjectTest, SetInvalidTypeInObject)
{
    auto obj = cjsonpp::arrayObject();
    cjsonpp::JSONObject value("test");
    const auto set_status = obj.set("key", value);
    ASSERT_FALSE(set_status.has_value());
    EXPECT_EQ(set_status.error().code, cjsonpp::result_code::invalid_type);
}

/**
 * @brief Test case for setting an invalid type in a JSONObject array.
 *
 * This test verifies that attempting to set a value of an invalid type
 * (in this case, a JSONObject with an object type) in a JSONObject array
 * throws an exception with the appropriate error message.
 *
 * @details
 * - Creates a JSONObject with an object type.
 * - Attempts to set an index-value pair in the JSONObject where the value is another JSONObject.
 * - Expects an exception to be thrown with the message "Not an array type".
 *
 * @throws std::exception if the value type is invalid for the JSONObject array.
 */
TEST_F(JSONObjectTest, SetInvalidTypeInArray)
{
    auto obj = cjsonpp::JSONObject();
    cjsonpp::JSONObject value("test");
    const auto add_status = obj.add(value);
    ASSERT_FALSE(add_status.has_value());
    EXPECT_EQ(add_status.error().code, cjsonpp::result_code::invalid_type);
}
