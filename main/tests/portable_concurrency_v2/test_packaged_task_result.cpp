/**
 * @file test_packaged_task_result.cpp
 * @brief Unit tests for portable_concurrency v2 packaged_task_result.
 */

#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "portable_concurrency/p_future_v2.hpp"

namespace
{
using namespace portable_concurrency::v2;

TEST(PackagedTaskResultTest, default_constructed_is_invalid)
{
    packaged_task_result<int(int)> task;
    EXPECT_FALSE(task.valid());
}

TEST(PackagedTaskResultTest, constructed_from_callable_is_valid)
{
    packaged_task_result<int(int)> task([](int v)
    {
        return v + 1;
    });
    EXPECT_TRUE(task.valid());
}

TEST(PackagedTaskResultTest, invoke_with_value_argument_delivers_result)
{
    packaged_task_result<int(int)> task([](int v)
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
    packaged_task_result<int(std::unique_ptr<int>)> task([](std::unique_ptr<int> p)
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
    packaged_task_result<future_result<int>()> task([]
    {
        return make_ready_result(42);
    });

    auto future = task.get_future();
    task();

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(PackagedTaskResultTest, unwraps_nested_shared_result)
{
    packaged_task_result<shared_result<int>()> task([]
    {
        return make_ready_result(7).share();
    });

    auto shared = task.get_future();
    task();

    const auto &result = shared.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 7);
}

TEST(PackagedTaskResultTest, unsatisfied_task_destruction_yields_broken_promise)
{
    future_result<int> future;
    {
        packaged_task_result<int()> task([]
        {
            return 5;
        });
        future = task.get_future();
    }

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
TEST(PackagedTaskResultTest, thrown_exception_maps_to_execution_failure)
{
    packaged_task_result<int()> task([]() -> int
    {
        throw 7;
    });

    auto future = task.get_future();
    task();

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::execution_failure);
}
#endif


// ---------------------------------------------------------------------------
// P2.3 — packaged_task parity depth
// ---------------------------------------------------------------------------

// -- Move-validity transitions -----------------------------------------------

TEST(PackagedTaskResultTest, move_constructed_from_valid_is_valid)
{
    packaged_task_result<int(int)> src([](int v) { return v; });
    packaged_task_result<int(int)> dst(std::move(src));
    EXPECT_TRUE(dst.valid());
}

TEST(PackagedTaskResultTest, move_constructed_from_invalid_is_invalid)
{
    packaged_task_result<int(int)> src;
    packaged_task_result<int(int)> dst(std::move(src));
    EXPECT_FALSE(dst.valid());
}

TEST(PackagedTaskResultTest, move_assigned_with_valid_is_valid)
{
    packaged_task_result<int(int)> src([](int v) { return v; });
    packaged_task_result<int(int)> dst;
    dst = std::move(src);
    EXPECT_TRUE(dst.valid());
}

TEST(PackagedTaskResultTest, move_assigned_with_invalid_is_invalid)
{
    packaged_task_result<int(int)> src;
    packaged_task_result<int(int)> dst;
    dst = std::move(src);
    EXPECT_FALSE(dst.valid());
}

TEST(PackagedTaskResultTest, moved_to_constructor_leaves_source_invalid)
{
    packaged_task_result<int(int)> src([](int v) { return v; });
    packaged_task_result<int(int)> dst(std::move(src));
    EXPECT_FALSE(src.valid());
}

TEST(PackagedTaskResultTest, moved_to_assignment_leaves_source_invalid)
{
    packaged_task_result<int(int)> src([](int v) { return v; });
    packaged_task_result<int(int)> dst;
    dst = std::move(src);
    EXPECT_FALSE(src.valid());
}

// -- Argument categories -----------------------------------------------------

TEST(PackagedTaskResultTest, invoke_with_lvalue_const_reference_argument)
{
    packaged_task_result<std::size_t(std::string)> task(
        [](const std::string &s) { return s.size(); });

    auto future = task.get_future();
    task("qwe");

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3u);
}

TEST(PackagedTaskResultTest, invoke_with_rvalue_reference_argument)
{
    packaged_task_result<int(std::unique_ptr<int>&&)> task(
        [](std::unique_ptr<int> &&p) { return *p; });

    auto future = task.get_future();
    task(std::make_unique<int>(42));

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

// -- Callable destruction timing ---------------------------------------------

TEST(PackagedTaskResultTest, destroys_callable_after_invocation)
{
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;

    packaged_task_result<int(int)> task(
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
    packaged_task_result<future_result<int>()> task([] {
        return future_result<int>{};   // default-constructed == no_state
    });

    auto future = task.get_future();
    task();

    auto result = future.get_result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), result_error::broken_promise);
}

} // namespace
