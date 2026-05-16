# tools

First-party platform abstraction and utility layer used by the publish/subscribe framework.

This folder provides:

- portable synchronization primitives
- task/threading abstractions (FreeRTOS + desktop backends)
- synchronous/asynchronous observer infrastructure
- bounded containers (ring/vector/queue + sync wrappers)
- utility components (expected, logger, gzip wrapper, platform helpers)

## Architecture At A Glance

1. Public facade headers in `main/tools/*.hpp` select backend-specific `.inl` implementations.
2. Backend implementations live in:
   - `main/tools/freertos/*.inl`
   - `main/tools/standard/*.inl`
3. A small set of `.cpp` files provides non-templated runtime/implementation units.
4. Higher-level modules (`sync_queue`, `sync_ring_*`, tasks, observer) build on low-level primitives (`critical_section`, `cond_var`, `sync_object`, ring containers).

## Public Headers Inventory (`main/tools/`)

| File | Key classes/types | Role / Purpose | Relationships |
|---|---|---|---|
| `async_observer.hpp` | `async_observer<Topic, Evt>` | Async observer built on synchronous subject/observer with decoupled handling. | Inherits from `sync_observer`; integrates with event/pub-sub flow. |
| `base_task.hpp` | `base_task` | Common non-copyable task base abstraction. | Base class for `generic_task`, `data_task`, `periodic_task`, `worker_task`. |
| `cond_var.hpp` | `cond_var` facade | Cross-platform condition variable abstraction. | Includes `freertos/cond_var_freertos.inl` or `standard/cond_var_std.inl`. |
| `critical_section.hpp` | `critical_section`, `isr_lock_guard` facade | Cross-platform mutual exclusion abstraction and ISR-safe lock helper contract. | Includes `freertos/critical_section_freertos.inl` or `standard/critical_section_std.inl`. |
| `data_task.hpp` | `data_task<...>` facade | Task abstraction specialized for queued data/event processing. | Includes `freertos/data_task_freertos.inl` or `standard/data_task_std.inl`; derives from `base_task`. |
| `expected.hpp` | `unexpected<E>`, `expected<T,E>`, `expected<void,E>` | Local expected/unexpected result type used across the codebase. | Foundation for exception-free APIs in tools and other modules. |
| `generic_task.hpp` | `generic_task<...>` facade | Generic task wrapper for running callable loops/jobs. | Includes `freertos/generic_task_freertos.inl` or `standard/generic_task_std.inl`; derives from `base_task`. |
| `gzip_wrapper.hpp` | `gzip_wrapper` | Compression/decompression wrapper over uzlib. | Implemented in `gzip_wrapper.cpp`; uses `logger` for diagnostics. |
| `histogram.hpp` | `histogram<T>` | Thread-safe histogram/statistics helper. | Uses synchronization primitives and container utilities. |
| `lock_free_ring_buffer.hpp` | `lock_free_ring_buffer<T, ...>` | Lock-free ring buffer for high-frequency producer/consumer paths. | Used by memory pool allocator and other low-level paths. |
| `logger.hpp` | `log_level`, logging macros/helpers | Unified logging abstraction used across modules. | Used by many components including `gzip_wrapper` and runtime code. |
| `memory_pipe.hpp` | `memory_pipe<...>` facade | Pipe-like in-memory transfer primitive. | Includes `freertos/memory_pipe_freertos.inl` or `standard/memory_pipe_std.inl`. |
| `non_copyable.hpp` | `non_copyable` | Utility base class to disable copy/move semantics where required. | Widely inherited by synchronization/tasks/container wrappers. |
| `periodic_task.hpp` | `periodic_task<...>` facade | Periodic execution task abstraction. | Includes `freertos/periodic_task_freertos.inl` or `standard/periodic_task_std.inl`; derives from `base_task`. |
| `platform_detection.hpp` | compile-time platform macros | Platform and compiler detection utilities. | Used by facades, runtime `.cpp`, and backend selection logic. |
| `platform_helpers.hpp` | helper APIs facade (cpu core count, task naming/scheduling helpers) | Platform helper API for common OS/platform operations. | Includes `freertos/platform_helpers_freertos.inl` or `standard/platform_helpers_std.inl`. |
| `ring_buffer.hpp` | `ring_buffer<T>`, `overflow_policy`, `write_status`, `push_range_overwrite_result` | Non-thread-safe circular buffer. | Basis for sync wrappers and queue-like bounded storage. |
| `ring_vector.hpp` | `ring_vector<T>`, `overflow_policy`, `write_status`, `push_range_overwrite_result` | Non-thread-safe ring container built over vector semantics. | Basis for `sync_ring_vector`. |
| `sync_dictionary.hpp` | `sync_dictionary<Key, Value, ...>` | Thread-safe dictionary/map wrapper with range helpers. | Uses `critical_section` and expected-style error/status patterns. |
| `sync_object.hpp` | `sync_object` facade | Cross-platform signaling/wait synchronization object. | Includes `freertos/sync_object_freertos.inl` or `standard/sync_object_std.inl`; out-of-line parts in `sync_object.cpp`. |
| `sync_observer.hpp` | `sync_observer<Topic, Evt>`, `sync_subject<Topic, Evt>` | Synchronous publish/subscribe observer pattern implementation. | Core event bus primitive used by async observer and app-level hubs. |
| `sync_priority_queue.hpp` | `sync_priority_queue<T, Compare>`, `sync_max_priority_queue<T>` | Thread-safe priority queue with configurable comparator; transparent integration with `async_observer`. | Uses `critical_section`; default comparator is `std::less<T>` for min-heap; template alias for max-heap convenience. |
| `sync_queue.hpp` | `sync_queue<T, ...>` | Thread-safe queue with ISR-safe variants and batch operations. | Uses `critical_section`; complements ring-based containers. |
| `sync_ring_buffer.hpp` | `sync_ring_buffer<T, ...>` | Thread-safe wrapper around ring buffer semantics. | Builds on ring-buffer logic + synchronization primitives. |
| `sync_ring_vector.hpp` | `sync_ring_vector<T, ...>` | Thread-safe wrapper around ring vector semantics. | Builds on ring-vector logic + synchronization primitives. |
| `timer_scheduler.hpp` | `timer_scheduler` facade, timer-related enums/types | Cross-platform timer scheduling abstraction. | Includes `freertos/timer_scheduler_freertos.inl` or `standard/timer_scheduler_std.inl`; implementation parts in `timer_scheduler.cpp`. Supports `timer_resolution_policy::high_resolution` on ESP32 FreeRTOS builds via `esp_timer`. |
| `time_list.hpp` | `time_list<TTimestamp, TValue>` | Non-thread-safe chronological list storing `<timestamp, value>` entries using `std::priority_queue` (earliest first). | Intended as a base helper; a synchronized wrapper can be layered on top (e.g., future `sync_time_list`). |
| `variant_overload.hpp` | `overload<Ts...>` | `std::visit` helper for composing variant visitors. | Utility used by FSM/event-dispatch code. |
| `worker_task.hpp` | `worker_task<Context>`, `worker_task_executor<Context>` facade | Worker task + executor bridge for scheduling work into worker context. | Includes `freertos/worker_task_freertos.inl` or `standard/worker_task_std.inl`; `is_executor` specialization ties into portable_concurrency. |

## Platform Backend Inventory (`main/tools/freertos/` and `main/tools/standard/`)

These files are paired implementations of the same abstractions.

| Facade header | FreeRTOS backend | Standard backend | Primary role |
|---|---|---|---|
| `cond_var.hpp` | `freertos/cond_var_freertos.inl` | `standard/cond_var_std.inl` | Condition variable implementation per platform. |
| `critical_section.hpp` | `freertos/critical_section_freertos.inl` | `standard/critical_section_std.inl` | Mutex/critical section + ISR lock semantics. |
| `data_task.hpp` | `freertos/data_task_freertos.inl` | `standard/data_task_std.inl` | Data-driven task implementation. |
| `generic_task.hpp` | `freertos/generic_task_freertos.inl` | `standard/generic_task_std.inl` | Generic task implementation. |
| `memory_pipe.hpp` | `freertos/memory_pipe_freertos.inl` | `standard/memory_pipe_std.inl` | Memory pipe implementation. |
| `periodic_task.hpp` | `freertos/periodic_task_freertos.inl` | `standard/periodic_task_std.inl` | Periodic task implementation. |
| `platform_helpers.hpp` | `freertos/platform_helpers_freertos.inl` | `standard/platform_helpers_std.inl` | Platform helper utilities (threads/tasks/core affinity where applicable). |
| `sync_object.hpp` | `freertos/sync_object_freertos.inl` | `standard/sync_object_std.inl` | Synchronization object API surface. |
| `sync_object.cpp` impl include | `freertos/sync_object_impl_freertos.inl` | `standard/sync_object_impl_std.inl` | Out-of-line sync object internals. |
| `timer_scheduler.hpp` | `freertos/timer_scheduler_freertos.inl` | `standard/timer_scheduler_std.inl` | Timer scheduler API surface, including `timer_resolution_policy`. |
| `timer_scheduler.cpp` impl include | `freertos/timer_scheduler_impl_freertos.inl` | `standard/timer_scheduler_impl_std.inl` | Out-of-line timer scheduler internals. |
| `worker_task.hpp` | `freertos/worker_task_freertos.inl` | `standard/worker_task_std.inl` | Worker task and executor integration. |

## Platform-Specific Utility Header (`main/tools/linux/`)

| File | Key types | Role / Purpose | Relationships |
|---|---|---|---|
| `linux/linux_sched_deadline.hpp` | `sched_attr` and helper functions | Linux-only scheduling helpers for SCHED_DEADLINE and task policy tuning. | Optional helper used on Linux builds; independent of FreeRTOS backends. |

## Source Files (`main/tools/*.cpp`)

| File | Role / Purpose | Relationships |
|---|---|---|
| `gzip_wrapper.cpp` | Implements gzip pack/unpack behavior over uzlib with CRC/size checks. | Implements `gzip_wrapper.hpp`; logs through `logger.hpp`. |
| `mem_pool_allocator.cpp` | Optional global new/delete caching allocator with small-block pool reuse. | Uses `critical_section` + `lock_free_ring_buffer`; enabled via compile definitions. |
| `sync_object.cpp` | Selects and compiles backend-specific sync object implementation details. | Includes either `sync_object_impl_freertos.inl` or `sync_object_impl_std.inl`. |
| `timer_scheduler.cpp` | Selects and compiles backend-specific timer scheduler implementation details. | Includes either `timer_scheduler_impl_freertos.inl` or `timer_scheduler_impl_std.inl`. |

## Relationship Map

1. Concurrency primitives layer:
   `critical_section`, `cond_var`, `sync_object` form the synchronization substrate.
2. Tasking layer:
   `base_task` -> `generic_task` / `data_task` / `periodic_task` / `worker_task`.
3. Execution bridge:
   `worker_task_executor` provides executor-style posting into worker tasks.
4. Container layer:
   `ring_buffer` / `ring_vector` / `lock_free_ring_buffer` are storage cores.
5. Synchronized container layer:
   `sync_queue`, `sync_priority_queue`, `sync_ring_buffer`, `sync_ring_vector`, `sync_dictionary`, `histogram` wrap storage with locks and ISR-safe variants.
   All support transparent integration with `async_observer` and `worker_task` via template parameters.
6. Eventing layer:
   `sync_subject` + `sync_observer` provide pub/sub; `async_observer` adds asynchronous handling.
7. Utility layer:
   `expected`, `non_copyable`, `variant_overload`, `platform_helpers`, `platform_detection`, `logger`, `gzip_wrapper` support all other layers.

## Notes For Contributors

- Prefer editing this folder (`main/tools/`) for framework behavior rather than vendor folders.
- Keep API facades stable and place platform-specific behavior in backend `.inl` files.
- If adding a new abstraction, mirror the existing facade + FreeRTOS/standard backend pattern.
- Do not treat `tools::isr_lock_guard` (or ISR guard wrappers) as interchangeable with `std::lock_guard`/`std::scoped_lock`.
   `tools::isr_lock_guard` is intended to call `isr_lock()`/`isr_unlock()`: on FreeRTOS those are ISR-only primitives.
   PC/standard builds may alias ISR locking to regular lock/unlock as a fallback, but this must not be generalized as an ISR-safe rule.
- If adding a new file here, update this README inventory accordingly.
