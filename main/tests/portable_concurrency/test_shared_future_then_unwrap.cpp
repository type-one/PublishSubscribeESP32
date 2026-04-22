#include <future>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

struct SharedFutureThenUnwrap : future_test {
  SharedFutureThenUnwrap() {
    std::tie(promise, future) = pco::make_promise<int>();
  }
  pco::promise<int> promise;
  pco::shared_future<int> future;
};

/**
 * \brief Verifies shared_future::then unwraps an inner future result.
 */
TEST_F(SharedFutureThenUnwrap, future_unwrapped) {
  pco::promise<void> inner_promise;
  auto cnt_f = future.then(
      [inner_future = inner_promise.get_future()](
          pco::shared_future<int>) mutable { return std::move(inner_future); });
  static_assert(std::is_same<decltype(cnt_f), pco::future<void>>::value, "");
}

/**
 * \brief Verifies shared_future::then unwraps an inner shared_future result.
 */
TEST_F(SharedFutureThenUnwrap, shared_future_unwrapped) {
  pco::promise<void> inner_promise;
  auto cnt_f = future.then(
      [inner_future = inner_promise.get_future().share()](
          pco::shared_future<int>) mutable { return std::move(inner_future); });
  static_assert(std::is_same<decltype(cnt_f), pco::shared_future<void>>::value,
                "");
}

/**
 * \brief Verifies the returned future stays pending until the inner future is fulfilled.
 */
TEST_F(SharedFutureThenUnwrap, result_is_not_ready_after_continuation_call) {
  pco::promise<void> inner_promise;
  pco::future<void> cnt_f = future.then(
      [&](pco::shared_future<int>) { return inner_promise.get_future(); });
  promise.set_value(42);
  EXPECT_FALSE(cnt_f.is_ready());
}

/**
 * \brief Verifies the returned future becomes ready when the inner future is fulfilled.
 */
TEST_F(SharedFutureThenUnwrap,
       result_is_ready_after_continuation_result_becomes_ready) {
  pco::promise<void> inner_promise;
  pco::future<void> cnt_f = future.then(
      [&](pco::shared_future<int>) { return inner_promise.get_future(); });
  promise.set_value(42);
  inner_promise.set_value();
  EXPECT_TRUE(cnt_f.is_ready());
}

/**
 * \brief Verifies errors from the inner future propagate through the unwrapped result.
 */
TEST_F(SharedFutureThenUnwrap, result_propagates_inner_future_error) {
  pco::promise<void> inner_promise;
  pco::future<void> cnt_f = future.then(
      [&](pco::shared_future<int>) { return inner_promise.get_future(); });
  promise.set_value(42);
  inner_promise.set_exception(
      std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

/**
 * \brief Verifies exceptions thrown before producing the inner future propagate to the returned future.
 */
TEST_F(SharedFutureThenUnwrap,
       exception_from_unwrapped_continuation_propagated_to_result) {
  pco::future<std::unique_ptr<int>> cnt_f = future.then(
      [](pco::shared_future<int>) -> pco::future<std::unique_ptr<int>> {
        throw std::runtime_error("Ooups");
      });
  promise.set_value(42);
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

/**
 * \brief Verifies an unwrapping shared_future continuation can run on a specific executor.
 */
TEST_F(SharedFutureThenUnwrap, run_continuation_on_specific_executor) {
  pco::future<std::thread::id> cnt_f =
      future.then(g_future_tests_env, [](pco::shared_future<int>) {
        return pco::make_ready_future(std::this_thread::get_id());
      });
  promise.set_value(42);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

/**
 * \brief Verifies unwrapping a shared_future preserves shared access to the same stored value.
 */
TEST_F(SharedFutureThenUnwrap,
       shared_future_and_unwrapped_shared_future_access_same_storage) {
  pco::promise<std::string> inner_promise;
  pco::shared_future<std::string> inner_future = inner_promise.get_future();
  pco::shared_future<std::string> cnt_f = future.then(
      [inner_future](pco::shared_future<int>) { return inner_future; });

  promise.set_value(42);
  inner_promise.set_value("qwe");
  EXPECT_EQ(&inner_future.get(), &cnt_f.get());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
