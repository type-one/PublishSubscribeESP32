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

// clang-format off
#if defined(CJSONPP_NO_EXCEPTION)
#include "CException/CException.h"
#define CJSONPP_THROW(msg, value) do { std::fprintf(stderr, "%s (%d)\n", msg, static_cast<int>(value)); \ 
                                       std::fflush(stderr); Throw(0); } while (false)
#else
#define CJSONPP_THROW(msg, value) throw std::runtime_error(msg + " (" + std::to_string(value) + ")")
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
            Holder(cJSON* obj, bool own)
                : o(obj)
                , own_(own)
            {
            }
            ~Holder()
            {
                if (own_)
                {
                    cJSON_Delete(o);
                }
            }

            inline cJSON* operator->()
            {
                return o;
            }

        private:
            // no copy constructor
            explicit Holder(const Holder&);

            // no assignment operator
            Holder& operator=(const Holder&);
        };

        typedef std::shared_ptr<Holder> HolderPtr;

        typedef std::set<JSONObject> ObjectSet;
        typedef std::shared_ptr<ObjectSet> ObjectSetPtr;

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

    public:
        inline cJSON* obj() const
        {
            return obj_->o;
        }

        std::string print(bool formatted = true) const
        {
            char* json = formatted ? cJSON_Print(obj_->o) : cJSON_PrintUnformatted(obj_->o);
            std::string retval(json);
            std::free(json);
            return retval;
        }

        // necessary for holding references in the set
        bool operator<(const JSONObject& other) const
        {
            return obj_->o < other.obj_->o;
        }

        // create empty object
        JSONObject()
            : obj_(new Holder(cJSON_CreateObject(), true))
            , refs_(new ObjectSet)
        {
        }

        // non-virtual destructor (no subclassing intended)
        ~JSONObject()
        {
        }

        // wrap existing cJSON object
        JSONObject(cJSON* obj, bool own)
            : obj_(new Holder(obj, own))
            , refs_(new ObjectSet)
        {
        }

        // wrap existing cJSON object with parent
        JSONObject(JSONObject parent, cJSON* obj, bool own)
            : obj_(new Holder(obj, own))
            , refs_(new ObjectSet)
        {
            refs_->insert(parent);
        }

        // create boolean object
        explicit JSONObject(bool value)
            : obj_(new Holder(value ? cJSON_CreateTrue() : cJSON_CreateFalse(), true))
        {
        }

        // create double object
        explicit JSONObject(double value)
            : obj_(new Holder(cJSON_CreateNumber(value), true))
        {
        }

        // create integer object
        explicit JSONObject(int value)
            : obj_(new Holder(cJSON_CreateNumber(static_cast<double>(value)), true))
        {
        }

        // create integer object
        explicit JSONObject(int64_t value)
            : obj_(new Holder(cJSON_CreateNumber(static_cast<double>(value)), true))
        {
        }

        // create string object
        explicit JSONObject(const char* value)
            : obj_(new Holder(cJSON_CreateString(value), true))
        {
        }

        explicit JSONObject(const std::string& value)
            : obj_(new Holder(cJSON_CreateString(value.c_str()), true))
        {
        }

        // create array object
        template <typename T, template <typename X, typename A> class ContT = std::vector>

        explicit JSONObject(const ContT<T, std::allocator<T>>& elems)
            : obj_(new Holder(cJSON_CreateArray(), true))
            , refs_(new ObjectSet)
        {
            for (typename ContT<T, std::allocator<T>>::const_iterator it = elems.begin(); it != elems.end(); it++)
            {
                add(*it);
            }
        }

        template <typename T>
        JSONObject(const std::initializer_list<T>& elems)
            : obj_(new Holder(cJSON_CreateArray(), true))
            , refs_(new ObjectSet)
        {
            for (auto& it : elems)
            {
                add(it);
            }
        }

        // for Qt-style containers
        template <typename T, template <typename X> class ContT>
        explicit JSONObject(const ContT<T>& elems)
            : obj_(new Holder(cJSON_CreateArray(), true))
            , refs_(new ObjectSet)
        {
            for (typename ContT<T>::const_iterator it = elems.begin(); it != elems.end(); it++)
            {
                add(*it);
            }
        }

        // copy constructor
        JSONObject(const JSONObject& other)
            : obj_(other.obj_)
            , refs_(other.refs_)
        {
        }

        // copy operator
        inline JSONObject& operator=(const JSONObject& other)
        {
            if (&other != this)
            {
                obj_ = other.obj_;
                refs_ = other.refs_;
            }
            return *this;
        }

        // get object type
        inline JSONType type() const
        {
            static JSONType vmap[] = { Bool, Bool, Null, Number, String, Array, Object };
            return vmap[(*obj_)->type & 0xff];
        }

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
            if (((*obj_)->type & 0xff) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & 0xff);
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
            if (((*obj_)->type & 0xff) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & 0xff);
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
        inline T get(const char* name) const
        {
            if (((*obj_)->type & 0xff) != cJSON_Object)
            {
                CJSONPP_THROW("Not an object", (*obj_)->type & 0xff);
            }

            cJSON* item = cJSON_GetObjectItem(obj_->o, name);
            if (item == nullptr)
            {
                CJSONPP_THROW("No such item", 0);
            }

            return as<T>(item);
        }

        template <typename T = JSONObject>
        inline JSONObject get(const std::string& value) const
        {
            return get<T>(value.c_str());
        }

        inline bool has(const char* name) const
        {
            return cJSON_GetObjectItem(obj_->o, name) != nullptr;
        }

        inline bool has(const std::string& name) const
        {
            return has(name.c_str());
        }

        // get value from array
        template <typename T = JSONObject>
        inline T get(int index) const
        {
            if (((*obj_)->type & 0xff) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & 0xff);
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
            if (((*obj_)->type & 0xff) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & 0xff);
            }
            JSONObject o(value);
            cJSON_AddItemReferenceToArray(obj_->o, o.obj_->o);
            refs_->insert(o);
        }

        // set value in object
        template <typename T>
        inline void set(const char* name, const T& value)
        {
            if (((*obj_)->type & 0xff) != cJSON_Object)
            {
                CJSONPP_THROW("Not an object type", (*obj_)->type & 0xff);
            }
            JSONObject o(value);
            cJSON_AddItemReferenceToObject(obj_->o, name, o.obj_->o);
            refs_->insert(o);
        }

        // set value in object
        template <typename T>
        inline void set(const std::string& name, const T& value)
        {
            set(name.c_str(), value);
        }

        // set value in object (std::string)
        inline void set(const std::string& name, const JSONObject& value)
        {
            return set(name.c_str(), value);
        }

        // remove item from object
        inline void remove(const char* name)
        {
            if (((*obj_)->type & 0xff) != cJSON_Object)
            {
                CJSONPP_THROW("Not an object type", (*obj_)->type & 0xff);
            }
            cJSON* detached = cJSON_DetachItemFromObject(obj_->o, name);
            if (!detached)
            {
                CJSONPP_THROW("No such item", 0);
            }
            for (ObjectSet::iterator it = refs_->begin(); it != refs_->end(); it++)
            {
                if (it->obj_->o == detached)
                {
                    refs_->erase(it);
                    break;
                }
            }
            cJSON_Delete(detached);
        }

        inline void remove(const std::string& name)
        {
            return remove(name.c_str());
        }

        // remove item from array
        inline void remove(int index)
        {
            if (((*obj_)->type & 0xff) != cJSON_Array)
            {
                CJSONPP_THROW("Not an array type", (*obj_)->type & 0xff);
            }
            cJSON* detached = cJSON_DetachItemFromArray(obj_->o, index);
            if (!detached)
            {
                CJSONPP_THROW("No such item", 0);
            }
            for (ObjectSet::iterator it = refs_->begin(); it != refs_->end(); it++)
            {
                if (it->obj_->o == detached)
                {
                    refs_->erase(it);
                    break;
                }
            }
            cJSON_Delete(detached);
        }
    };

    // parse from C string
    inline JSONObject parse(const char* str)
    {
        cJSON* cjson = cJSON_Parse(str);
        if (nullptr == cjson)
        {
            CJSONPP_THROW("Parse error", 0);
        }
        return JSONObject(cjson, true);
    }

    // parse from std::string
    inline JSONObject parse(const std::string& str)
    {
        return parse(str.c_str());
    }

    // create null object
    inline JSONObject nullObject()
    {
        return JSONObject(cJSON_CreateNull(), true);
    }

    // create empty array object
    inline JSONObject arrayObject()
    {
        return JSONObject(cJSON_CreateArray(), true);
    }

    // Specialized getters
    template <>
    inline int JSONObject::as<int>(cJSON* obj) const
    {
        if ((obj->type & 0xff) != cJSON_Number)
        {
            CJSONPP_THROW("Bad value type", obj->type & 0xff);
        }
        return obj->valueint;
    }

    template <>
    inline int64_t JSONObject::as<int64_t>(cJSON* obj) const
    {
        if ((obj->type & 0xff) != cJSON_Number)
        {
            CJSONPP_THROW("Not a number type", obj->type & 0xff);
        }
        return static_cast<int64_t>(obj->valuedouble);
    }

    template <>
    inline std::string JSONObject::as<std::string>(cJSON* obj) const
    {
        if ((obj->type & 0xff) != cJSON_String)
        {
            CJSONPP_THROW("Not a string type", obj->type & 0xff);
        }
        return obj->valuestring;
    }

    template <>
    inline double JSONObject::as<double>(cJSON* obj) const
    {
        if ((obj->type & 0xff) != cJSON_Number)
        {
            CJSONPP_THROW("Not a number type", obj->type & 0xff);
        }
        return obj->valuedouble;
    }

    template <>
    inline bool JSONObject::as<bool>(cJSON* obj) const
    {
        if ((obj->type & 0xff) == cJSON_True)
        {
            return true;
        }
        else if ((obj->type & 0xff) == cJSON_False)
        {
            return false;
        }
        else
        {
            CJSONPP_THROW("Not a boolean type", obj->type & 0xff);
        }

        return false;
    }

    template <>
    inline JSONObject JSONObject::as<JSONObject>(cJSON* obj) const
    {
        return JSONObject(*this, obj, false);
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
