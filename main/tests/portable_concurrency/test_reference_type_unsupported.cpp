/**
 * @file test_reference_type_unsupported.cpp
 * @brief Tests documenting intentional divergences between current result-based API and legacy v1 API.
 *
 * ## Intentional Non-Parity: Design Constraints in the Result-Based API
 *
 * The following behaviors differ from v1 by design and are **not parity defects**.
 *
 * ### 1. Reference return types are unsupported
 * v1 supports `pco::future<T&>` and `pco::promise<T&>` with reference semantics.
 * current result-based API enforces `static_assert(!std::is_reference_v<T>)` on `pco::future_result<T,E>`,
 * `pco::shared_result<T,E>`, and `pco::packaged_task_result<T(...)>` to avoid lifetime and
 * move-semantics complexity in async contexts.
 * Workaround: use `std::reference_wrapper<T>` or pointers.
 *
 * ### 2. Allocator-extended constructor for pco::promise_result is not supported
 * v1 `pco::promise<T>` accepts `(std::allocator_arg_t, Allocator)` constructor.
 * current result-based API `pco::promise_result<T,E>` does not provide this overload; custom allocation is
 * out of scope for the no-exceptions embedded target. Verified by
 * `PromiseResultTest.allocator_constructor_is_not_supported`.
 *
 * ### 3. get_future() called twice returns invalid future (no exception thrown)
 * v1 throws `std::future_error(std::future_errc::future_already_retrieved)`.
 * the result-based API silently returns an invalid (no-state) `pco::future_result` to remain compatible
 * with `-fno-exceptions` builds. Verified by
 * `PromiseResultTest.get_future_twice_returns_invalid_second_future`.
 *
 * ### 4. Exception propagation is not supported
 * v1 propagates arbitrary exceptions via `std::exception_ptr` through `set_exception`.
 * the result-based API maps exceptional states to `pco::result_error::execution_failure`.
 * There are no try/catch paths in the result-based API; the design is exception-free for
 * compatibility with `-fno-exceptions` embedded targets (e.g. ESP32).
 *
 * ### 5. Error type is pco::result_error enum, not std::future_error/std::error_code
 * v1 reports errors via `std::future_error` (which is a `std::exception` subclass).
 * the result-based API uses a scoped `pco::result_error` enum as the default error type E, with no
 * `std::error_category` or `std::error_code` compatibility layer.
 */

#include <gtest/gtest.h>

#include "portable_concurrency/future.hpp"

namespace
{

/**
 * @brief Verifies the design constraint: std::reference_wrapper<T> is the idiomatic workaround.
 */
TEST(ReferenceTypeUnsupportedTest, reference_wrapper_provides_workaround)
{
    pco::promise_result<std::reference_wrapper<int>> promise;
    auto future = promise.get_future();

    int value = 42;
    promise.set_value(std::ref(value));

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().get(), 42);
}

/**
 * @brief Verifies pointer-based approach as an alternative workaround.
 */
TEST(ReferenceTypeUnsupportedTest, pointers_provide_alternative_workaround)
{
    pco::promise_result<int *> promise;
    auto future = promise.get_future();

    int value = 99;
    promise.set_value(&value);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), 99);
}

} // namespace
