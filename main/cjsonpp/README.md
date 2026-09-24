Type-safe thin C++ wrapper over cJSON.

Version 0.5 (result-based API)

https://github.com/ancwrd1/cjsonpp

This wrapper uses explicit result values for error handling.
Public exception-style helpers are not part of the API.

## Current Public API

Core operations are:

- Parse: `parse_result(...)`
- Read: `get<T>(...)`, `as<T>()`, `has(...)`, `type()`
- Write/mutate: `set(...)`, `add(...)`, `remove(...)`
- Print/raw access: `print(...)`, `obj()`
- Constructors/helpers: `JSONObject(...)`, `nullObject()`, `arrayObject()`

Errors are returned as `cjsonpp::result_error` with:

- `code` (`result_code`)
- `detail` (type/index detail)
- `message` (human-readable text)

Result aliases:

- `cjsonpp_result<T>`
- `cjsonpp_status` (alias for `cjsonpp_result<void>`)

## Quick Start

```cpp
#include <iostream>
#include <string>

#include "cjsonpp/cjsonpp.hpp"

void parse_example(const std::string& json_text)
{
    auto parsed = cjsonpp::parse_result(json_text);
    if (!parsed)
    {
        std::cerr << "parse failed: " << parsed.error().message << "\n";
        return;
    }

    const auto& obj = parsed.value();

    auto int_value = obj.get<int>("intval");
    if (int_value)
    {
        std::cout << "intval=" << int_value.value() << "\n";
    }

    auto nested = obj.get<cjsonpp::JSONObject>("nested");
    if (nested)
    {
        auto nested_int = nested.value().get<int>("v");
        if (nested_int)
        {
            std::cout << "nested.v=" << nested_int.value() << "\n";
        }
    }
}
```

## Constructing Objects

```cpp
cjsonpp::JSONObject obj;
std::vector<int> vec = {1, 2, 3, 4};

static_cast<void>(obj.set("intval", 1234));
static_cast<void>(obj.set("arrval", vec));
static_cast<void>(obj.set("doubleval", 100.1));
static_cast<void>(obj.set("nullval", cjsonpp::nullObject()));
```

## Constructing Arrays

```cpp
cjsonpp::JSONObject arr = cjsonpp::arrayObject();
static_cast<void>(arr.add("s1"));
static_cast<void>(arr.add("s2"));

cjsonpp::JSONObject obj;
static_cast<void>(obj.set("arrval", arr));
```

## Reading Arrays

```cpp
auto arr_result = obj.get<cjsonpp::JSONObject>("arrval");
if (arr_result)
{
    const auto& arr = arr_result.value();
    const int count = cJSON_GetArraySize(arr.obj());
    for (int idx = 0; idx < count; ++idx)
    {
        auto item = arr.get<std::string>(idx);
        if (item)
        {
            std::cout << item.value() << "\n";
        }
    }
}
```

## Supported Value Conversions

`JSONObject::as<T>()` / `JSONObject::get<T>(...)` currently provide built-in support for:

- `int`
- `std::int64_t`
- `double`
- `std::string`
- `bool`
- `JSONObject`

## JSON Type Enum

`JSONObject::type()` returns `cjsonpp::JSONType`:

- `JSONType::Bool`
- `JSONType::Null`
- `JSONType::String`
- `JSONType::Number`
- `JSONType::Array`
- `JSONType::Object`
- `JSONType::Raw`
- `JSONType::Invalid`

## API Note

Older `try_*` names (`try_get`, `try_as`, `try_set`, `try_add`, `try_remove`) are not the current public API names in this repository.

## ESP32 PSRAM allocation

Call `cjsonpp::initialize_allocators()` once at startup, after ESP-IDF initializes the
heap and before creating any JSON objects or starting tasks that use cJSON. The
project entry point already does this. Applications embedding the wrapper must
make this call themselves; do not create JSON objects in global constructors.

With `ESP_PLATFORM` and `CONFIG_SPIRAM` defined, cJSON allocations prefer
`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` and fall back to
`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`. Deallocation uses `heap_caps_free`.
Otherwise initialization leaves the existing cJSON hooks unchanged.

These hooks apply globally to cJSON nodes, strings, and printing buffers. They do
not control the caller's input buffer or C++ allocations such as `std::string`.
Do not replace hooks while objects or printed buffers exist or while cJSON is in
use by another thread. `JSONObject::print()` releases its cJSON buffer through
`cJSON_free()` to respect the active hooks.
