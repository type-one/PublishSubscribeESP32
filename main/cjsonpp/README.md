Type-safe thin C++ wrapper over cJSON.

Version 0.5 (result-based API)

https://github.com/ancwrd1/cjsonpp

This wrapper uses explicit result values for error handling.
Public exception-style helpers are not part of the API anymore.

Core concepts

- Parse with `parse_result(...)`
- Read values with `try_get<T>(...)` and `try_as<T>()`
- Write values with `try_set(...)`, `try_add(...)`, `try_remove(...)`
- Errors are returned as `result_error` with:
  - `code` (`result_code`)
  - `detail` (type/index detail)
  - `message` (human-readable text)

Quick start

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

	auto int_value = obj.try_get<int>("intval");
	if (int_value)
	{
		std::cout << "intval=" << int_value.value() << "\n";
	}

	auto nested = obj.try_get<cjsonpp::JSONObject>("nested");
	if (nested)
	{
		auto nested_int = nested.value().try_get<int>("v");
		if (nested_int)
		{
			std::cout << "nested.v=" << nested_int.value() << "\n";
		}
	}
}
```

Constructing objects

```cpp
cjsonpp::JSONObject obj;
std::vector<int> vec = { 1, 2, 3, 4 };

obj.try_set("intval", 1234);
obj.try_set("arrval", vec);
obj.try_set("doubleval", 100.1);
obj.try_set("nullval", cjsonpp::nullObject());
```

Constructing arrays

```cpp
cjsonpp::JSONObject arr = cjsonpp::arrayObject();
arr.try_add("s1");
arr.try_add("s2");

cjsonpp::JSONObject obj;
obj.try_set("arrval", arr);
```

Reading arrays

```cpp
auto arr_result = obj.try_get<cjsonpp::JSONObject>("arrval");
if (arr_result)
{
	const auto& arr = arr_result.value();
	const int count = cJSON_GetArraySize(arr.obj());
	for (int idx = 0; idx < count; ++idx)
	{
		auto item = arr.try_get<std::string>(idx);
		if (item)
		{
			std::cout << item.value() << "\n";
		}
	}
}
```

Supported value types

- int
- int64_t
- double
- std::string
- bool
- JSONObject

Extending type conversion

To add support for custom value types, add a specialization of `JSONObject::as_result<T>(cJSON*)`.

Example:

```cpp
template <>
inline cjsonpp::cjsonpp_result<QString> cjsonpp::JSONObject::as_result<QString>(cJSON* obj) const
{
	auto str_result = as_result<std::string>(obj);
	if (!str_result)
	{
		return tools::unexpected<cjsonpp::result_error> { str_result.error() };
	}
	return QString::fromStdString(str_result.value());
}
```
