/**
 * @file test_reference_type_unsupported.cpp
 * @brief Tests documenting intentional divergences between portable_concurrency v2 and v1.
 *
 * ## Intentional Non-Parity: Design Constraints in v2
 *
 * The following behaviors differ from v1 by design and are **not parity defects**.
 *
 * ### 1. Reference return types are unsupported
 * v1 supports `pco::future<T&>` and `pco::promise<T&>` with reference semantics.
 * v2 enforces `static_assert(!std::is_reference_v<T>)` on `future_result<T,E>`,
 * `shared_result<T,E>`, and `packaged_task_result<T(...)>` to avoid lifetime and
 * move-semantics complexity in async contexts.
 * Workaround: use `std::reference_wrapper<T>` or pointers.
 *
 * ### 2. Allocator-extended constructor for promise_result is not supported
 * v1 `pco::promise<T>` accepts `(std::allocator_arg_t, Allocator)` constructor.
 * v2 `promise_result<T,E>` does not provide this overload; custom allocation is
 * out of scope for the no-exceptions embedded target. Verified by
 * `PromiseResultTest.allocator_constructor_is_not_supported`.
 *
 * ### 3. get_future() called twice returns invalid future (no exception thrown)
 * v1 throws `std::future_error(std::future_errc::future_already_retrieved)`.
 * v2 silently returns an invalid (no-state) `future_result` to remain compatible
 * with `-fno-exceptions` builds. Verified by
 * `PromiseResultTest.get_future_twice_returns_invalid_second_future`.
 *
 * ### 4. Exception propagation is conditional on PC_V2_HAS_EXCEPTIONS
 * v1 propagates arbitrary exceptions via `std::exception_ptr` through `set_exception`.
 * v2 maps exceptional states to `result_error::execution_failure` by default.
 * Exception-propagating overloads (`set_exception`, `then` exception paths) are
 * only available when `PC_V2_HAS_EXCEPTIONS` is defined. This is intentional for
 * embedded / no-exceptions targets.
 *
 * ### 5. Error type is result_error enum, not std::future_error/std::error_code
 * v1 reports errors via `std::future_error` (which is a `std::exception` subclass).
 * v2 uses a scoped `result_error` enum as the default error type E, with no
 * `std::error_category` or `std::error_code` compatibility layer.
 */

#include <gtest/gtest.h>

#include "portable_concurrency/p_future.hpp"

namespace
{
using namespace pco::v2;

/**
 * @brief Verifies the design constraint: std::reference_wrapper<T> is the idiomatic workaround.
 */
TEST(ReferenceTypeUnsupportedTest, reference_wrapper_provides_workaround)
{
    promise_result<std::reference_wrapper<int>> promise;
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
    promise_result<int *> promise;
    auto future = promise.get_future();

    int value = 99;
    promise.set_value(&value);

    auto result = future.get_result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), 99);
}

} // namespace
