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

    cJSON* JSONObject::Holder::operator->()
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
        std::free(json);
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

    // non-virtual destructor (no subclassing intended)
    JSONObject::~JSONObject()
    {
    }

    // wrap existing cJSON object
    JSONObject::JSONObject(cJSON* obj, bool own)
        : obj_(new Holder(obj, own))
        , refs_(new ObjectSet)
    {
    }

    // wrap existing cJSON object with parent
    JSONObject::JSONObject(JSONObject parent, cJSON* obj, bool own)
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

    // copy constructor
    JSONObject::JSONObject(const JSONObject& other)
        : obj_(other.obj_)
        , refs_(other.refs_)
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
        static JSONType vmap[] = { Bool, Bool, Null, Number, String, Array, Object };
        return vmap[(*obj_)->type & 0xff];
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

    void JSONObject::remove(const std::string& name)
    {
        return remove(name.c_str());
    }

    // remove item from array
    void JSONObject::remove(int index)
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

    // parse from C string
    JSONObject parse(const char* str)
    {
        cJSON* cjson = cJSON_Parse(str);
        if (nullptr == cjson)
        {
            CJSONPP_THROW("Parse error", 0);
        }
        return JSONObject(cjson, true);
    }

    // parse from std::string
    JSONObject parse(const std::string& str)
    {
        return parse(str.c_str());
    }

    // create null object
    JSONObject nullObject()
    {
        return JSONObject(cJSON_CreateNull(), true);
    }

    // create empty array object
    JSONObject arrayObject()
    {
        return JSONObject(cJSON_CreateArray(), true);
    }

} // namespace cjsonpp
