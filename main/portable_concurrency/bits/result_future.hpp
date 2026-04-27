/**
 * @file result_future.hpp
 * @brief Portable concurrency result-future API entrypoint.
 * @author Laurent Lardinois, Sergey Vidyuk
 * @date April 2026
 * @note Split into focused internal headers.
 */

//-----------------------------------------------------------------------------//
// C++ Publish/Subscribe Pattern - Spare time development for fun              //
// (c) 2025-2026 Laurent Lardinois https://be.linkedin.com/in/laurentlardinois //
//                                                                             //
// https://github.com/type-one/PublishSubscribeESP32                           //
//                                                                             //
// MIT License                                                                 //
//-----------------------------------------------------------------------------//

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "coro.hpp"
#include "execution_impl.hpp"
#include "fwd.hpp"
#include "tools/cond_var.hpp"
#include "tools/critical_section.hpp"
#include "tools/expected.hpp"
#include "unique_function.hpp"

#include "result_future/factories.hpp"
#include "result_future/future.hpp"
#include "result_future/promise.hpp"
#include "result_future/result_shared_state_impl.hpp"
#include "result_future/shared.hpp"
#include "result_future/then.hpp"
#include "result_future/when_all.hpp"
#include "result_future/when_any.hpp"
