#include <future>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

#include "test_helpers.h"
#include "test_tools.h"

using namespace std::literals;

namespace {

template <typename T> class PackagedTaskTest : public ::testing::Test {};
TYPED_TEST_CASE(PackagedTaskTest, TestTypes);

namespace tests {

// Construction/Assignment

/**
 * \brief Verifies default constructed is invalid for packaged task.
 */
TEST(packaged_task, default_constructed_is_invalid) {
  pco::packaged_task<int(std::string)> task;
  EXPECT_FALSE(task.valid());
}

/**
 * \brief Verifies cunstructed from funcion object is valid for packaged task.
 */
TEST(packaged_task, cunstructed_from_funcion_object_is_valid) {
  pco::packaged_task<void(int)> task{[](int) {}};
  EXPECT_TRUE(task.valid());
}

/**
 * \brief Verifies move constructed from valid is valid for packaged task.
 */
TEST(packaged_task, move_constructed_from_valid_is_valid) {
  pco::packaged_task<void(int)> src_task{[](int) {}};
  pco::packaged_task<void(int)> dst_task{std::move(src_task)};
  EXPECT_TRUE(dst_task.valid());
}

/**
 * \brief Verifies move constructed from invalid is invalid for packaged task.
 */
TEST(packaged_task, move_constructed_from_invalid_is_invalid) {
  pco::packaged_task<void(int)> src_task;
  pco::packaged_task<void(int)> dst_task{std::move(src_task)};
  EXPECT_FALSE(dst_task.valid());
}

/**
 * \brief Verifies move assigned with valid is valid for packaged task.
 */
TEST(packaged_task, move_assigned_with_valid_is_valid) {
  pco::packaged_task<void(int)> src_task{[](int) {}};
  pco::packaged_task<void(int)> dst_task;
  dst_task = std::move(src_task);
  EXPECT_TRUE(dst_task.valid());
}

/**
 * \brief Verifies move assigned with invalid is invalid for packaged task.
 */
TEST(packaged_task, move_assigned_with_invalid_is_invalid) {
  pco::packaged_task<void(int)> src_task;
  pco::packaged_task<void(int)> dst_task;
  dst_task = std::move(src_task);
  EXPECT_FALSE(dst_task.valid());
}

/**
 * \brief Verifies moved to constructor is invalid for packaged task.
 */
TEST(packaged_task, moved_to_constructor_is_invalid) {
  pco::packaged_task<void(int)> src_task{[](int) {}};
  pco::packaged_task<void(int)> dst_task{std::move(src_task)};
  EXPECT_FALSE(src_task.valid());
}

/**
 * \brief Verifies moved to assignment is invalid for packaged task.
 */
TEST(packaged_task, moved_to_assignment_is_invalid) {
  pco::packaged_task<void(int)> src_task{[](int) {}};
  pco::packaged_task<void(int)> dst_task;
  dst_task = std::move(src_task);
  EXPECT_FALSE(src_task.valid());
}

// Invocation

/**
 * \brief Verifies invoke with copyable argument for packaged task.
 */
TEST(packaged_task, invoke_with_copyable_argument) {
  pco::packaged_task<size_t(std::string)> task{
      [](std::string arg) { return arg.size(); }};
  pco::future<size_t> f = task.get_future();

  task("qwe");
  EXPECT_EQ(f.get(), 3u);
}

/**
 * \brief Verifies invoke with move only argument for packaged task.
 */
TEST(packaged_task, invoke_with_move_only_argument) {
  pco::packaged_task<int(std::unique_ptr<int>)> task{
      [](std::unique_ptr<int> arg) { return *arg; }};
  pco::future<int> f = task.get_future();

  task(std::make_unique<int>(42));
  EXPECT_EQ(f.get(), 42);
}

/**
 * \brief Verifies invoke with lvalue const reference argument for packaged task.
 */
TEST(packaged_task, invoke_with_lvalue_const_reference_argument) {
  pco::packaged_task<size_t(std::string)> task{
      [](const std::string &arg) { return arg.size(); }};
  pco::future<size_t> f = task.get_future();

  task("qwe");
  EXPECT_EQ(f.get(), 3u);
}

/**
 * \brief Verifies invoke with rvalue reference argument for packaged task.
 */
TEST(packaged_task, invoke_with_rvalue_reference_argument) {
  pco::packaged_task<int(std::unique_ptr<int> &&)> task{
      [](std::unique_ptr<int> &&arg) { return *arg; }};
  pco::future<int> f = task.get_future();

  task(std::make_unique<int>(42));
  EXPECT_EQ(f.get(), 42);
}

/**
 * \brief Verifies invoke with mutable reference argument for packaged task.
 */
TEST(packaged_task, invoke_with_mutable_reference_argument) {
  pco::packaged_task<int *(int &)> task{[](int &arg) { return &arg; }};
  pco::future<int *> f = task.get_future();

  int the_val = 42;
  task(the_val);
  EXPECT_EQ(f.get(), &the_val);
}

/**
 * \brief Verifies destroys stored function after invocation for packaged task.
 */
TEST(packaged_task, destroys_stored_function_after_invocation) {
  auto sp = std::make_shared<int>(42);
  std::weak_ptr<int> wp = sp;
  pco::packaged_task<int(int)> task{
      [sp = std::exchange(sp, nullptr)](int val) { return val + *sp; }};
  pco::future<int> future = task.get_future();
  task(100500);
  EXPECT_TRUE(wp.expired());
}

// Old tests to refactor

template <typename T> void swap_valid_task_with_invalid() {
  pco::packaged_task<T()> task1(some_value<T>);
  pco::packaged_task<T()> task2;
  EXPECT_TRUE(task1.valid());
  EXPECT_FALSE(task2.valid());

  task2.swap(task1);
  EXPECT_FALSE(task1.valid());
  EXPECT_TRUE(task2.valid());
}

template <typename T> void get_task_future_twice() {
  pco::packaged_task<T()> task(some_value<T>);
  pco::future<T> f1, f2;
  EXPECT_NO_THROW(f1 = task.get_future());
  EXPECT_FUTURE_ERROR(f2 = task.get_future(),
                      std::future_errc::future_already_retrieved);
}

template <typename T> void successfull_call_makes_state_ready() {
  pco::packaged_task<T()> task(some_value<T>);
  pco::future<T> f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  task();
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_SOME_VALUE(f);
}

template <typename T> void failed_call_makes_state_ready() {
  pco::packaged_task<T()> task(
      []() -> T { throw std::runtime_error("operation failed"); });
  pco::future<T> f = task.get_future();
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  EXPECT_NO_THROW(task());
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(f.is_ready());

  EXPECT_RUNTIME_ERROR(f, "operation failed");
}

template <typename T> void swap_valid_tasks() {
  pco::packaged_task<T()> task1(some_value<T>);
  pco::packaged_task<T()> task2(some_value<T>);
  ASSERT_TRUE(task1.valid());
  ASSERT_TRUE(task2.valid());

  auto f1 = task1.get_future(), f2 = task2.get_future();
  ASSERT_TRUE(f1.valid());
  EXPECT_FALSE(f1.is_ready());
  ASSERT_TRUE(f2.valid());
  EXPECT_FALSE(f2.is_ready());

  task2.swap(task1);

  task1();
  EXPECT_FALSE(f1.is_ready());
  EXPECT_TRUE(f2.is_ready());

  task2();
  EXPECT_TRUE(f1.is_ready());
  EXPECT_TRUE(f2.is_ready());
}

template <typename T> void call_task_twice() {
  pco::packaged_task<T()> task(some_value<T>);
  ASSERT_TRUE(task.valid());

  ASSERT_NO_THROW(task());
  EXPECT_FUTURE_ERROR(task(), std::future_errc::promise_already_satisfied);
}

template <typename T> void function_task() {
  pco::packaged_task<T()> task(some_value<T>);
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task));
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template <typename T> void copyable_functor_task() {
  auto func = [timeout = 5ms]() -> T {
    std::this_thread::sleep_for(timeout);
    return some_value<T>();
  };
  static_assert(!std::is_function<decltype(func)>::value &&
                    std::is_copy_constructible<decltype(func)>::value,
                "Test written incorrectly");
  pco::packaged_task<T()> task{func};
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task));
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template <typename T> void moveonly_functor_task() {
  auto timeout_ptr = std::make_unique<std::chrono::milliseconds>(5ms);
  auto func = [timeout = std::move(timeout_ptr)]() -> T {
    std::this_thread::sleep_for(*timeout);
    return some_value<T>();
  };
  static_assert(!std::is_function<decltype(func)>::value &&
                    !std::is_copy_constructible<decltype(func)>::value &&
                    std::is_move_constructible<decltype(func)>::value,
                "Test written incorrectly");
  pco::packaged_task<T()> task{std::move(func)};
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task));
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

template <typename T> void two_param_task() {
  pco::packaged_task<T(std::chrono::milliseconds, int)> task(
      [](std::chrono::milliseconds tm, int n) -> T {
        EXPECT_EQ(tm, 1ms);
        EXPECT_EQ(n, 5);
        return some_value<T>();
      });
  auto f = task.get_future();
  g_future_tests_env->run_async(std::move(task), 1ms, 5);
  ASSERT_TRUE(f.valid());
  EXPECT_SOME_VALUE(f);
}

} // namespace tests

/**
 * \brief Verifies swap valid task with invalid for packaged task.
 */
TYPED_TEST(PackagedTaskTest, swap_valid_task_with_invalid) {
  tests::swap_valid_task_with_invalid<TypeParam>();
}
/**
 * \brief Verifies get task future twice for packaged task.
 */
TYPED_TEST(PackagedTaskTest, get_task_future_twice) {
  tests::get_task_future_twice<TypeParam>();
}
/**
 * \brief Verifies successfull call makes state ready for packaged task.
 */
TYPED_TEST(PackagedTaskTest, successfull_call_makes_state_ready) {
  tests::successfull_call_makes_state_ready<TypeParam>();
}
/**
 * \brief Verifies failed call makes state ready for packaged task.
 */
TYPED_TEST(PackagedTaskTest, failed_call_makes_state_ready) {
  tests::failed_call_makes_state_ready<TypeParam>();
}
/**
 * \brief Verifies swap valid tasks for packaged task.
 */
TYPED_TEST(PackagedTaskTest, swap_valid_tasks) {
  tests::swap_valid_tasks<TypeParam>();
}
/**
 * \brief Verifies call task twice for packaged task.
 */
TYPED_TEST(PackagedTaskTest, call_task_twice) {
  tests::call_task_twice<TypeParam>();
}
/**
 * \brief Verifies function task for packaged task.
 */
TYPED_TEST(PackagedTaskTest, function_task) {
  tests::function_task<TypeParam>();
}
/**
 * \brief Verifies copyable functor task for packaged task.
 */
TYPED_TEST(PackagedTaskTest, copyable_functor_task) {
  tests::copyable_functor_task<TypeParam>();
}
/**
 * \brief Verifies moveonly functor task for packaged task.
 */
TYPED_TEST(PackagedTaskTest, moveonly_functor_task) {
  tests::moveonly_functor_task<TypeParam>();
}
/**
 * \brief Verifies two param task for packaged task.
 */
TYPED_TEST(PackagedTaskTest, two_param_task) {
  tests::two_param_task<TypeParam>();
}

/**
 * \brief Verifies get future on invalid throws no state for packaged task.
 */
TYPED_TEST(PackagedTaskTest, get_future_on_invalid_throws_no_state) {
  pco::packaged_task<TypeParam()> task;
  EXPECT_FUTURE_ERROR((void)task.get_future(), std::future_errc::no_state);
}

/**
 * \brief Verifies run invalid throws no state for packaged task.
 */
TYPED_TEST(PackagedTaskTest, run_invalid_throws_no_state) {
  pco::packaged_task<TypeParam()> task;
  EXPECT_FUTURE_ERROR(task(), std::future_errc::no_state);
}

} // anonymous namespace
