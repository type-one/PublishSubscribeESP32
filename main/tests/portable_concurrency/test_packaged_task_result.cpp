/**
 * @file test_packaged_task_result.cpp
 * @brief Unit tests for portable_concurrency v2 pco::packaged_task_result.
 */

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <utility>

#include "portable_concurrency/p_future.hpp"

namespace
{

TEST(PackagedTaskResultTest, default_constructed_is_invalid)
{
    pco::packaged_task_result<int(int)> task;
    EXPECT_FALSE(task.valid());
}

TEST(PackagedTaskResultTest, constructed_from_callable_is_valid)
{
    pco::packaged_task_result<int(int)> task([](int v)
    {
        return v + 1;
    });
    EXPECT_TRUE(task.valid());
}

TEST(PackagedTaskResultTest, invoke_with_value_argument_delivers_result)
{
    pco::packaged_task_result<int(int)> task([](int v)
    {
        return v * 2;
    });

    auto future = task.get_future();
    task(21);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(PackagedTaskResultTest, invoke_with_move_only_argument)
{
    pco::packaged_task_result<int(std::unique_ptr<int>)> task([](std::unique_ptr<int> p)
    {
        return *p;
    });

    auto future = task.get_future();
    task(std::make_unique<int>(42));

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(PackagedTaskResultTest, unwraps_nested_future_result)
{
    pco::packaged_task_result<pco::future_result<int>()> task([]
    {
        return pco::make_ready_result(42);
    });

    auto future = task.get_future();
    task();

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(PackagedTaskResultTest, unwraps_nested_shared_result)
{
    pco::packaged_task_result<pco::shared_result<int>()> task([]
    {
        return pco::make_ready_result(7).share();
    });

    auto shared = task.get_future();
    task();

    const auto &result = shared.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

TEST(PackagedTaskResultTest, unsatisfied_task_destruction_yields_broken_promise)
{
    pco::future_result<int> future;
    {
        pco::packaged_task_result<int()> task([]
        {
            return 5;
        });
        future = task.get_future();
    }

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::broken_promise);
}

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
TEST(PackagedTaskResultTest, thrown_exception_propagates)
{
    pco::packaged_task_result<int()> task([]() -> int
    {
        throw 7;
    });

    auto future = task.get_future();

    // v2 no longer catches exceptions in pco::packaged_task_result internals.
    // In exception-enabled builds, the callable exception propagates to caller.
    EXPECT_THROW(task(), int);

    // Promise was not fulfilled due to propagation; when task goes out of scope,
    // future transitions to broken_promise.
    task = pco::packaged_task_result<int()>{};
    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::broken_promise);
}
#endif


// ---------------------------------------------------------------------------
// P2.3 — packaged_task parity depth
// ---------------------------------------------------------------------------

// -- Move-validity transitions -----------------------------------------------

TEST(PackagedTaskResultTest, move_constructed_from_valid_is_valid)
{
    pco::packaged_task_result<int(int)> src([](int v) { return v; });
    pco::packaged_task_result<int(int)> dst(std::move(src));
    EXPECT_TRUE(dst.valid());
}

TEST(PackagedTaskResultTest, move_constructed_from_invalid_is_invalid)
{
    pco::packaged_task_result<int(int)> src;
    pco::packaged_task_result<int(int)> dst(std::move(src));
    EXPECT_FALSE(dst.valid());
}

TEST(PackagedTaskResultTest, move_assigned_with_valid_is_valid)
{
    pco::packaged_task_result<int(int)> src([](int v) { return v; });
    pco::packaged_task_result<int(int)> dst;
    dst = std::move(src);
    EXPECT_TRUE(dst.valid());
}

TEST(PackagedTaskResultTest, move_assigned_with_invalid_is_invalid)
{
    pco::packaged_task_result<int(int)> src;
    pco::packaged_task_result<int(int)> dst;
    dst = std::move(src);
    EXPECT_FALSE(dst.valid());
}

TEST(PackagedTaskResultTest, moved_to_constructor_leaves_source_invalid)
{
    pco::packaged_task_result<int(int)> src([](int v) { return v; });
    pco::packaged_task_result<int(int)> dst(std::move(src));
    EXPECT_FALSE(src.valid());
}

TEST(PackagedTaskResultTest, moved_to_assignment_leaves_source_invalid)
{
    pco::packaged_task_result<int(int)> src([](int v) { return v; });
    pco::packaged_task_result<int(int)> dst;
    dst = std::move(src);
    EXPECT_FALSE(src.valid());
}

// -- Argument categories -----------------------------------------------------

TEST(PackagedTaskResultTest, invoke_with_lvalue_const_reference_argument)
{
    pco::packaged_task_result<std::size_t(std::string)> task(
        [](const std::string &s) { return s.size(); });

    auto future = task.get_future();
    task("qwe");

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3u);
}

TEST(PackagedTaskResultTest, invoke_with_rvalue_reference_argument)
{
    pco::packaged_task_result<int(std::unique_ptr<int>&&)> task(
        [](std::unique_ptr<int> &&p) { return *p; });

    auto future = task.get_future();
    task(std::make_unique<int>(42));

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(PackagedTaskResultTest, return_reference_wrapper_preserves_reference_semantics)
{
    int value = 11;
    pco::packaged_task_result<std::reference_wrapper<int>()> task([&value]() {
        return std::ref(value);
    });

    auto future = task.get_future();
    task();

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());

    result.value().get() = 77;
    EXPECT_EQ(value, 77);
}

TEST(PackagedTaskResultTest, accepts_reference_wrapper_argument)
{
    int value = 10;
    pco::packaged_task_result<int(std::reference_wrapper<int>)> task(
        [](std::reference_wrapper<int> wrapped) {
            wrapped.get() += 5;
            return wrapped.get();
        });

    auto future = task.get_future();
    task(std::ref(value));

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 15);
    EXPECT_EQ(value, 15);
}

// -- Callable destruction timing ---------------------------------------------

TEST(PackagedTaskResultTest, destroys_callable_after_invocation)
{
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;

    pco::packaged_task_result<int(int)> task(
        [sp = std::exchange(sp, nullptr)](int val) { return val + *sp; });

    auto future = task.get_future();
    task(100500);
    // run() has completed: the callable's local copy is destroyed at end of run()
    EXPECT_TRUE(wp.expired());

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 100542);
}

// -- Invalid-handle unwrap parity --------------------------------------------

TEST(PackagedTaskResultTest, invalid_nested_future_result_unwraps_to_broken_promise)
{
    pco::packaged_task_result<pco::future_result<int>()> task([] {
        return pco::future_result<int>{};   // default-constructed == no_state
    });

    auto future = task.get_future();
    task();

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pco::result_error::broken_promise);
}

} // namespace
