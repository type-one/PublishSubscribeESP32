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
#define CJSONPP_THROW(msg, value) throw std::runtime_error(msg + " (" + std::to_string(value) + ")") /* NOLINT keep it */
#endif
// clang-format on

namespace cjsonpp
{

    // JSON type wrapper enum
    enum JSONType
    {
        Bool,
        Null,
        String,
        Number,
        Array,
        Object
    };

    // JSONObject class is a thin wrapper over cJSON data type
    class JSONObject
    {
        // internal cJSON holder with ownership flag
        struct Holder
        {
            cJSON* o;
            bool own_;
            Holder(cJSON* obj, bool own);
            ~Holder();

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
        [[nodiscard]] cJSON* obj() const;

        [[nodiscard]] std::string print(bool formatted) const;

        [[nodiscard]] std::string print() const
        {
            return print(true);
        }

        // necessary for holding references in the set
        bool operator<(const JSONObject& other) const;

        // create empty object
        JSONObject();

        // non-virtual destructor (no subclassing intended)
        ~JSONObject() = default;

        // wrap existing cJSON object
        JSONObject(cJSON* obj, bool own);

        // wrap existing cJSON object with parent
        JSONObject(const JSONObject& parent, cJSON* obj, bool own);

        // create boolean object
        explicit JSONObject(bool value);

        // create double object
        explicit JSONObject(double value);

        // create integer object
        explicit JSONObject(int value);

        // create integer object
        explicit JSONObject(int64_t value);

        // create string object
        explicit JSONObject(const char* value);

        explicit JSONObject(const std::string& value);

        // create array object
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

        // for Qt-style containers
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

        // copy constructor
        JSONObject(const JSONObject& other) = default;

        // copy operator
        JSONObject& operator=(const JSONObject& other);

        // non movable
        JSONObject(JSONObject&&) noexcept = delete;
        JSONObject& operator=(JSONObject&&) noexcept = delete;

        // get object type
        [[nodiscard]] JSONType type() const;

        // get value from this object
        template <typename T>
        inline T as() const
        {
            return as<T>(obj_->o);
        }

        // get array
        template <typename T = JSONObject, template <typename X, typename A> class ContT = std::vector>
        inline ContT<T, std::allocator<T>> asArray() const
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & byte_mask);
            }

            ContT<T, std::allocator<T>> retval;
            for (int i = 0; i < cJSON_GetArraySize(obj_->o); i++)
            {
                retval.push_back(as<T>(cJSON_GetArrayItem(obj_->o, i)));
            }

            return retval;
        }

        // for Qt-style containers
        template <typename T, template <typename X> class ContT>
        inline ContT<T> asArray() const
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

        // get object by name
        template <typename T = JSONObject>
        [[nodiscard]] inline T get(const char* name) const
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

        template <typename T = JSONObject>
        [[nodiscard]] inline JSONObject get(const std::string& value) const
        {
            return get<T>(value.c_str());
        }

        bool has(const char* name) const;

        [[nodiscard]] bool has(const std::string& name) const;

        // get value from array
        template <typename T = JSONObject>
        inline T get(int index) const
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

        // add value to array
        template <typename T>
        inline void add(const T& value)
        {
            if (((*obj_)->type & byte_mask) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & byte_mask);
            }
            JSONObject output(value);
            cJSON_AddItemReferenceToArray(obj_->o, output.obj_->o);
            refs_->insert(output);
        }

        // set value in object
        template <typename T>
        inline void set(const char* name, const T& value)
        {
            if (((*obj_)->type & byte_mask) != cJSON_Object)
            {
                CJSONPP_THROW("Not an object type", (*obj_)->type & byte_mask);
            }
            JSONObject output(value);
            cJSON_AddItemReferenceToObject(obj_->o, name, output.obj_->o);
            refs_->insert(output);
        }

        // set value in object
        template <typename T>
        inline void set(const std::string& name, const T& value)
        {
            set(name.c_str(), value);
        }

        // set value in object (std::string)
        void set(const std::string& name, const JSONObject& value);

        // remove item from object
        void remove(const char* name);

        void remove(const std::string& name);

        // remove item from array
        void remove(int index);
    };

    // parse from C string
    JSONObject parse(const char* str);

    // parse from std::string
    JSONObject parse(const std::string& str);

    // create null object
    JSONObject nullObject();

    // create empty array object
    JSONObject arrayObject();

    // Specialized getters
    template <>
    inline int JSONObject::as<int>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            CJSONPP_THROW("Bad value type", obj->type & byte_mask);
        }
        return obj->valueint;
    }

    template <>
    inline int64_t JSONObject::as<int64_t>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            CJSONPP_THROW("Not a number type", obj->type & byte_mask);
        }
        return static_cast<int64_t>(obj->valuedouble);
    }

    template <>
    inline std::string JSONObject::as<std::string>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_String)
        {
            CJSONPP_THROW("Not a string type", obj->type & byte_mask);
        }
        return obj->valuestring;
    }

    template <>
    inline double JSONObject::as<double>(cJSON* obj) const
    {
        if ((obj->type & byte_mask) != cJSON_Number)
        {
            CJSONPP_THROW("Not a number type", obj->type & byte_mask);
        }
        return obj->valuedouble;
    }

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

    template <>
    inline JSONObject JSONObject::as<JSONObject>(cJSON* obj) const
    {
        return { *this, obj, false };
    }

    // Ex: asArray<JSONObject>(jsonArrayObject, std::back_inserter(vectorToFill));
    template <class T, class TOutputIterator>
    void asArray(const JSONObject& data, TOutputIterator output)
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
