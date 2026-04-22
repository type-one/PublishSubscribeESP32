/**
 * @file portable_concurrency.cpp
 * @brief Portable concurrency component.
 * @author Sergey Vidyuk
 * @date 2017-10-05
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-10-05                                                   //
// https://github.com/VestniK/portable_concurrency                            //
// Public Domain (CC0 1.0)                                                    //
// https://creativecommons.org/publicdomain/zero/1.0/                         //
//-----------------------------------------------------------------------------//

#include <atomic>
#include "tools/critical_section.hpp"
#include "tools/cond_var.hpp"
#include <functional>
#include <future>

#include "closable_queue.hpp"
#include "future.hpp"
#include "future_state.h"
#include "latch.h"
#include "make_future.h"
#include "once_consumable_stack.hpp"
#include "promise.h"
#include "shared_future.hpp"
#include "shared_state.h"
#include "small_unique_function.hpp"
#include "thread_pool.h"
#include "unique_function.hpp"
#include "when_all.h"
#include "when_any.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces,cppcoreguidelines-rvalue-reference-param-not-moved)
namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace detail {

[[noreturn]] void throw_bad_func_call() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  throw std::bad_function_call{};
#else
  std::terminate();
#endif
}

template class small_unique_function<void()>;
template struct forward_list_deleter<continuation>;
template class once_consumable_stack<continuation>;

void continuations_stack::push(continuation &&continuation_fn) {
  if (!stack_.push(continuation_fn)) {
    continuation_fn();
  }
}

void continuations_stack::execute() {
  auto continuations = stack_.consume();
  for (auto &continuation_fn : continuations) {
    continuation_fn();
  }
}

bool continuations_stack::executed() const { return stack_.is_consumed(); }

void wait(future_state_base &state) {
  tools::critical_section mtx;
  tools::cond_var condition_variable;
  bool ready = false;
  state.push([&] {
    std::lock_guard<tools::critical_section> guard{mtx};
    ready = true;
    condition_variable.notify_one();
  });

  std::unique_lock<tools::critical_section> lock{mtx};
  condition_variable.wait(lock, [&] { return ready; });
}

template class closable_queue<unique_function<void()>>;

[[noreturn]] void throw_no_state() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  throw std::future_error{std::future_errc::no_state};
#else
  std::terminate();
#endif
}

[[noreturn]] void throw_already_satisfied() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  throw std::future_error(std::future_errc::promise_already_satisfied);
#else
  std::terminate();
#endif
}

[[noreturn]] void throw_already_retrieved() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  throw std::future_error(std::future_errc::future_already_retrieved);
#else
  std::terminate();
#endif
}

std::exception_ptr make_broken_promise() {
  return std::make_exception_ptr(
      std::future_error{std::future_errc::broken_promise});
}

void future_state_base::push(continuation &&cnt) {
  this->continuations().push(std::move(cnt));
}

} // namespace detail
// NOLINTEND(modernize-concat-nested-namespaces,cppcoreguidelines-rvalue-reference-param-not-moved)

namespace {

// P0443R7 states that if task submitted to static_thread_pool exits via
// exception then std::terminate is called. This behavior is established by
// marking this function noexcept.
void process_queue(
  detail::closable_queue<unique_function<void()>> &queue,
  const std::atomic<bool> &stopped
) noexcept {
  unique_function<void()> task;
  while (!stopped.load(std::memory_order_relaxed) && queue.pop(task)) {
    try {
      task();
    } catch (...) {
      std::terminate();
    }
  }
}

} // namespace

template class unique_function<void()>;

latch::~latch() {
  std::unique_lock<tools::critical_section> lock{mutex_};
  cv_.wait(lock, [this] { return waiters_ == 0; });
}

void latch::count_down_and_wait() {
  std::unique_lock<tools::critical_section> lock{mutex_};
  ++waiters_;
  assert(counter_ > 0);
  if (--counter_ == 0) {
    --waiters_;
    cv_.notify_all();
    return;
  }
  cv_.wait(lock, [this] { return counter_ == 0; });
  if (--waiters_ == 0) {
    cv_.notify_one();
  }
}

void latch::count_down(ptrdiff_t n) {
  {
    std::lock_guard<tools::critical_section> lock{mutex_};
    assert(counter_ >= n);
    assert(n >= 0);
    counter_ -= n;
    if (counter_ > 0) {
      return;
    }
  }
  cv_.notify_all();
}

bool latch::is_ready() const noexcept {
  std::lock_guard<tools::critical_section> lock{mutex_};
  return counter_ == 0;
}

void latch::wait() const {
  std::unique_lock<tools::critical_section> lock{mutex_};
  ++waiters_;
  cv_.wait(lock, [this] { return counter_ == 0; });
  if (--waiters_ == 0) {
    cv_.notify_one();
  }
}

template <> void future<void>::get() {
  if (!state_) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    throw std::future_error(std::future_errc::no_state);
#else
    std::terminate();
#endif
  }
  wait();
  auto state = std::move(state_);
  state->value_ref();
}

template <>
typename shared_future<void>::get_result_type shared_future<void>::get() const {
  if (!state_) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    throw std::future_error(std::future_errc::no_state);
#else
    std::terminate();
#endif
  }
  wait();
  state_->value_ref();
}

future<void> make_ready_future() {
  auto promise_and_future = make_promise<void>();
  promise_and_future.first.set_value();
  return std::move(promise_and_future.second);
}

future<std::tuple<>> when_all() { return make_ready_future(std::tuple<>{}); }

future<when_any_result<std::tuple<>>> when_any() {
  return make_ready_future(when_any_result<std::tuple<>>{
      static_cast<std::size_t>(-1), std::tuple<>{}});
}

static_thread_pool::static_thread_pool(std::size_t num_threads) {
  threads_.reserve(num_threads);
  while (num_threads-- > 0) {
    threads_.emplace_back(&static_thread_pool::attach, this);
  }
}

static_thread_pool::~static_thread_pool() {
  stop();
  wait();
}

void static_thread_pool::attach() {
  {
    std::lock_guard<tools::critical_section> lock{mutex_};
    ++attached_threads_;
  }
  process_queue(queue_, stopped_);
  {
    std::unique_lock<tools::critical_section> lock{mutex_};
    --attached_threads_;
    cv_.wait(lock, [&] { return attached_threads_ == 0; });
  }
  cv_.notify_all();
}

void static_thread_pool::stop() {
  stopped_.store(true, std::memory_order_relaxed);
  queue_.close();
}

void static_thread_pool::wait() {
  queue_.close();
  for (auto &thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  threads_.clear();
}

} // namespace cxx14_v1
} // namespace portable_concurrency
