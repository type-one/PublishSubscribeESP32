# portable_concurrency_v2 Parity Backlog

Goal: make `portable_concurrency_v2` in exceptions-off mode equivalent in features and behavior to `portable_concurrency` (exceptions-based), except for documented intentional divergences.

Historical note: references to `tests/portable_concurrency/*` in this backlog are source-parity anchors from the pre-removal v1 suite. The v1 files are no longer present in this repository.

## Priority Legend
- P0: blocking feature parity gap
- P1: high-value behavioral parity and migration safety
- P2: depth and hardening parity
- P3: documentation and policy hygiene

## P0 (Do First)

- [x] P0.1 Interruptible continuation parity (feature-level)
  - Target parity from v1:
    - `tests/portable_concurrency/test_cancelation.cpp` (InterruptableContinuation tests)
  - Scope:
    - Add v2 continuation form(s) that allow continuation-side control over destination completion.
    - Ensure continuation can observe destination await state (`is_awaiten`-style).
    - Preserve broken-promise behavior when continuation does not fulfill destination.
  - Deliverables:
    - API and implementation in v2 result future layer.
    - New v2 tests covering all above behaviors.
  - Status:
    - Implemented `then(...)` interruptible continuation overloads for both `future_result` and `shared_result` in `portable_concurrency/bits/result_future.h`.
    - Added no-exceptions parity tests in `tests/portable_concurrency_v2/test_continuation_result_no_exceptions.cpp`.

- [x] P0.2 Abandon/cancellation matrix parity
  - Target parity from v1:
    - `tests/portable_concurrency/test_abandon.cpp`
  - Scope:
    - Cover async/next/then paths, future/shared variants, and invalid nested-handle returns.
    - Verify destruction semantics of stored callables and promise state transitions.
  - Deliverables:
    - New `test_abandon_result.cpp` (or equivalent split) in v2 test folder.
  - Status:
    - Added `tests/portable_concurrency_v2/test_abandon_result.cpp` with 14 parity tests covering dropped async work, dropped continuations, packaged task abandonment, promise abandonment, and invalid nested-handle returns.
    - Registered the new test file in host CMake variants (`CMakeLists.txt`, `CMakeLists_PC.txt`) at the time of implementation.
    - Updated `resolve_nested_handle(...)` in `portable_concurrency/bits/result_future.h` to map invalid nested-handle (`no_state`) outcomes to `broken_promise`, matching v1 abandon semantics.
    - Fixed `promise_result` move-assignment abandonment behavior to avoid deadlock and correctly publish `broken_promise` for replaced pending states.

## P1 (High Value)

- [x] P1.1 Notify edge-case parity
  - Target parity from v1:
    - `tests/portable_concurrency/test_notify.cpp`
  - Add missing v2 checks:
    - notify on broken promise
    - notify not called when handle destroyed before completion
    - async completion path
    - error completion path coverage for future_result/shared_result
  - Status:
    - Added future-side notify parity tests in `tests/portable_concurrency_v2/test_future_result.cpp`:
      - `notify_runs_when_future_becomes_ready_with_error`
      - `notify_runs_when_promise_is_broken`
      - `notify_not_called_if_future_destroyed_before_completion`
      - `notify_runs_for_async_completion`
    - Added shared-side notify parity tests in `tests/portable_concurrency_v2/test_shared_result.cpp`:
      - `notify_runs_when_shared_becomes_ready_with_error`
      - `notify_runs_when_promise_is_broken`
      - `notify_not_called_if_shared_destroyed_before_completion`
      - `notify_runs_for_async_completion`
    - Validation: `./publish_subscribe_tests --gtest_filter='FutureResultTest.notify_*:SharedResultTest.notify_*'` passes (16/16).

- [x] P1.2 Promise parity completion
  - Target parity from v1:
    - `tests/portable_concurrency/test_promise.cpp`
  - Add missing v2 checks:
    - set-before-get_future retrieval behavior
    - moved-from no_state behavior
    - repeated completion semantics under v2 rules
    - allocator behavior decision (implement or explicitly mark unsupported)
  - Status:
    - Added promise parity tests in `tests/portable_concurrency_v2/test_promise_result.cpp`:
      - `can_retrieve_value_set_before_get_future`
      - `repeated_completion_without_future_is_harmless`
      - `repeated_completion_with_future_preserves_first_result`
      - `moved_from_get_future_returns_invalid_future`
      - `moved_from_set_value_is_noop`
      - `moved_from_set_error_is_noop`
      - `allocator_constructor_is_not_supported`
    - Validation: `./publish_subscribe_tests --gtest_filter='PromiseResultTest.*'` passes (27/27).

- [x] P1.3 Shared continuation multi-subscriber parity
  - Target parity from v1:
    - `tests/portable_concurrency/test_shared_future_then.cpp` (`all_of_multiple_continuations_are_invoked`)
  - Scope:
    - Multiple continuations attached to same shared source.
    - Deterministic, all-fired semantics with consistent observed result.
  - Status:
    - Added 5 parity tests in `tests/portable_concurrency_v2/test_shared_result.cpp`:
      - `all_then_value_continuations_are_invoked`
      - `all_continuations_observe_consistent_value`
      - `all_continuations_invoked_on_broken_promise`
      - `continuation_attached_after_readiness_fires_inline`
      - `multiple_notify_callbacks_all_invoked`
    - Validation: all 5 pass (0 ms each, no regressions in SharedResultTest suite).

## P2 (Depth and Hardening)

- [x] P2.1 when_all/when_any stress and edge parity
  - Target parity from v1:
    - `tests/portable_concurrency/test_when_all_tuple.cpp`
    - `tests/portable_concurrency/test_when_all_vector.cpp`
    - `tests/portable_concurrency/test_when_any_tuple.cpp`
    - `tests/portable_concurrency/test_when_any_vector.cpp`
  - Status:
    - Added 4 parity tests in `tests/portable_concurrency_v2/test_when_all_result.cpp`:
      - `some_inputs_initially_ready_combined_waits_for_pending`
      - `all_inputs_initially_ready_combined_is_immediately_ready`
      - `concurrent_readiness_collects_all_results`
      - `iterator_overload_preserves_input_order_in_results`
    - Added 4 parity tests in `tests/portable_concurrency_v2/test_when_any_result.cpp`:
      - `later_readiness_does_not_change_winner_index`
      - `concurrent_readiness_resolves_with_valid_winner_index`
      - `iterator_overload_winner_index_stable_after_more_complete`
      - `already_ready_input_wins_with_correct_index`
    - Validation: all 8 pass (34 ms total, no regressions).

- [x] P2.2 async parity depth
  - Target parity from v1:
    - `tests/portable_concurrency/test_async.cpp`
  - Status:
    - Added 4 parity tests in `tests/portable_concurrency_v2/test_async_result.cpp`:
      - `executes_callable_on_specified_executor_thread`
      - `unwraps_nested_future_result`
      - `unwraps_nested_shared_result`
      - `destroys_callable_after_invocation`
    - Updated `async_result(...)` in `portable_concurrency/bits/result_future.h` to preserve v1-style nested handle unwrapping and callable lifetime semantics.
    - Validation: all 4 pass (`AsyncResultTest.*`, 8 ms total).

- [x] P2.3 packaged_task parity depth
  - Target parity from v1:
    - `tests/portable_concurrency/test_packaged_task.cpp`
    - `tests/portable_concurrency/test_packaged_task_unwrap.cpp`
  - Status:
    - Added 10 parity tests in `tests/portable_concurrency_v2/test_packaged_task_result.cpp`:
      - `move_constructed_from_valid_is_valid`
      - `move_constructed_from_invalid_is_invalid`
      - `move_assigned_with_valid_is_valid`
      - `move_assigned_with_invalid_is_invalid`
      - `moved_to_constructor_leaves_source_invalid`
      - `moved_to_assignment_leaves_source_invalid`
      - `invoke_with_lvalue_const_reference_argument`
      - `invoke_with_rvalue_reference_argument`
      - `destroys_callable_after_invocation`
      - `invalid_nested_future_result_unwraps_to_broken_promise`
    - Validation: all 10 pass (`PackagedTaskResultTest.*`, 1 ms total).

## P3 (Policy and Documentation)

- [x] P3.1 Document intentional non-parity
  - Documented intentional divergences in `tests/portable_concurrency_v2/test_reference_type_unsupported.cpp`:
    1. Reference return types unsupported (static_assert; use `reference_wrapper<T>` or pointers)
    2. Allocator-extended `promise_result` constructor not provided (no-exceptions / embedded target)
    3. `get_future()` twice returns invalid future instead of throwing `future_already_retrieved`
    4. Exception propagation not supported; all exceptional states map to `result_error::execution_failure` (exception-free design for ESP32 / `-fno-exceptions` targets)
    5. Error type is `result_error` enum, not `std::future_error` / `std::error_code`

## Execution Order
1. P0.1 Interruptible continuation parity
2. P0.2 Abandon/cancellation matrix parity
3. P1.1 Notify edge-case parity
4. P1.2 Promise parity completion
5. P1.3 Shared continuation multi-subscriber parity
6. P2.1 when_all/when_any hardening
7. P2.2 async parity depth
8. P2.3 packaged_task parity depth
9. P3.1 intentional non-parity docs

## Definition of Done
- Every v1 behavior in async/future/promise/packaged_task/notify/when_all/when_any/cancelation is either:
  - implemented and tested in v2 exceptions-off, or
  - explicitly documented as intentional divergence with rationale.
- v2 tests compile and pass in host build variants used in this repo.
- New tests are linked into the active host CMake paths (`CMakeLists.txt`, `CMakeLists_PC.txt`) when needed.

## Progress Tracker
- [x] P0 complete
- [x] P1 complete
- [x] P2 complete
- [x] P3 complete
