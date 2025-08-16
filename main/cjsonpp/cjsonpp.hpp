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

// modified for clang-tidy checks

// clang-format off
#if defined(CJSONPP_NO_EXCEPTION)
#include "tools/logger.hpp"
#include "CException/CException.h"
#define CJSONPP_THROW(msg, value) do { LOG_ERROR("%s (%d)", msg, static_cast<int>(value)); /* NOLINT keep it */ \
                                       Throw(0); } while (false)                           /* NOLINT keep it*/
#else
#include <exception>
#define CJSONPP_THROW(msg, value) throw std::runtime_error(std::string{msg} + " (" + std::to_string(value) + ")") /* NOLINT keep it */
#endif
// clang-format on

namespace cjsonpp
{

    // JSON type wrapper enum
    enum JSONType : std::uint8_t
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

            // no move constructor
            explicit Holder(Holder&&) noexcept = delete;

            // no move operator
            Holder& operator=(Holder&&) noexcept = delete;
        };

        using HolderPtr = std::shared_ptr<Holder>;

        using ObjectSet = std::set<JSONObject>;
        using ObjectSetPtr = std::shared_ptr<ObjectSet>;

        // get value (specialized below)
        template <typename T>
        T as(cJSON* obj) const;

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
         * @brief Deleted move constructor for JSONObject.
         *
         * This move constructor is deleted to prevent moving of JSONObject instances.
         */
        JSONObject(JSONObject&&) noexcept = delete;

        /**
         * @brief Deleted move assignment operator for JSONObject.
         *
         * This operator is deleted to prevent moving of JSONObject instances.
         */
        JSONObject& operator=(JSONObject&&) noexcept = delete;

        /**
         * @brief Get the type of the JSON object value.
         *
         * @return JSONType The type of the JSON object value.
         */
        [[nodiscard]] JSONType type() const;

        /**
         * @brief get and cast value from this object
         *
         * @tparam T type of the value
         * @return T type casted value
         */
        template <typename T>
        T as() const
        {
            return as<T>(obj_->o);
        }

        /**
         * @brief Converts the current JSON object to an array of JSON objects.
         *
         * This function checks if the current JSON object is of array type. If it is,
         * it iterates through the array and converts each item to the specified type T,
         * storing them in a container of type ContT.
         *
         * @tparam T The type to which each item in the array should be converted. Defaults to JSONObject.
         * @tparam ContT The container type to store the converted items. Defaults to std::vector.
         * @return A container of type ContT containing the converted items.
         * @throws std::runtime_error (or CException) if the current JSON object is not of array type.
         */
        template <typename T = JSONObject, template <typename X, typename A> class ContT = std::vector>
        ContT<T, std::allocator<T>> asArray() const
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & byte_mask); // NOLINT keep it
            }

            ContT<T, std::allocator<T>> retval;
            for (int i = 0; i < cJSON_GetArraySize(obj_->o); i++)
            {
                retval.push_back(as<T>(cJSON_GetArrayItem(obj_->o, i)));
            }

            return retval;
        }

        /**
         * @brief Converts the cJSON object to an array of the specified type. (for Qt-style containers)
         *
         * This function checks if the cJSON object is of array type and then
         * converts it to a container of the specified type.
         *
         * @tparam T The type of elements in the array.
         * @tparam ContT The template container type that holds elements of type T.
         * @return ContT<T> A container of type ContT holding elements of type T.
         * @throws std::runtime_error (or CException) if the cJSON object is not an array.
         */
        template <typename T, template <typename X> class ContT>
        ContT<T> asArray() const
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & byte_mask);
            }

            ContT<T> retval;
            for (int i = 0; i < cJSON_GetArraySize(obj_->o); i++)
            {
                retval.push_back(as<T>(cJSON_GetArrayItem(obj_->o, i)));
            }

            return retval;
        }

        /**
         * @brief Retrieves an item from the JSON object by name.
         *
         * This function retrieves an item from the JSON object by its name.
         * If the object is not of type cJSON_Object, an exception is thrown.
         * If the item with the specified name does not exist, an exception is thrown.
         *
         * @tparam T The type to which the item should be cast. Defaults to JSONObject.
         * @param name The name of the item to retrieve.
         * @return T The item cast to the specified type.
         * @throws std::runtime_error (or CException) if the object is not of type cJSON_Object or if the item does not
         * exist.
         */
        template <typename T = JSONObject>
        [[nodiscard]] T get(const char* name) const
        {
            if (((*obj_)->type & byte_mask) != cJSON_Object)
            {
                CJSONPP_THROW("Not an object", (*obj_)->type & byte_mask);
            }

            cJSON* item = cJSON_GetObjectItem(obj_->o, name);
            if (item == nullptr)
            {
                CJSONPP_THROW("No such item", 0);
            }

            return as<T>(item);
        }

        /**
         * @brief Retrieves a JSON object associated with the given key.
         *
         * This function template retrieves a JSON object of type T associated with the specified key.
         *
         * @tparam T The type of JSON object to retrieve. Defaults to JSONObject.
         * @param value The key associated with the JSON object to retrieve.
         * @return JSONObject The JSON object associated with the specified key.
         */
        template <typename T = JSONObject>
        [[nodiscard]] JSONObject get(const std::string& value) const
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
         * @brief Retrieves an item from a JSON array at the specified index.
         *
         * This function checks if the current JSON object is an array and retrieves
         * the item at the given index. If the object is not an array or the index is
         * out of bounds, an exception is thrown.
         *
         * @tparam T The type to which the JSON item should be converted. Defaults to JSONObject.
         * @param index The index of the item to retrieve from the JSON array.
         * @return T The JSON item at the specified index, converted to the specified type.
         * @throws std::runtime_error (or CException) if the current JSON object is not an array or if the index is out
         * of bounds.
         */
        template <typename T = JSONObject>
        T get(int index) const
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & byte_mask);
            }

            cJSON* item = cJSON_GetArrayItem(obj_->o, index);
            if (item == nullptr)
            {
                CJSONPP_THROW("No such item", 0);
            }

            return as<T>(item);
        }

        /**
         * @brief Adds a value to the JSON array.
         *
         * This function adds a value to the JSON array. It checks if the current object is of array type,
         * and if so, it creates a JSONObject from the given value and adds it to the array.
         *
         * @tparam T The type of the value to be added.
         * @param value The value to be added to the JSON array.
         * @throws std::runtime_error (or CException) if the current object is not of array type.
         */
        template <typename T>
        void add(const T& value)
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & byte_mask);
            }
            JSONObject output(value);
            cJSON_AddItemReferenceToArray(obj_->o, output.obj_->o);
            refs_->insert(output);
        }

        /**
         * @brief Sets a value in the JSON object with the given name.
         *
         * This function is a template that allows setting a value of any type in the JSON object.
         *
         * @tparam T The type of the value to be set.
         * @param name The name of the JSON key as a null-terminated C string.
         * @param value The value to be set in the JSON object.
         * @throws std::runtime_error (or CException) if the current object is not of JSON object.
         */
        template <typename T>
        void set(const char* name, const T& value)
        {
            if (((*obj_)->type & byte_mask) != cJSON_Object)
            {
                CJSONPP_THROW("Not an object type", (*obj_)->type & byte_mask);
            }

            JSONObject output(value);
            cJSON_AddItemReferenceToObject(obj_->o, name, output.obj_->o);
            refs_->insert(output);
        }

        /**
         * @brief Sets a value in the JSON object with the given name.
         *
         * This function is a template that allows setting a value of any type in the JSON object.
         *
         * @tparam T The type of the value to be set.
         * @param name The name of the JSON key as a string.
         * @param value The value to be set in the JSON object.
         */
        template <typename T>
        void set(const std::string& name, const T& value)
        {
            set(name.c_str(), value);
        }


        /**
         * @brief Sets a JSON object with the given name and value.
         *
         * @param name The name of the JSON object to set.
         * @param value The JSON object to set.
         */
        void set(const std::string& name, const JSONObject& value);

        /**
         * @brief Removes from object an item with the specified name.
         *
         * @param name The name of the item to remove as a null-terminated C string.
         */
        void remove(const char* name);

        /**
         * @brief Removes from object an item with the specified name.
         *
         * @param name The name of the item to remove as a std::string
         */
        void remove(const std::string& name);

        /**
         * @brief Removes an item from array at the specified index.
         *
         * @param index The index of the item to remove.
         */
        void remove(int index);
    };

    /**
     * @brief Parses a JSON string and returns a JSONObject.
     *
     * @param str The JSON string to be parsed, as a null-terminated C string.
     * @return JSONObject The parsed JSON object.
     */
    JSONObject parse(const char* str);

    /**
     * @brief Parses a JSON string and returns a JSONObject.
     *
     * @param str The JSON string to be parsed, as a std::string.
     * @return JSONObject The parsed JSON object.
     */
    JSONObject parse(const std::string& str);

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
     * @return The integer value of the cJSON object.
     * @throws std::runtime_error (or CException) if the type of the cJSON object is not a number.
     */
    template <>
    inline int JSONObject::as<int>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            CJSONPP_THROW("Bad value type", obj->type & byte_mask);
        }
        return obj->valueint;
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to an int64_t value.
     *
     * This template specialization of the `as` method converts a cJSON object
     * to an int64_t value. It checks if the cJSON object is of type number
     * and throws an exception if it is not.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return The int64_t value of the cJSON object.
     * @throws std::runtime_error (or CException) if the cJSON object is not of type number.
     */
    template <>
    inline std::int64_t JSONObject::as<std::int64_t>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            CJSONPP_THROW("Not a number type", obj->type & byte_mask);
        }
        return static_cast<std::int64_t>(obj->valuedouble);
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to a std::string.
     *
     * This template specialization converts a cJSON object to a std::string.
     * It checks if the cJSON object is of string type and throws an exception if not.
     *
     * @param obj The cJSON object to be converted.
     * @return The std::string representation of the cJSON object.
     * @throws std::runtime_error (or CException) if the cJSON object is not of string type.
     */
    template <>
    inline std::string JSONObject::as<std::string>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_String)
        {
            CJSONPP_THROW("Not a string type", obj->type & byte_mask);
        }
        return obj->valuestring;
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to a double.
     *
     * This template specialization of the `as` method converts a given cJSON object
     * to a double value. It checks if the type of the cJSON object is a number and
     * throws an exception if it is not.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return The double value of the cJSON object.
     * @throws std::runtime_error (or CException) if the cJSON object is not of type number.
     */
    template <>
    inline double JSONObject::as<double>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            CJSONPP_THROW("Not a number type", obj->type & byte_mask);
        }
        return obj->valuedouble;
    }

    /**
     * @brief Specialized getter: Converts a cJSON object to a boolean value.
     *
     * This template specialization of the as function converts a cJSON object to a boolean value.
     * It checks the type of the cJSON object and returns true if the type is cJSON_True,
     * false if the type is cJSON_False, and throws an exception otherwise.
     *
     * @param obj Pointer to the cJSON object to be converted.
     * @return Boolean value representing the cJSON object.
     * @throws std::runtime_error (or CException) if the cJSON object is not of boolean type.
     */
    template <>
    inline bool JSONObject::as<bool>(cJSON* obj) const
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
            CJSONPP_THROW("Not a boolean type", obj->type & byte_mask);
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
    inline JSONObject JSONObject::as<JSONObject>(cJSON* obj) const
    {
        return { *this, obj, false };
    }


    /**
     * @brief Converts a JSONObject to an array and outputs each element to the given output iterator.
     *
     * Ex: asArray<JSONObject>(jsonArrayObject, std::back_inserter(vectorToFill));
     *
     * @tparam T The type of the elements in the JSONObject.
     * @tparam TOutputIterator The type of the output iterator.
     * @param data The JSONObject to be converted to an array.
     * @param output The output iterator where the elements of the array will be stored.
     */
    template <class T, class TOutputIterator>
    inline void asArray(const JSONObject& data, TOutputIterator output)
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
