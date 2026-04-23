# portable_concurrency

Result-based async/future API for this repository.

## Public API Surface

Use these headers:

- `portable_concurrency/p_future.hpp` (primary unified entrypoint)
- `portable_concurrency/p_future_policy.hpp` (policy aliases/helpers)
- `portable_concurrency/p_execution.hpp`
- `portable_concurrency/p_thread_pool.hpp`
- `portable_concurrency/p_latch.hpp`
- `portable_concurrency/p_functional.hpp`
- `portable_concurrency/p_functional_fwd.hpp`

## Migration Status

- The legacy v1 future/promise API is frozen and not supported for new code.
- Build wiring no longer compiles the legacy v1 future stack translation unit.
- Runtime primitives required by thread pool/latch remain available.

## Include Guidance

Use:

```cpp
#include "portable_concurrency/p_future.hpp"
```

This exposes the result-based API via:

- `portable_concurrency::v2::*` result primitives
- policy aliases such as `portable_concurrency::future_t<T>`

## Timed Wait Guidance

`p_timed_waiter.hpp` is deprecated under v1 freeze.
Use timed waits directly on result handles:

- `future_result::wait_for(...)`
- `future_result::wait_until(...)`
- `shared_result::wait_for(...)`
- `shared_result::wait_until(...)`

## Notes

This repository maintains C++20 as primary target with C++17 compatibility where applicable in framework code.
