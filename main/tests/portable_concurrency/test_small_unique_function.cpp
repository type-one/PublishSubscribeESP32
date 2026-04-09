#include <array>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "portable_concurrency/p_functional.hpp"
#include "portable_concurrency/p_future.hpp"

namespace {
namespace test {

struct small_unique_function : ::testing::Test {};

/**
 * \brief Verifies default constructed is null for small unique function.
 */
TEST_F(small_unique_function, default_constructed_is_null) {
  pc::detail::small_unique_function<int()> func;
  EXPECT_FALSE(func);
}

/**
 * \brief Verifies wrapps func pointer for small unique function.
 */
TEST_F(small_unique_function, wrapps_func_pointer) {
  pc::detail::small_unique_function<size_t(const char *)> foo = std::strlen;
  EXPECT_EQ(foo("hello"), 5u);
}

/**
 * \brief Verifies wrapps member func pointer for small unique function.
 */
TEST_F(small_unique_function, wrapps_member_func_pointer) {
  struct large_t {
    std::array<int, 128> arr;

    void fill(int first, int step) {
      for (int &item : arr)
        first = (item = first) + step;
    }
  } large;

  pc::detail::small_unique_function<void(large_t &, int, int)> foo =
      &large_t::fill;
  foo(large, 0, 1);
  EXPECT_EQ(large.arr[0], 0);
  EXPECT_EQ(large.arr[127], 127);
}

/**
 * \brief Verifies wrapps refference wrapper for small unique function.
 */
TEST_F(small_unique_function, wrapps_refference_wrapper) {
  struct {
    std::array<int, 128> arr;

    size_t operator()() const { return arr.size(); }
  } large;

  pc::detail::small_unique_function<size_t()> foo = std::ref(large);
  EXPECT_EQ(foo(), 128u);
}

/**
 * \brief Verifies wrapps packaged task for small unique function.
 */
TEST_F(small_unique_function, wrapps_packaged_task) {
  pc::packaged_task<size_t(const std::string &)> task{
      [](const std::string &str) { return str.size(); }};
  auto future = task.get_future();

  pc::detail::small_unique_function<void(const std::string &)> foo =
      std::move(task);
  foo("hello");
  ASSERT_TRUE(future.is_ready());
  EXPECT_EQ(future.get(), 5u);
}

/**
 * \brief Verifies lamda with this as only capture for small unique function.
 */
TEST_F(small_unique_function, lamda_with_this_as_only_capture) {
  struct {
    int x;
    auto foo() {
      return [this](int a) { return a * x; };
    }
  } obj;
  pc::detail::small_unique_function<int(int)> f = obj.foo();
  EXPECT_TRUE(f);
}

/**
 * \brief Verifies const object can be invoked for small unique function.
 */
TEST_F(small_unique_function, const_object_can_be_invoked) {
  const pc::detail::small_unique_function<int(int)> f = [m = 0](int x) mutable {
    return x * (++m);
  };
  EXPECT_EQ(f(2), 2);
}

} // namespace test
} // anonymous namespace
