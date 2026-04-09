#include <future>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

#include "test_tools.h"

/**
 * \brief Verifies packaged_task unwrap preserves an inner future return type.
 */
TEST(packaged_task_unwrap, retruns_future_of_proper_type) {
  pc::packaged_task<pc::future<int>()> task{[] { return pc::future<int>{}; }};
  auto future = task.get_future();
  static_assert(std::is_same<decltype(future), pc::future<int>>::value,
                "invalid future type");
}

/**
 * \brief Verifies packaged_task unwrap preserves an inner shared_future return type.
 */
TEST(packaged_task_unwrap, retruns_shared_future_of_proper_type) {
  pc::packaged_task<pc::shared_future<std::string>()> task{
      [] { return pc::shared_future<std::string>{}; }};
  auto future = task.get_future();
  static_assert(
      std::is_same<decltype(future), pc::shared_future<std::string>>::value,
      "invalid future type");
}

/**
 * \brief Verifies unwrap converts an inner future to shared_future when the task signature requires it.
 */
TEST(packaged_task_unwrap, converts_future_to_shared_future_when_needed) {
  pc::packaged_task<pc::shared_future<std::unique_ptr<int>>()> task{
      [] { return pc::future<std::unique_ptr<int>>{}; }};
  auto future = task.get_future();
  static_assert(std::is_same<decltype(future),
                             pc::shared_future<std::unique_ptr<int>>>::value,
                "invalid future type");
}

/**
 * \brief Verifies unwrapping an invalid future yields broken_promise.
 */
TEST(packaged_task_unwrap, invalid_future_unwraps_to_broken_promise) {
  pc::packaged_task<pc::future<int &>(char)> task{
      [](char) { return pc::future<int &>{}; }};
  pc::future<int &> future = task.get_future();
  task('c');
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies unwrapping an invalid shared_future yields broken_promise.
 */
TEST(packaged_task_unwrap, invalid_shared_future_unwraps_to_broken_promise) {
  pc::packaged_task<pc::shared_future<int &>(char, double)> task{
      [](char, double) { return pc::shared_future<int &>{}; }};
  pc::shared_future<int &> future = task.get_future();
  task('c', 3.14);
  EXPECT_FUTURE_ERROR(future.get(), std::future_errc::broken_promise);
}

/**
 * \brief Verifies a ready inner future produces a ready unwrapped result.
 */
TEST(packaged_task_unwrap, ready_future_unwraps_to_ready) {
  pc::packaged_task<pc::future<int>()> task{
      [] { return pc::make_ready_future(42); }};
  pc::future<int> future = task.get_future();
  task();
  EXPECT_TRUE(future.is_ready());
}

/**
 * \brief Verifies a ready inner shared_future produces a ready unwrapped result.
 */
TEST(packaged_task_unwrap, ready_shared_future_unwraps_to_ready) {
  pc::packaged_task<pc::shared_future<int>()> task{
      [] { return pc::make_ready_future(42); }};
  pc::shared_future<int> future = task.get_future();
  task();
  EXPECT_TRUE(future.is_ready());
}

/**
 * \brief Verifies an unwrapped ready future exposes the inner value.
 */
TEST(packaged_task_unwrap, unwraped_ready_future_contains_proper_value) {
  pc::packaged_task<pc::future<int>()> task{
      [] { return pc::make_ready_future(42); }};
  pc::future<int> future = task.get_future();
  task();
  EXPECT_EQ(future.get(), 42);
}

/**
 * \brief Verifies an unwrapped ready shared_future exposes the inner value.
 */
TEST(packaged_task_unwrap, unwraped_ready_shared_future_contains_proper_value) {
  pc::packaged_task<pc::shared_future<int>()> task{
      [] { return pc::make_ready_future(42); }};
  pc::shared_future<int> future = task.get_future();
  task();
  EXPECT_EQ(future.get(), 42);
}

/**
 * \brief Verifies an unwrapped future stays pending while the inner future is still pending.
 */
TEST(packaged_task_unwrap, not_ready_future_unwraps_to_not_ready) {
  pc::promise<std::string> promise;
  pc::packaged_task<pc::future<std::string>(pc::promise<std::string> &)> task{
      [](pc::promise<std::string> &promise) { return promise.get_future(); }};
  pc::future<std::string> future = task.get_future();
  task(promise);
  EXPECT_FALSE(future.is_ready());
}

/**
 * \brief Verifies an unwrapped shared_future stays pending while the inner shared_future is still pending.
 */
TEST(packaged_task_unwrap, not_ready_shared_future_unwraps_to_not_ready) {
  pc::promise<std::string> promise;
  pc::packaged_task<pc::shared_future<std::string>(pc::promise<std::string> &)>
      task{[](pc::promise<std::string> &promise) {
        return promise.get_future();
      }};
  pc::shared_future<std::string> future = task.get_future();
  task(promise);
  EXPECT_FALSE(future.is_ready());
}

/**
 * \brief Verifies the unwrapped future becomes ready when the inner future is fulfilled.
 */
TEST(packaged_task_unwrap,
     unwraped_becomes_ready_when_initial_future_becomes_ready) {
  pc::promise<std::string> promise;
  pc::packaged_task<pc::future<std::string>(pc::promise<std::string> &)> task{
      [](pc::promise<std::string> &promise) { return promise.get_future(); }};
  pc::future<std::string> future = task.get_future();
  task(promise);
  promise.set_value("Hello");
  EXPECT_TRUE(future.is_ready());
}

/**
 * \brief Verifies the unwrapped shared_future becomes ready when the inner shared_future is fulfilled.
 */
TEST(packaged_task_unwrap,
     unwraped_becomes_ready_when_initial_shared_future_becomes_ready) {
  pc::promise<std::string> promise;
  pc::packaged_task<pc::shared_future<std::string>(pc::promise<std::string> &)>
      task{[](pc::promise<std::string> &promise) {
        return promise.get_future();
      }};
  pc::shared_future<std::string> future = task.get_future();
  task(promise);
  promise.set_value("Hello");
  EXPECT_TRUE(future.is_ready());
}

/**
 * \brief Verifies the unwrapped future returns the inner future result.
 */
TEST(packaged_task_unwrap, unwraped_contains_result_of_initial_future) {
  pc::promise<std::string> promise;
  pc::packaged_task<pc::future<std::string>(pc::promise<std::string> &)> task{
      [](pc::promise<std::string> &promise) { return promise.get_future(); }};
  pc::future<std::string> future = task.get_future();
  task(promise);
  promise.set_value("Hello");
  EXPECT_EQ(future.get(), "Hello");
}

/**
 * \brief Verifies the unwrapped shared_future returns the inner shared_future result.
 */
TEST(packaged_task_unwrap, unwraped_contains_result_of_initial_shared_future) {
  pc::promise<std::string> promise;
  pc::packaged_task<pc::shared_future<std::string>(pc::promise<std::string> &)>
      task{[](pc::promise<std::string> &promise) {
        return promise.get_future();
      }};
  pc::shared_future<std::string> future = task.get_future();
  task(promise);
  promise.set_value("Hello");
  EXPECT_EQ(future.get(), "Hello");
}

/**
 * \brief Verifies unwrapping a shared_future preserves access to the same shared result object.
 */
TEST(packaged_task_unwrap,
     unwraped_shared_future_provides_access_to_the_result_of_inital_task) {
  pc::shared_future<int> shared =
      pc::async(g_future_tests_env, [] { return 100500; });
  pc::packaged_task<pc::shared_future<int>()> task{[&] { return shared; }};
  pc::shared_future<int> unwraped = task.get_future();
  task();
  EXPECT_EQ(std::addressof(shared.get()), std::addressof(unwraped.get()));
}
