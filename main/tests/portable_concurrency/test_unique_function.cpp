#include <cmath>
#include <cstring>
#include <exception>
#include <functional>

#include <gtest/gtest.h>

#include "portable_concurrency/p_functional.hpp"

namespace {

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define PC_TEST_HAS_EXCEPTIONS 1
#else
#define PC_TEST_HAS_EXCEPTIONS 0
#endif

namespace test {

struct point {
  int x;
  int y;

  int dist_from_center() const { return std::sqrt(x * x + y * y); }
};

struct big {
  std::uint64_t u0;
  std::uint64_t u1;
  std::uint64_t u2;
  std::uint64_t u3;
  std::uint64_t u4;
  std::uint64_t u5;
};

struct move_only_functor {
  move_only_functor(int val) : data(new int{val}) {}

  int operator()(int x) const { return x + *data; }

  std::unique_ptr<int> data;
};

struct throw_on_move_functor {
  throw_on_move_functor() = default;

  throw_on_move_functor(throw_on_move_functor &&rhs) {
    if (rhs.must_throw) {
#if PC_TEST_HAS_EXCEPTIONS
      throw std::runtime_error{"move ctor"};
#else
      std::terminate();
#endif
    }
    must_throw = rhs.must_throw;
  }
  throw_on_move_functor &operator=(throw_on_move_functor &&rhs) {
    if (rhs.must_throw) {
#if PC_TEST_HAS_EXCEPTIONS
      throw std::runtime_error{"move ctor"};
#else
      std::terminate();
#endif
    }
    must_throw = rhs.must_throw;
    return *this;
  }

  void operator()(bool must_throw) { this->must_throw = must_throw; }

  bool must_throw = false;
};

/**
 * \brief Verifies default constructed is empty for unique function.
 */
TEST(UniqueFunction, default_constructed_is_empty) {
  pco::unique_function<void()> f;
  EXPECT_FALSE(f);
#if PC_TEST_HAS_EXCEPTIONS
  ASSERT_THROW(f(), std::bad_function_call);
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies nullptr constructed is empty for unique function.
 */
TEST(UniqueFunction, nullptr_constructed_is_empty) {
  pco::unique_function<void()> f = nullptr;
  EXPECT_FALSE(f);
#if PC_TEST_HAS_EXCEPTIONS
  ASSERT_THROW(f(), std::bad_function_call);
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies move ctor do not throw when captured func move throws for unique function.
 */
TEST(UniqueFunction, move_ctor_do_not_throw_when_captured_func_move_throws) {
#if PC_TEST_HAS_EXCEPTIONS
  pco::unique_function<void(bool)> f0 = throw_on_move_functor{};
  f0(true);
  EXPECT_NO_THROW(pco::unique_function<void(bool)>{std::move(f0)});
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies move assign do not throw when captured func move throws for unique function.
 */
TEST(UniqueFunction, move_assign_do_not_throw_when_captured_func_move_throws) {
#if PC_TEST_HAS_EXCEPTIONS
  pco::unique_function<void(bool)> f0 = throw_on_move_functor{}, f1;
  f0(true);
  EXPECT_NO_THROW(f1 = std::move(f0));
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies null func ptr constructed is empty for unique function.
 */
TEST(UniqueFunction, null_func_ptr_constructed_is_empty) {
  using func_t = void(int);

  func_t *fptr = nullptr;
  pco::unique_function<void(int)> f = fptr;
  EXPECT_FALSE(f);
#if PC_TEST_HAS_EXCEPTIONS
  ASSERT_THROW(f(5), std::bad_function_call);
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies empty std func constructed is empty for unique function.
 */
TEST(UniqueFunction, empty_std_func_constructed_is_empty) {
  std::function<int(std::string)> stdfunc;

  pco::unique_function<int(std::string)> f = stdfunc;
  EXPECT_FALSE(f);
#if PC_TEST_HAS_EXCEPTIONS
  ASSERT_THROW(f("qwe"), std::bad_function_call);
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies null member func pointer is empty for unique function.
 */
TEST(UniqueFunction, null_member_func_pointer_is_empty) {
  using mem_fn_ptr_t = decltype(&std::string::size);
  mem_fn_ptr_t null_mem_fn = nullptr;

  pco::unique_function<size_t(std::string)> f = null_mem_fn;
  EXPECT_FALSE(f);
#if PC_TEST_HAS_EXCEPTIONS
  ASSERT_THROW(f("qwe"), std::bad_function_call);
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies null member obj ptr constructed is empty for unique function.
 */
TEST(UniqueFunction, null_member_obj_ptr_constructed_is_empty) {
  decltype(&point::x) null_xptr = nullptr;

  pco::unique_function<int(point)> f = null_xptr;
  EXPECT_FALSE(f);
#if PC_TEST_HAS_EXCEPTIONS
  ASSERT_THROW(f(point{100, 500}), std::bad_function_call);
#else
  GTEST_SKIP() << "Exception-only check in no-exception build";
#endif
}

/**
 * \brief Verifies raw function call for unique function.
 */
TEST(UniqueFunction, raw_function_call) {
  pco::unique_function<size_t(const char *)> f = std::strlen;

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), 5u);
}

/**
 * \brief Verifies member function call for unique function.
 */
TEST(UniqueFunction, member_function_call) {
  pco::unique_function<int(const point &)> f = &point::dist_from_center;

  EXPECT_TRUE(f);
  EXPECT_EQ(f(point{3, 4}), 5);
}

/**
 * \brief Verifies member object get for unique function.
 */
TEST(UniqueFunction, member_object_get) {
  pco::unique_function<int(point)> f = &point::x;

  EXPECT_TRUE(f);
  EXPECT_EQ(f(point{100, 500}), 100);
}

/**
 * \brief Verifies functor call for unique function.
 */
TEST(UniqueFunction, functor_call) {
  pco::unique_function<std::string(const char *)> f =
      [accum = std::string{}](const char *val) mutable { return accum += val; };

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), "Hello");
  EXPECT_EQ(f(" World"), "Hello World");
}

/**
 * \brief Verifies std function call for unique function.
 */
TEST(UniqueFunction, std_function_call) {
  std::function<std::string(const char *)> std_f =
      [accum = std::string{}](const char *val) mutable { return accum += val; };

  pco::unique_function<std::string(const char *)> f = std_f;

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), "Hello");
  EXPECT_EQ(f(" World"), "Hello World");
  EXPECT_EQ(std_f("Hello"), "Hello");
}

/**
 * \brief Verifies refference wrapper call for unique function.
 */
TEST(UniqueFunction, refference_wrapper_call) {
  auto func = [accum = std::string{}](const char *val) mutable {
    return accum += val;
  };

  pco::unique_function<std::string(const char *)> f = std::ref(func);

  EXPECT_TRUE(f);
  EXPECT_EQ(f("Hello"), "Hello");
  EXPECT_EQ(func(" World"), "Hello World");
}

/**
 * \brief Verifies call small non copyable function for unique function.
 */
TEST(UniqueFunction, call_small_non_copyable_function) {
  pco::unique_function<int(int)> f = move_only_functor{42};
  EXPECT_EQ(f(42), 84);
}

/**
 * \brief Verifies call small functor after move ctor for unique function.
 */
TEST(UniqueFunction, call_small_functor_after_move_ctor) {
  pco::unique_function<int(point)> f0 = &point::dist_from_center;
  EXPECT_TRUE(f0);
  pco::unique_function<int(point)> f1 = std::move(f0);
  EXPECT_TRUE(f1);
  EXPECT_EQ(f1(point{3, 4}), 5);
}

/**
 * \brief Verifies call big functor after move ctor for unique function.
 */
TEST(UniqueFunction, call_big_functor_after_move_ctor) {
  pco::unique_function<uint64_t(uint64_t)> f0 =
      [c = big{1, 2, 3, 4, 5, 6}](uint64_t x) {
        return x + c.u0 + c.u1 + c.u2 + c.u3 + c.u4 + c.u5;
      };
  EXPECT_TRUE(f0);
  pco::unique_function<uint64_t(uint64_t)> f1 = std::move(f0);
  EXPECT_TRUE(f1);
  EXPECT_EQ(f1(42), 63u);
}

/**
 * \brief Verifies call small non copyable function after move ctor for unique function.
 */
TEST(UniqueFunction, call_small_non_copyable_function_after_move_ctor) {
  pco::unique_function<int(int)> f0 = move_only_functor{42};
  pco::unique_function<int(int)> f1 = std::move(f0);
  EXPECT_EQ(f1(42), 84);
}

/**
 * \brief Verifies call small functor after move asign for unique function.
 */
TEST(UniqueFunction, call_small_functor_after_move_asign) {
  pco::unique_function<int(point)> f0 = &point::dist_from_center;
  pco::unique_function<int(point)> f1;

  EXPECT_TRUE(f0);
  EXPECT_FALSE(f1);

  f1 = std::move(f0);

  EXPECT_TRUE(f1);
  EXPECT_EQ(f1(point{3, 4}), 5);
}

/**
 * \brief Verifies call big functor after move asign for unique function.
 */
TEST(UniqueFunction, call_big_functor_after_move_asign) {
  pco::unique_function<uint64_t(uint64_t)> f0 =
      [c = big{1, 2, 3, 4, 5, 6}](uint64_t x) {
        return x + c.u0 + c.u1 + c.u2 + c.u3 + c.u4 + c.u5;
      };
  pco::unique_function<uint64_t(uint64_t)> f1;
  EXPECT_TRUE(f0);
  EXPECT_FALSE(f1);

  f1 = std::move(f0);

  EXPECT_TRUE(f1);
  EXPECT_EQ(f1(42), 63u);
}

/**
 * \brief Verifies call small non copyable function after move asign for unique function.
 */
TEST(UniqueFunction, call_small_non_copyable_function_after_move_asign) {
  pco::unique_function<int(int)> f0 = move_only_functor{42};
  pco::unique_function<int(int)> f1;
  f1 = std::move(f0);
  EXPECT_EQ(f1(42), 84);
}

/**
 * \brief Verifies pass builtin type argument for unique function.
 */
TEST(UniqueFunction, pass_builtin_type_argument) {
  pco::unique_function<int(int)> f = [](int x) { return 2 * x; };
  EXPECT_EQ(f(21), 42);
}

/**
 * \brief Verifies pass copyable type argument for unique function.
 */
TEST(UniqueFunction, pass_copyable_type_argument) {
  pco::unique_function<size_t(std::string)> f = [](std::string s) {
    return s.size();
  };
  EXPECT_EQ(f("foo"), 3u);
}

/**
 * \brief Verifies pass moveable type argument for unique function.
 */
TEST(UniqueFunction, pass_moveable_type_argument) {
  pco::unique_function<int(std::unique_ptr<int>)> f =
      [](std::unique_ptr<int> p) { return *p; };
  EXPECT_EQ(f(std::make_unique<int>(42)), 42);
}

/**
 * \brief Verifies pass moveable type argument to big function for unique function.
 */
TEST(UniqueFunction, pass_moveable_type_argument_to_big_function) {
  pco::unique_function<int(std::unique_ptr<int>)> f =
      [obj = big{1, 2, 3, 4, 5, 6}](std::unique_ptr<int> p) {
        return *p + obj.u3;
      };
  EXPECT_EQ(f(std::make_unique<int>(42)), 46);
}

/**
 * \brief Verifies pass const refference type argument for unique function.
 */
TEST(UniqueFunction, pass_const_refference_type_argument) {
  pco::unique_function<size_t(const std::string &)> f =
      [](const std::string &s) { return s.size(); };
  EXPECT_EQ(f("foo"), 3u);
}

/**
 * \brief Verifies pass rvalue refference type argument for unique function.
 */
TEST(UniqueFunction, pass_rvalue_refference_type_argument) {
  pco::unique_function<int(std::unique_ptr<int> &&)> f =
      [](std::unique_ptr<int> &&p) { return *p; };
  EXPECT_EQ(f(std::make_unique<int>(42)), 42);
}

/**
 * \brief Verifies pass refference type argument for unique function.
 */
TEST(UniqueFunction, pass_refference_type_argument) {
  pco::unique_function<void(std::string &)> f = [](std::string &s) {
    s = "'" + s + "'";
  };
  std::string str = "Hello";
  f(str);
  EXPECT_EQ(str, "'Hello'");
}

/**
 * \brief Verifies const object can be invoked for unique function.
 */
TEST(UniqueFunction, const_object_can_be_invoked) {
  const pco::unique_function<int(int)> f = [m = 1](int x) mutable {
    return x * (++m);
  };
  EXPECT_EQ(f(2), 4);
}

/**
 * \brief Verifies void function ignores wrapped callable return type for unique function.
 */
TEST(UniqueFunction, void_function_ignores_wrapped_callable_return_type) {
  bool invoked = false;
  pco::unique_function<void(int)> f = [&](int x) -> int {
    invoked = true;
    return 2 * x;
  };
  f(2);
  EXPECT_TRUE(invoked);
}

} // namespace test

} // anonymous namespace
