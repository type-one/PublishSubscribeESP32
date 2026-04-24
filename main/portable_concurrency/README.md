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

- The legacy v1 future/promise API has been removed from this repository.
- Build wiring compiles only the result-based v2 future/runtime implementation.
- Runtime primitives required by thread pool/latch remain available.

## Include Guidance

Use:

```cpp
#include "portable_concurrency/p_future.hpp"
```

This exposes the result-based API via:

- `portable_concurrency::v2::*` result primitives
- policy aliases such as `portable_concurrency::future_t<T>`

The older `p_future_v2.hpp` wrapper is no longer part of the public surface.

## Timed Wait Guidance

Use timed waits directly on result handles:

- `future_result::wait_for(...)`
- `future_result::wait_until(...)`
- `shared_result::wait_for(...)`
- `shared_result::wait_until(...)`

## Notes

This repository maintains C++20 as primary target with C++17 compatibility where applicable in framework code.
