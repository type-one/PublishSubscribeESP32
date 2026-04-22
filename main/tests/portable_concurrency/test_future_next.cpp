#include <future>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

#include "test_helpers.h"
#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

struct FutureNext : future_test {
  pco::promise<int> promise;
  pco::future<int> future = promise.get_future();
};

/**
 * \brief Verifies src future is invalidated for future next.
 */
TEST_F(FutureNext, src_future_is_invalidated) {
  pco::future<void> cnt_f = future.next([](int) {});
  EXPECT_FALSE(future.valid());
}

/**
 * \brief Verifies arg is moved to continuation for future next.
 */
TEST_F(FutureNext, arg_is_moved_to_continuation) {
  pco::future<void> cnt_f = future.next([](auto &&arg) {
    static_assert(std::is_same<std::decay_t<decltype(arg)>, int>::value, "");
    static_assert(std::is_rvalue_reference<decltype(arg)>::value, "");
  });
}

/**
 * \brief Verifies continuation receives value for future next.
 */
TEST_F(FutureNext, continuation_receives_value) {
  pco::future<std::string> cnt_f =
      future.next([](int val) { return to_string(val); });
  promise.set_value(42);
  EXPECT_EQ(cnt_f.get(), "42");
}

/**
 * \brief Verifies continuation is not called in case of exception for future next.
 */
TEST_F(FutureNext, continuation_is_not_called_in_case_of_exception) {
  unsigned call_count = 0;
  pco::future<void> cnt_f = future.next([&call_count](int) { ++call_count; });
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_EQ(call_count, 0u);
}

/**
 * \brief Verifies exception propagated to result of next for future next.
 */
TEST_F(FutureNext, exception_propagated_to_result_of_next) {
  pco::future<void> cnt_f = future.next([](int) {});
  promise.set_exception(std::make_exception_ptr(std::runtime_error{"Ooups"}));
  EXPECT_RUNTIME_ERROR(cnt_f, "Ooups");
}

/**
 * \brief Verifies is executed for void future for future next.
 */
TEST_F(FutureNext, is_executed_for_void_future) {
  pco::promise<void> void_promise;
  pco::future<void> void_future = void_promise.get_future();
  pco::future<int> cnt_f = void_future.next([]() { return 42; });
  void_promise.set_value();
  EXPECT_EQ(cnt_f.get(), 42);
}

/**
 * \brief Verifies is executed for ref future for future next.
 */
TEST_F(FutureNext, is_executed_for_ref_future) {
  pco::promise<int &> ref_promise;
  pco::future<int &> ref_future = ref_promise.get_future();
  pco::future<int *> cnt_f = ref_future.next([](int &val) { return &val; });
  int a = 42;
  ref_promise.set_value(a);
  EXPECT_EQ(cnt_f.get(), &a);
}

/**
 * \brief Verifies continuation executed on specified executor for future next.
 */
TEST_F(FutureNext, continuation_executed_on_specified_executor) {
  pco::future<std::thread::id> cnt_f = future.next(
      g_future_tests_env, [](int) { return std::this_thread::get_id(); });
  promise.set_value(42);
  EXPECT_TRUE(g_future_tests_env->uses_thread(cnt_f.get()));
}

/**
 * \brief Verifies upwraps future for future next.
 */
TEST_F(FutureNext, upwraps_future) {
  pco::promise<std::string> inner_promise;
  pco::future<std::string> cnt_f =
      future.next([&inner_promise](int) { return inner_promise.get_future(); });
  promise.set_value(100500);
  inner_promise.set_value("qwe");
  EXPECT_EQ(cnt_f.get(), "qwe");
}

/**
 * \brief Verifies upwraps shared future for future next.
 */
TEST_F(FutureNext, upwraps_shared_future) {
  auto cnt_f =
      future.next([](int) { return pco::make_ready_future(100500).share(); });
  static_assert(std::is_same<decltype(cnt_f), pco::shared_future<int>>::value,
                "");
}

/**
 * \brief Verifies desroys continuation after invocation for future next.
 */
TEST_F(FutureNext, desroys_continuation_after_invocation) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;
  auto cnt_f = future.next(
      [sp = std::exchange(sp, nullptr)](int val) { return val + *sp; });
  promise.set_value(100);
  EXPECT_TRUE(wp.expired());
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
