#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "result_future.h"

namespace portable_concurrency {
inline namespace cxx14_v1 {
namespace v2 {

template <typename Signature> class packaged_task_result;

namespace detail {

template <typename R>
struct packaged_task_result_error {
  using type = result_error;
};

template <typename T, typename E>
struct packaged_task_result_error<future_result<T, E>> {
  using type = E;
};

template <typename T, typename E>
struct packaged_task_result_error<shared_result<T, E>> {
  using type = E;
};

template <typename R, typename E>
struct packaged_task_result_future {
  using type = future_result<R, E>;
};

template <typename T, typename E>
struct packaged_task_result_future<future_result<T, E>, E> {
  using type = future_result<T, E>;
};

template <typename T, typename E>
struct packaged_task_result_future<shared_result<T, E>, E> {
  using type = shared_result<T, E>;
};

template <typename R>
struct packaged_task_result_value {
  using type = R;
};

template <typename T, typename E>
struct packaged_task_result_value<future_result<T, E>> {
  using type = T;
};

template <typename T, typename E>
struct packaged_task_result_value<shared_result<T, E>> {
  using type = T;
};

} // namespace detail

template <typename R, typename... A>
class packaged_task_result<R(A...)> {
private:
  using error_type = typename detail::packaged_task_result_error<R>::type;
  using value_type = typename detail::packaged_task_result_value<R>::type;
  using future_type = typename detail::packaged_task_result_future<R, error_type>::type;

  static_assert(!std::is_reference_v<value_type>,
                "portable_concurrency v2 does not support reference return types. "
                "Use pointers or std::reference_wrapper<T> instead. "
                "This design constraint avoids move-semantics ambiguity and lifetime issues.");

  struct state_base {
    virtual ~state_base() = default;
    virtual future_type get_future() = 0;
    virtual void run(A... args) = 0;
  };

  template <typename F>
  struct state_impl final : state_base {
    explicit state_impl(F &&f) : fn(std::forward<F>(f)) {}

    future_type get_future() override {
      auto f = promise.get_future();
      if constexpr (detail::is_shared_result<std::decay_t<R>>::value) {
        return std::move(f).share();
      } else {
        return f;
      }
    }

    void run(A... args) override {
      if (!fn.has_value()) {
        return;
      }

      auto local_fn = std::move(*fn);
      fn.reset();

#if defined(PC_V2_HAS_EXCEPTIONS)
      try {
        invoke_and_set(local_fn, std::forward<A>(args)...);
      } catch (...) {
        promise.set_error(error_type::execution_failure);
      }
#else
      invoke_and_set(local_fn, std::forward<A>(args)...);
#endif
    }

    template <typename Fn, typename... Args>
    void invoke_and_set(Fn &callable, Args &&...call_args) {
      using raw_result_t = std::decay_t<std::invoke_result_t<Fn &, Args...>>;
      if constexpr (detail::is_result_handle<raw_result_t>::value) {
        detail::resolve_nested_handle(promise, std::invoke(callable, std::forward<Args>(call_args)...));
      } else if constexpr (std::is_void<value_type>::value) {
        std::invoke(callable, std::forward<Args>(call_args)...);
        promise.set_value();
      } else {
        promise.set_value(std::invoke(callable, std::forward<Args>(call_args)...));
      }
    }

    promise_result<value_type, error_type> promise;
    std::optional<std::decay_t<F>> fn;
  };

public:
  packaged_task_result() noexcept = default;
  ~packaged_task_result() = default;

  template <typename F>
  explicit packaged_task_result(F &&f)
      : state_(std::make_shared<state_impl<F>>(std::forward<F>(f))) {
    static_assert(
        std::is_convertible<std::invoke_result_t<std::decay_t<F> &, A...>, R>::value,
        "F must be callable with signature R(A...)");
  }

  packaged_task_result(const packaged_task_result &) = delete;
  packaged_task_result(packaged_task_result &&) noexcept = default;
  packaged_task_result &operator=(const packaged_task_result &) = delete;
  packaged_task_result &operator=(packaged_task_result &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(state_); }

  void swap(packaged_task_result &other) noexcept { std::swap(state_, other.state_); }

  future_type get_future() {
    if (!state_) {
      return future_type{};
    }
    return state_->get_future();
  }

  void operator()(A... args) {
    if (!state_) {
      return;
    }
    state_->run(std::forward<A>(args)...);
  }

private:
  std::shared_ptr<state_base> state_;
};

} // namespace v2
} // namespace cxx14_v1
} // namespace portable_concurrency
