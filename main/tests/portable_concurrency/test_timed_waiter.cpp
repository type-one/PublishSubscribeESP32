#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"
#include "portable_concurrency/p_latch.hpp"
#include "portable_concurrency/p_timed_waiter.hpp"

#include "test_tools.h"

namespace portable_concurrency {
namespace {
namespace test {

using namespace std::literals;

template <typename Future> struct timed_waiter_poling : ::testing::Test {
  Future future;
  pco::timed_waiter waiter;

  timed_waiter_poling() {
    future = pco::async(g_future_tests_env, [] {
      std::this_thread::sleep_for(25ms);
      return 100500;
    });
    waiter = pco::timed_waiter{future};
  }
};
using poling_futures =
    ::testing::Types<pco::future<int>, pco::shared_future<int>>;
TYPED_TEST_CASE(timed_waiter_poling, poling_futures);

/**
 * \brief Verifies future is ready after waiter returns ready for timed waiter poling.
 */
TYPED_TEST(timed_waiter_poling, future_is_ready_after_waiter_returns_ready) {
  while (this->waiter.wait_for(500us) == pco::future_status::timeout)
    ;
  EXPECT_TRUE(this->future.is_ready());
}

template <typename Future> struct timed_waiter : ::testing::Test {
  pco::latch task_latch;
  Future future;
  pco::timed_waiter waiter;

  timed_waiter() : task_latch{2} {
    future = pco::async(g_future_tests_env,
                       [this] { task_latch.count_down_and_wait(); });
    waiter = pco::timed_waiter{future};
  }
};
using futures = ::testing::Types<pco::future<void>, pco::shared_future<void>>;
TYPED_TEST_CASE(timed_waiter, futures);

/**
 * \brief Verifies wait for returns when future becomes ready for timed waiter.
 */
TYPED_TEST(timed_waiter, wait_for_returns_when_future_becomes_ready) {
  this->task_latch.count_down();
  EXPECT_EQ(this->waiter.wait_for(30min), pco::future_status::ready);
}

/**
 * \brief Verifies wait until returns when future becomes ready for timed waiter.
 */
TYPED_TEST(timed_waiter, wait_until_returns_when_future_becomes_ready) {
  this->task_latch.count_down();
  EXPECT_EQ(this->waiter.wait_until(std::chrono::system_clock::now() + 30min),
            pco::future_status::ready);
}

/**
 * \brief Verifies wait for returns with timeout if future not ready for timed waiter.
 */
TYPED_TEST(timed_waiter, wait_for_returns_with_timeout_if_future_not_ready) {
  EXPECT_EQ(this->waiter.wait_for(5ms), pco::future_status::timeout);
  this->task_latch.count_down();
}

/**
 * \brief Verifies wait until returns with timeout if future not ready for timed waiter.
 */
TYPED_TEST(timed_waiter, wait_until_returns_with_timeout_if_future_not_ready) {
  EXPECT_EQ(this->waiter.wait_until(std::chrono::steady_clock::now() + 5ms),
            pco::future_status::timeout);
  this->task_latch.count_down();
}

template <typename Future> struct timed_waiter_on_ready : ::testing::Test {
  Future future = pco::make_ready_future();
  pco::timed_waiter waiter;

  timed_waiter_on_ready() { waiter = pco::timed_waiter{future}; }
};
TYPED_TEST_CASE(timed_waiter_on_ready, futures);

/**
 * \brief Verifies wait until returns ready for timed waiter on ready.
 */
TYPED_TEST(timed_waiter_on_ready, wait_until_returns_ready) {
  EXPECT_EQ(this->waiter.wait_until(std::chrono::steady_clock::now() + 30min),
            pco::future_status::ready);
}
/**
 * \brief Verifies wait for returns ready for timed waiter on ready.
 */
TYPED_TEST(timed_waiter_on_ready, wait_for_returns_ready) {
  EXPECT_EQ(this->waiter.wait_for(2h), pco::future_status::ready);
}

} // namespace test
} // anonymous namespace
} // namespace portable_concurrency
