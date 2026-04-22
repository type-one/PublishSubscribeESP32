#include <future>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

#include "test_tools.h"

using namespace std::literals;

class null_executor_t {
private:
  template <typename F> friend void post(null_executor_t, F &&) {}
};
constexpr null_executor_t null_executor{};

namespace portable_concurrency {

template <> struct is_executor<null_executor_t> : std::true_type {};

namespace {
namespace test {

// Abandon in async
/**
 * \brief Verifies fulfils future with broken promise error for abandon task scheduled by async.
 */
TEST(abandon_task_scheduled_by_async,
     fulfils_future_with_broken_promise_error) {
  pco::future<void> future = pco::async(null_executor, [] {});
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies destroys stored function object for abandon task scheduled by async.
 */
TEST(abandon_task_scheduled_by_async, destroys_stored_function_object) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;
  pco::future<void> future =
      pco::async(null_executor, [sp = std::exchange(sp, nullptr)] {});
  EXPECT_TRUE(wp.expired());
}

// Abandon continuation
/**
 * \brief Verifies fulfils future with broken promise error for abandon task passed to future next.
 */
TEST(abandon_task_passed_to_future_next,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.next(null_executor, [](int val) { return std::to_string(val); });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies destroys stored function object for abandon task passed to future next.
 */
TEST(abandon_task_passed_to_future_next, destroys_stored_function_object) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;

  pco::promise<int> promise;
  pco::future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.next(null_executor, [sp = std::exchange(sp, nullptr)](int val) {
        return std::to_string(val);
      });
  promise.set_value(42);
  EXPECT_TRUE(wp.expired());
}

/**
 * \brief Verifies fulfils future with broken promise error for abandon task passed to future then.
 */
TEST(abandon_task_passed_to_future_then,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::future<int> future = promise.get_future();
  pco::future<std::string> cnt_f = future.then(
      null_executor, [](pco::future<int> f) { return std::to_string(f.get()); });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies destroys stored function object for abandon task passed to future then.
 */
TEST(abandon_task_passed_to_future_then, destroys_stored_function_object) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;

  pco::promise<int> promise;
  pco::future<int> future = promise.get_future();
  pco::future<std::string> cnt_f = future.then(
      null_executor, [sp = std::exchange(sp, nullptr)](pco::future<int> val) {
        return std::to_string(val.get());
      });
  promise.set_value(42);
  EXPECT_TRUE(wp.expired());
}

/**
 * \brief Verifies fulfils future with broken promise error for abandon task passed to shared future next.
 */
TEST(abandon_task_passed_to_shared_future_next,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::shared_future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.next(null_executor, [](int val) { return std::to_string(val); });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies destroys stored function object for abandon task passed to shared future next.
 */
TEST(abandon_task_passed_to_shared_future_next,
     destroys_stored_function_object) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;

  pco::promise<int> promise;
  pco::shared_future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.next(null_executor, [sp = std::exchange(sp, nullptr)](int val) {
        return std::to_string(val);
      });
  promise.set_value(42);
  EXPECT_TRUE(wp.expired());
}

/**
 * \brief Verifies fulfils future with broken promise error for abandon task passed to shared future then.
 */
TEST(abandon_task_passed_to_shared_future_then,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::shared_future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.then(null_executor, [](pco::shared_future<int> f) {
        return std::to_string(f.get());
      });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies destroys stored function object for abandon task passed to shared future then.
 */
TEST(abandon_task_passed_to_shared_future_then,
     destroys_stored_function_object) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;

  pco::promise<int> promise;
  pco::shared_future<int> future = promise.get_future();
  pco::future<std::string> cnt_f = future.then(
      null_executor,
      [sp = std::exchange(sp, nullptr)](pco::shared_future<int> val) {
        return std::to_string(val.get());
      });
  promise.set_value(42);
  EXPECT_TRUE(wp.expired());
}

// Abandon packaged_task
/**
 * \brief Verifies fulfils future with broken promise error for premature destruction of packaged task.
 */
TEST(premature_destruction_of_packaged_task,
     fulfils_future_with_broken_promise_error) {
  pco::future<size_t> future;
  {
    pco::packaged_task<size_t(size_t, const std::string &)> task{
        [](size_t a, const std::string &b) { return a + b.size(); }};
    future = task.get_future();
  }
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies fulfils future with broken promise error for assignment to not yet called packaged task.
 */
TEST(assignment_to_not_yet_called_packaged_task,
     fulfils_future_with_broken_promise_error) {
  pco::packaged_task<size_t(size_t, const std::string &)> task{
      [](size_t a, const std::string &b) { return a + b.size(); }};
  pco::future<size_t> future = task.get_future();
  task = {};
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies destroys stored function object for premature destruction of packaged task.
 */
TEST(premature_destruction_of_packaged_task, destroys_stored_function_object) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;
  pco::future<void> future;
  {
    pco::packaged_task<void()> task{[sp = std::exchange(sp, nullptr)] {}};
    future = task.get_future();
  }
  EXPECT_TRUE(wp.expired());
}

// Abandon promise
/**
 * \brief Verifies fulfils future with broken promise error for premature destruction of promise.
 */
TEST(premature_destruction_of_promise,
     fulfils_future_with_broken_promise_error) {
  pco::future<size_t> future;
  {
    pco::promise<size_t> promise;
    future = promise.get_future();
  }
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies fulfils future with broken promise error for premature destruction of promise of ref.
 */
TEST(premature_destruction_of_promise_of_ref,
     fulfils_future_with_broken_promise_error) {
  pco::future<size_t &> future;
  {
    pco::promise<size_t &> promise;
    future = promise.get_future();
  }
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies fulfils future with broken promise error for assignment to not set promise.
 */
TEST(assignment_to_not_set_promise, fulfils_future_with_broken_promise_error) {
  pco::promise<void> promise;
  pco::future<void> future = promise.get_future();
  promise = {};
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

// TODO think if the tests bellow should realle live as part of task abandon
// tests
/**
 * \brief Verifies fulfils future with broken promise error for return invalid future from future then continuation.
 */
TEST(return_invalid_future_from_future_then_continuation,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.then([](pco::future<int>) { return pco::future<std::string>{}; });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies fulfils future with broken promise error for return invalid future from future next continuation.
 */
TEST(return_invalid_future_from_future_next_continuation,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.next([](int) { return pco::future<std::string>{}; });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies fulfils future with broken promise error for return invalid future from shared future then continuation.
 */
TEST(return_invalid_future_from_shared_future_then_continuation,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::shared_future<int> future = promise.get_future();
  pco::future<std::string> cnt_f = future.then(
      [](pco::shared_future<int>) { return pco::future<std::string>{}; });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies fulfils future with broken promise error for return invalid future from shared future next continuation.
 */
TEST(return_invalid_future_from_shared_future_next_continuation,
     fulfils_future_with_broken_promise_error) {
  pco::promise<int> promise;
  pco::shared_future<int> future = promise.get_future();
  pco::future<std::string> cnt_f =
      future.next([](int) { return pco::future<std::string>{}; });
  promise.set_value(42);
  EXPECT_FUTURE_ERROR(cnt_f.get(), std::future_errc::broken_promise);
}

} // namespace test
} // namespace
} // namespace portable_concurrency
