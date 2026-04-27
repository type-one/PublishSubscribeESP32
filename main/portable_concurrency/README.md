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
