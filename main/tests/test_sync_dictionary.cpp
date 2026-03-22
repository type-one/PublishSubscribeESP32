/**
 * @file test_sync_dictionary.cpp
 * @brief Unit tests for the tools::sync_dictionary class template.
 *
 * This file contains a series of unit tests for the tools::sync_dictionary class template.
 * The tests cover various functionalities including adding, finding, removing, and clearing
 * elements in the dictionary with both integer and string keys. Additionally, the tests
 * verify the thread-safety of the dictionary by performing concurrent operations.
 *
 * The tests are implemented using the Google Test framework.
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

#include <atomic>
#include <chrono>
#include <complex>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
#include <ranges>
#endif

#include "tools/sync_dictionary.hpp"


/**
 * @class SyncDictionaryTest
 * @brief Test fixture for testing the tools::sync_dictionary class template.
 *
 * @tparam T The type of the values stored in the dictionary.
 *
 * This class template provides a test fixture for testing the
 * tools::sync_dictionary class template with different key types.
 * It sets up and tears down unique pointers to dictionaries with integer
 * and string keys.
 *
 * @note This class inherits from ::testing::Test to integrate with the
 * Google Test framework.
 */
template <typename T>
class SyncDictionaryTest : public ::testing::Test
{
protected:
    std::unique_ptr<tools::sync_dictionary<int, T>> dict_int_key;
    std::unique_ptr<tools::sync_dictionary<std::string, T>> dict_string_key;

    void SetUp() override
    {
        dict_int_key = std::make_unique<tools::sync_dictionary<int, T>>();
        dict_string_key = std::make_unique<tools::sync_dictionary<std::string, T>>();
    }

    void TearDown() override
    {
        dict_int_key.reset();
        dict_string_key.reset();
    }
};

using MyTypes = ::testing::Types<int, float, double, char, std::complex<double>>;
TYPED_TEST_SUITE(SyncDictionaryTest, MyTypes);

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
template <typename Dict, typename KeyArg, typename ValueArg>
concept has_sync_dict_add_call = requires(Dict& dict_ref, KeyArg&& key_arg, ValueArg&& value_arg)
{
    dict_ref.add(std::forward<KeyArg>(key_arg), std::forward<ValueArg>(value_arg));
};

template <typename Dict, typename Collection>
concept has_sync_dict_add_range_call = requires(Dict& dict_ref, Collection&& collection)
{
    dict_ref.add_range(std::forward<Collection>(collection));
};

template <typename Dict, typename Collection>
concept has_sync_dict_add_collection_call = requires(Dict& dict_ref, Collection&& collection)
{
    dict_ref.add_collection(std::forward<Collection>(collection));
};

template <typename Dict, typename Collection>
concept has_sync_dict_remove_collection_call = requires(Dict& dict_ref, Collection&& collection)
{
    dict_ref.remove_collection(std::forward<Collection>(collection));
};
#endif

/**
 * @brief Test case for adding and finding integer keys in the synchronized dictionary.
 *
 * This test verifies that integer keys can be added to the dictionary and subsequently found.
 * It checks the following:
 * - Adding a key-value pair with integer keys.
 * - Finding an existing key returns the correct value.
 * - Finding a non-existing key returns no value.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, AddAndFindIntKey)
{
    this->dict_int_key->add(1, static_cast<TypeParam>(1));
    this->dict_int_key->add(2, static_cast<TypeParam>(2));

    EXPECT_TRUE(this->dict_int_key->find(1).has_value());
    EXPECT_EQ(this->dict_int_key->find(1).value(), static_cast<TypeParam>(1));
    EXPECT_TRUE(this->dict_int_key->find(2).has_value());
    EXPECT_EQ(this->dict_int_key->find(2).value(), static_cast<TypeParam>(2));
    EXPECT_FALSE(this->dict_int_key->find(3).has_value());
}

/**
 * @brief Test case for adding and finding elements with string keys in SyncDictionary.
 *
 * This test verifies that elements can be added to the dictionary with string keys
 * and subsequently found. It checks the following:
 * - Adding an element with key "one" and value 1.
 * - Adding an element with key "two" and value 2.
 * - Finding the element with key "one" and verifying its value is 1.
 * - Finding the element with key "two" and verifying its value is 2.
 * - Ensuring that an element with key "three" does not exist in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, AddAndFindStringKey)
{
    this->dict_string_key->add("one", static_cast<TypeParam>(1));
    this->dict_string_key->add("two", static_cast<TypeParam>(2));

    EXPECT_TRUE(this->dict_string_key->find("one").has_value());
    EXPECT_EQ(this->dict_string_key->find("one").value(), static_cast<TypeParam>(1));
    EXPECT_TRUE(this->dict_string_key->find("two").has_value());
    EXPECT_EQ(this->dict_string_key->find("two").value(), static_cast<TypeParam>(2));
    EXPECT_FALSE(this->dict_string_key->find("three").has_value());
}

/**
 * @brief Test case for removing an integer key from the dictionary.
 *
 * This test adds two key-value pairs to the dictionary with integer keys,
 * removes one of the keys, and then verifies that the removed key is no longer
 * present while the other key still exists.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, RemoveIntKey)
{
    this->dict_int_key->add(1, static_cast<TypeParam>(1));
    this->dict_int_key->add(2, static_cast<TypeParam>(2));
    this->dict_int_key->remove(1);

    EXPECT_FALSE(this->dict_int_key->find(1).has_value());
    EXPECT_TRUE(this->dict_int_key->find(2).has_value());
}

/**
 * @brief Test case for removing a string key from the dictionary.
 *
 * This test adds two key-value pairs to the dictionary with string keys,
 * removes one of the keys, and then verifies that the removed key is no
 * longer present while the other key still exists.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, RemoveStringKey)
{
    this->dict_string_key->add("one", static_cast<TypeParam>(1));
    this->dict_string_key->add("two", static_cast<TypeParam>(2));
    this->dict_string_key->remove("one");

    EXPECT_FALSE(this->dict_string_key->find("one").has_value());
    EXPECT_TRUE(this->dict_string_key->find("two").has_value());
}

/**
 * @brief Test case for adding a collection with integer keys to the SyncDictionary.
 *
 * This test verifies that a collection of key-value pairs with integer keys can be added
 * to the SyncDictionary and that the values can be correctly retrieved using the keys.
 *
 * @tparam TypeParam The type of the values in the SyncDictionary.
 *
 * The test performs the following checks:
 * - Adds a collection of key-value pairs to the SyncDictionary.
 * - Verifies that the key 1 exists in the dictionary and its associated value is correct.
 * - Verifies that the key 2 exists in the dictionary and its associated value is correct.
 */
TYPED_TEST(SyncDictionaryTest, AddCollectionIntKey)
{
    std::map<int, TypeParam> collection = { { 1, static_cast<TypeParam>(1) }, { 2, static_cast<TypeParam>(2) } };
    this->dict_int_key->add_collection(collection);

    EXPECT_TRUE(this->dict_int_key->find(1).has_value());
    EXPECT_EQ(this->dict_int_key->find(1).value(), static_cast<TypeParam>(1));
    EXPECT_TRUE(this->dict_int_key->find(2).has_value());
    EXPECT_EQ(this->dict_int_key->find(2).value(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for adding a collection with string keys to the dictionary.
 *
 * This test verifies that a collection of key-value pairs with string keys can be added to the dictionary.
 * It checks that the keys "one" and "two" are present in the dictionary and that their corresponding values
 * are correctly stored.
 *
 * @tparam TypeParam The type of the values in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, AddCollectionStringKey)
{
    std::map<std::string, TypeParam> collection
        = { { "one", static_cast<TypeParam>(1) }, { "two", static_cast<TypeParam>(2) } };
    this->dict_string_key->add_collection(collection);

    EXPECT_TRUE(this->dict_string_key->find("one").has_value());
    EXPECT_EQ(this->dict_string_key->find("one").value(), static_cast<TypeParam>(1));
    EXPECT_TRUE(this->dict_string_key->find("two").has_value());
    EXPECT_EQ(this->dict_string_key->find("two").value(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for verifying the collection retrieval with integer keys.
 *
 * This test adds two key-value pairs to the dictionary with integer keys and
 * verifies that the collection retrieved from the dictionary contains the
 * correct number of elements and the correct key-value pairs.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, GetCollectionIntKey)
{
    this->dict_int_key->add(1, static_cast<TypeParam>(1));
    this->dict_int_key->add(2, static_cast<TypeParam>(2));

    auto collection = this->dict_int_key->snapshot();
    EXPECT_EQ(collection.size(), 2);
    EXPECT_EQ(collection[1], static_cast<TypeParam>(1));
    EXPECT_EQ(collection[2], static_cast<TypeParam>(2));
}

/**
 * @brief Test case for verifying the retrieval of a collection with string keys from the SyncDictionary.
 *
 * This test adds two key-value pairs to the dictionary with string keys and verifies that the collection
 * retrieved from the dictionary contains the correct number of elements and the correct key-value pairs.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 *
 * @test
 * - Adds the key-value pair ("one", 1) to the dictionary.
 * - Adds the key-value pair ("two", 2) to the dictionary.
 * - Retrieves the collection from the dictionary.
 * - Checks that the size of the collection is 2.
 * - Checks that the value associated with the key "one" is 1.
 * - Checks that the value associated with the key "two" is 2.
 */
TYPED_TEST(SyncDictionaryTest, GetCollectionStringKey)
{
    this->dict_string_key->add("one", static_cast<TypeParam>(1));
    this->dict_string_key->add("two", static_cast<TypeParam>(2));

    auto collection = this->dict_string_key->snapshot();
    EXPECT_EQ(collection.size(), 2);
    EXPECT_EQ(collection["one"], static_cast<TypeParam>(1));
    EXPECT_EQ(collection["two"], static_cast<TypeParam>(2));
}

/**
 * @brief Test case for checking the empty state and size of the dictionary with integer keys.
 *
 * This test verifies the following:
 * - The dictionary is initially empty.
 * - The size of the dictionary is initially 0.
 * - After adding an element with an integer key, the dictionary is no longer empty.
 * - The size of the dictionary is updated to 1 after adding an element.
 */
TYPED_TEST(SyncDictionaryTest, EmptyAndSizeIntKey)
{
    EXPECT_TRUE(this->dict_int_key->empty());
    EXPECT_EQ(this->dict_int_key->size(), 0);

    this->dict_int_key->add(1, static_cast<TypeParam>(1));
    EXPECT_FALSE(this->dict_int_key->empty());
    EXPECT_EQ(this->dict_int_key->size(), 1);
}

/**
 * @brief Test case to verify the behavior of the SyncDictionary with string keys when it is empty and after adding an
 * element.
 *
 * This test checks the following:
 * - The dictionary is initially empty.
 * - The size of the dictionary is initially 0.
 * - After adding an element with a string key, the dictionary is no longer empty.
 * - The size of the dictionary is 1 after adding the element.
 */
TYPED_TEST(SyncDictionaryTest, EmptyAndSizeStringKey)
{
    EXPECT_TRUE(this->dict_string_key->empty());
    EXPECT_EQ(this->dict_string_key->size(), 0);

    this->dict_string_key->add("one", static_cast<TypeParam>(1));
    EXPECT_FALSE(this->dict_string_key->empty());
    EXPECT_EQ(this->dict_string_key->size(), 1);
}

/**
 * @brief Test case for clearing a dictionary with integer keys.
 *
 * This test adds two elements to the dictionary with integer keys,
 * then clears the dictionary and verifies that it is empty and its size is zero.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, ClearIntKey)
{
    this->dict_int_key->add(1, static_cast<TypeParam>(1));
    this->dict_int_key->add(2, static_cast<TypeParam>(2));
    this->dict_int_key->clear();

    EXPECT_TRUE(this->dict_int_key->empty());
    EXPECT_EQ(this->dict_int_key->size(), 0);
}

/**
 * @brief Test case for clearing a dictionary with string keys.
 *
 * This test adds two key-value pairs to the dictionary and then clears it.
 * It verifies that the dictionary is empty and its size is zero after clearing.
 */
TYPED_TEST(SyncDictionaryTest, ClearStringKey)
{
    this->dict_string_key->add("one", static_cast<TypeParam>(1));
    this->dict_string_key->add("two", static_cast<TypeParam>(2));
    this->dict_string_key->clear();

    EXPECT_TRUE(this->dict_string_key->empty());
    EXPECT_EQ(this->dict_string_key->size(), 0);
}

/**
 * @brief Test case for adding a duplicate key to the dictionary with an integer key.
 *
 * This test verifies that when a duplicate key is added to the dictionary,
 * the value associated with the key is updated to the new value.
 *
 * @tparam TypeParam The type of the value stored in the dictionary.
 *
 * @test
 * - Add a key-value pair (1, 1) to the dictionary.
 * - Add another key-value pair (1, 2) with the same key but a different value.
 * - Check that the key (1) exists in the dictionary.
 * - Verify that the value associated with the key (1) is updated to 2.
 */
TYPED_TEST(SyncDictionaryTest, AddDuplicateKeyIntKey)
{
    this->dict_int_key->add(1, static_cast<TypeParam>(1));
    this->dict_int_key->add(1, static_cast<TypeParam>(2));

    EXPECT_TRUE(this->dict_int_key->find(1).has_value());
    EXPECT_EQ(this->dict_int_key->find(1).value(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for adding a duplicate key to the dictionary with string keys.
 *
 * This test verifies that when a duplicate key is added to the dictionary,
 * the value associated with the key is updated to the new value.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 *
 * @test
 * - Adds a key-value pair ("one", 1) to the dictionary.
 * - Adds another key-value pair ("one", 2) with the same key but a different value.
 * - Checks that the key "one" exists in the dictionary.
 * - Checks that the value associated with the key "one" is updated to 2.
 */
TYPED_TEST(SyncDictionaryTest, AddDuplicateKeyStringKey)
{
    this->dict_string_key->add("one", static_cast<TypeParam>(1));
    this->dict_string_key->add("one", static_cast<TypeParam>(2));

    EXPECT_TRUE(this->dict_string_key->find("one").has_value());
    EXPECT_EQ(this->dict_string_key->find("one").value(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for adding an unordered collection with integer keys to the dictionary.
 *
 * This test verifies that an unordered collection of key-value pairs with integer keys
 * can be added to the dictionary and that the values can be correctly retrieved.
 *
 * @tparam TypeParam The type of the values in the dictionary.
 *
 * The test performs the following checks:
 * - Adds a collection of key-value pairs to the dictionary.
 * - Verifies that the dictionary contains the expected keys.
 * - Verifies that the values associated with the keys are correct.
 */
TYPED_TEST(SyncDictionaryTest, AddUnorderedCollectionIntKey)
{
    std::unordered_map<int, TypeParam> collection
        = { { 1, static_cast<TypeParam>(1) }, { 2, static_cast<TypeParam>(2) } };
    this->dict_int_key->add_collection(collection);

    EXPECT_TRUE(this->dict_int_key->find(1).has_value());
    EXPECT_EQ(this->dict_int_key->find(1).value(), static_cast<TypeParam>(1));
    EXPECT_TRUE(this->dict_int_key->find(2).has_value());
    EXPECT_EQ(this->dict_int_key->find(2).value(), static_cast<TypeParam>(2));
}

/**
 * @brief Test case for adding an unordered collection with string keys to the dictionary.
 *
 * This test verifies that an unordered collection of key-value pairs with string keys
 * can be added to the dictionary and that the values can be correctly retrieved.
 *
 * @tparam TypeParam The type of the values in the dictionary.
 *
 * The test performs the following checks:
 * - Adds a collection with keys "one" and "two" and corresponding values.
 * - Checks that the key "one" exists in the dictionary and its value is correct.
 * - Checks that the key "two" exists in the dictionary and its value is correct.
 */
TYPED_TEST(SyncDictionaryTest, AddUnorderedCollectionStringKey)
{
    std::unordered_map<std::string, TypeParam> collection
        = { { "one", static_cast<TypeParam>(1) }, { "two", static_cast<TypeParam>(2) } };
    this->dict_string_key->add_collection(collection);

    EXPECT_TRUE(this->dict_string_key->find("one").has_value());
    EXPECT_EQ(this->dict_string_key->find("one").value(), static_cast<TypeParam>(1));
    EXPECT_TRUE(this->dict_string_key->find("two").has_value());
    EXPECT_EQ(this->dict_string_key->find("two").value(), static_cast<TypeParam>(2));
}


/**
 * @brief Test case for concurrent addition and finding of integer keys in a synchronized dictionary.
 *
 * This test spawns two threads:
 * - The first thread adds integer keys and their corresponding values to the dictionary.
 * - The second thread checks if the keys added by the first thread can be found in the dictionary.
 *
 * The test ensures that the dictionary can handle concurrent operations without data races or inconsistencies.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddAndFindIntKey)
{
    std::atomic<int> count { 0 };

    std::thread t1(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                this->dict_int_key->add(i, static_cast<TypeParam>(i));
                count.fetch_add(1);
            }
        });

    std::thread t2(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                while (count.load() <= i)
                {
                    std::this_thread::yield();
                }
                EXPECT_TRUE(this->dict_int_key->find(i).has_value());
            }
        });

    t1.join();
    t2.join();
}

/**
 * @brief Test case for concurrent addition and removal of integer keys in SyncDictionary.
 *
 * This test spawns two threads: one for adding integer keys and their corresponding values
 * to the dictionary, and another for removing those keys. The test ensures that after both
 * threads have completed their operations, the dictionary is empty.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddAndRemoveIntKey)
{
    std::atomic<int> count { 0 };

    std::thread t1(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                this->dict_int_key->add(i, static_cast<TypeParam>(i));
                count.fetch_add(1);
            }
        });

    std::thread t2(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                while (count.load() <= i)
                {
                    std::this_thread::yield();
                }
                this->dict_int_key->remove(i);
            }
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(this->dict_int_key->empty());
}

/**
 * @brief Test case for concurrent addition and clearing of dictionary with integer keys.
 *
 * This test spawns two threads: one for adding key-value pairs to the dictionary
 * and another for clearing the dictionary. It ensures that the dictionary is empty
 * after both threads have completed their execution.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddAndClearIntKey)
{
    std::atomic<int> count { 0 };

    std::thread t1(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                this->dict_int_key->add(i, static_cast<TypeParam>(i));
                count.fetch_add(1);
            }
        });

    std::thread t2(
        [this, &count]()
        {
            while (count.load() < 100)
            {
                std::this_thread::yield();
            }
            this->dict_int_key->clear();
        });

    t1.join();
    t2.join();

    EXPECT_TRUE(this->dict_int_key->empty());
}

/**
 * @brief Test case for concurrent addition and retrieval of a collection with integer keys.
 *
 * This test spawns two threads:
 * - The first thread adds 100 elements to the dictionary with integer keys.
 * - The second thread retrieves the collection from the dictionary and checks its size.
 *
 * @note This test assumes that the dictionary implementation is thread-safe.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddAndGetCollectionIntKey)
{
    std::atomic<int> count { 0 };

    std::thread t1(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                this->dict_int_key->add(i, static_cast<TypeParam>(i));
                count.fetch_add(1);
            }
        });

    std::thread t2(
        [this, &count]()
        {
            while (count.load() < 100)
            {
                std::this_thread::yield();
            }
            auto collection = this->dict_int_key->snapshot();
            EXPECT_EQ(collection.size(), 100);
        });

    t1.join();
    t2.join();
}

/**
 * @brief Test case for concurrent addition and size check with integer keys.
 *
 * This test spawns two threads:
 * - The first thread adds 100 elements to the dictionary with integer keys.
 * - The second thread checks if the size of the dictionary is 100.
 *
 * @tparam TypeParam The type of the values stored in the dictionary.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddAndSizeIntKey)
{
    std::atomic<int> count { 0 };

    std::thread t1(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                this->dict_int_key->add(i, static_cast<TypeParam>(i));
                count.fetch_add(1);
            }
        });

    std::thread t2(
        [this, &count]()
        {
            while (count.load() < 100)
            {
                std::this_thread::yield();
            }
            EXPECT_EQ(this->dict_int_key->size(), 100);
        });

    t1.join();
    t2.join();
}

/**
 * @brief Test case for concurrent addition and checking if the dictionary is empty with integer keys.
 *
 * This test spawns two threads:
 * - The first thread adds 100 key-value pairs to the dictionary, where keys and values are integers.
 * - The second thread checks if the dictionary is not empty.
 *
 * The test ensures that the dictionary can handle concurrent additions and that it is not empty after additions.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddAndEmptyIntKey)
{
    std::atomic<int> count { 0 };

    std::thread t1(
        [this, &count]()
        {
            for (int i = 0; i < 100; ++i)
            {
                this->dict_int_key->add(i, static_cast<TypeParam>(i));
                count.fetch_add(1);
            }
        });

    std::thread t2(
        [this, &count]()
        {
            while (count.load() < 100)
            {
                std::this_thread::yield();
            }
            EXPECT_FALSE(this->dict_int_key->empty());
        });

    t1.join();
    t2.join();
}

/**
 * @brief Test case for concurrent addition of a collection and finding elements by integer key.
 *
 * This test case verifies that elements can be concurrently added to the dictionary
 * and found by their integer keys without any issues.
 *
 * @tparam TypeParam The type of the elements in the dictionary.
 *
 * The test performs the following steps:
 * 1. Creates a collection of 100 elements with integer keys.
 * 2. Starts a thread to add the collection to the dictionary.
 * 3. Starts another thread to find each element by its key in the dictionary.
 * 4. Joins both threads to ensure they complete before the test ends.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddCollectionAndFindIntKey)
{
    std::atomic<int> count { 0 };

    std::map<int, TypeParam> collection;
    for (int i = 0; i < 100; ++i)
    {
        collection[i] = static_cast<TypeParam>(i);
    }

    std::thread t1(
        [this, &collection, &count]()
        {
            this->dict_int_key->add_collection(collection);
            count.fetch_add(1);
        });

    std::thread t2(
        [this, &count]()
        {
            while (count.load() < 1)
            {
                std::this_thread::yield();
            }

            for (int i = 0; i < 100; ++i)
            {
                EXPECT_TRUE(this->dict_int_key->find(i).has_value());
            }
        });

    t1.join();
    t2.join();
}

/**
 * @brief Test case for concurrently adding an unordered collection and finding elements by integer key.
 *
 * This test case verifies that elements can be concurrently added to a dictionary with integer keys
 * and that all added elements can be found successfully.
 *
 * @tparam TypeParam The type of the values in the dictionary.
 *
 * The test performs the following steps:
 * 1. Creates an unordered map with integer keys and values of type TypeParam.
 * 2. Fills the unordered map with 100 elements, where each key-value pair is (i, static_cast<TypeParam>(i)).
 * 3. Starts a thread to add the unordered map to the dictionary.
 * 4. Starts another thread to check if each key from 0 to 99 can be found in the dictionary.
 * 5. Joins both threads to ensure they complete before the test ends.
 */
TYPED_TEST(SyncDictionaryTest, ConcurrentAddUnorderedCollectionAndFindIntKey)
{
    std::atomic<int> count { 0 };

    std::unordered_map<int, TypeParam> collection;
    for (int i = 0; i < 100; ++i)
    {
        collection[i] = static_cast<TypeParam>(i);
    }

    std::thread t1(
        [this, &collection, &count]()
        {
            this->dict_int_key->add_collection(collection);
            count.fetch_add(1);
        });

    std::thread t2(
        [this, &count]()
        {
            while (count.load() < 1)
            {
                std::this_thread::yield();
            }

            for (int i = 0; i < 100; ++i)
            {
                EXPECT_TRUE(this->dict_int_key->find(i).has_value());
            }
        });

    t1.join();
    t2.join();
}

/**
 * @brief Verifies sync_dictionary add overloads support exact and conversion call paths.
 *
 * This test validates lvalue, rvalue, and conversion-based insertion/update
 * behavior for string key/value dictionaries after perfect-forwarding support.
 */
TEST(SyncDictionaryPerfectForwardingTest, AddSupportsExactAndConversionPaths)
{
    tools::sync_dictionary<std::string, std::string> str_dict;

    std::string key_lvalue = "lvalue-key";
    std::string value_lvalue = "lvalue-value";

    str_dict.add(key_lvalue, value_lvalue);
    str_dict.add(std::string("rvalue-key"), std::string("rvalue-value"));
    str_dict.add("conversion-key", "conversion-value");
    str_dict.add("conversion-key", std::string("conversion-updated"));

    auto lvalue_found = str_dict.find("lvalue-key");
    auto rvalue_found = str_dict.find("rvalue-key");
    auto conversion_found = str_dict.find("conversion-key");

    ASSERT_TRUE(lvalue_found.has_value());
    ASSERT_TRUE(rvalue_found.has_value());
    ASSERT_TRUE(conversion_found.has_value());

    EXPECT_EQ(lvalue_found.value(), "lvalue-value");
    EXPECT_EQ(rvalue_found.value(), "rvalue-value");
    EXPECT_EQ(conversion_found.value(), "conversion-updated");
}

/**
 * @brief Probe type used to observe copy/move behavior for add forwarding paths.
 */
struct sync_dict_forwarding_probe
{
    static int copy_ctor_count;
    static int move_ctor_count;

    int payload = 0;

    sync_dict_forwarding_probe() = default;
    explicit sync_dict_forwarding_probe(int value)
        : payload(value)
    {
    }

    sync_dict_forwarding_probe(const sync_dict_forwarding_probe& other)
        : payload(other.payload)
    {
        ++copy_ctor_count;
    }

    sync_dict_forwarding_probe(sync_dict_forwarding_probe&& other) noexcept
        : payload(other.payload)
    {
        ++move_ctor_count;
    }

    sync_dict_forwarding_probe& operator=(const sync_dict_forwarding_probe&) = default;
    sync_dict_forwarding_probe& operator=(sync_dict_forwarding_probe&&) noexcept = default;

    static void reset()
    {
        copy_ctor_count = 0;
        move_ctor_count = 0;
    }
};

int sync_dict_forwarding_probe::copy_ctor_count = 0;
int sync_dict_forwarding_probe::move_ctor_count = 0;

/**
 * @brief Verifies lvalue and rvalue add paths exercise copy and move construction respectively.
 */
TEST(SyncDictionaryPerfectForwardingTest, AddLvalueAndRvalueTriggerCopyMovePaths)
{
    {
        sync_dict_forwarding_probe::reset();
        tools::sync_dictionary<int, sync_dict_forwarding_probe> probe_dict;

        sync_dict_forwarding_probe probe_lvalue(10);
        probe_dict.add(1, probe_lvalue);

        EXPECT_GE(sync_dict_forwarding_probe::copy_ctor_count, 1);
    }

    {
        sync_dict_forwarding_probe::reset();
        tools::sync_dictionary<int, sync_dict_forwarding_probe> probe_dict;

        probe_dict.add(2, sync_dict_forwarding_probe(20));

        EXPECT_GE(sync_dict_forwarding_probe::move_ctor_count, 1);
    }
}

/**
 * @brief Verifies range-based add_range and contains support.
 */
TEST(SyncDictionaryRangeTest, AddRangeFromRangeAndContains)
{
    tools::sync_dictionary<std::string, std::string> str_dict;

    const std::vector<std::pair<std::string, std::string>> entries = {
        { "alpha", "one" },
        { "beta", "two" }
    };

    str_dict.add_range(entries);
    str_dict.add_range({ { "gamma", "three" }, { "beta", "two-updated" } });

    EXPECT_TRUE(str_dict.contains("alpha"));
    EXPECT_TRUE(str_dict.contains("beta"));
    EXPECT_TRUE(str_dict.contains("gamma"));

    auto beta_value = str_dict.find("beta");
    ASSERT_TRUE(beta_value.has_value());
    EXPECT_EQ(beta_value.value(), "two-updated");
}

/**
 * @brief Verifies add_collection remains covered as a compatibility alias.
 */
TEST(SyncDictionaryRangeTest, AddCollectionCompatibilityCoverage)
{
    tools::sync_dictionary<std::string, std::string> str_dict;

    const std::vector<std::pair<std::string, std::string>> entries = {
        { "legacy-alpha", "one" },
        { "legacy-beta", "two" }
    };

    str_dict.add_collection(entries);
    str_dict.add_collection({ { "legacy-gamma", "three" }, { "legacy-beta", "two-updated" } });

    EXPECT_TRUE(str_dict.contains("legacy-alpha"));
    EXPECT_TRUE(str_dict.contains("legacy-beta"));
    EXPECT_TRUE(str_dict.contains("legacy-gamma"));

    auto beta_value = str_dict.find("legacy-beta");
    ASSERT_TRUE(beta_value.has_value());
    EXPECT_EQ(beta_value.value(), "two-updated");
}

/**
 * @brief Verifies range-based remove_collection support.
 */
TEST(SyncDictionaryRangeTest, RemoveCollectionFromRange)
{
    tools::sync_dictionary<std::string, std::string> str_dict;

    str_dict.add_range({
        { "key-1", "value-1" },
        { "key-2", "value-2" },
        { "key-3", "value-3" }
    });

    const std::vector<std::string> keys_to_remove = { "key-1", "key-3" };
    str_dict.remove_collection(keys_to_remove);
    str_dict.remove_collection({ "missing", "key-2" });

    EXPECT_FALSE(str_dict.contains("key-1"));
    EXPECT_FALSE(str_dict.contains("key-2"));
    EXPECT_FALSE(str_dict.contains("key-3"));
    EXPECT_TRUE(str_dict.empty());
}

#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))
/**
 * @brief Verifies C++20 requires constraints for sync_dictionary add forwarding API.
 *
 * This test checks that valid add argument pairs are accepted while
 * non-constructible key/value argument pairs are rejected.
 */
TEST(SyncDictionaryPerfectForwardingTest, Cpp20RequiresConstraints)
{
    using str_dict_t = tools::sync_dictionary<std::string, std::string>;
    using int_dict_t = tools::sync_dictionary<int, int>;

    static_assert(has_sync_dict_add_call<str_dict_t, const char*, const char*>);
    static_assert(has_sync_dict_add_call<str_dict_t, std::string&&, std::string&&>);
    static_assert(!has_sync_dict_add_call<int_dict_t, const char*, int>);
    static_assert(!has_sync_dict_add_call<int_dict_t, int, const char*>);

    SUCCEED();
}

/**
 * @brief Verifies C++20 range-call constraints for add/remove APIs.
 */
TEST(SyncDictionaryRangeTest, Cpp20RangeConstraints)
{
    using dict_t = tools::sync_dictionary<std::string, std::string>;

    static_assert(has_sync_dict_add_range_call<dict_t, std::vector<std::pair<std::string, std::string>>&>);
    static_assert(has_sync_dict_add_collection_call<dict_t, std::vector<std::pair<std::string, std::string>>&>);
    static_assert(has_sync_dict_remove_collection_call<dict_t, std::vector<std::string>&>);

    static_assert(has_sync_dict_add_range_call<dict_t, std::initializer_list<std::pair<std::string, std::string>>>);
    static_assert(has_sync_dict_add_collection_call<dict_t, std::initializer_list<std::pair<std::string, std::string>>>);
    static_assert(has_sync_dict_remove_collection_call<dict_t, std::initializer_list<std::string>>);

    const auto transformed = std::views::iota(0, 2)
        | std::views::transform([](const int value)
          {
              return std::pair<std::string, std::string>("k" + std::to_string(value), "v");
          });
    static_assert(has_sync_dict_add_range_call<dict_t, decltype(transformed)>);
    static_assert(has_sync_dict_add_collection_call<dict_t, decltype(transformed)>);

    SUCCEED();
}
#endif
