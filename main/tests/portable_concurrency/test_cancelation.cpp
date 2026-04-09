#include <future>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

#include "test_tools.h"

namespace portable_concurrency {
namespace test {
namespace {

[[gnu::unused]] void foo(pc::promise<int>, pc::future<std::string>) {}

static_assert(
    std::is_same<detail::promise_arg_t<decltype(foo), future<std::string>>,
                 int>::value,
    "PromiseArgDeduce: should work on function reference");

static_assert(
    std::is_same<detail::promise_arg_t<decltype(&foo), future<std::string>>,
                 int>::value,
    "PromiseArgDeduce: should work on function pointer");

/**
 * \brief Verifies promise_arg_t deduces the promised value type from a lambda.
 */
TEST(PromiseArgDeduce, shoud_work_on_lambda) {
  auto lambda = [c = 5u](pc::promise<char> p, pc::future<std::string> f) {
    p.set_value(f.get()[c]);
  };
  static_assert(
      std::is_same<detail::promise_arg_t<decltype(lambda), future<std::string>>,
                   char>::value,
      "PromiseArgDeduce, should work on lambda");
}

/**
 * \brief Verifies promise_arg_t also works for mutable lambdas.
 */
TEST(PromiseArgDeduce, shoud_work_on_mutable_lambda) {
  auto lambda = [c = 5u](pc::promise<char> p,
                         pc::future<std::string> f) mutable {
    p.set_value(f.get()[c++]);
  };
  static_assert(
      std::is_same<detail::promise_arg_t<decltype(lambda), future<std::string>>,
                   char>::value,
      "PromiseArgDeduce, should work on mutable lambda");
}

/**
 * \brief Verifies destroying the future releases a value already stored in the promise state.
 */
TEST(PromiseCancelation, no_object_held_by_promise_after_future_destruction) {
  auto val = std::make_shared<int>(42);
  std::weak_ptr<int> weak = val;
  auto promise = pc::make_promise<std::shared_ptr<int>>();
  promise.first.set_value(std::move(val));
  promise.second = {};
  EXPECT_FALSE(weak.lock());
}

/**
 * \brief Verifies set_value becomes a no-op after the paired future is canceled.
 */
TEST(PromiseCancelation, set_value_do_nothing_on_canceled_promise) {
  auto promise = pc::make_promise<int>();
  promise.second = {};
  EXPECT_NO_THROW(promise.first.set_value(42));
}

/**
 * \brief Verifies set_exception becomes a no-op after the paired future is canceled.
 */
TEST(PromiseCancelation, set_exception_do_nothing_on_canceled_promise) {
  auto promise = pc::make_promise<int>();
  promise.second = {};
  EXPECT_NO_THROW(promise.first.set_exception(
      std::make_exception_ptr(std::runtime_error{"Ooups"})));
}

/**
 * \brief Verifies canceling a packaged_task future releases the stored callable.
 */
TEST(PackagedTaskCancelation, function_object_is_destroyed_on_cancel) {
  auto val = std::make_shared<int>(42);
  std::weak_ptr<int> weak = val;
  pc::packaged_task<void()> task{[val = std::move(val)] {}};
  { auto f = task.get_future(); }
  EXPECT_FALSE(weak.lock());
}

/**
 * \brief Verifies a packaged_task remains valid after its future is canceled.
 */
TEST(PackagedTaskCancelation, canceled_task_remain_valid) {
  pc::packaged_task<void()> task{[] {}};
  { auto f = task.get_future(); }
  EXPECT_TRUE(task.valid());
}

/**
 * \brief Verifies invoking a packaged_task after cancelation does not throw.
 */
TEST(PackagedTaskCancelation, run_canceled_task_do_not_throw) {
  pc::packaged_task<void()> task{[] {}};
  { auto f = task.get_future(); }
  EXPECT_NO_THROW(task());
}

/**
 * \brief Verifies invoking a canceled packaged_task does not run the stored callable.
 */
TEST(PackagedTaskCancelation,
     run_canceled_task_do_not_execute_stored_function) {
  bool executed = false;
  pc::packaged_task<void()> task{[&executed] { executed = true; }};
  { auto f = task.get_future(); }
  task();
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies a then continuation is skipped when the continuation future is destroyed first.
 */
TEST(ContinuationCancelation,
     then_continuation_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  bool executed = false;
  {
    auto f =
        promise.second.then([&executed](pc::future<int>) { executed = true; });
  }
  promise.first.set_value(42);
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies an unwrapped then continuation is skipped when the continuation future is destroyed first.
 */
TEST(ContinuationCancelation,
     then_unwrapped_continuation_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  bool executed = false;
  {
    auto f = promise.second.then([&executed](pc::future<int>) {
      executed = true;
      return pc::make_ready_future();
    });
  }
  promise.first.set_value(42);
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies an executor-scheduled then continuation is skipped after continuation future destruction.
 */
TEST(
    ContinuationCancelation,
    then_continuation_with_executor_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  std::atomic<bool> executed{false};
  {
    auto f = promise.second.then(
        g_future_tests_env, [&executed](pc::future<int>) { executed = true; });
  }
  promise.first.set_value(42);
  g_future_tests_env->wait_current_tasks();
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies a next continuation is skipped when the continuation future is destroyed first.
 */
TEST(ContinuationCancelation,
     next_continuation_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  bool executed = false;
  {
    auto f = promise.second.next([&executed](int) { executed = true; });
  }
  promise.first.set_value(42);
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies an executor-scheduled next continuation is skipped after continuation future destruction.
 */
TEST(
    ContinuationCancelation,
    next_continuation_with_executor_is_not_executed_afeter_future_destruction) {
  auto promise = pc::make_promise<int>();
  std::atomic<bool> executed{false};
  {
    auto f = promise.second.next(g_future_tests_env,
                                 [&executed](int) { executed = true; });
  }
  promise.first.set_value(42);
  g_future_tests_env->wait_current_tasks();
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies promise::is_awaiten reports true while a future still observes the promise.
 */
TEST(Promise, is_awaiten_returns_true_before_future_destruction) {
  auto p = pc::make_promise<int>();
  auto f = std::move(p.second);
  EXPECT_TRUE(p.first.is_awaiten());
}

/**
 * \brief Verifies promise::is_awaiten reports false after the observing future is destroyed.
 */
TEST(Promise, is_awaiten_returns_false_after_future_destruction) {
  auto p = pc::make_promise<int &>();
  p.second = {};
  EXPECT_FALSE(p.first.is_awaiten());
}

/**
 * \brief Verifies promise::is_awaiten stays true after converting the future to a shared_future.
 */
TEST(Promise, is_awaiten_returns_true_after_future_sharing) {
  auto p = pc::make_promise<int>();
  pc::shared_future<int> sf = std::move(p.second);
  EXPECT_TRUE(p.first.is_awaiten());
}

/**
 * \brief Verifies promise::is_awaiten stays true while a continuation remains attached.
 */
TEST(Promise, is_awaiten_returns_true_after_attaching_continuation) {
  auto p = pc::make_promise<int>();
  auto f = p.second.next([](int val) { return 5 * val; });
  EXPECT_TRUE(p.first.is_awaiten());
}

/**
 * \brief Verifies promise::is_awaiten becomes false once the continuation future is destroyed.
 */
TEST(Promise, is_awaiten_returns_false_after_continuation_future_destroyed) {
  auto p = pc::make_promise<int>();
  {
    auto f = p.second.next([](int val) { return 5 * val; });
  }
  EXPECT_FALSE(p.first.is_awaiten());
}

/**
 * \brief Verifies detach invalidates the original future handle.
 */
TEST(FutureDetach, invalidates_future) {
  auto p = pc::make_promise<int>();
  p.second.detach();
  EXPECT_FALSE(p.second.valid());
}

/**
 * \brief Verifies promise::is_awaiten stays true after a future is detached.
 */
TEST(Promise, is_awaiten_returns_true_after_future_detach) {
  auto p = pc::make_promise<std::string>();
  p.second.detach();
  EXPECT_TRUE(p.first.is_awaiten());
}

/**
 * \brief Verifies detach does not keep later-added continuation work alive.
 */
TEST(FutureDetach, do_not_prevents_tasks_cancelation_added_after_detaching) {
  auto p = pc::make_promise<std::string>();
  bool executed = false;
  {
    auto f = p.second.detach().next([&](std::string) { executed = true; });
  }
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies detach invalidates the original shared_future handle.
 */
TEST(SharedFutureDetach, invalidates_future) {
  auto p = pc::make_promise<int>();
  auto f = p.second.share();
  f.detach();
  EXPECT_FALSE(f.valid());
}

/**
 * \brief Verifies promise::is_awaiten stays true after a shared_future is detached.
 */
TEST(Promise, is_awaiten_returns_true_after_shared_future_detach) {
  auto p = pc::make_promise<std::string>();
  p.second.share().detach();
  EXPECT_TRUE(p.first.is_awaiten());
}

/**
 * \brief Verifies shared_future detach does not keep later-added continuation work alive.
 */
TEST(SharedFutureDetach,
     do_not_prevents_tasks_cancelation_added_after_detaching) {
  auto p = pc::make_promise<std::string>();
  bool executed = false;
  {
    auto f = p.second.share().detach().next(
        [&](const std::string &) { executed = true; });
  }
  EXPECT_FALSE(executed);
}

/**
 * \brief Verifies a canceler is not invoked when the promise is merely created.
 */
TEST(Canceler, is_not_called_by_promise_constructor) {
  size_t call_count = 0u;
  auto promise = pc::make_promise<int>(pc::canceler_arg, [&] { ++call_count; });
  EXPECT_EQ(call_count, 0u);
}

/**
 * \brief Verifies a canceler runs exactly once when the future is destroyed before any result is set.
 */
TEST(Canceler, is_called_once_if_future_destroyed_before_set) {
  size_t call_count = 0u;
  {
    auto promise =
        pc::make_promise<int>(pc::canceler_arg, [&] { ++call_count; });
  }
  EXPECT_EQ(call_count, 1u);
}

/**
 * \brief Verifies a canceler is not called after a value has already been delivered.
 */
TEST(Canceler, is_not_called_if_future_destroyed_after_set_value) {
  size_t call_count = 0u;
  {
    auto promise =
        pc::make_promise<int>(pc::canceler_arg, [&] { ++call_count; });
    promise.first.set_value(42);
  }
  EXPECT_EQ(call_count, 0u);
}

/**
 * \brief Verifies a canceler is not called after an exception has already been delivered.
 */
TEST(Canceler, is_not_called_if_future_destroyed_after_set_exception) {
  size_t call_count = 0u;
  {
    auto promise =
        pc::make_promise<int &>(pc::canceler_arg, [&] { ++call_count; });
    promise.first.set_exception(
        std::make_exception_ptr(std::runtime_error{"qwe"}));
  }
  EXPECT_EQ(call_count, 0u);
}

/**
 * \brief Verifies abandoning the promise does not invoke the canceler while the future still owns the state.
 */
TEST(Canceler, is_not_called_if_promise_abandoned) {
  size_t call_count = 0u;
  {
    pc::future<void> f;
    {
      auto promise =
          pc::make_promise<void>(pc::canceler_arg, [&] { ++call_count; });
      f = std::move(promise.second);
    }
  }
  EXPECT_EQ(call_count, 0u);
}

/**
 * \brief Verifies a canceler may perform non-const cleanup work.
 */
TEST(Canceller, non_const_operation_is_supported) {
  std::shared_ptr<int> resource = std::make_shared<int>(42);
  std::weak_ptr<int> weak = resource;
  auto promise = pc::make_promise<int>(
      pc::canceler_arg,
      [resource = std::move(resource)]() mutable { resource.reset(); });
  promise.second = {};
  EXPECT_TRUE(weak.expired());
}

/**
 * \brief Verifies canceler-owned resources are destroyed after the canceler runs.
 */
TEST(Cancelleer, is_destroyed_after_being_called) {
  std::shared_ptr<int> resource = std::make_shared<int>(42);
  std::weak_ptr<int> weak = resource;
  auto promise = pc::make_promise<int>(pc::canceler_arg,
                                       [resource = std::move(resource)] {});
  promise.second = {};
  EXPECT_TRUE(weak.expired());
}

/**
 * \brief Verifies a canceler may capture and destroy move-only state.
 */
TEST(Canceller, non_copyable_operation_is_supported) {
  bool called = false;
  auto deleter = [&called](int *p) {
    called = true;
    delete p;
  };
  std::unique_ptr<int, decltype(deleter)> resource{new int{42}, deleter};
  auto promise = pc::make_promise<int>(
      pc::canceler_arg,
      [resource = std::move(resource)]() mutable { resource.reset(); });
  promise.second = {};
  EXPECT_TRUE(called);
}

/**
 * \brief Verifies an interruptible continuation yields broken_promise when it never fulfills its result promise.
 */
TEST(InterruptableContinuation, broken_promise_delivered_if_valie_is_not_set) {
  auto promise = pc::make_promise<int>();
  pc::future<std::string> future =
      promise.second.then([](pc::promise<std::string>, pc::future<int>) {});
  promise.first.set_value(42);
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies an interruptible future continuation can observe that its result is no longer awaited.
 */
TEST(InterruptableContinuation,
     future_continuation_detects_if_result_is_not_awaiten) {
  auto promise = pc::make_promise<int>();
  pc::future<std::string &> future;
  bool was_awaiten = true;
  future =
      promise.second.then([&](pc::promise<std::string &> p, pc::future<int>) {
        future = {};
        was_awaiten = p.is_awaiten();
      });
  promise.first.set_value(42);
  EXPECT_EQ(was_awaiten, false);
}

/**
 * \brief Verifies an interruptible shared_future continuation can observe that its result is no longer awaited.
 */
TEST(InterruptableContinuation,
     shared_future_continuation_detects_if_result_is_not_awaiten) {
  auto promise = pc::make_promise<int>();
  pc::future<void> future;
  bool was_awaiten = true;
  future = promise.second.share().then(
      [&](pc::promise<void> p, pc::shared_future<int>) {
        future = {};
        was_awaiten = p.is_awaiten();
      });
  promise.first.set_value(42);
  EXPECT_EQ(was_awaiten, false);
}

} // namespace
} // namespace test
} // namespace portable_concurrency
