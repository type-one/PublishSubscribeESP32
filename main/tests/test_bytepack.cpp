
/**
 * @file test_bytepack.cpp
 * @brief Unit tests for serialization and deserialization using bytepack::binary_stream.
 * 
 * This file contains unit tests for serializing and deserializing various structures 
 * (Address, Person, Company) and a std::map of Company objects using the bytepack::binary_stream.
 * The tests ensure that the data is correctly serialized to a binary stream and deserialized back 
 * to the original structures with the same data.
 * 
 * The tests are implemented using the Google Test framework.
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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bytepack/bytepack.hpp"

/**
 * @brief A structure representing an address with street, city, and zip code.
 *
 * This structure provides methods to serialize and deserialize its data members
 * using a binary stream.
 */

struct Address
{
    std::string street;
    std::string city;
    int zip_code;

    bool serialize(bytepack::binary_stream<>& stream) const
    {
        return stream.write(street, city, zip_code);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        return stream.read(street, city, zip_code);
    }
};

/**
 * @brief A structure representing an person with name, age and address.
 *
 * This structure provides methods to serialize and deserialize its data members
 * using a binary stream.
 */

struct Person
{
    std::string name;
    int age;
    Address address;

    bool serialize(bytepack::binary_stream<>& stream) const
    {
        return stream.write(name, age) && address.serialize(stream);
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        return stream.read(name, age) && address.deserialize(stream);
    }
};

/**
 * @brief A structure representing an company with name and employees.
 *
 * This structure provides methods to serialize and deserialize its data members
 * using a binary stream.
 */

struct Company
{
    std::string name;
    std::vector<Person> employees;

    bool serialize(bytepack::binary_stream<>& stream) const
    {
        if (!stream.write(name, static_cast<std::uint32_t>(employees.size())))
        {
            return false;
        }
        for (const auto& employee : employees)
        {
            if (!employee.serialize(stream))
            {
                return false;
            }
        }
        return true;
    }

    bool deserialize(bytepack::binary_stream<>& stream)
    {
        std::uint32_t employee_count;
        if (!stream.read(name, employee_count))
        {
            return false;
        }
        employees.resize(employee_count);
        for (auto& employee : employees)
        {
            if (!employee.deserialize(stream))
            {
                return false;
            }
        }
        return true;
    }
};

/**
 * @class SerializationTest
 * @brief A test fixture class for testing serialization using bytepack::binary_stream.
 *
 * This class inherits from ::testing::Test and provides setup and teardown
 * functionality for tests that involve serialization.
 *
 */
class SerializationTest : public ::testing::Test
{
protected:
    /**
     * @brief A unique pointer to a bytepack::binary_stream object used for serialization.
     */
    std::unique_ptr<bytepack::binary_stream<>> stream;

    /**
     * @brief Sets up the test environment by initializing the bytepack::binary_stream object.
     */
    void SetUp() override
    {
        stream = std::make_unique<bytepack::binary_stream<>>(256);
    }

    /**
     * @brief Tears down the test environment by resetting the bytepack::binary_stream object.
     */
    void TearDown() override
    {
        stream.reset();
    }
};

/**
 * @brief Test case for serializing and deserializing an Address object.
 *
 * This test verifies that an Address object can be correctly serialized to a stream
 * and then deserialized back to an Address object with the same data.
 *
 * @test
 * - Create an Address object with sample data.
 * - Serialize the Address object to a stream.
 * - Deserialize the Address object from the stream.
 * - Verify that the original and deserialized Address objects have the same data.
 */
TEST_F(SerializationTest, AddressSerialization)
{
    Address original_address { "123 Main St", "Anytown", 12345 };

    ASSERT_TRUE(original_address.serialize(*stream));

    Address deserialized_address;
    stream->reset();
    ASSERT_TRUE(deserialized_address.deserialize(*stream));

    ASSERT_EQ(original_address.street, deserialized_address.street);
    ASSERT_EQ(original_address.city, deserialized_address.city);
    ASSERT_EQ(original_address.zip_code, deserialized_address.zip_code);
}

/**
 * @brief Test case for serializing and deserializing a Person object.
 *
 * This test verifies that a Person object can be correctly serialized to a stream
 * and then deserialized back to a Person object with the same data.
 *
 * @test
 * - Create a Person object with sample data.
 * - Serialize the Person object to a stream.
 * - Deserialize the Person object from the stream.
 * - Verify that the original and deserialized Person objects have the same data.
 */
TEST_F(SerializationTest, PersonSerialization)
{
    Address address { "123 Main St", "Anytown", 12345 };
    Person original_person { "John Doe", 30, address };

    ASSERT_TRUE(original_person.serialize(*stream));

    Person deserialized_person;
    stream->reset();
    ASSERT_TRUE(deserialized_person.deserialize(*stream));

    ASSERT_EQ(original_person.name, deserialized_person.name);
    ASSERT_EQ(original_person.age, deserialized_person.age);
    ASSERT_EQ(original_person.address.street, deserialized_person.address.street);
    ASSERT_EQ(original_person.address.city, deserialized_person.address.city);
    ASSERT_EQ(original_person.address.zip_code, deserialized_person.address.zip_code);
}

/**
 * @brief Test case for serializing and deserializing a Company object.
 *
 * This test verifies that a Company object with multiple employees can be correctly serialized to a stream
 * and then deserialized back to a Company object with the same data.
 *
 * @test
 * - Create a Company object with sample data.
 * - Serialize the Company object to a stream.
 * - Deserialize the Company object from the stream.
 * - Verify that the original and deserialized Company objects have the same data.
 */
TEST_F(SerializationTest, CompanySerialization)
{
    Address address1 { "123 Main St", "Anytown", 12345 };
    Address address2 { "456 Elm St", "Othertown", 67890 };
    Person person1 { "John Doe", 30, address1 };
    Person person2 { "Jane Smith", 25, address2 };
    Company original_company { "Tech Corp", { person1, person2 } };
    stream = std::make_unique<bytepack::binary_stream<>>(512);

    ASSERT_TRUE(original_company.serialize(*stream));

    Company deserialized_company;
    stream->reset();
    ASSERT_TRUE(deserialized_company.deserialize(*stream));

    ASSERT_EQ(original_company.name, deserialized_company.name);
    ASSERT_EQ(original_company.employees.size(), deserialized_company.employees.size());
    for (size_t i = 0; i < original_company.employees.size(); ++i)
    {
        ASSERT_EQ(original_company.employees[i].name, deserialized_company.employees[i].name);
        ASSERT_EQ(original_company.employees[i].age, deserialized_company.employees[i].age);
        ASSERT_EQ(original_company.employees[i].address.street, deserialized_company.employees[i].address.street);
        ASSERT_EQ(original_company.employees[i].address.city, deserialized_company.employees[i].address.city);
        ASSERT_EQ(original_company.employees[i].address.zip_code, deserialized_company.employees[i].address.zip_code);
    }
}

/**
 * @brief Test case for serializing and deserializing an empty company.
 *
 * This test verifies that a company with no employees can be serialized
 * and deserialized correctly. It checks that the company name is preserved
 * and that the employees list remains empty after deserialization.
 */
TEST_F(SerializationTest, EmptyCompanySerialization)
{
    Company original_company { "Empty Corp", {} };

    ASSERT_TRUE(original_company.serialize(*stream));

    Company deserialized_company;
    stream->reset();
    ASSERT_TRUE(deserialized_company.deserialize(*stream));

    ASSERT_EQ(original_company.name, deserialized_company.name);
    ASSERT_TRUE(deserialized_company.employees.empty());
}

/**
 * @brief Test case for serializing and deserializing a large Company object.
 *
 * This test verifies that a Company object with a large number of employees can be correctly serialized to a stream
 * and then deserialized back to a Company object with the same data.
 *
 * @test
 * - Create a Company object with 100 employees.
 * - Serialize the Company object to a stream.
 * - Deserialize the Company object from the stream.
 * - Verify that the original and deserialized Company objects have the same data.
 */
TEST_F(SerializationTest, LargeCompanySerialization)
{
    std::vector<Person> employees;
    for (int i = 0; i < 100; ++i)
    {
        employees.push_back(
            { "Employee " + std::to_string(i), 20 + i, { "Street " + std::to_string(i), "City", 10000 + i } });
    }
    Company original_company { "Large Corp", employees };
    stream = std::make_unique<bytepack::binary_stream<>>(4096);

    ASSERT_TRUE(original_company.serialize(*stream));

    Company deserialized_company;
    stream->reset();
    ASSERT_TRUE(deserialized_company.deserialize(*stream));

    ASSERT_EQ(original_company.name, deserialized_company.name);
    ASSERT_EQ(original_company.employees.size(), deserialized_company.employees.size());
    for (size_t i = 0; i < original_company.employees.size(); ++i)
    {
        ASSERT_EQ(original_company.employees[i].name, deserialized_company.employees[i].name);
        ASSERT_EQ(original_company.employees[i].age, deserialized_company.employees[i].age);
        ASSERT_EQ(original_company.employees[i].address.street, deserialized_company.employees[i].address.street);
        ASSERT_EQ(original_company.employees[i].address.city, deserialized_company.employees[i].address.city);
        ASSERT_EQ(original_company.employees[i].address.zip_code, deserialized_company.employees[i].address.zip_code);
    }
}

/**
 * @brief Test case for serializing and deserializing a std::map of Company objects.
 *
 * This test verifies that a std::map of Company objects can be correctly serialized to a stream
 * and then deserialized back to a std::map of Company objects with the same data.
 *
 * @test
 * - Create a std::map of Company objects with sample data.
 * - Serialize the std::map of Company objects to a stream.
 * - Deserialize the std::map of Company objects from the stream.
 * - Verify that the original and deserialized std::map of Company objects have the same data.
 */
TEST_F(SerializationTest, MapOfCompaniesSerialization)
{
    Address address1 { "123 Main St", "Anytown", 12345 };
    Address address2 { "456 Elm St", "Othertown", 67890 };
    Person person1 { "John Doe", 30, address1 };
    Person person2 { "Jane Smith", 25, address2 };
    Company company1 { "Tech Corp", { person1, person2 } };
    Company company2 { "Biz Inc", { person2, person1 } };

    std::map<std::string, Company> original_map;
    original_map["company1"] = company1;
    original_map["company2"] = company2;

    // Serialize the map
    ASSERT_TRUE(stream->write(static_cast<std::uint32_t>(original_map.size())));
    for (const auto& pair : original_map)
    {
        ASSERT_TRUE(stream->write(pair.first));
        ASSERT_TRUE(pair.second.serialize(*stream));
    }

    // Deserialize the map
    std::map<std::string, Company> deserialized_map;
    std::uint32_t map_size;
    ASSERT_TRUE(stream->read(map_size));
    for (std::uint32_t i = 0; i < map_size; ++i)
    {
        std::string key;
        Company value;
        ASSERT_TRUE(stream->read(key));
        ASSERT_TRUE(value.deserialize(*stream));
        deserialized_map[key] = value;
    }

    // Verify the data
    ASSERT_EQ(original_map.size(), deserialized_map.size());
    for (const auto& pair : original_map)
    {
        const auto& deserialized_pair = deserialized_map.find(pair.first);
        ASSERT_NE(deserialized_pair, deserialized_map.end());
        ASSERT_EQ(pair.second.name, deserialized_pair->second.name);
        ASSERT_EQ(pair.second.employees.size(), deserialized_pair->second.employees.size());
        for (size_t i = 0; i < pair.second.employees.size(); ++i)
        {
            ASSERT_EQ(pair.second.employees[i].name, deserialized_pair->second.employees[i].name);
            ASSERT_EQ(pair.second.employees[i].age, deserialized_pair->second.employees[i].age);
            ASSERT_EQ(pair.second.employees[i].address.street, deserialized_pair->second.employees[i].address.street);
            ASSERT_EQ(pair.second.employees[i].address.city, deserialized_pair->second.employees[i].address.city);
            ASSERT_EQ(
                pair.second.employees[i].address.zip_code, deserialized_pair->second.employees[i].address.zip_code);
        }
    }
}

/**
 * @brief Test case for deserializing an Address object from a bad stream.
 *
 * This test verifies that deserialization fails when the stream contains invalid data.
 *
 * @test
 * - Create a stream with invalid data.
 * - Attempt to deserialize an Address object from the stream.
 * - Verify that deserialization fails.
 */
TEST_F(SerializationTest, AddressDeserializationError)
{
    // Create a stream with invalid data
    std::vector<std::uint8_t> invalid_data = { 0x01, 0x02, 0x03 };
    stream->write(invalid_data.data(), invalid_data.size());
    stream->reset();

    Address deserialized_address;
    ASSERT_FALSE(deserialized_address.deserialize(*stream));
}

/**
 * @brief Test case for deserializing a Person object from a bad stream.
 *
 * This test verifies that deserialization fails when the stream contains invalid data.
 *
 * @test
 * - Create a stream with invalid data.
 * - Attempt to deserialize a Person object from the stream.
 * - Verify that deserialization fails.
 */
TEST_F(SerializationTest, PersonDeserializationError)
{
    // Create a stream with invalid data
    std::vector<std::uint8_t> invalid_data = { 0x01, 0x02, 0x03 };
    stream->write(invalid_data.data(), invalid_data.size());
    stream->reset();

    Person deserialized_person;
    ASSERT_FALSE(deserialized_person.deserialize(*stream));
}

/**
 * @brief Test case for deserializing a Company object from a bad stream.
 *
 * This test verifies that deserialization fails when the stream contains invalid data.
 *
 * @test
 * - Create a stream with invalid data.
 * - Attempt to deserialize a Company object from the stream.
 * - Verify that deserialization fails.
 */
TEST_F(SerializationTest, CompanyDeserializationError)
{
    // Create a stream with invalid data
    std::vector<std::uint8_t> invalid_data = { 0x01, 0x02, 0x03 };
    stream->write(invalid_data.data(), invalid_data.size());
    stream->reset();

    Company deserialized_company;
    ASSERT_FALSE(deserialized_company.deserialize(*stream));
}

/**
 * @brief Test case for deserializing a std::map of Company objects from a bad stream.
 *
 * This test verifies that deserialization fails when the stream contains invalid data.
 *
 * @test
 * - Create a stream with invalid data.
 * - Attempt to deserialize a std::map of Company objects from the stream.
 * - Verify that deserialization fails.
 */
TEST_F(SerializationTest, MapOfCompaniesDeserializationError)
{
    // Create a stream with invalid data
    std::vector<std::uint8_t> invalid_data = { 0x01, 0x02, 0x03 };
    stream->write(invalid_data.data(), invalid_data.size());
    stream->reset();

    std::map<std::string, Company> deserialized_map;
    std::uint32_t map_size;
    ASSERT_FALSE(stream->read(map_size));
}
