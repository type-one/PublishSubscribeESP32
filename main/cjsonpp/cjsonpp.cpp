#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include <initializer_list>
#include <memory>

#define CJSONPP_NO_EXCEPTION
#include "cJSON/cJSON.h"
#include "cjsonpp/cjsonpp.hpp"

// modified for clang-tidy checks

namespace cjsonpp
{
    JSONObject::Holder::Holder(cJSON* obj, bool own)
        : o(obj)
        , own_(own)
    {
    }

    JSONObject::Holder::~Holder()
    {
        if (own_)
        {
            cJSON_Delete(o);
        }
    }

    cJSON* JSONObject::Holder::operator->() const
    {
        return o;
    }

    cJSON* JSONObject::obj() const
    {
        return obj_->o;
    }

    std::string JSONObject::print(bool formatted) const
    {
        char* json = formatted ? cJSON_Print(obj_->o) : cJSON_PrintUnformatted(obj_->o);
        std::string retval(json);
        std::free(json); // NOLINT allocated from C with malloc
        return retval;
    }


    // necessary for holding references in the set
    bool JSONObject::operator<(const JSONObject& other) const
    {
        return obj_->o < other.obj_->o;
    }

    // create empty object
    JSONObject::JSONObject()
        : obj_(new Holder(cJSON_CreateObject(), true))
        , refs_(new ObjectSet)
    {
    }

    // wrap existing cJSON object
    JSONObject::JSONObject(cJSON* obj, bool own)
        : obj_(new Holder(obj, own))
        , refs_(new ObjectSet)
    {
    }

    // wrap existing cJSON object with parent
    JSONObject::JSONObject(const JSONObject& parent, cJSON* obj, bool own)
        : obj_(new Holder(obj, own))
        , refs_(new ObjectSet)
    {
        refs_->insert(parent);
    }

    // create boolean object
    JSONObject::JSONObject(bool value)
        : obj_(new Holder(value ? cJSON_CreateTrue() : cJSON_CreateFalse(), true))
    {
    }

    // create double object
    JSONObject::JSONObject(double value)
        : obj_(new Holder(cJSON_CreateNumber(value), true))
    {
    }

    // create integer object
    JSONObject::JSONObject(int value)
        : obj_(new Holder(cJSON_CreateNumber(static_cast<double>(value)), true))
    {
    }

    // create integer object
    JSONObject::JSONObject(int64_t value)
        : obj_(new Holder(cJSON_CreateNumber(static_cast<double>(value)), true))
    {
    }

    // create string object
    JSONObject::JSONObject(const char* value)
        : obj_(new Holder(cJSON_CreateString(value), true))
    {
    }

    JSONObject::JSONObject(const std::string& value)
        : obj_(new Holder(cJSON_CreateString(value.c_str()), true))
    {
    }

    // copy operator
    JSONObject& JSONObject::operator=(const JSONObject& other)
    {
        if (&other != this)
        {
            obj_ = other.obj_;
            refs_ = other.refs_;
        }
        return *this;
    }

    // get object type
    JSONType JSONObject::type() const
    {
        constexpr const std::size_t nb_json_types = 7U;
        static std::array<JSONType, nb_json_types> vmap = { Bool, Bool, Null, Number, String, Array, Object };
        constexpr const int mask = 0xff;
        return vmap.at((*obj_)->type & mask);
    }

    bool JSONObject::has(const char* name) const
    {
        return cJSON_GetObjectItem(obj_->o, name) != nullptr;
    }

    bool JSONObject::has(const std::string& name) const
    {
        return has(name.c_str());
    }

    // set value in object (std::string)
    void JSONObject::set(const std::string& name, const JSONObject& value)
    {
        return set(name.c_str(), value);
    }

    // remove item from object
    void JSONObject::remove(const char* name)
    {
        constexpr const int mask = 0xff;
        if (((*obj_)->type & mask) != cJSON_Object)
        {
            CJSONPP_THROW("Not an object type", (*obj_)->type & mask);
        }
        cJSON* detached = cJSON_DetachItemFromObject(obj_->o, name);
        if (nullptr == detached)
        {
            CJSONPP_THROW("No such item", 0);
        }
        for (auto it = refs_->begin(); it != refs_->end(); it++)
        {
            if (it->obj_->o == detached)
            {
                refs_->erase(it);
                break;
            }
        }
        cJSON_Delete(detached);
    }

    void JSONObject::remove(const std::string& name)
    {
        return remove(name.c_str());
    }

    // remove item from array
    void JSONObject::remove(int index)
    {
        constexpr const int mask = 0xff;
        if (((*obj_)->type & mask) != cJSON_Array)
        {
            CJSONPP_THROW("Not an array type", (*obj_)->type & mask);
        }
        cJSON* detached = cJSON_DetachItemFromArray(obj_->o, index);
        if (nullptr == detached)
        {
            CJSONPP_THROW("No such item", 0);
        }
        for (auto it = refs_->begin(); it != refs_->end(); it++)
        {
            if (it->obj_->o == detached)
            {
                refs_->erase(it);
                break;
            }
        }
        cJSON_Delete(detached);
    }

    // parse from C string
    JSONObject parse(const char* str)
    {
        cJSON* cjson = cJSON_Parse(str);
        if (nullptr == cjson)
        {
            CJSONPP_THROW("Parse error", 0);
        }
        return { cjson, true };
    }

    // parse from std::string
    JSONObject parse(const std::string& str)
    {
        return parse(str.c_str());
    }

    // create null object
    JSONObject nullObject()
    {
        return { cJSON_CreateNull(), true };
    }

    // create empty array object
    JSONObject arrayObject()
    {
        return { cJSON_CreateArray(), true };
    }

} // namespace cjsonpp
