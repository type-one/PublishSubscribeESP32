
//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois      //
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

#pragma once

#if !defined(__LOGGER_HPP__)
#define __LOGGER_HPP__

#if defined(ESP_PLATFORM)
#if defined(DEBUG)
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#else
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif
#include <esp_log.h>
#else
#include <cstdio>
#endif

#if defined(__func__)
#define _FUNCTION_ALIAS_ __func__
#else
#define _FUNCTION_ALIAS_ __FUNCTION__
#endif

#if defined(ESP_PLATFORM)
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html
// ESP_LOGE - Error (lowest)
// ESP_LOGW - Warning
// ESP_LOGI - Info
// ESP_LOGD - Debug
// ESP_LOGV - Verbose (highest)
#define LOG_ERROR(...) ESP_LOGE(_FUNCTION_ALIAS_, __VA_ARGS__)
#define LOG_WARNING(...) ESP_LOGW(_FUNCTION_ALIAS_, __VA_ARGS__)
#define LOG_INFO(...) ESP_LOGI(_FUNCTION_ALIAS_, __VA_ARGS__)
#define LOG_DEBUG(...) ESP_LOGD(_FUNCTION_ALIAS_, __VA_ARGS__)
#define LOG_VERBOSE(...) ESP_LOGV(_FUNCTION_ALIAS_, __VA_ARGS__)
#else
// clang-format off
#define LOG_ERROR(...) do { std::fprintf(stderr, "[ERROR] %s [function %s, line %d]", __FILE__, _FUNCTION_ALIAS_, __LINE__); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); std::fflush(stderr); } while(false)
#define LOG_WARNING(...) do { std::fprintf(stderr, "[WARNING] %s [function %s, line %d]", __FILE__, _FUNCTION_ALIAS_, __LINE__); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); std::fflush(stderr); } while(false)
#define LOG_INFO(...) do { std::printf("[INFO] %s [function %s, line %d]", __FILE__, _FUNCTION_ALIAS_, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while(false)

#if defined(DEBUG)
#define LOG_DEBUG(...) do { std::printf("[DEBUG] %s [function %s, line %d]", __FILE__, _FUNCTION_ALIAS_, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while(false)
#define LOG_VERBOSE(...) do { std::printf("[VERBOSE] %s [function %s, line %d]", __FILE__, _FUNCTION_ALIAS_, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while(false)
#else
#define LOG_DEBUG(...)
#define LOG_VERBOSE(...)
#endif

// clang-format on
#endif

#endif //  __LOGGER_HPP__
