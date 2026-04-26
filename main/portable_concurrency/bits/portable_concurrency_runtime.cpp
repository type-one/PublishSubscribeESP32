/**
 * @file portable_concurrency_runtime.cpp
 * @brief Runtime primitives for portable_concurrency public wrappers.
 * @author Sergey Vidyuk
 * @date 2017-10-05
 * @license https://creativecommons.org/publicdomain/zero/1.0/
 * @see https://creativecommons.org/publicdomain/zero/1.0/
 */

//-----------------------------------------------------------------------------//
// Portable Concurrency Framework                                              //
// Original author: Sergey Vidyuk                                              //
// Original date: 2017-10-05                                                   //
// https://github.com/VestniK/portable_concurrency                             //
// Public Domain (CC0 1.0)                                                     //
// https://creativecommons.org/publicdomain/zero/1.0/                          //
//-----------------------------------------------------------------------------//

#include <atomic>
#include <functional>
#include <future>

#include "tools/cond_var.hpp"
#include "tools/critical_section.hpp"

#include "closable_queue.hpp"
#include "continuations_stack.hpp"
#include "latch_impl.hpp"
#include "once_consumable_stack.hpp"
#include "small_unique_function.hpp"
#include "thread_pool_impl.hpp"
#include "unique_function.hpp"

namespace pco {
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
  auto continuation_local = std::move(continuation_fn);
  if (!stack_.push(continuation_local)) {
    continuation_local();
  }
}

void continuations_stack::execute() {
  auto continuations = stack_.consume();
  for (auto &continuation_fn : continuations) {
    continuation_fn();
  }
}

bool continuations_stack::executed() const { return stack_.is_consumed(); }

template class closable_queue<unique_function<void()>>;

} // namespace detail

namespace {

void process_queue(
  detail::closable_queue<unique_function<void()>> &queue,
  const std::atomic<bool> &stopped
) noexcept {
  unique_function<void()> task;
  while (!stopped.load(std::memory_order_relaxed) && queue.pop(task)) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
      task();
    } catch (...) {
      std::terminate();
    }
#else
    task();
#endif
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

} // namespace pco
