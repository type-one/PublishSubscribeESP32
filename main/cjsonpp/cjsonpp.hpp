/**
 * @file cjsonpp.hpp
 * @brief A thin wrapper over cJSON data type providing a JSON object interface.
 *
 * This file contains the definition of the JSONObject class and related functions.
 * The JSONObject class provides a convenient interface for working with JSON data
 * using the cJSON library. It includes methods for creating, parsing, and manipulating
 * JSON objects, arrays, and values.
 *
 * @author Dmitry Pankratov
 * @date 2015
 * MIT license
 * file modified by Laurent Lardinois in January 2025
 */

#pragma once

#ifndef CJSONPP_H
#define CJSONPP_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include <initializer_list>
#include <memory>

#include "cJSON/cJSON.h"
#include "cjsonpp/cjsonpp_result.hpp"

namespace cjsonpp
{

    // JSON type wrapper enum
    enum class JSONType : std::uint8_t
    {
        Bool,
        Null,
        String,
        Number,
        Array,
        Object,
        Raw,
        Invalid
    };

    /**
     * @brief Create a cjsonpp error payload.
     */
    inline result_error make_error(result_code code_value, int detail_value, const char* message_text)
    {
        return result_error { code_value, detail_value, message_text };
    }

    /**
     * @brief JSONObject class is a thin wrapper over cJSON data type
     *
     */
    class JSONObject
    {
        /**
         * @brief A struct that holds a cJSON object and manages its ownership.
         */
        struct Holder // internal cJSON holder with ownership flag
        {
            cJSON* o;
            bool own_;
            Holder(cJSON* obj, bool own);
            ~Holder();

            /**
             * @brief Overloaded arrow operator to access the underlying cJSON object.
             * @return A pointer to the underlying cJSON object.
             */
            cJSON* operator->() const;

            // no copy constructor
            explicit Holder(const Holder&) = delete;

            // no assignment operator
            Holder& operator=(const Holder&) = delete;

            // move constructor
            explicit Holder(Holder&& other) noexcept;

            // move operator
            Holder& operator=(Holder&& other) noexcept;
        };

        using HolderPtr = std::shared_ptr<Holder>;

        using ObjectSet = std::set<JSONObject>;
        using ObjectSetPtr = std::shared_ptr<ObjectSet>;

        // get value (specialized below)
        template <typename T>
        cjsonpp_result<T> as_result(cJSON* obj) const;

        HolderPtr obj_;

        // Track added holders so that they are not destroyed prematurely before this object dies.
        // The idea behind it: when a value is added to the object or array it is stored as a reference
        //   in the cJSON structure and also added to the holder set.
        // Holders are stored in the shared set to make sure JSONObject copies will have it as well.
        // This is only relevant for object and array types.
        // Concurrency is not handled for performance reasons so it's better to avoid sharing JSONObjects
        //   across threads.
        ObjectSetPtr refs_;

        static constexpr const int byte_mask = 0xff;

    public:
        /**
         * @brief Returns a pointer to the cJSON object.
         *
         * @return A pointer to the cJSON object.
         */
        [[nodiscard]] cJSON* obj() const;

        /**
         * @brief This method returns a string representation of the JSON object.
         *
         * @param formatted If true, the JSON string will be formatted with indentation.
         * @return A string representation of the JSON object.
         */
        [[nodiscard]] std::string print(bool formatted) const;

        /**
         * This method returns a string indented representation of the JSON object.
         *
         * @return A indented string containing the JSON representation of the object.
         */
        [[nodiscard]] std::string print() const
        {
            return print(true);
        }

        /**
         * @brief Necessary for holding references in the set.
         *
         * @param other The other JSONObject to compare with.
         * @return True if this JSONObject is less than the other, false otherwise.
         */
        bool operator<(const JSONObject& other) const;

        /**
         * @brief create empty object
         */
        JSONObject();

        /**
         * @brief non-virtual destructor (no subclassing intended)
         */
        ~JSONObject() = default;

        /**
         * @brief wrap existing cJSON object
         *
         * @param obj existing instance of cJSON object
         * @param own ownership flag (true = own, false otherwise)
         */
        JSONObject(cJSON* obj, bool own);

        /**
         * @brief wrap existing cJSON object with parent
         *
         * @param parent read-only reference to parent
         * @param obj existing instance of cJSON object
         * @param own ownership flag (true = own, false otherwise)
         */
        JSONObject(const JSONObject& parent, cJSON* obj, bool own);

        /**
         * @brief create a boolean object
         *
         * @param value boolean value
         */
        explicit JSONObject(bool value);

        /**
         * @brief create double object
         *
         * @param value double precision floating point value
         */
        explicit JSONObject(double value);

        /**
         * @brief create integer object
         *
         * @param value integer value
         */
        explicit JSONObject(int value);

        /**
         * @brief create integer object
         *
         * @param value 64-bit integer value
         */
        explicit JSONObject(std::int64_t value);

        /**
         * @brief create string object
         *
         * @param value a null-terminated C string
         */
        explicit JSONObject(const char* value);

        /**
         * @brief create string object
         *
         * @param value std::string
         */
        explicit JSONObject(const std::string& value);

        /**
         * @brief Constructs a JSONObject (array object) from a container of elements.
         *
         * This constructor initializes a JSONObject using the elements provided in the container.
         * It creates a new cJSON array and adds each element from the container to this array.
         *
         * @tparam T The type of elements in the container.
         * @tparam ContT The type of the container, defaulting to std::vector.
         * @param elems The container of elements to initialize the JSONObject with.
         */
        template <typename T, template <typename X, typename A> class ContT = std::vector>
        explicit JSONObject(const ContT<T, std::allocator<T>>& elems)
            : obj_(new Holder(cJSON_CreateArray(), true))
            , refs_(new ObjectSet)
        {
            for (auto itr = elems.begin(); itr != elems.end(); ++itr)
            {
                add(*itr);
            }
        }

        /**
         * @brief Constructs a JSONObject (array object) from an initializer list.
         *
         * This constructor initializes a JSONObject using an initializer list of elements.
         * It creates a new cJSON array and adds each element from the initializer list to the array.
         *
         * @tparam T The type of elements in the initializer list.
         * @param elems The initializer list of elements to be added to the JSONObject.
         */
        template <typename T>
        JSONObject(const std::initializer_list<T>& elems)
            : obj_(new Holder(cJSON_CreateArray(), true))
            , refs_(new ObjectSet)
        {
            for (auto& itr : elems)
            {
                add(itr);
            }
        }

        /**
         * @brief Constructs a JSONObject (array object) from a container of elements (Qt-style containers).
         *
         * This constructor initializes a JSONObject using the elements from the provided container.
         * It creates a new cJSON array and adds each element from the container to this array.
         *
         * @tparam T The type of elements in the container.
         * @tparam ContT The type of the container, which must be a template class that takes a single type parameter.
         * @param elems The container of elements to initialize the JSONObject with.
         */
        template <typename T, template <typename X> class ContT>
        explicit JSONObject(const ContT<T>& elems)
            : obj_(new Holder(cJSON_CreateArray(), true))
            , refs_(new ObjectSet)
        {
            for (auto itr = elems.begin(); itr != elems.end(); ++itr)
            {
                add(*itr);
            }
        }

        /**
         * @brief Default copy constructor for JSONObject.
         *
         * This constructor creates a new JSONObject as a copy of an existing one.
         *
         * @param other The JSONObject instance to copy from.
         */
        JSONObject(const JSONObject& other) = default;

        /**
         * @brief Assignment operator for JSONObject.
         *
         * This operator allows assigning one JSONObject to another.
         *
         * @param other The JSONObject to be assigned.
         * @return A reference to the assigned JSONObject.
         */
        JSONObject& operator=(const JSONObject& other);

        /**
         * @brief Move constructor for JSONObject.
         *
         * This move constructor allows moving of JSONObject instances.
         */
        JSONObject(JSONObject&& other) noexcept;

        /**
         * @brief Move assignment operator for JSONObject.
         *
         * This operator allows moving of JSONObject instances.
         */
        JSONObject& operator=(JSONObject&& other) noexcept;

        /**
         * @brief Get the type of the JSON object value.
         *
         * @return JSONType The type of the JSON object value.
         */
        [[nodiscard]] JSONType type() const;

        /**
         * @brief Try to cast the current value to T without throwing.
         */
        template <typename T>
        [[nodiscard]]
        cjsonpp_result<T> as() const
        {
            return as_result<T>(obj_->o);
        }

        /**
         * @brief Retrieves an item from an object by name without throwing.
         */
        template <typename T = JSONObject>
        [[nodiscard]] cjsonpp_result<T> get(const char* name) const
        {
            if (((*obj_)->type & byte_mask) != cJSON_Object)
            {
                return tools::unexpected<result_error> { make_error(
                    result_code::invalid_type, (*obj_)->type & byte_mask, "Not an object") };
            }

            cJSON* item = cJSON_GetObjectItem(obj_->o, name);
            if (item == nullptr)
            {
                return tools::unexpected<result_error> { make_error(result_code::missing_item, 0, "No such item") };
            }

            return as_result<T>(item);
        }

        /**
         * @brief Retrieves an item from an object by name without throwing.
         */
        template <typename T = JSONObject>
        [[nodiscard]] cjsonpp_result<T> get(const std::string& value) const
        {
            return get<T>(value.c_str());
        }

        /**
         * @brief Checks if the JSON object contains a member with the specified name.
         *
         * @param name The name of the member to check for.
         * @return true if the member exists, false otherwise.
         */
        bool has(const char* name) const;

        /**
         * @brief Checks if the JSON object contains a field with the given name.
         *
         * @param name The name of the field to check for.
         * @return true if the field exists, false otherwise.
         */
        [[nodiscard]] bool has(const std::string& name) const;

        /**
         * @brief Retrieves an item from an array by index without throwing.
         */
        template <typename T = JSONObject>
        [[nodiscard]]
        cjsonpp_result<T> get(int index) const
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                return tools::unexpected<result_error> { make_error(
                    result_code::invalid_type, (*obj_)->type & byte_mask, "Not an array type") };
            }

            cJSON* item = cJSON_GetArrayItem(obj_->o, index);
            if (item == nullptr)
            {
                return tools::unexpected<result_error> { make_error(result_code::missing_item, 0, "No such item") };
            }

            return as_result<T>(item);
        }

        /**
         * @brief Adds a value to an array without throwing.
         */
        template <typename T>
        cjsonpp_status add(const T& value)
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                return tools::unexpected<result_error> { make_error(
                    result_code::invalid_type, (*obj_)->type & byte_mask, "Not an array type") };
            }
            JSONObject output(value);
            cJSON_AddItemReferenceToArray(obj_->o, output.obj_->o);
            refs_->insert(output);
            return cjsonpp_status {};
        }

        /**
         * @brief Sets a key/value pair in an object without throwing.
         */
        template <typename T>
        cjsonpp_status set(const char* name, const T& value)
        {
            if (((*obj_)->type & byte_mask) != cJSON_Object)
            {
                return tools::unexpected<result_error> { make_error(
                    result_code::invalid_type, (*obj_)->type & byte_mask, "Not an object type") };
            }

            JSONObject output(value);
            cJSON_AddItemReferenceToObject(obj_->o, name, output.obj_->o);
            refs_->insert(output);
            return cjsonpp_status {};
        }

        /**
         * @brief Sets a key/value pair in an object without throwing.
         */
        template <typename T>
        cjsonpp_status set(const std::string& name, const T& value)
        {
            return set(name.c_str(), value);
        }

        /**
         * @brief Removes from object an item with the specified name without throwing.
         */
        cjsonpp_status remove(const char* name);

        /**
         * @brief Removes from object an item with the specified name without throwing.
         */
        cjsonpp_status remove(const std::string& name)
        {
            return remove(name.c_str());
        }
        /**
         * @brief Removes an item from array at the specified index without throwing.
         */
        cjsonpp_status remove(int index);
    };

    /**
     * @brief Parses a JSON string and returns a result instead of throwing.
     */
    cjsonpp_result<JSONObject> parse_result(const char* str);

    /**
     * @brief Parses a JSON string and returns a result instead of throwing.
     */
    cjsonpp_result<JSONObject> parse_result(const std::string& str);

    /**
     * @brief create null object
     *
     * @return JSONObject
     */
    JSONObject nullObject();

    /**
     * @brief create empty array object
     *
     * @return JSONObject
     */
    JSONObject arrayObject();

    /**
     * @brief Specialized getter: specialization of the as template function for int type.
     *
     * This function converts a cJSON object to an integer if the type of the cJSON object is a number.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return The integer value of the cJSON object, or an error payload on type mismatch.
     */
    template <>
    inline cjsonpp_result<int> JSONObject::as_result<int>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            return tools::unexpected<result_error> { make_error(
                result_code::invalid_type, obj->type & byte_mask, "Bad value type") };
        }
        return obj->valueint;
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to an int64_t value.
     *
     * This template specialization of the `as` method converts a cJSON object
     * to an int64_t value. It checks if the cJSON object is of type number.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return The int64_t value of the cJSON object, or an error payload on type mismatch.
     */
    template <>
    inline cjsonpp_result<std::int64_t> JSONObject::as_result<std::int64_t>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            return tools::unexpected<result_error> { make_error(
                result_code::invalid_type, obj->type & byte_mask, "Not a number type") };
        }
        return static_cast<std::int64_t>(obj->valuedouble);
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to a std::string.
     *
     * This template specialization converts a cJSON object to a std::string.
     * It checks if the cJSON object is of string type.
     *
     * @param obj The cJSON object to be converted.
     * @return The std::string representation of the cJSON object, or an error payload on type mismatch.
     */
    template <>
    inline cjsonpp_result<std::string> JSONObject::as_result<std::string>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_String)
        {
            return tools::unexpected<result_error> { make_error(
                result_code::invalid_type, obj->type & byte_mask, "Not a string type") };
        }
        return cjsonpp_result<std::string> { std::string { obj->valuestring != nullptr ? obj->valuestring : "" } };
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to a double.
     *
     * This template specialization of the `as` method converts a given cJSON object
     * to a double value. It checks if the type of the cJSON object is a number.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return The double value of the cJSON object, or an error payload on type mismatch.
     */
    template <>
    inline cjsonpp_result<double> JSONObject::as_result<double>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            return tools::unexpected<result_error> { make_error(
                result_code::invalid_type, obj->type & byte_mask, "Not a number type") };
        }
        return obj->valuedouble;
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to a boolean value.
     *
     * This template specialization of the as function converts a cJSON object to a boolean value.
     * It checks the type of the cJSON object and returns true if the type is cJSON_True,
     * false if the type is cJSON_False, and returns an error otherwise.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return Boolean value representing the cJSON object, or an error payload on type mismatch.
     */
    template <>
    inline cjsonpp_result<bool> JSONObject::as_result<bool>(cJSON* obj) const
    {
        bool result = false;
        if ((obj->type & byte_mask) == cJSON_True)
        {
            result = true;
        }
        else if ((obj->type & byte_mask) == cJSON_False)
        {
            result = false;
        }
        else
        {
            return tools::unexpected<result_error> { make_error(
                result_code::invalid_type, obj->type & byte_mask, "Not a boolean type") };
        }

        return result;
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to a JSONObject.
     *
     * This template specialization converts a cJSON object pointer to a JSONObject instance.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return JSONObject instance representing the cJSON object.
     */
    template <>
    inline cjsonpp_result<JSONObject> JSONObject::as_result<JSONObject>(cJSON* obj) const
    {
        return cjsonpp_result<JSONObject> { JSONObject { *this, obj, false } };
    }


    /**
     * @brief Converts a JSONObject to an array and outputs each element to the given output iterator.
     *
     * Ex: as_array<JSONObject>(jsonArrayObject, std::back_inserter(vectorToFill));
     *
     * @tparam T The type of the elements in the JSONObject.
     * @tparam TOutputIterator The type of the output iterator.
     * @param data The JSONObject to be converted to an array.
     * @param output The output iterator where the elements of the array will be stored.
     */
    template <class T, class TOutputIterator>
    inline void as_array(const JSONObject& data, TOutputIterator output)
    {
        cJSON* current = cJSON_GetArrayItem(data.obj(), 0);
        while (current)
        {
            *output = JSONObject(current, false);

            ++output;
            current = current->next;
        }
    }

} // namespace cjsonpp

#endif
