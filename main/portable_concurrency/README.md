# portable_concurrency

Exception-free async/future API for this repository, derived from
[Sergey Vidyuk's portable_concurrency](https://github.com/VestniK/portable_concurrency)
(original code used exceptions; this version replaces them with `tools::expected`).

The design is inspired by the C++ standardisation proposals:

- [P0159R0 — Extensions for Concurrency TS](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0159r0.html)
- [P0443R14 — A Unified Executors Proposal for C++](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0443r14.html)

## Key Features

- **Executor-aware**: post callables to any executor via `pco::async(executor, callable)`.
- **Continuations**: chain asynchronous steps with `.next()` / `.then_value()` / `.then()`.
- **Shared futures**: convert a move-only `future_result` to a copyable `shared_result` via `.share()`.
- **Composition**: `when_all` and `when_any` combinators over heterogeneous future packs.
- **Automatic cancellation**: continuations that have no live future handle attached are not executed.
- **Interruptible continuations**: `then(canceler_arg, callable)` overloads let a continuation check
  whether its result is still awaited.
- **Exception-free**: all operations return `tools::expected<T, E>`; no exceptions are thrown or
  required.  The error type defaults to `pco::result_error`.

## Public API Surface

Use these headers:

- `portable_concurrency/future.hpp` (primary unified entrypoint)
- `portable_concurrency/execution.hpp`
- `portable_concurrency/thread_pool.hpp`
- `portable_concurrency/latch.hpp`
- `portable_concurrency/functional.hpp`
- `portable_concurrency/functional_fwd.hpp`

## Include Guidance

```cpp
#include "portable_concurrency/future.hpp"
```

This exposes:

- `pco::future_result<T, E>` — move-only asynchronous result handle.
- `pco::shared_result<T, E>` — copyable shared result handle.
- `pco::promise_result<T, E>` — producer-side handle.
- `pco::packaged_task_result<R(A...)>` — task wrapper producing a `future_result`.
- Convenience aliases `pco::future_t<T>`, `pco::shared_future_t<T>`, `pco::promise_t<T>`.
- Free functions `pco::async(executor, callable)`, `pco::when_all(...)`, `pco::when_any(...)`.

## Timed Wait Guidance

Use timed waits directly on result handles:

- `future_result::wait_for(...)`
- `future_result::wait_until(...)`
- `shared_result::wait_for(...)`
- `shared_result::wait_until(...)`

## Notes

This repository maintains C++20 as primary target with C++17 compatibility where applicable in framework code.

## Inventory By File

This section documents every file under `main/portable_concurrency/`, with its role and its relationship to other parts of the subsystem.

### Top-Level Public Headers

| File | Key classes/types/functions | Purpose | Main relationships |
|---|---|---|---|
| `future.hpp` | `future_t`, `shared_future_t`, `promise_t`, `make_ready_default`, `make_error_default`, `make_async_default` | Unified public entrypoint for result-based async API in this repository. | Includes `bits/result_future.hpp` and `bits/packaged_task_result.hpp`. Exposes aliases around `future_result/shared_result/promise_result`. |
| `execution.hpp` | `is_executor`, `inplace_executor_t`, `inplace_executor`, ADL `post(...)` contract (via impl header) | Executor model and customization point used by async dispatch and continuation scheduling. | Thin wrapper over `bits/execution_impl.hpp`; used by async factories and continuation overloads. |
| `thread_pool.hpp` | `static_thread_pool`, `static_thread_pool::executor_type` | Fixed-size worker pool and executor adapter. | Wraps `bits/thread_pool_impl.hpp`; executor integrates via `is_executor` + `post`. |
| `latch.hpp` | `latch` | One-shot countdown synchronization primitive. | Wraps `bits/latch_impl.hpp`; runtime definitions in `bits/portable_concurrency_runtime.cpp`. |
| `functional.hpp` | `unique_function<R(A...)>` | Move-only callable wrapper used for task transport and continuations. | Wraps `bits/unique_function.hpp`; backed by `small_unique_function`. |
| `functional_fwd.hpp` | Forward declarations of `unique_function` | Lightweight declarations to reduce include cost when only type declarations are needed. | Wraps `bits/unique_function_fwd.hpp`. |

### Internal Entry and Runtime Files

| File | Key classes/types/functions | Purpose | Main relationships |
|---|---|---|---|
| `bits/result_future.hpp` | Internal aggregation header | Pulls together all result-future internals (`future`, `shared`, `promise`, combinators, factories). | Core include used by public `future.hpp`. |
| `bits/packaged_task_result.hpp` | `packaged_task_result<R(A...)>` + trait mappers | Move-only packaged task for result-based futures; supports nested handle unwrapping. | Depends on `result_future.hpp`; conceptually similar to `std::packaged_task` but exception-free with `tools::expected`. |
| `bits/portable_concurrency_runtime.cpp` | Runtime defs for `latch`, `static_thread_pool`, explicit template instantiations, `continuations_stack` methods | Out-of-line runtime implementation and explicit instantiation unit. | Complements multiple headers: `latch_impl.hpp`, `thread_pool_impl.hpp`, queue/stack/function wrappers. |

### Result-Future Subsystem (`bits/result_future/`)

| File | Key classes/types/functions | Purpose | Main relationships |
|---|---|---|---|
| `bits/result_future/types_and_detail.hpp` | `result_error`, `when_any_result`, `result_shared_state`, many type traits/deduction helpers | Foundational shared types and internal metaprogramming utilities. | Used by almost every other file in `result_future/`. |
| `bits/result_future/promise.hpp` | `promise_result<T,E>`, `make_result_promise`, nested-handle resolver helpers | Producer side of async state; fulfills success/error and bridges nested handles. | Creates/updates `result_shared_state`; consumed by `future_result` and `shared_result`. |
| `bits/result_future/future.hpp` | `future_result<T,E>` | Move-only consumer handle: wait/get/continuations/subscription/share/coroutine await support. | Built on `promise.hpp` + shared state from `types_and_detail.hpp`. |
| `bits/result_future/shared.hpp` | `shared_result<T,E>` | Copyable consumer handle with repeated reads and continuation APIs. | Shares same underlying state model as `future_result`. |
| `bits/result_future/factories.hpp` | `make_ready_result`, `make_error_result`, `async_result` | Ready/error constructors and async dispatch helper for executors. | Uses `is_executor` + ADL `post`; returns `future_result`. |
| `bits/result_future/when_all.hpp` | `when_all(...)` overload set | Aggregates many futures/shared-results, resolving when all complete. | Uses subscription callbacks on input handles; completes a `promise_result`. |
| `bits/result_future/when_any.hpp` | `when_any(...)` overload set | Races many futures/shared-results, resolving on first completion. | Uses shared context with atomic winner flag + subscriptions. |
| `bits/result_future/then.hpp` | Facade include | Semantic placeholder documenting continuation APIs. | Continuation method bodies are in `future.hpp` and `shared.hpp`. |
| `bits/result_future/result_shared_state_impl.hpp` | Facade include | Internal shared-state implementation facade. | Actual state lives in `types_and_detail.hpp`. |
| `bits/result_future/combinators.hpp` | Includes factories + combinators | Backward-compatible convenience include. | Includes `factories.hpp`, `when_all.hpp`, `when_any.hpp`. |

### Core Concurrency/Execution Internals (`bits/`)

| File | Key classes/types/functions | Purpose | Main relationships |
|---|---|---|---|
| `bits/execution_impl.hpp` | `is_executor`, `inplace_executor_t`, `inplace_executor` | Executor trait and default inline executor model. | Governs participation of async/continuation overloads; consumed by factories and pool executor. |
| `bits/thread_pool_impl.hpp` | `detail::queue_executor`, `static_thread_pool` | Thread-pool implementation and queue-backed executor adapter. | Uses `closable_queue<unique_function<void()>>`; specializes `is_executor` for pool executor. |
| `bits/latch_impl.hpp` | `latch` | Header declaration for latch API and state layout. | Runtime behavior implemented in `portable_concurrency_runtime.cpp`. |
| `bits/closable_queue_fwd.hpp` | `detail::closable_queue<T>` declaration | Thread-safe closeable producer-consumer queue declaration. | Implemented in `closable_queue.hpp`; used by thread pool. |
| `bits/closable_queue.hpp` | `closable_queue<T>::pop/push/close` | Queue implementation with close semantics and blocking pop. | Used by `static_thread_pool` worker loop. |
| `bits/continuations_stack.hpp` | `detail::continuation`, `detail::continuations_stack` | One-shot continuation buffer that executes all queued continuations once. | Built on `once_consumable_stack<small_unique_function<void()>>`; key primitive for continuation dispatch. |
| `bits/once_consumable_stack_fwd.hpp` | `alloc_mem_guard`, `forward_list_node`, `forward_list_deleter`, `once_consumable_stack<T>` declaration | Lock-free once-consumable stack forward API and allocator-aware node utilities. | Implemented in `once_consumable_stack.hpp`; used by `continuations_stack`. |
| `bits/once_consumable_stack.hpp` | `forward_list_iterator`, `once_consumable_stack` methods | Implementation of multi-producer, single-consume stack semantics. | Consumed by continuation scheduling internals. |
| `bits/unique_function_fwd.hpp` | `unique_function<R(A...)>` declaration | Public callable wrapper declaration and interface docs. | Wraps/bridges to `small_unique_function`. |
| `bits/unique_function.hpp` | `unique_function` methods | Type-erased move-only callable wrapper implementation with fallback heap path when SBO is insufficient. | Uses `small_unique_function.hpp` + `invoke.hpp`. |
| `bits/small_unique_function_fwd.hpp` | `function_invocation_error`, `small_buffer`, `small_unique_function<R(A...)>` declaration | SBO callable wrapper declaration and invocation error model. | Low-level storage engine for `unique_function`. |
| `bits/small_unique_function.hpp` | `callable_vtbl`, SBO storage/invoke logic | In-place non-throwing callable erasure/invocation implementation. | Used directly by continuations and indirectly by `unique_function`. |
| `bits/invoke.hpp` | `detail::invoke(...)` overload set | C++14-style INVOKE implementation (member pointers, reference_wrapper, callables). | Utility used by function wrappers and task invocation paths. |
| `bits/either.hpp` | `detail::either<...>`, `monostate`, `in_place_index_t` | Lightweight move-only discriminated union utility for internal storage patterns. | Uses config/type-trait helpers; internal support type. |
| `bits/concurrency_type_traits.hpp` | `invoke_result_t`, `is_future`, `are_futures`, `remove_future`, `add_future`, `promise_arg_t`, `swallow` | Type trait toolbox for continuation and future composition logic. | Shared utility used by multiple internals and legacy abstractions. |
| `bits/concurrency_config.hpp` | `PC_NODISCARD` macro | Compiler portability helpers for attributes/configuration. | Included by utility internals such as `either.hpp`. |
| `bits/coro.hpp` | `coroutine_handle`, `suspend_never`, `PC_HAS_COROUTINES` | Coroutine feature detection and compatibility aliases (`std` vs `std::experimental`). | Enables coroutine integration in `future_result` when available. |
| `bits/fwd.hpp` | `canceler_arg_t`, `future_status`, legacy forward declarations | Shared tags/status and forward declarations retained for compatibility. | Referenced by trait utilities and execution/wait APIs. |
| `bits/voidify.hpp` | `detail::voidify<T>` | C++11/14-friendly `void_t` equivalent for SFINAE. | Utility dependency for traits and detection idioms. |

### Non-Code Support Files

| File | Purpose |
|---|---|
| `LICENSE` | Upstream portable_concurrency licensing terms retained locally. |
| `.gitignore` | Local ignore rules for this subtree. |
| `README.md` | This local adaptation and API inventory document. |

## Relationship Map (How Pieces Fit)

1. Public includes are intentionally thin wrappers:
  `future.hpp`, `execution.hpp`, `thread_pool.hpp`, `latch.hpp`, `functional.hpp`, `functional_fwd.hpp` mostly include implementation headers from `bits/`.
2. The core async data flow is:
  caller creates work (`async_result` or `packaged_task_result`) -> producer writes via `promise_result` -> shared state (`result_shared_state`) transitions ready -> consumers (`future_result`/`shared_result`) wait/get/chain.
3. Continuations are scheduled through function wrappers and continuation stacks:
  `unique_function`/`small_unique_function` + `continuations_stack` + once-consumable stack internals.
4. Executor integration is ADL-based:
  any custom executor type can participate by specializing `is_executor` and providing `post(exec, task)`.
5. `static_thread_pool` is one such executor provider:
  it bridges posted tasks into `closable_queue`, drained by worker threads.
6. Composition primitives (`when_all`, `when_any`) are built on handle subscriptions, not polling threads.

## Practical Navigation

1. Start here for usage: `future.hpp`.
2. Need custom scheduling model: `execution.hpp` + `thread_pool.hpp`.
3. Need internals for continuation semantics: `bits/result_future/future.hpp`, `bits/result_future/shared.hpp`, `bits/continuations_stack.hpp`.
4. Need type-erasure behavior details: `bits/unique_function.hpp`, `bits/small_unique_function.hpp`, `bits/invoke.hpp`.
