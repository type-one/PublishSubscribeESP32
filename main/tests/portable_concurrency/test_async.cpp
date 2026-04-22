#include <future>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

#include "test_tools.h"

using namespace std::literals;

namespace portable_concurrency {
namespace {
namespace test {

struct Async : future_test {};

/**
 * \brief Verifies returns valid future for async.
 */
TEST_F(Async, returns_valid_future) {
  pco::future<int> future = pco::async(g_future_tests_env, [] { return 42; });
  EXPECT_TRUE(future.valid());
}

/**
 * \brief Verifies delivers function result for async.
 */
TEST_F(Async, delivers_function_result) {
  pco::future<int> future = pco::async(g_future_tests_env, [] { return 42; });
  EXPECT_EQ(future.get(), 42);
}

/**
 * \brief Verifies executes functor on specified executor for async.
 */
TEST_F(Async, executes_functor_on_specified_executor) {
  pco::future<std::thread::id> future =
      pco::async(g_future_tests_env, [] { return std::this_thread::get_id(); });
  EXPECT_TRUE(g_future_tests_env->uses_thread(future.get()));
}

/**
 * \brief Verifies unwraps future for async.
 */
TEST_F(Async, unwraps_future) {
  auto future = pco::async(g_future_tests_env, [] {
    return pco::async(g_future_tests_env, [] { return 100500; });
  });
  static_assert(std::is_same<decltype(future), pco::future<int>>::value, "");
  EXPECT_EQ(future.get(), 100500);
}

/**
 * \brief Verifies unwraps shared future for async.
 */
TEST_F(Async, unwraps_shared_future) {
  auto future = pco::async(g_future_tests_env, [] {
    return pco::async(g_future_tests_env, [] { return 100500; }).share();
  });
  static_assert(std::is_same<decltype(future), pco::shared_future<int>>::value,
                "");
  EXPECT_EQ(future.get(), 100500);
}

/**
 * \brief Verifies captures parameters for async.
 */
TEST_F(Async, captures_parameters) {
  pco::future<size_t> future =
      pco::async(g_future_tests_env, std::hash<std::string>{}, "qwe");
  EXPECT_EQ(future.get(), std::hash<std::string>{}("qwe"));
}

/**
 * \brief Verifies destroys function object after invocation for async.
 */
TEST_F(Async, destroys_function_object_after_invocation) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;
  pco::future<int> future = pco::async(
      g_future_tests_env, [sp = std::exchange(sp, nullptr)] { return *sp; });
  g_future_tests_env->wait_current_tasks();
  EXPECT_TRUE(wp.expired());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
