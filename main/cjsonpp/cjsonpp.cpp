/**
 * @file cjsonpp.cpp
 * @brief A thin wrapper over cJSON data type providing a JSON object interface.
 *
 * This file contains the implementation of the JSONObject class and related functions.
 * The JSONObject class provides a convenient interface for working with JSON data
 * using the cJSON library. It includes methods for creating, parsing, and manipulating
 * JSON objects, arrays, and values.
 *
 * @author Dmitry Pankratov
 * @date 2015
 * MIT license
 * file modified by Laurent Lardinois in January 2025
 */

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <initializer_list>
#include <memory>

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

    JSONObject::Holder::Holder(Holder&& other) noexcept
        : o { other.o }
        , own_ { other.own_ }
    {
    }

    // move operator
    JSONObject::Holder& JSONObject::Holder::operator=(Holder&& other) noexcept
    {
        if (this != &other)
        {
            if (nullptr != o)
            {
                if (own_)
                {
                    cJSON_Delete(o);
                }
            }
            o = other.o;
            own_ = other.own_;
        }

        return *this;
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

    JSONObject::JSONObject(JSONObject&& other) noexcept
        : obj_ { std::move(other.obj_) }
        , refs_ { std::move(other.refs_) }
    {
    }

    JSONObject& JSONObject::operator=(JSONObject&& other) noexcept
    {
        if (this != &other)
        {
            obj_ = std::move(other.obj_);
            refs_ = std::move(other.refs_);
        }
        return *this;
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
        constexpr const int mask = 0xff;
        const auto idx = (*obj_)->type & mask;

        JSONType ret = JSONType::Null;

        switch (idx)
        {
            case cJSON_False:
            case cJSON_True:
                ret = JSONType::Bool;
                break;

            case cJSON_NULL:
                ret = JSONType::Null;
                break;

            case cJSON_Number:
                ret = JSONType::Number;
                break;

            case cJSON_String:
                ret = JSONType::String;
                break;

            case cJSON_Array:
                ret = JSONType::Array;
                break;

            case cJSON_Object:
                ret = JSONType::Object;
                break;

            case cJSON_Raw:
                ret = JSONType::Raw;
                break;

            case cJSON_Invalid:
            default:
                ret = JSONType::Invalid;
                break;
        }

        return ret;
    }

    bool JSONObject::has(const char* name) const
    {
        return cJSON_GetObjectItem(obj_->o, name) != nullptr;
    }

    bool JSONObject::has(const std::string& name) const
    {
        return has(name.c_str());
    }

    // remove item from object without throwing
    cjsonpp_status JSONObject::remove(const char* name)
    {
        constexpr const int mask = 0xff;
        if (((*obj_)->type & mask) != cJSON_Object)
        {
            return tools::unexpected<result_error> { make_error(
                result_code::invalid_type, (*obj_)->type & mask, "Not an object type") };
        }
        cJSON* detached = cJSON_DetachItemFromObject(obj_->o, name);
        if (nullptr == detached)
        {
            return tools::unexpected<result_error> { make_error(result_code::missing_item, 0, "No such item") };
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
        return cjsonpp_status {};
    }

    // remove item from array without throwing
    cjsonpp_status JSONObject::remove(int index)
    {
        constexpr const int mask = 0xff;
        if (((*obj_)->type & mask) != cJSON_Array)
        {
            return tools::unexpected<result_error> { make_error(
                result_code::invalid_type, (*obj_)->type & mask, "Not an array type") };
        }
        cJSON* detached = cJSON_DetachItemFromArray(obj_->o, index);
        if (nullptr == detached)
        {
            return tools::unexpected<result_error> { make_error(result_code::missing_item, 0, "No such item") };
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
        return cjsonpp_status {};
    }

    // parse from C string without throwing
    cjsonpp_result<JSONObject> parse_result(const char* str)
    {
        cJSON* cjson = cJSON_Parse(str);
        if (nullptr == cjson)
        {
            return tools::unexpected<result_error> { make_error(result_code::parse_error, 0, "Parse error") };
        }
        return cjsonpp_result<JSONObject> { JSONObject { cjson, true } };
    }

    // parse from std::string without throwing
    cjsonpp_result<JSONObject> parse_result(const std::string& str)
    {
        return parse_result(str.c_str());
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
