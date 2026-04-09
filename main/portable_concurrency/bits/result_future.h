#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "execution.h"
#include "tools/critical_section.hpp"
#include "tools/cond_var.hpp"
#include "expected_compat.h"
#include "fwd.h"
#include "unique_function.hpp"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace v2 {

enum class result_error {
  no_state = 1,
  broken_promise,
  execution_failure,
  continuation_failure,
};

template <typename T, typename E = result_error> class future_result;
template <typename T, typename E = result_error> class shared_result;
template <typename T, typename E = result_error> class promise_result;

template <typename Sequence> struct when_any_result {
  std::size_t index;
  Sequence futures;
};

namespace detail {

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define PC_V2_HAS_EXCEPTIONS
#endif

// Helper to deduce invoke result properly for void types in then_value chains
template <typename F, typename T, typename = void>
struct result_then_value_type {
  using raw_type = std::decay_t<std::invoke_result_t<F, T>>;
  using type = raw_type;
};

template <typename F>
struct result_then_value_type<F, void, std::enable_if_t<std::is_invocable_v<F>>> {
  using raw_type = std::decay_t<std::invoke_result_t<F>>;
  using type = raw_type;
};

template <typename F, typename T, typename = void>
struct result_shared_then_value_type {
  using raw_type = std::decay_t<std::invoke_result_t<F, const T &>>;
  using type = raw_type;
};

template <typename F>
struct result_shared_then_value_type<F, void, std::enable_if_t<std::is_invocable_v<F>>> {
  using raw_type = std::decay_t<std::invoke_result_t<F>>;
  using type = raw_type;
};

template <typename F, typename Arg>
struct result_then_result_type {
  using raw_type = std::decay_t<std::invoke_result_t<F, Arg>>;
  using type = raw_type;
};

// Deduce result type for interruptible continuation callbacks.
// Expected callable shape:
//   void(promise_result<R, E>, future_result<T, E>)
// or
//   void(promise_result<R, E>, shared_result<T, E>)
template <typename Source, typename E>
struct interrupt_promise_deducer {
  template <typename R>
  static R deduce(void (*)(promise_result<R, E>, Source));

  template <typename R, typename C>
  static R deduce_method(void (C::*)(promise_result<R, E>, Source) const);

  template <typename R, typename C>
  static R deduce_method(void (C::*)(promise_result<R, E>, Source));

  template <typename F>
  static auto deduce(F) -> decltype(deduce_method(&F::operator()));
};

template <typename Func, typename Source, typename E, typename = void>
struct interrupt_promise_arg {};

template <typename Func, typename Source, typename E>
struct interrupt_promise_arg<
    Func, Source, E,
    std::void_t<decltype(
        interrupt_promise_deducer<Source, E>::deduce(
            std::declval<std::decay_t<Func>>()))>> {
  using type = decltype(
      interrupt_promise_deducer<Source, E>::deduce(
          std::declval<std::decay_t<Func>>()));
};

template <typename Func, typename Source, typename E>
using interrupt_promise_arg_t =
    typename interrupt_promise_arg<Func, Source, E>::type;

template <typename T, typename E> struct result_shared_state {
  result_shared_state() = default;

  template <typename F>
  explicit result_shared_state(canceler_arg_t, F &&cancel_action)
      : cancel_action_(std::forward<F>(cancel_action)) {}

  ~result_shared_state() {
    if (!ready_ && cancel_action_) {
      cancel_action_();
    }
  }

  tools::critical_section mutex_;
  tools::cond_var cv_;
  bool ready_ = false;
  std::optional<expected<T, E>> result_;
  std::vector<std::function<void()>> on_ready_cbs_;
  unique_function<void()> cancel_action_;
};

template <typename T, typename E>
using result_state_ptr = std::shared_ptr<result_shared_state<T, E>>;

template <typename F, typename... A>
using invoke_decay_t = std::decay_t<std::invoke_result_t<F, A...>>;

template <typename T>
struct is_result_future : std::false_type {};

template <typename T, typename E>
struct is_result_future<future_result<T, E>> : std::true_type {};

template <typename T>
struct is_shared_result : std::false_type {};

template <typename T, typename E>
struct is_shared_result<shared_result<T, E>> : std::true_type {};

template <typename T>
struct is_result_handle
    : std::disjunction<is_result_future<std::decay_t<T>>, is_shared_result<std::decay_t<T>>> {};

template <typename... T>
struct are_result_futures : std::conjunction<is_result_future<std::decay_t<T>>...> {};

template <typename... T>
struct are_shared_results : std::conjunction<is_shared_result<std::decay_t<T>>...> {};

template <typename... T>
struct are_result_handles : std::conjunction<is_result_handle<std::decay_t<T>>...> {};

template <typename R>
struct unwrapped_result_value {
  using type = R;
};

template <typename T, typename E>
struct unwrapped_result_value<future_result<T, E>> {
  using type = T;
};

template <typename T, typename E>
struct unwrapped_result_value<shared_result<T, E>> {
  using type = T;
};

template <typename Traits>
using unwrapped_result_value_t = typename unwrapped_result_value<typename Traits::raw_type>::type;

template <typename NextT, typename E, typename Handle>
void resolve_nested_handle(promise_result<NextT, E> &promise, Handle &&handle);

template <typename Alloc, typename T>
using rebind_alloc_t = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

template <typename T, typename E, typename Clock, typename Duration>
future_status wait_until_ready(const result_state_ptr<T, E> &state,
                               const std::chrono::time_point<Clock, Duration> &abs_time) {
  if (!state) {
    return future_status::timeout;
  }

  std::unique_lock<tools::critical_section> lock(state->mutex_);
  if (state->ready_) {
    return future_status::ready;
  }

  return state->cv_.wait_until(lock, abs_time, [&state] { return state->ready_; })
             ? future_status::ready
             : future_status::timeout;
}

} // namespace detail

template <typename T, typename E> class promise_result {
static_assert(!std::is_reference_v<T>,
              "portable_concurrency v2 does not support reference return types. "
              "Use pointers or std::reference_wrapper<T> instead. "
              "This design constraint avoids move-semantics ambiguity and lifetime issues.");
public:
  using value_type = T;
  using error_type = E;

  promise_result() : state_(std::make_shared<detail::result_shared_state<T, E>>()) {}

  template <typename F>
  promise_result(canceler_arg_t tag, F &&cancel_action)
      : state_(std::make_shared<detail::result_shared_state<T, E>>(tag, std::forward<F>(cancel_action))) {}

  explicit promise_result(detail::result_state_ptr<T, E> state) : state_(std::move(state)) {}

  promise_result(const promise_result &) = delete;
  promise_result &operator=(const promise_result &) = delete;
  promise_result(promise_result &&) noexcept = default;
  promise_result &operator=(promise_result &&rhs) noexcept {
    if (this != &rhs) {
      // Use get_state() so we also abandon pending state reachable only through
      // weak_state_ (the common post-get_future() case).
      auto old_state = get_state();
      auto new_state = rhs.state_;

      if (old_state) {
        std::vector<std::function<void()>> callbacks;
        if (new_state && old_state == new_state) {
          std::lock_guard<tools::critical_section> guard(old_state->mutex_);
          if (!old_state->ready_) {
            old_state->result_.emplace(make_expected_error<T, E>(E::broken_promise));
            old_state->ready_ = true;
            callbacks = std::move(old_state->on_ready_cbs_);
            old_state->cv_.notify_all();
          }
        } else if (new_state) {
          std::scoped_lock lock(old_state->mutex_, new_state->mutex_);
          if (!old_state->ready_) {
            old_state->result_.emplace(make_expected_error<T, E>(E::broken_promise));
            old_state->ready_ = true;
            callbacks = std::move(old_state->on_ready_cbs_);
            old_state->cv_.notify_all();
          }
        } else {
          std::lock_guard<tools::critical_section> guard(old_state->mutex_);
          if (!old_state->ready_) {
            old_state->result_.emplace(make_expected_error<T, E>(E::broken_promise));
            old_state->ready_ = true;
            callbacks = std::move(old_state->on_ready_cbs_);
            old_state->cv_.notify_all();
          }
        }
        for (auto &callback : callbacks) {
          callback();
        }
      }

      state_ = std::move(rhs.state_);
      weak_state_ = std::move(rhs.weak_state_);
      future_retrieved_ = rhs.future_retrieved_;
      rhs.future_retrieved_ = false;
    }
    return *this;
  }

  ~promise_result() {
    abandon_pending_state();
  }

private:
  void abandon_pending_state() {
    auto state = get_state();
    if (!state) {
      return;
    }

    std::vector<std::function<void()>> cbs;
    {
      std::lock_guard<tools::critical_section> guard(state->mutex_);
      if (!state->ready_) {
        state->result_.emplace(make_expected_error<T, E>(E::broken_promise));
        state->ready_ = true;
        cbs = std::move(state->on_ready_cbs_);
        state->cv_.notify_all();
      }
    }
    for (auto &cb : cbs)
      cb();
  }

public:

  future_result<T, E> get_future() {
    if (future_retrieved_ || !state_) {
      return future_result<T, E> {};
    }
    future_retrieved_ = true;
    weak_state_ = state_;
    auto future = future_result<T, E>(state_);
    state_.reset();
    return future;
  }

  void set_result(expected<T, E> result) {
    auto state = get_state();
    if (!state) {
      return;
    }

    std::vector<std::function<void()>> cbs;
    {
      std::lock_guard<tools::critical_section> guard(state->mutex_);
      if (state->ready_) {
        return;
      }

      state->result_.emplace(std::move(result));
      state->ready_ = true;
      state->cancel_action_ = unique_function<void()>{};
      cbs = std::move(state->on_ready_cbs_);
      state->cv_.notify_all();
    }
    for (auto &cb : cbs)
      cb();
  }

  template <typename U = T,
            typename std::enable_if<!std::is_void<U>::value, int>::type = 0>
  void set_value(U value) {
    set_result(expected<T, E>(std::move(value)));
  }

  template <typename U = T,
            typename std::enable_if<std::is_void<U>::value, int>::type = 0>
  void set_value() {
    set_result(expected<void, E>());
  }

  void set_error(E error) { set_result(make_expected_error<T, E>(std::move(error))); }

  [[nodiscard]] bool is_awaiten() const noexcept {
    return static_cast<bool>(state_) || !weak_state_.expired();
  }

  explicit operator bool() const noexcept { return is_awaiten(); }

private:
  detail::result_state_ptr<T, E> get_state() const {
    if (state_) {
      return state_;
    }
    return weak_state_.lock();
  }

  detail::result_state_ptr<T, E> state_;
  std::weak_ptr<detail::result_shared_state<T, E>> weak_state_;
  bool future_retrieved_ = false;
};

namespace detail {
template <typename NextT, typename E, typename Handle>
void resolve_nested_handle(promise_result<NextT, E> &promise, Handle &&handle) {
  using handle_t = std::decay_t<Handle>;
  static_assert(is_result_handle<handle_t>::value,
                "resolve_nested_handle requires future_result or shared_result handle");
  static_assert(std::is_same<E, typename handle_t::error_type>::value,
                "Nested result handle must share the same error type");

  if constexpr (is_result_future<handle_t>::value) {
    auto nested = std::forward<Handle>(handle).get_result();
    if (!nested.has_value()) {
      if (nested.error() == E::no_state) {
        // Continuation returned an invalid nested handle; match v1 behavior
        // by surfacing broken_promise on the outer result.
        promise.set_error(E::broken_promise);
      } else {
        promise.set_error(nested.error());
      }
      return;
    }

    if constexpr (std::is_void<NextT>::value) {
      promise.set_value();
    } else {
      promise.set_value(std::move(nested).value());
    }
  } else {
    const auto &nested = handle.get_result();
    if (!nested.has_value()) {
      if (nested.error() == E::no_state) {
        promise.set_error(E::broken_promise);
      } else {
        promise.set_error(nested.error());
      }
      return;
    }

    if constexpr (std::is_void<NextT>::value) {
      promise.set_value();
    } else {
      promise.set_value(nested.value());
    }
  }
}
} // namespace detail

template <typename T, typename E> class future_result {
static_assert(!std::is_reference_v<T>,
              "portable_concurrency v2 does not support reference return types. "
              "Use pointers or std::reference_wrapper<T> instead. "
              "This design constraint avoids move-semantics ambiguity and lifetime issues.");
public:
  using value_type = T;
  using error_type = E;
  using result_type = expected<T, E>;

  future_result() = default;

  explicit future_result(detail::result_state_ptr<T, E> state) : state_(std::move(state)) {}

  future_result(const future_result &) = delete;
  future_result &operator=(const future_result &) = delete;
  future_result(future_result &&) noexcept = default;
  future_result &operator=(future_result &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(state_); }

  void wait() const {
    if (!state_) {
      return;
    }

    std::unique_lock<tools::critical_section> lock(state_->mutex_);
    state_->cv_.wait(lock, [this] { return state_->ready_; });
  }

  template <typename Rep, typename Period>
  [[nodiscard]] future_status wait_for(const std::chrono::duration<Rep, Period> &rel_time) const {
    return wait_until(std::chrono::steady_clock::now() + rel_time);
  }

  template <typename Clock, typename Duration>
  [[nodiscard]] future_status wait_until(const std::chrono::time_point<Clock, Duration> &abs_time) const {
    return detail::wait_until_ready(state_, abs_time);
  }

  [[nodiscard]] bool is_ready() const {
    if (!state_) {
      return false;
    }
    std::lock_guard<tools::critical_section> guard(state_->mutex_);
    return state_->ready_;
  }

  result_type get_result() {
    if (!state_) {
      return make_expected_error<T, E>(E::no_state);
    }

    wait();
    return take_ready_result();
  }

  // Internal helper for combinators that are invoked only after readiness.
  result_type take_ready_result() {
    if (!state_) {
      return make_expected_error<T, E>(E::no_state);
    }

    std::optional<result_type> out;
    {
      std::lock_guard<tools::critical_section> guard(state_->mutex_);
      out.emplace(std::move(*state_->result_));
      state_->result_.reset();
    } // mutex released before shared_ptr drops its ref, preventing use-after-free
    state_.reset();
    return std::move(*out);
  }

  template <typename F>
  auto then_value(F &&fn) &&
      -> future_result<detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>, E> {
    using next_value_t = detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>;
    using next_raw_t = typename detail::result_then_value_type<F, T>::raw_type;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
      next_promise.set_error(E::no_state);
      return next_future;
    }

    struct Ctx {
      future_result self;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{std::move(*this), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      result_type current = ctx->self.take_ready_result();
      if (!current.has_value()) {
        ctx->promise.set_error(current.error());
        return;
      }

#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (std::is_void<T>::value) {
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn());
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn();
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn());
          }
        } else {
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise,
                                          ctx->fn(std::move(current).value()));
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn(std::move(current).value());
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(std::move(current).value()));
          }
        }
      } catch (...) {
        ctx->promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (std::is_void<T>::value) {
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(ctx->promise, ctx->fn());
        } else if constexpr (std::is_void<next_value_t>::value) {
          ctx->fn();
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn());
        }
      } else {
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(ctx->promise,
                                        ctx->fn(std::move(current).value()));
        } else if constexpr (std::is_void<next_value_t>::value) {
          ctx->fn(std::move(current).value());
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(std::move(current).value()));
        }
      }
#endif
    });

    return next_future;
  }

  template <typename F> auto then_error(F &&fn) && -> future_result<T, E> {
    promise_result<T, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (std::is_void<T>::value) {
          std::forward<F>(fn)(E::no_state);
          next_promise.set_value();
        } else {
          next_promise.set_value(std::forward<F>(fn)(E::no_state));
        }
      } catch (...) {
        next_promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (std::is_void<T>::value) {
        std::forward<F>(fn)(E::no_state);
        next_promise.set_value();
      } else {
        next_promise.set_value(std::forward<F>(fn)(E::no_state));
      }
#endif
      return next_future;
    }

    struct Ctx {
      future_result self;
      std::decay_t<F> fn;
      promise_result<T, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{std::move(*this), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      result_type current = ctx->self.take_ready_result();
      if (current.has_value()) {
        ctx->promise.set_result(std::move(current));
        return;
      }

#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (std::is_void<T>::value) {
          ctx->fn(current.error());
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(current.error()));
        }
      } catch (...) {
        ctx->promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (std::is_void<T>::value) {
        ctx->fn(current.error());
        ctx->promise.set_value();
      } else {
        ctx->promise.set_value(ctx->fn(current.error()));
      }
#endif
    });

    return next_future;
  }

  template <typename F>
  auto then_result(F &&fn) &&
      -> future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, result_type>>, E> {
    using traits_t = detail::result_then_result_type<F, result_type>;
    using next_value_t = detail::unwrapped_result_value_t<traits_t>;
    using next_raw_t = typename traits_t::raw_type;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
      result_type current = make_expected_error<T, E>(E::no_state);
#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(next_promise, std::forward<F>(fn)(std::move(current)));
        } else if constexpr (std::is_void<next_value_t>::value) {
          std::forward<F>(fn)(std::move(current));
          next_promise.set_value();
        } else {
          next_promise.set_value(std::forward<F>(fn)(std::move(current)));
        }
      } catch (...) {
        next_promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (detail::is_result_handle<next_raw_t>::value) {
        detail::resolve_nested_handle(next_promise, std::forward<F>(fn)(std::move(current)));
      } else if constexpr (std::is_void<next_value_t>::value) {
        std::forward<F>(fn)(std::move(current));
        next_promise.set_value();
      } else {
        next_promise.set_value(std::forward<F>(fn)(std::move(current)));
      }
#endif
      return next_future;
    }

    struct Ctx {
      future_result self;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{std::move(*this), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      result_type current = ctx->self.take_ready_result();

#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
        } else if constexpr (std::is_void<next_value_t>::value) {
          ctx->fn(std::move(current));
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(std::move(current)));
        }
      } catch (...) {
        ctx->promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (detail::is_result_handle<next_raw_t>::value) {
        detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
      } else if constexpr (std::is_void<next_value_t>::value) {
        ctx->fn(std::move(current));
        ctx->promise.set_value();
      } else {
        ctx->promise.set_value(ctx->fn(std::move(current)));
      }
#endif
    });

    return next_future;
  }

  template <typename F>
  auto then(F &&fn) &&
      -> future_result<detail::interrupt_promise_arg_t<F, future_result<T, E>, E>, E> {
    return std::move(*this).then(inplace_executor, std::forward<F>(fn));
  }

  template <typename Exec, typename F>
  auto then(Exec &&exec, F &&fn) &&
      -> future_result<detail::interrupt_promise_arg_t<F, future_result<T, E>, E>, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    using next_value_t = detail::interrupt_promise_arg_t<F, future_result<T, E>, E>;
    using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                                            std::reference_wrapper<std::remove_reference_t<Exec>>,
                                            std::decay_t<Exec>>;

    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
      next_promise.set_error(E::no_state);
      return next_future;
    }

    struct Ctx {
      future_result self;
      exec_store_t exec;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto stored_exec = [&]() -> exec_store_t {
      if constexpr (std::is_lvalue_reference<Exec>::value) {
        return std::ref(exec);
      } else {
        return std::forward<Exec>(exec);
      }
    }();

    auto ctx = std::make_shared<Ctx>(
        Ctx{std::move(*this), std::move(stored_exec), std::forward<F>(fn), std::move(next_promise)});

    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      if constexpr (std::is_lvalue_reference<Exec>::value) {
        post(ctx->exec.get(), [ctx]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
          std::invoke(ctx->fn, std::move(ctx->promise), std::move(ctx->self));
        });
      } else {
        post(ctx->exec, [ctx]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
          std::invoke(ctx->fn, std::move(ctx->promise), std::move(ctx->self));
        });
      }
    });

    return next_future;
  }

  template <typename F>
  auto next(F &&fn) &&
      -> decltype(std::move(*this).then_value(std::forward<F>(fn))) {
    return std::move(*this).then_value(std::forward<F>(fn));
  }

  // Executor-aware then_value: dispatches continuation to executor
  template <typename Exec, typename F>
  auto then_value(Exec &&exec, F &&fn) &&
      -> future_result<detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    using next_value_t = detail::unwrapped_result_value_t<detail::result_then_value_type<F, T>>;
    using next_raw_t = typename detail::result_then_value_type<F, T>::raw_type;
    using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                                            std::reference_wrapper<std::remove_reference_t<Exec>>,
                                            std::decay_t<Exec>>;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
      next_promise.set_error(E::no_state);
      return next_future;
    }

    struct Ctx {
      future_result self;
      exec_store_t exec;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto stored_exec = [&]() -> exec_store_t {
      if constexpr (std::is_lvalue_reference<Exec>::value) {
        return std::ref(exec);
      } else {
        return std::forward<Exec>(exec);
      }
    }();

    auto ctx = std::make_shared<Ctx>(Ctx{std::move(*this), std::move(stored_exec), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      result_type current = ctx->self.take_ready_result();
      if (!current.has_value()) {
        ctx->promise.set_error(current.error());
        return;
      }

      if constexpr (std::is_lvalue_reference<Exec>::value) {
        post(ctx->exec.get(), [ctx, current = std::move(current)]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
#if defined(PC_V2_HAS_EXCEPTIONS)
          try {
            if constexpr (std::is_void<T>::value) {
              if constexpr (detail::is_result_handle<next_raw_t>::value) {
                detail::resolve_nested_handle(ctx->promise, ctx->fn());
              } else if constexpr (std::is_void<next_value_t>::value) {
                ctx->fn();
                ctx->promise.set_value();
              } else {
                ctx->promise.set_value(ctx->fn());
              }
            } else {
              if constexpr (detail::is_result_handle<next_raw_t>::value) {
                detail::resolve_nested_handle(ctx->promise,
                                              ctx->fn(std::move(current).value()));
              } else if constexpr (std::is_void<next_value_t>::value) {
                ctx->fn(std::move(current).value());
                ctx->promise.set_value();
              } else {
                ctx->promise.set_value(ctx->fn(std::move(current).value()));
              }
            }
          } catch (...) {
            ctx->promise.set_error(E::continuation_failure);
          }
#else
          if constexpr (std::is_void<T>::value) {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise, ctx->fn());
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn();
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn());
            }
          } else {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise,
                                            ctx->fn(std::move(current).value()));
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn(std::move(current).value());
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn(std::move(current).value()));
            }
          }
#endif
        });
      } else {
        post(ctx->exec, [ctx, current = std::move(current)]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
#if defined(PC_V2_HAS_EXCEPTIONS)
          try {
            if constexpr (std::is_void<T>::value) {
              if constexpr (detail::is_result_handle<next_raw_t>::value) {
                detail::resolve_nested_handle(ctx->promise, ctx->fn());
              } else if constexpr (std::is_void<next_value_t>::value) {
                ctx->fn();
                ctx->promise.set_value();
              } else {
                ctx->promise.set_value(ctx->fn());
              }
            } else {
              if constexpr (detail::is_result_handle<next_raw_t>::value) {
                detail::resolve_nested_handle(ctx->promise,
                                              ctx->fn(std::move(current).value()));
              } else if constexpr (std::is_void<next_value_t>::value) {
                ctx->fn(std::move(current).value());
                ctx->promise.set_value();
              } else {
                ctx->promise.set_value(ctx->fn(std::move(current).value()));
              }
            }
          } catch (...) {
            ctx->promise.set_error(E::continuation_failure);
          }
#else
          if constexpr (std::is_void<T>::value) {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise, ctx->fn());
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn();
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn());
            }
          } else {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise,
                                            ctx->fn(std::move(current).value()));
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn(std::move(current).value());
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn(std::move(current).value()));
            }
          }
#endif
        });
      }
    });

    return next_future;
  }

  template <typename Exec, typename F>
  auto next(Exec &&exec, F &&fn) &&
      -> decltype(std::move(*this).then_value(std::forward<Exec>(exec), std::forward<F>(fn))) {
    return std::move(*this).then_value(std::forward<Exec>(exec), std::forward<F>(fn));
  }

  // Executor-aware then_error: dispatches error handler to executor
  template <typename Exec, typename F>
  auto then_error(Exec &&exec, F &&fn) &&
      -> future_result<T, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                                            std::reference_wrapper<std::remove_reference_t<Exec>>,
                                            std::decay_t<Exec>>;

    promise_result<T, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
      post(std::forward<Exec>(exec),
           [fn = std::forward<F>(fn), promise = std::move(next_promise)]() mutable {
             if (!promise.is_awaiten()) {
               return;
             }
#if defined(PC_V2_HAS_EXCEPTIONS)
             try {
               if constexpr (std::is_void<T>::value) {
                 fn(E::no_state);
                 promise.set_value();
               } else {
                 promise.set_value(fn(E::no_state));
               }
             } catch (...) {
               promise.set_error(E::continuation_failure);
             }
#else
             if constexpr (std::is_void<T>::value) {
               fn(E::no_state);
               promise.set_value();
             } else {
               promise.set_value(fn(E::no_state));
             }
#endif
           });
      return next_future;
    }

    struct Ctx {
      future_result self;
      exec_store_t exec;
      std::decay_t<F> fn;
      promise_result<T, E> promise;
    };

    auto stored_exec = [&]() -> exec_store_t {
      if constexpr (std::is_lvalue_reference<Exec>::value) {
        return std::ref(exec);
      } else {
        return std::forward<Exec>(exec);
      }
    }();

    auto ctx = std::make_shared<Ctx>(Ctx{std::move(*this), std::move(stored_exec), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      result_type current = ctx->self.take_ready_result();
      if (current.has_value()) {
        ctx->promise.set_result(std::move(current));
        return;
      }

      if constexpr (std::is_lvalue_reference<Exec>::value) {
        post(ctx->exec.get(), [ctx, error = current.error()]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
#if defined(PC_V2_HAS_EXCEPTIONS)
          try {
            if constexpr (std::is_void<T>::value) {
              ctx->fn(error);
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn(error));
            }
          } catch (...) {
            ctx->promise.set_error(E::continuation_failure);
          }
#else
          if constexpr (std::is_void<T>::value) {
            ctx->fn(error);
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(error));
          }
#endif
        });
      } else {
        post(ctx->exec, [ctx, error = current.error()]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
#if defined(PC_V2_HAS_EXCEPTIONS)
          try {
            if constexpr (std::is_void<T>::value) {
              ctx->fn(error);
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn(error));
            }
          } catch (...) {
            ctx->promise.set_error(E::continuation_failure);
          }
#else
          if constexpr (std::is_void<T>::value) {
            ctx->fn(error);
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(error));
          }
#endif
        });
      }
    });

    return next_future;
  }

  // Executor-aware then_result: dispatches full result handler to executor
  template <typename Exec, typename F>
  auto then_result(Exec &&exec, F &&fn) &&
      -> future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, result_type>>, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    using traits_t = detail::result_then_result_type<F, result_type>;
    using next_value_t = detail::unwrapped_result_value_t<traits_t>;
    using next_raw_t = typename traits_t::raw_type;
    using exec_store_t = std::conditional_t<std::is_lvalue_reference<Exec>::value,
                                            std::reference_wrapper<std::remove_reference_t<Exec>>,
                                            std::decay_t<Exec>>;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
      result_type current = make_expected_error<T, E>(E::no_state);
      post(std::forward<Exec>(exec),
           [current = std::move(current),
            fn = std::forward<F>(fn),
            promise = std::move(next_promise)]() mutable {
             if (!promise.is_awaiten()) {
               return;
             }
#if defined(PC_V2_HAS_EXCEPTIONS)
             try {
               if constexpr (detail::is_result_handle<next_raw_t>::value) {
                 detail::resolve_nested_handle(promise, fn(std::move(current)));
               } else if constexpr (std::is_void<next_value_t>::value) {
                 fn(std::move(current));
                 promise.set_value();
               } else {
                 promise.set_value(fn(std::move(current)));
               }
             } catch (...) {
               promise.set_error(E::continuation_failure);
             }
#else
             if constexpr (detail::is_result_handle<next_raw_t>::value) {
               detail::resolve_nested_handle(promise, fn(std::move(current)));
             } else if constexpr (std::is_void<next_value_t>::value) {
               fn(std::move(current));
               promise.set_value();
             } else {
               promise.set_value(fn(std::move(current)));
             }
#endif
           });
      return next_future;
    }

    struct Ctx {
      future_result self;
      exec_store_t exec;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto stored_exec = [&]() -> exec_store_t {
      if constexpr (std::is_lvalue_reference<Exec>::value) {
        return std::ref(exec);
      } else {
        return std::forward<Exec>(exec);
      }
    }();

    auto ctx = std::make_shared<Ctx>(Ctx{std::move(*this), std::move(stored_exec), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      result_type current = ctx->self.take_ready_result();
      if constexpr (std::is_lvalue_reference<Exec>::value) {
        post(ctx->exec.get(), [ctx, current = std::move(current)]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
#if defined(PC_V2_HAS_EXCEPTIONS)
          try {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn(std::move(current));
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn(std::move(current)));
            }
          } catch (...) {
            ctx->promise.set_error(E::continuation_failure);
          }
#else
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn(std::move(current));
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(std::move(current)));
          }
#endif
        });
      } else {
        post(ctx->exec, [ctx, current = std::move(current)]() mutable {
          if (!ctx->promise.is_awaiten()) {
            return;
          }
#if defined(PC_V2_HAS_EXCEPTIONS)
          try {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn(std::move(current));
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn(std::move(current)));
            }
          } catch (...) {
            ctx->promise.set_error(E::continuation_failure);
          }
#else
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn(std::move(current)));
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn(std::move(current));
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(std::move(current)));
          }
#endif
        });
      }
    });

    return next_future;
  }

  // Register a callback invoked when this future becomes ready.
  // If already ready the callback is invoked inline (lock released first).
  // Silently dropped if the future has no state.
  void subscribe(std::function<void()> cb) {
    if (!state_) {
      return;
    }
    std::unique_lock<tools::critical_section> lock(state_->mutex_);
    if (state_->ready_) {
      lock.unlock();
      cb();
      return;
    }
    state_->on_ready_cbs_.push_back(std::move(cb));
  }

  template <typename F>
  void notify(F &&notification) {
    auto cb = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
    subscribe([cb]() mutable { (*cb)(); });
  }

  template <typename Exec, typename F>
  void notify(Exec &&exec, F &&notification) {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    auto exec_obj = std::make_shared<std::decay_t<Exec>>(std::forward<Exec>(exec));
    auto cb = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
    subscribe([exec_obj, cb]() mutable {
      post(*exec_obj, [cb]() mutable { (*cb)(); });
    });
  }

  // Keeps the shared state alive until readiness and transfers this handle
  // out, leaving the source invalid.
  future_result detach() {
    if (state_) {
      auto keep_alive = state_;
      std::unique_lock<tools::critical_section> lock(state_->mutex_);
      if (!state_->ready_) {
        state_->on_ready_cbs_.push_back([keep_alive]() mutable {});
      }
    }
    return std::move(*this);
  }

  shared_result<T, E> share() && {
    return shared_result<T, E>(std::exchange(state_, nullptr));
  }

private:
  detail::result_state_ptr<T, E> state_;
};

template <typename T, typename E> class shared_result {
static_assert(!std::is_reference_v<T>,
              "portable_concurrency v2 does not support reference return types. "
              "Use pointers or std::reference_wrapper<T> instead. "
              "This design constraint avoids move-semantics ambiguity and lifetime issues.");
public:
  using value_type = T;
  using error_type = E;
  using result_type = expected<T, E>;

  shared_result() = default;

  explicit shared_result(detail::result_state_ptr<T, E> state) : state_(std::move(state)) {}

  shared_result(const shared_result &other) : state_(other.state_) {}

  shared_result &operator=(const shared_result &other) {
    if (this != &other) {
      state_ = other.state_;
      invalid_result_.reset();
    }
    return *this;
  }

  shared_result(shared_result &&) noexcept = default;
  shared_result &operator=(shared_result &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(state_); }

  void wait() const {
    if (!state_) {
      return;
    }

    std::unique_lock<tools::critical_section> lock(state_->mutex_);
    state_->cv_.wait(lock, [this] { return state_->ready_; });
  }

  template <typename Rep, typename Period>
  [[nodiscard]] future_status wait_for(const std::chrono::duration<Rep, Period> &rel_time) const {
    return wait_until(std::chrono::steady_clock::now() + rel_time);
  }

  template <typename Clock, typename Duration>
  [[nodiscard]] future_status wait_until(const std::chrono::time_point<Clock, Duration> &abs_time) const {
    return detail::wait_until_ready(state_, abs_time);
  }

  [[nodiscard]] bool is_ready() const {
    if (!state_) {
      return false;
    }
    std::lock_guard<tools::critical_section> guard(state_->mutex_);
    return state_->ready_;
  }

  const result_type &get_result() const {
    if (!state_) {
      if (!invalid_result_) {
        invalid_result_ = std::make_unique<result_type>(make_expected_error<T, E>(E::no_state));
      }
      return *invalid_result_;
    }

    wait();
    std::lock_guard<tools::critical_section> guard(state_->mutex_);
    return *state_->result_;
  }

  void subscribe(std::function<void()> cb) const {
    if (!state_) {
      return;
    }
    std::unique_lock<tools::critical_section> lock(state_->mutex_);
    if (state_->ready_) {
      lock.unlock();
      cb();
      return;
    }
    state_->on_ready_cbs_.push_back(std::move(cb));
  }

  template <typename F>
  void notify(F &&notification) const {
    auto cb = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
    subscribe([cb]() mutable { (*cb)(); });
  }

  template <typename Exec, typename F>
  void notify(Exec &&exec, F &&notification) const {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    auto exec_obj = std::make_shared<std::decay_t<Exec>>(std::forward<Exec>(exec));
    auto cb = std::make_shared<std::decay_t<F>>(std::forward<F>(notification));
    subscribe([exec_obj, cb]() mutable {
      post(*exec_obj, [cb]() mutable { (*cb)(); });
    });
  }

  // Keeps the shared state alive until readiness and transfers this handle
  // out, leaving the source invalid.
  shared_result detach() {
    if (state_) {
      auto keep_alive = state_;
      std::unique_lock<tools::critical_section> lock(state_->mutex_);
      if (!state_->ready_) {
        state_->on_ready_cbs_.push_back([keep_alive]() mutable {});
      }
    }
    return std::move(*this);
  }

  template <typename F>
  auto then_value(F &&fn) const
      -> future_result<detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>, E> {
    using next_value_t = detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>;
    using next_raw_t = typename detail::result_shared_then_value_type<F, T>::raw_type;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    struct Ctx {
      shared_result self;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{*this, std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }
      const auto &current = ctx->self.get_result();
      if (!current.has_value()) {
        ctx->promise.set_error(current.error());
        return;
      }

#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (std::is_void<T>::value) {
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn());
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn();
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn());
          }
        } else {
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn(current.value()));
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn(current.value());
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(current.value()));
          }
        }
      } catch (...) {
        ctx->promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (std::is_void<T>::value) {
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(ctx->promise, ctx->fn());
        } else if constexpr (std::is_void<next_value_t>::value) {
          ctx->fn();
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn());
        }
      } else {
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(ctx->promise, ctx->fn(current.value()));
        } else if constexpr (std::is_void<next_value_t>::value) {
          ctx->fn(current.value());
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(current.value()));
        }
      }
#endif
    });

    return next_future;
  }

  template <typename Exec, typename F>
  auto then_value(Exec &&exec, F &&fn) const
      -> future_result<detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    using next_value_t = detail::unwrapped_result_value_t<detail::result_shared_then_value_type<F, T>>;
    using next_raw_t = typename detail::result_shared_then_value_type<F, T>::raw_type;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    struct Ctx {
      shared_result self;
      std::decay_t<Exec> exec;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{*this, std::forward<Exec>(exec), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }
      const auto &current = ctx->self.get_result();
      if (!current.has_value()) {
        ctx->promise.set_error(current.error());
        return;
      }

      post(ctx->exec, [ctx]() mutable {
        if (!ctx->promise.is_awaiten()) {
          return;
        }
        const auto &ready = ctx->self.get_result();
#if defined(PC_V2_HAS_EXCEPTIONS)
        try {
          if constexpr (std::is_void<T>::value) {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise, ctx->fn());
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn();
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn());
            }
          } else {
            if constexpr (detail::is_result_handle<next_raw_t>::value) {
              detail::resolve_nested_handle(ctx->promise, ctx->fn(ready.value()));
            } else if constexpr (std::is_void<next_value_t>::value) {
              ctx->fn(ready.value());
              ctx->promise.set_value();
            } else {
              ctx->promise.set_value(ctx->fn(ready.value()));
            }
          }
        } catch (...) {
          ctx->promise.set_error(E::continuation_failure);
        }
#else
        if constexpr (std::is_void<T>::value) {
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn());
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn();
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn());
          }
        } else {
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn(ready.value()));
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn(ready.value());
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(ready.value()));
          }
        }
#endif
      });
    });

    return next_future;
  }

  template <typename F>
  auto then(F &&fn) const
      -> future_result<detail::interrupt_promise_arg_t<F, shared_result<T, E>, E>, E> {
    return this->then(inplace_executor, std::forward<F>(fn));
  }

  template <typename Exec, typename F>
  auto then(Exec &&exec, F &&fn) const
      -> future_result<detail::interrupt_promise_arg_t<F, shared_result<T, E>, E>, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    using next_value_t = detail::interrupt_promise_arg_t<F, shared_result<T, E>, E>;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    if (!state_) {
      next_promise.set_error(E::no_state);
      return next_future;
    }

    struct Ctx {
      shared_result self;
      std::decay_t<Exec> exec;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(
        Ctx{*this, std::forward<Exec>(exec), std::forward<F>(fn), std::move(next_promise)});

    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }

      post(ctx->exec, [ctx]() mutable {
        if (!ctx->promise.is_awaiten()) {
          return;
        }
        std::invoke(ctx->fn, std::move(ctx->promise), ctx->self);
      });
    });

    return next_future;
  }

  template <typename F>
  auto next(F &&fn) const
      -> decltype(this->then_value(std::forward<F>(fn))) {
    return this->then_value(std::forward<F>(fn));
  }

  template <typename Exec, typename F>
  auto next(Exec &&exec, F &&fn) const
      -> decltype(this->then_value(std::forward<Exec>(exec), std::forward<F>(fn))) {
    return this->then_value(std::forward<Exec>(exec), std::forward<F>(fn));
  }

  template <typename F, typename U = T,
            typename std::enable_if<std::is_void<U>::value || std::is_copy_constructible<U>::value,
                                    int>::type = 0>
  auto then_error(F &&fn) const -> future_result<T, E> {
    promise_result<T, E> next_promise;
    auto next_future = next_promise.get_future();

    struct Ctx {
      shared_result self;
      std::decay_t<F> fn;
      promise_result<T, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{*this, std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }
      const auto &current = ctx->self.get_result();
      if (current.has_value()) {
        if constexpr (std::is_void<T>::value) {
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(current.value());
        }
        return;
      }

#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (std::is_void<T>::value) {
          ctx->fn(current.error());
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(current.error()));
        }
      } catch (...) {
        ctx->promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (std::is_void<T>::value) {
        ctx->fn(current.error());
        ctx->promise.set_value();
      } else {
        ctx->promise.set_value(ctx->fn(current.error()));
      }
#endif
    });

    return next_future;
  }

  template <typename Exec, typename F, typename U = T,
            typename std::enable_if<std::is_void<U>::value || std::is_copy_constructible<U>::value,
                                    int>::type = 0>
  auto then_error(Exec &&exec, F &&fn) const -> future_result<T, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    promise_result<T, E> next_promise;
    auto next_future = next_promise.get_future();

    struct Ctx {
      shared_result self;
      std::decay_t<Exec> exec;
      std::decay_t<F> fn;
      promise_result<T, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{*this, std::forward<Exec>(exec), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }
      const auto &current = ctx->self.get_result();
      if (current.has_value()) {
        if constexpr (std::is_void<T>::value) {
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(current.value());
        }
        return;
      }

      post(ctx->exec, [ctx]() mutable {
        if (!ctx->promise.is_awaiten()) {
          return;
        }
        const auto &ready = ctx->self.get_result();
#if defined(PC_V2_HAS_EXCEPTIONS)
        try {
          if constexpr (std::is_void<T>::value) {
            ctx->fn(ready.error());
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(ready.error()));
          }
        } catch (...) {
          ctx->promise.set_error(E::continuation_failure);
        }
#else
        if constexpr (std::is_void<T>::value) {
          ctx->fn(ready.error());
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(ready.error()));
        }
#endif
      });
    });

    return next_future;
  }

  template <typename F>
  auto then_result(F &&fn) const
      -> future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, const result_type &>>, E> {
    using traits_t = detail::result_then_result_type<F, const result_type &>;
    using next_value_t = detail::unwrapped_result_value_t<traits_t>;
    using next_raw_t = typename traits_t::raw_type;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    struct Ctx {
      shared_result self;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{*this, std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }
      const auto &current = ctx->self.get_result();
#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
        } else if constexpr (std::is_void<next_value_t>::value) {
          ctx->fn(current);
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(current));
        }
      } catch (...) {
        ctx->promise.set_error(E::continuation_failure);
      }
#else
      if constexpr (detail::is_result_handle<next_raw_t>::value) {
        detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
      } else if constexpr (std::is_void<next_value_t>::value) {
        ctx->fn(current);
        ctx->promise.set_value();
      } else {
        ctx->promise.set_value(ctx->fn(current));
      }
#endif
    });

    return next_future;
  }

  template <typename Exec, typename F>
  auto then_result(Exec &&exec, F &&fn) const
      -> future_result<detail::unwrapped_result_value_t<detail::result_then_result_type<F, const result_type &>>, E> {
    static_assert(is_executor<std::decay_t<Exec>>::value,
                  "Exec must satisfy portable_concurrency::is_executor");

    using traits_t = detail::result_then_result_type<F, const result_type &>;
    using next_value_t = detail::unwrapped_result_value_t<traits_t>;
    using next_raw_t = typename traits_t::raw_type;
    promise_result<next_value_t, E> next_promise;
    auto next_future = next_promise.get_future();

    struct Ctx {
      shared_result self;
      std::decay_t<Exec> exec;
      std::decay_t<F> fn;
      promise_result<next_value_t, E> promise;
    };

    auto ctx = std::make_shared<Ctx>(Ctx{*this, std::forward<Exec>(exec), std::forward<F>(fn), std::move(next_promise)});
    ctx->self.subscribe([ctx]() mutable {
      if (!ctx->promise.is_awaiten()) {
        return;
      }
      post(ctx->exec, [ctx]() mutable {
        if (!ctx->promise.is_awaiten()) {
          return;
        }
        const auto &current = ctx->self.get_result();
#if defined(PC_V2_HAS_EXCEPTIONS)
        try {
          if constexpr (detail::is_result_handle<next_raw_t>::value) {
            detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
          } else if constexpr (std::is_void<next_value_t>::value) {
            ctx->fn(current);
            ctx->promise.set_value();
          } else {
            ctx->promise.set_value(ctx->fn(current));
          }
        } catch (...) {
          ctx->promise.set_error(E::continuation_failure);
        }
#else
        if constexpr (detail::is_result_handle<next_raw_t>::value) {
          detail::resolve_nested_handle(ctx->promise, ctx->fn(current));
        } else if constexpr (std::is_void<next_value_t>::value) {
          ctx->fn(current);
          ctx->promise.set_value();
        } else {
          ctx->promise.set_value(ctx->fn(current));
        }
#endif
      });
    });

    return next_future;
  }

private:
  detail::result_state_ptr<T, E> state_;
  mutable std::unique_ptr<result_type> invalid_result_;
};

template <typename T, typename E = result_error>
std::pair<promise_result<T, E>, future_result<T, E>> make_result_promise() {
  promise_result<T, E> promise;
  auto future = promise.get_future();
  return {std::move(promise), std::move(future)};
}

template <typename T, typename E = result_error, typename F>
std::pair<promise_result<T, E>, future_result<T, E>> make_result_promise(canceler_arg_t tag, F &&cancel_action) {
  promise_result<T, E> promise(tag, std::forward<F>(cancel_action));
  auto future = promise.get_future();
  return {std::move(promise), std::move(future)};
}

template <typename T, typename E = result_error>
future_result<std::decay_t<T>, E> make_ready_result(T &&value) {
  auto promise_and_future = make_result_promise<std::decay_t<T>, E>();
  promise_and_future.first.set_value(std::forward<T>(value));
  return std::move(promise_and_future.second);
}

template <typename E = result_error>
future_result<void, E> make_ready_result() {
  auto promise_and_future = make_result_promise<void, E>();
  promise_and_future.first.set_value();
  return std::move(promise_and_future.second);
}

template <typename T, typename E = result_error>
future_result<T, E> make_error_result(E error) {
  auto promise_and_future = make_result_promise<T, E>();
  promise_and_future.first.set_error(std::move(error));
  return std::move(promise_and_future.second);
}

inline future_result<std::tuple<>, result_error> when_all() {
  auto promise_and_future = make_result_promise<std::tuple<>, result_error>();
  auto promise = std::move(promise_and_future.first);
  auto future = std::move(promise_and_future.second);
  promise.set_value(std::tuple<>{});
  return future;
}

inline future_result<when_any_result<std::tuple<>>, result_error> when_any() {
  auto promise_and_future = make_result_promise<when_any_result<std::tuple<>>, result_error>();
  auto promise = std::move(promise_and_future.first);
  auto future = std::move(promise_and_future.second);
  promise.set_value(when_any_result<std::tuple<>>{static_cast<std::size_t>(-1), std::tuple<>{}});
  return future;
}

template <typename... Futures>
auto when_all(Futures &&...futures)
    -> std::enable_if_t<detail::are_result_futures<Futures...>::value,
                        future_result<std::tuple<typename std::decay_t<Futures>::result_type...>,
                                      typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::error_type>> {
  using first_future_t = std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>;
  using error_t = typename first_future_t::error_type;

  static_assert((std::is_same<error_t, typename std::decay_t<Futures>::error_type>::value && ...),
                "All futures passed to v2::when_all must share the same error type");

  using results_tuple_t = std::tuple<typename std::decay_t<Futures>::result_type...>;

  auto promise_and_future = make_result_promise<results_tuple_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  auto future_tuple = std::make_tuple(std::forward<Futures>(futures)...);

  bool all_valid = true;
  std::apply([&](auto &...f) { ((all_valid = all_valid && f.valid()), ...); }, future_tuple);
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  struct WhenAllCtx {
    std::atomic<std::size_t> remaining_;
    std::tuple<std::decay_t<Futures>...> futures;
    promise_result<results_tuple_t, error_t> promise;

    WhenAllCtx(std::size_t remaining,
               std::tuple<std::decay_t<Futures>...> f,
               promise_result<results_tuple_t, error_t> p)
        : remaining_(remaining), futures(std::move(f)), promise(std::move(p)) {}
  };

  auto ctx = std::make_shared<WhenAllCtx>(sizeof...(Futures), std::move(future_tuple), std::move(promise));

  auto subscribe_one = [&](auto &f) {
    f.subscribe([ctx]() mutable {
      if (ctx->remaining_.fetch_sub(1) == 1) {
        auto results = std::apply(
            [](auto &...ready_futures) {
              return results_tuple_t{ready_futures.take_ready_result()...};
            },
            ctx->futures);
        ctx->promise.set_value(std::move(results));
      }
    });
  };

  std::apply([&](auto &...f) { (subscribe_one(f), ...); }, ctx->futures);

  return combined_future;
}

template <typename... SharedResults>
auto when_all(SharedResults &&...shared_results)
    -> std::enable_if_t<detail::are_shared_results<SharedResults...>::value,
                        future_result<std::tuple<std::decay_t<SharedResults>...>,
                                      typename std::decay_t<std::tuple_element_t<0, std::tuple<SharedResults...>>>::error_type>> {
  using first_shared_t = std::decay_t<std::tuple_element_t<0, std::tuple<SharedResults...>>>;
  using error_t = typename first_shared_t::error_type;
  using shared_tuple_t = std::tuple<std::decay_t<SharedResults>...>;

  static_assert((std::is_same<error_t, typename std::decay_t<SharedResults>::error_type>::value && ...),
                "All shared_results passed to v2::when_all must share the same error type");

  auto promise_and_future = make_result_promise<shared_tuple_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  auto shared_tuple = std::make_tuple(std::forward<SharedResults>(shared_results)...);

  bool all_valid = true;
  std::apply([&](auto &...shared) { ((all_valid = all_valid && shared.valid()), ...); }, shared_tuple);
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  struct WhenAllSharedCtx {
    std::atomic<std::size_t> remaining_;
    shared_tuple_t shareds;
    promise_result<shared_tuple_t, error_t> promise;

    WhenAllSharedCtx(std::size_t remaining,
                     shared_tuple_t input_shareds,
                     promise_result<shared_tuple_t, error_t> input_promise)
        : remaining_(remaining), shareds(std::move(input_shareds)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAllSharedCtx>(sizeof...(SharedResults), std::move(shared_tuple), std::move(promise));

  auto subscribe_one = [&](auto &shared) {
    shared.subscribe([ctx]() mutable {
      if (ctx->remaining_.fetch_sub(1) == 1) {
        ctx->promise.set_value(std::move(ctx->shareds));
      }
    });
  };

  std::apply([&](auto &...shared) { (subscribe_one(shared), ...); }, ctx->shareds);

  return combined_future;
}

template <typename... Handles>
auto when_all(Handles &&...handles)
    -> std::enable_if_t<detail::are_result_handles<Handles...>::value &&
                            !detail::are_result_futures<Handles...>::value &&
                            !detail::are_shared_results<Handles...>::value,
                        future_result<std::tuple<std::decay_t<Handles>...>,
                                      typename std::decay_t<std::tuple_element_t<0, std::tuple<Handles...>>>::error_type>> {
  using first_handle_t = std::decay_t<std::tuple_element_t<0, std::tuple<Handles...>>>;
  using error_t = typename first_handle_t::error_type;
  using handles_tuple_t = std::tuple<std::decay_t<Handles>...>;

  static_assert((std::is_same<error_t, typename std::decay_t<Handles>::error_type>::value && ...),
                "All handles passed to v2::when_all must share the same error type");

  auto promise_and_future = make_result_promise<handles_tuple_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  auto handle_tuple = std::make_tuple(std::forward<Handles>(handles)...);

  bool all_valid = true;
  std::apply([&](auto &...h) { ((all_valid = all_valid && h.valid()), ...); }, handle_tuple);
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  struct WhenAllMixedCtx {
    std::atomic<std::size_t> remaining_;
    handles_tuple_t handles;
    promise_result<handles_tuple_t, error_t> promise;

    WhenAllMixedCtx(std::size_t remaining,
                    handles_tuple_t input_handles,
                    promise_result<handles_tuple_t, error_t> input_promise)
        : remaining_(remaining), handles(std::move(input_handles)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAllMixedCtx>(sizeof...(Handles), std::move(handle_tuple), std::move(promise));

  auto subscribe_one = [&](auto &handle) {
    handle.subscribe([ctx]() mutable {
      if (ctx->remaining_.fetch_sub(1) == 1) {
        ctx->promise.set_value(std::move(ctx->handles));
      }
    });
  };

  std::apply([&](auto &...h) { (subscribe_one(h), ...); }, ctx->handles);

  return combined_future;
}

template <typename... Futures>
auto when_any(Futures &&...futures)
    -> std::enable_if_t<detail::are_result_handles<Futures...>::value,
                        future_result<when_any_result<std::tuple<std::decay_t<Futures>...>>,
                                      typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::error_type>> {
  using first_future_t = std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>;
  using error_t = typename first_future_t::error_type;
  using futures_tuple_t = std::tuple<std::decay_t<Futures>...>;
  using any_result_t = when_any_result<futures_tuple_t>;

  static_assert((std::is_same<error_t, typename std::decay_t<Futures>::error_type>::value && ...),
                "All futures passed to v2::when_any must share the same error type");

  auto promise_and_future = make_result_promise<any_result_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  auto future_tuple = std::make_tuple(std::forward<Futures>(futures)...);

  bool all_valid = true;
  std::apply([&](auto &...f) { ((all_valid = all_valid && f.valid()), ...); }, future_tuple);
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  constexpr std::size_t npos = static_cast<std::size_t>(-1);
  std::size_t ready_index = npos;
  {
    std::size_t idx = 0;
    auto check_ready = [&](auto &f) {
      if (ready_index == npos && f.is_ready()) {
        ready_index = idx;
      }
      ++idx;
    };
    std::apply([&](auto &...f) { (check_ready(f), ...); }, future_tuple);
  }

  if (ready_index != npos) {
    promise.set_value(any_result_t{ready_index, std::move(future_tuple)});
    return combined_future;
  }

  // No futures ready yet: register continuation callbacks — no polling thread.
  // Shared context keeps the futures tuple and promise alive until the first
  // callback fires and atomically claims ownership via done_.
  struct WhenAnyCtx {
    std::atomic<bool> done_{false};
    futures_tuple_t futures;
    promise_result<any_result_t, error_t> promise;
    WhenAnyCtx(futures_tuple_t f, promise_result<any_result_t, error_t> p)
        : futures(std::move(f)), promise(std::move(p)) {}
  };

  auto ctx = std::make_shared<WhenAnyCtx>(std::move(future_tuple), std::move(promise));

  std::size_t reg_idx = 0;
  auto register_one = [&](auto &f) {
    const std::size_t my_idx = reg_idx++;
    f.subscribe([ctx, my_idx]() mutable {
      bool expected = false;
      if (ctx->done_.compare_exchange_strong(expected, true)) {
        ctx->promise.set_value(any_result_t{my_idx, std::move(ctx->futures)});
      }
    });
  };
  std::apply([&](auto &...f) { (register_one(f), ...); }, ctx->futures);

  return combined_future;
}

template <typename InputIt>
auto when_all(InputIt first, InputIt last)
    -> std::enable_if_t<
        detail::is_result_future<typename std::iterator_traits<InputIt>::value_type>::value,
        future_result<std::vector<typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::result_type>,
                      typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::error_type>> {
  using future_t = std::decay_t<typename std::iterator_traits<InputIt>::value_type>;
  using result_t = typename future_t::result_type;
  using error_t = typename future_t::error_type;
  using futures_vector_t = std::vector<future_t>;
  using results_vector_t = std::vector<result_t>;

  auto promise_and_future = make_result_promise<results_vector_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  futures_vector_t futures(std::make_move_iterator(first), std::make_move_iterator(last));
  if (futures.empty()) {
    promise.set_value(results_vector_t{});
    return combined_future;
  }

  bool all_valid = true;
  for (auto &future : futures) {
    all_valid = all_valid && future.valid();
  }
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  struct WhenAllVectorCtx {
    std::atomic<std::size_t> remaining_;
    futures_vector_t futures;
    promise_result<results_vector_t, error_t> promise;

    WhenAllVectorCtx(std::size_t remaining,
                     futures_vector_t input_futures,
                     promise_result<results_vector_t, error_t> input_promise)
        : remaining_(remaining), futures(std::move(input_futures)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAllVectorCtx>(futures.size(), std::move(futures), std::move(promise));

  for (auto &future : ctx->futures) {
    future.subscribe([ctx]() mutable {
      if (ctx->remaining_.fetch_sub(1) == 1) {
        results_vector_t results;
        results.reserve(ctx->futures.size());
        for (auto &ready_future : ctx->futures) {
          results.push_back(ready_future.take_ready_result());
        }
        ctx->promise.set_value(std::move(results));
      }
    });
  }

  return combined_future;
}

template <typename Future, typename Alloc>
auto when_all(std::vector<Future, Alloc> futures)
    -> std::enable_if_t<detail::is_result_future<Future>::value,
                        future_result<std::vector<typename Future::result_type,
                                                  detail::rebind_alloc_t<Alloc, typename Future::result_type>>,
                                      typename Future::error_type>> {
  using future_t = Future;
  using result_t = typename future_t::result_type;
  using error_t = typename future_t::error_type;
  using futures_vector_t = std::vector<future_t, Alloc>;
  using results_vector_t = std::vector<result_t, detail::rebind_alloc_t<Alloc, result_t>>;

  auto promise_and_future = make_result_promise<results_vector_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  if (futures.empty()) {
    promise.set_value(results_vector_t{});
    return combined_future;
  }

  bool all_valid = true;
  for (auto &future : futures) {
    all_valid = all_valid && future.valid();
  }
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  struct WhenAllVectorCtx {
    std::atomic<std::size_t> remaining_;
    futures_vector_t futures;
    promise_result<results_vector_t, error_t> promise;

    WhenAllVectorCtx(std::size_t remaining,
                     futures_vector_t input_futures,
                     promise_result<results_vector_t, error_t> input_promise)
        : remaining_(remaining), futures(std::move(input_futures)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAllVectorCtx>(futures.size(), std::move(futures), std::move(promise));

  for (auto &future : ctx->futures) {
    future.subscribe([ctx]() mutable {
      if (ctx->remaining_.fetch_sub(1) == 1) {
        results_vector_t results;
        results.reserve(ctx->futures.size());
        for (auto &ready_future : ctx->futures) {
          results.push_back(ready_future.take_ready_result());
        }
        ctx->promise.set_value(std::move(results));
      }
    });
  }

  return combined_future;
}

template <typename InputIt>
auto when_all(InputIt first, InputIt last)
    -> std::enable_if_t<
        detail::is_shared_result<typename std::iterator_traits<InputIt>::value_type>::value,
        future_result<std::vector<std::decay_t<typename std::iterator_traits<InputIt>::value_type>>,
                      typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::error_type>> {
  using shared_t = std::decay_t<typename std::iterator_traits<InputIt>::value_type>;
  using error_t = typename shared_t::error_type;
  using shared_vector_t = std::vector<shared_t>;

  auto promise_and_future = make_result_promise<shared_vector_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  shared_vector_t shareds(first, last);
  if (shareds.empty()) {
    promise.set_value(shared_vector_t{});
    return combined_future;
  }

  bool all_valid = true;
  for (auto &shared : shareds) {
    all_valid = all_valid && shared.valid();
  }
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  struct WhenAllSharedVectorCtx {
    std::atomic<std::size_t> remaining_;
    shared_vector_t shareds;
    promise_result<shared_vector_t, error_t> promise;

    WhenAllSharedVectorCtx(std::size_t remaining,
                           shared_vector_t input_shareds,
                           promise_result<shared_vector_t, error_t> input_promise)
        : remaining_(remaining), shareds(std::move(input_shareds)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAllSharedVectorCtx>(shareds.size(), std::move(shareds), std::move(promise));

  for (auto &shared : ctx->shareds) {
    shared.subscribe([ctx]() mutable {
      if (ctx->remaining_.fetch_sub(1) == 1) {
        ctx->promise.set_value(std::move(ctx->shareds));
      }
    });
  }

  return combined_future;
}

template <typename SharedResult, typename Alloc>
auto when_all(std::vector<SharedResult, Alloc> shareds)
    -> std::enable_if_t<detail::is_shared_result<SharedResult>::value,
                        future_result<std::vector<SharedResult, Alloc>, typename SharedResult::error_type>> {
  using shared_t = SharedResult;
  using error_t = typename shared_t::error_type;
  using shared_vector_t = std::vector<shared_t, Alloc>;

  auto promise_and_future = make_result_promise<shared_vector_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  if (shareds.empty()) {
    promise.set_value(shared_vector_t{});
    return combined_future;
  }

  bool all_valid = true;
  for (auto &shared : shareds) {
    all_valid = all_valid && shared.valid();
  }
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  struct WhenAllSharedVectorCtx {
    std::atomic<std::size_t> remaining_;
    shared_vector_t shareds;
    promise_result<shared_vector_t, error_t> promise;

    WhenAllSharedVectorCtx(std::size_t remaining,
                           shared_vector_t input_shareds,
                           promise_result<shared_vector_t, error_t> input_promise)
        : remaining_(remaining), shareds(std::move(input_shareds)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAllSharedVectorCtx>(shareds.size(), std::move(shareds), std::move(promise));

  for (auto &shared : ctx->shareds) {
    shared.subscribe([ctx]() mutable {
      if (ctx->remaining_.fetch_sub(1) == 1) {
        ctx->promise.set_value(std::move(ctx->shareds));
      }
    });
  }

  return combined_future;
}

template <typename InputIt>
auto when_any(InputIt first, InputIt last)
    -> std::enable_if_t<
        detail::is_result_handle<typename std::iterator_traits<InputIt>::value_type>::value,
        future_result<when_any_result<std::vector<std::decay_t<typename std::iterator_traits<InputIt>::value_type>>>,
                      typename std::decay_t<typename std::iterator_traits<InputIt>::value_type>::error_type>> {
  using future_t = std::decay_t<typename std::iterator_traits<InputIt>::value_type>;
  using error_t = typename future_t::error_type;
  using futures_vector_t = std::vector<future_t>;
  using any_result_t = when_any_result<futures_vector_t>;

  auto promise_and_future = make_result_promise<any_result_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  futures_vector_t futures(std::make_move_iterator(first), std::make_move_iterator(last));
  if (futures.empty()) {
    promise.set_value(any_result_t{static_cast<std::size_t>(-1), futures_vector_t{}});
    return combined_future;
  }

  bool all_valid = true;
  for (auto &future : futures) {
    all_valid = all_valid && future.valid();
  }
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  constexpr auto npos = static_cast<std::size_t>(-1);
  std::size_t ready_index = npos;
  for (std::size_t idx = 0; idx < futures.size(); ++idx) {
    if (ready_index == npos && futures[idx].is_ready()) {
      ready_index = idx;
    }
  }
  if (ready_index != npos) {
    promise.set_value(any_result_t{ready_index, std::move(futures)});
    return combined_future;
  }

  struct WhenAnyVectorCtx {
    std::atomic<bool> done_{false};
    futures_vector_t futures;
    promise_result<any_result_t, error_t> promise;

    WhenAnyVectorCtx(futures_vector_t input_futures,
                     promise_result<any_result_t, error_t> input_promise)
        : futures(std::move(input_futures)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAnyVectorCtx>(std::move(futures), std::move(promise));

  for (std::size_t idx = 0; idx < ctx->futures.size(); ++idx) {
    ctx->futures[idx].subscribe([ctx, idx]() mutable {
      bool expected = false;
      if (ctx->done_.compare_exchange_strong(expected, true)) {
        ctx->promise.set_value(any_result_t{idx, std::move(ctx->futures)});
      }
    });
  }

  return combined_future;
}

template <typename Future, typename Alloc>
auto when_any(std::vector<Future, Alloc> futures)
    -> std::enable_if_t<detail::is_result_handle<Future>::value,
                        future_result<when_any_result<std::vector<Future, Alloc>>, typename Future::error_type>> {
  using future_t = Future;
  using error_t = typename future_t::error_type;
  using futures_vector_t = std::vector<future_t, Alloc>;
  using any_result_t = when_any_result<futures_vector_t>;

  auto promise_and_future = make_result_promise<any_result_t, error_t>();
  auto promise = std::move(promise_and_future.first);
  auto combined_future = std::move(promise_and_future.second);

  if (futures.empty()) {
    promise.set_value(any_result_t{static_cast<std::size_t>(-1), futures_vector_t{}});
    return combined_future;
  }

  bool all_valid = true;
  for (auto &future : futures) {
    all_valid = all_valid && future.valid();
  }
  if (!all_valid) {
    promise.set_error(error_t::no_state);
    return combined_future;
  }

  constexpr auto npos = static_cast<std::size_t>(-1);
  std::size_t ready_index = npos;
  for (std::size_t idx = 0; idx < futures.size(); ++idx) {
    if (ready_index == npos && futures[idx].is_ready()) {
      ready_index = idx;
    }
  }
  if (ready_index != npos) {
    promise.set_value(any_result_t{ready_index, std::move(futures)});
    return combined_future;
  }

  struct WhenAnyVectorCtx {
    std::atomic<bool> done_{false};
    futures_vector_t futures;
    promise_result<any_result_t, error_t> promise;

    WhenAnyVectorCtx(futures_vector_t input_futures,
                     promise_result<any_result_t, error_t> input_promise)
        : futures(std::move(input_futures)), promise(std::move(input_promise)) {}
  };

  auto ctx = std::make_shared<WhenAnyVectorCtx>(std::move(futures), std::move(promise));

  for (std::size_t idx = 0; idx < ctx->futures.size(); ++idx) {
    ctx->futures[idx].subscribe([ctx, idx]() mutable {
      bool expected = false;
      if (ctx->done_.compare_exchange_strong(expected, true)) {
        ctx->promise.set_value(any_result_t{idx, std::move(ctx->futures)});
      }
    });
  }

  return combined_future;
}

template <typename Exec, typename F, typename... A>
auto async_result(Exec &&exec, F &&fn, A &&...args)
    -> future_result<
           typename detail::unwrapped_result_value<
               typename detail::invoke_decay_t<F, A...>>::type,
           result_error> {
  static_assert(is_executor<std::decay_t<Exec>>::value,
                "Exec must satisfy portable_concurrency::is_executor");

  using raw_value_t = typename detail::invoke_decay_t<F, A...>;
  using value_t = typename detail::unwrapped_result_value<raw_value_t>::type;

  auto promise_and_future = make_result_promise<value_t, result_error>();
  auto promise = std::move(promise_and_future.first);
  auto future = std::move(promise_and_future.second);

  if constexpr (detail::is_result_handle<raw_value_t>::value) {
    // Callable returns future_result<T,E> or shared_result<T,E>: unwrap it
    // so async_result returns future_result<T, result_error> directly.
    //
    // Use notify (not then_result) to subscribe to the inner handle.
    // then_result creates its own intermediate promise/future; if that
    // future is discarded the promise becomes un-awaited and the callback
    // short-circuits via is_awaiten(), so the outer promise is never set.
    // notify avoids the extra promise layer entirely.
    struct UnwrapCtx {
      raw_value_t inner;
      promise_result<value_t, result_error> outer;
    };

    auto task = [promise = std::move(promise),
                 fn = std::forward<F>(fn),
                 params = std::make_tuple(std::forward<A>(args)...)]() mutable {
      // Call the callable in an inner scope so fn/params (and any captured
      // shared_ptrs) are released before we subscribe on the inner handle.
      // This also frees the pool thread to pick up the inner task.
      raw_value_t inner = [&]() {
        auto fn_local = std::move(fn);
        auto params_local = std::move(params);
        return std::apply(std::move(fn_local), std::move(params_local));
      }();

      auto ctx = std::make_shared<UnwrapCtx>(
          UnwrapCtx{std::move(inner), std::move(promise)});

      if constexpr (detail::is_result_future<raw_value_t>::value) {
        // future_result: get_result() moves the value out (non-blocking when ready).
        ctx->inner.notify([ctx]() mutable {
          auto r = ctx->inner.get_result();
          if (r.has_value()) {
            if constexpr (std::is_void_v<value_t>)
              ctx->outer.set_value();
            else
              ctx->outer.set_value(std::move(r).value());
          } else {
            ctx->outer.set_error(r.error());
          }
        });
      } else {
        // shared_result: get_result() returns const ref, does not consume.
        ctx->inner.notify([ctx]() mutable {
          const auto &r = ctx->inner.get_result();
          if (r.has_value()) {
            if constexpr (std::is_void_v<value_t>)
              ctx->outer.set_value();
            else
              ctx->outer.set_value(r.value());
          } else {
            ctx->outer.set_error(r.error());
          }
        });
      }
    };

    post(std::forward<Exec>(exec), std::move(task));
  } else {
    // Standard path: callable returns a plain value (no unwrapping needed).
    // Move fn and params into an inner scope so they are destroyed before
    // promise.set_value() is called; this ensures callable captures are
    // released before any waiter on the future can observe them.
    auto task = [promise = std::move(promise),
                 fn = std::forward<F>(fn),
                 params = std::make_tuple(std::forward<A>(args)...)]() mutable {
#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        if constexpr (std::is_void_v<value_t>) {
          {
            auto fn_local = std::move(fn);
            auto params_local = std::move(params);
            std::apply(fn_local, std::move(params_local));
          }
          promise.set_value();
        } else {
          auto val = [&]() {
            auto fn_local = std::move(fn);
            auto params_local = std::move(params);
            return std::apply(fn_local, std::move(params_local));
          }();
          promise.set_value(std::move(val));
        }
      } catch (...) {
        promise.set_error(result_error::execution_failure);
      }
#else
      if constexpr (std::is_void_v<value_t>) {
        {
          auto fn_local = std::move(fn);
          auto params_local = std::move(params);
          std::apply(fn_local, std::move(params_local));
        }
        promise.set_value();
      } else {
        auto val = [&]() {
          auto fn_local = std::move(fn);
          auto params_local = std::move(params);
          return std::apply(fn_local, std::move(params_local));
        }();
        promise.set_value(std::move(val));
      }
#endif
    };

    post(std::forward<Exec>(exec), std::move(task));
  }

  return future;
}

} // namespace v2
} // namespace cxx14_v1
} // namespace portable_concurrency
