#include <algorithm>
#include <list>

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"
#include "portable_concurrency/p_latch.hpp"

#include "simple_arena_allocator.h"
#include "test_helpers.h"

namespace {

/**
 * \brief Verifies empty sequence for when all vector.
 */
TEST(WhenAllVectorTest, empty_sequence) {
  std::list<pco::future<int>> empty;
  auto f = pco::when_all(empty.begin(), empty.end());

  ASSERT_TRUE(f.valid());
  ASSERT_TRUE(f.is_ready());

  std::vector<pco::future<int>> res = f.get();
  EXPECT_EQ(res.size(), 0u);
}

/**
 * \brief Verifies single future for when all vector.
 */
TEST(WhenAllVectorTest, single_future) {
  pco::promise<std::string> p;
  auto raw_f = p.get_future();
  auto f = pco::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value("Hello");
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(res[0].get(), "Hello");
}

/**
 * \brief Verifies single shared future for when all vector.
 */
TEST(WhenAllVectorTest, single_shared_future) {
  pco::promise<std::unique_ptr<int>> p;
  auto raw_f = p.get_future().share();
  auto f = pco::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  EXPECT_FALSE(f.is_ready());

  p.set_value(std::make_unique<int>(42));
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(*res[0].get(), 42);
}

/**
 * \brief Verifies single ready future for when all vector.
 */
TEST(WhenAllVectorTest, single_ready_future) {
  auto raw_f = pco::make_ready_future(123);
  auto f = pco::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(res[0].get(), 123);
}

/**
 * \brief Verifies single ready shared future for when all vector.
 */
TEST(WhenAllVectorTest, single_ready_shared_future) {
  auto raw_f = pco::make_ready_future(123).share();
  auto f = pco::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_EQ(res[0].get(), 123);
}

/**
 * \brief Verifies single error future for when all vector.
 */
TEST(WhenAllVectorTest, single_error_future) {
  auto raw_f = pco::make_exceptional_future<future_tests_env &>(
      std::runtime_error("panic"));
  auto f = pco::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_RUNTIME_ERROR(res[0], "panic");
}

/**
 * \brief Verifies single error shared future for when all vector.
 */
TEST(WhenAllVectorTest, single_error_shared_future) {
  auto raw_f = pco::make_exceptional_future<future_tests_env &>(
                   std::runtime_error("panic"))
                   .share();
  auto f = pco::when_all(&raw_f, &raw_f + 1);
  ASSERT_TRUE(f.valid());
  EXPECT_TRUE(raw_f.valid());
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 1u);

  ASSERT_TRUE(res[0].valid());
  ASSERT_TRUE(res[0].is_ready());
  EXPECT_RUNTIME_ERROR(res[0], "panic");
}

/**
 * \brief Verifies multiple futures for when all vector.
 */
TEST(WhenAllVectorTest, multiple_futures) {
  pco::promise<int> ps[5];
  pco::future<int> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

  auto f = pco::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto &fi : fs)
    EXPECT_FALSE(fi.valid());

  for (std::size_t pos : {3, 0, 1, 4, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(static_cast<int>(42 * pos));
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  int idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), 42 * idx++);
  }
}

/**
 * \brief Verifies multiple shared futures for when all vector.
 */
TEST(WhenAllVectorTest, multiple_shared_futures) {
  pco::promise<std::string> ps[5];
  pco::shared_future<std::string> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);

  auto f = pco::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());
  for (const auto &fi : fs)
    EXPECT_TRUE(fi.valid());

  for (std::size_t pos : {3, 0, 1, 4, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(to_string(42 * pos));
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), to_string(42 * idx++));
  }
}

/**
 * \brief Verifies multiple futures one initionally ready for when all vector.
 */
TEST(WhenAllVectorTest, multiple_futures_one_initionally_ready) {
  pco::promise<void> ps[5];
  pco::future<void> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[1].set_value();
  ASSERT_TRUE(fs[1].is_ready());

  auto f = pco::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos : {4, 0, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value();
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    ASSERT_NO_THROW(fc.get());
  }
}

/**
 * \brief Verifies multiple shared futures one initionally ready for when all vector.
 */
TEST(WhenAllVectorTest, multiple_shared_futures_one_initionally_ready) {
  int some_vars[5] = {5, 4, 3, 2, 1};
  pco::promise<int &> ps[5];
  pco::shared_future<int &> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[0].set_value(some_vars[0]);
  ASSERT_TRUE(fs[0].is_ready());

  auto f = pco::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  for (std::size_t pos : {4, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(some_vars[pos]);
  }
  ASSERT_TRUE(f.is_ready());

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(&fc.get(), &some_vars[idx++]);
  }
}

/**
 * \brief Verifies multiple futures one initionally error for when all vector.
 */
TEST(WhenAllVectorTest, multiple_futures_one_initionally_error) {
  pco::promise<std::unique_ptr<int>> ps[5];
  pco::future<std::unique_ptr<int>> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pco::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos : {0, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(nullptr);
  }

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    if (idx++ == 4)
      EXPECT_RUNTIME_ERROR(fc, "epic fail");
    else
      EXPECT_EQ(fc.get(), nullptr);
  }
}

/**
 * \brief Verifies multiple shared futures one initionally error for when all vector.
 */
TEST(WhenAllVectorTest, multiple_shared_futures_one_initionally_error) {
  pco::promise<std::size_t> ps[5];
  pco::shared_future<std::size_t> fs[5];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  ps[4].set_exception(std::make_exception_ptr(std::runtime_error("epic fail")));
  ASSERT_TRUE(fs[4].is_ready());

  auto f = pco::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());

  for (std::size_t pos : {0, 1, 3, 2}) {
    EXPECT_FALSE(f.is_ready());
    ps[pos].set_value(17 * pos);
  }

  auto res = f.get();
  EXPECT_EQ(res.size(), 5u);
  std::size_t idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    if (idx == 4)
      EXPECT_RUNTIME_ERROR(fc, "epic fail");
    else
      EXPECT_EQ(fc.get(), 17 * idx);
    ++idx;
  }
}

/**
 * \brief Verifies futures becomes ready concurrently for when all vector.
 */
TEST(WhenAllVectorTest, futures_becomes_ready_concurrently) {
  pco::promise<std::size_t> ps[3];
  pco::shared_future<std::size_t> fs[3];

  std::transform(std::begin(ps), std::end(ps), std::begin(fs),
                 get_promise_future);
  auto f = pco::when_all(std::begin(fs), std::end(fs));
  ASSERT_TRUE(f.valid());
  EXPECT_FALSE(f.is_ready());

  pco::latch latch{4};
  std::size_t idx = 0;
  for (auto &p : ps) {
    g_future_tests_env->run_async(
        [val = 5 * idx++, &latch, p = std::move(p)]() mutable {
          latch.count_down_and_wait();
          p.set_value(val);
        });
  }

  latch.count_down_and_wait();
  auto res = f.get();
  idx = 0;
  for (auto &fc : res) {
    ASSERT_TRUE(fc.valid());
    ASSERT_TRUE(fc.is_ready());
    EXPECT_EQ(fc.get(), 5 * idx++);
  }
}

/**
 * \brief Verifies is immediatelly ready on empty arg for when all on vector.
 */
TEST(when_all_on_vector, is_immediatelly_ready_on_empty_arg) {
  EXPECT_TRUE(pco::when_all(std::vector<pco::future<int>>{}).is_ready());
}

/**
 * \brief Verifies is immediatelly ready on vector of ready futures for when all on vector.
 */
TEST(when_all_on_vector, is_immediatelly_ready_on_vector_of_ready_futures) {
  EXPECT_TRUE(pco::when_all(std::vector<pco::shared_future<int>>{
                               {pco::make_ready_future(42),
                                pco::make_ready_future(100500)}})
                  .is_ready());
}

/**
 * \brief Verifies is not ready on vector with not ready futures for when all on vector.
 */
TEST(when_all_on_vector, is_not_ready_on_vector_with_not_ready_futures) {
  pco::promise<int> p;
  std::vector<pco::future<int>> futures;
  futures.push_back(pco::make_ready_future(42));
  futures.push_back(p.get_future());
  EXPECT_FALSE(pco::when_all(std::move(futures)).is_ready());
}

/**
 * \brief Verifies contains vector of ready futures when becomes ready for when all on vector.
 */
TEST(when_all_on_vector, contains_vector_of_ready_futures_when_becomes_ready) {
  std::vector<pco::future<int>> futures;
  futures.push_back(pco::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pco::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(pco::when_all(std::move(futures))
                  .next([](std::vector<pco::future<int>> futures) {
                    return std::all_of(futures.begin(), futures.end(),
                                       pco::future_ready);
                  })
                  .get());
}

/**
 * \brief Verifies reuses buffer of passed argument for when all on vector.
 */
TEST(when_all_on_vector, reuses_buffer_of_passed_argument) {
  std::vector<pco::future<int>> futures;
  futures.reserve(2);
  const auto *buf_ptr = futures.data();
  futures.push_back(pco::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pco::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(pco::when_all(std::move(futures))
                  .next([buf_ptr](std::vector<pco::future<int>> futures) {
                    return futures.data() == buf_ptr;
                  })
                  .get());
}

/**
 * \brief Verifies works with vector using custom allocator for when all on vector.
 */
TEST(when_all_on_vector, works_with_vector_using_custom_allocator) {
  static_arena<> arena;
  arena_vector<pco::future<int>> futures{{arena}};
  futures.reserve(2);
  const auto *buf_ptr = futures.data();
  futures.push_back(pco::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pco::async(g_future_tests_env, [] { return 100500; }));
  EXPECT_TRUE(pco::when_all(std::move(futures))
                  .next([buf_ptr](arena_vector<pco::future<int>> futures) {
                    return futures.data() == buf_ptr;
                  })
                  .get());
}

/**
 * \brief Verifies preserves order of futures for when all on vector.
 */
TEST(when_all_on_vector, preserves_order_of_futures) {
  static_arena<> arena;
  arena_vector<pco::future<int>> futures{{arena}};
  futures.reserve(2);
  futures.push_back(pco::async(g_future_tests_env, [] { return 42; }));
  futures.push_back(pco::async(g_future_tests_env, [] { return 100500; }));
  auto results = pco::when_all(std::move(futures))
                     .next([](arena_vector<pco::future<int>> futures) {
                       arena_vector<int> res{futures.get_allocator()};
                       std::transform(futures.begin(), futures.end(),
                                      std::back_inserter(res), pco::future_get);
                       return res;
                     })
                     .get();
  EXPECT_EQ(results, (arena_vector<int>{{42, 100500}, {arena}}));
}

} // anonymous namespace
