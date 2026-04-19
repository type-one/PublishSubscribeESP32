# GitHub Copilot Instructions

## Repository Intent

This repository contains a lightweight publish/subscribe framework that targets:

- FreeRTOS/ESP32 (idf.py + ESP32 CMake flow)
- Linux/Windows desktop (PC CMake flow)

Core first-party framework code lives in `main/tools/`.
Other folders in `main/` may contain third-party vendor code.

## Edit Scope Rules

- Prefer edits in first-party code (`main/tools/`, tests, and project glue code).
- Do not modify third-party library code unless explicitly requested.
- When unsure whether a folder is third-party, ask before large edits.

## C++ Version Policy

- Default to C++20.
- Maintain C++17 compatibility when applicable.
- C++23 is acceptable only with a C++20/C++17 fallback.

Fallback guidance:

- Use feature checks (`__cplusplus`, `_MSVC_LANG`, feature-test macros).
- Provide equivalent implementation paths for older standards.

## Style and Naming

- Follow repository `.clang-format` and `.clang-tidy` (root files are authoritative).
- Use snake_case naming for functions/methods/variables.
- Prefix class members with `m_`.
- If a class has private members, include an explicit `private:` section.
- Avoid `protected:` sections unless explicitly required by the design.
- For code changes, run formatting and static analysis on modified files before finalizing.

## Standard Library Usage

- Prefer STL containers (`std::vector`, `std::array`, `std::map`, `std::unordered_map`, `std::string`, …) and the facilities in `main/tools/` over custom data structures.
- Use `<algorithm>` functions (`std::find`, `std::transform`, `std::accumulate`, `std::sort`, …) instead of hand-written loops whenever the intent becomes clearer.
- In C++20 code, prefer `<ranges>` pipelines (`std::views::filter`, `std::views::transform`, `std::ranges::sort`, …) for expressive, composable data processing.
- Always provide a C++17-compatible fallback using `<algorithm>` when `<ranges>` or range adaptors are used:
  - Guard with `#if (__cplusplus >= 202002L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L))`.
  - Supply the equivalent `<algorithm>` implementation in the `#else` branch.
- Do not re-implement what the STL already provides correctly and portably.
- When the STL is unavailable or insufficient on a target platform, reach for the helpers in `main/tools/` before writing new infrastructure.

## Template API Design

- For templated APIs, follow the style used in `main/tools/*.hpp`:
  - keep exact-type `T` overloads for explicit/common call paths,
  - add perfect-forwarding overloads (`U&&` / `Args&&...`) and forward with `std::forward`.
- Constrain forwarding overloads to intended types (`requires` on C++20, SFINAE fallback for C++17) to avoid ambiguous or overly-generic matches.

## Object Lifetime and API Signatures

- For class ownership semantics:
  - Apply the Rule of Five where needed,
  - or inherit from `tools::non_copyable` when copy/move must be disallowed.
- Use smart pointers when applicable and encode ownership intent (`std::unique_ptr` for sole ownership, `std::shared_ptr` for shared ownership).
- Avoid raw pointers for ownership; allow raw pointers/references only for justified non-owning access.
- Function/method parameter policy:
  - use pass-by-value for simple/scalar types,
  - use pass-by-value for movable types when a local owning copy is intended,
  - use `const T&` for heavier types when copy is not required.
- Constructor policy:
  - if a constructor takes a movable type by value, move it into members using `std::move(...)`.

### Good vs Bad (Ownership and API Design)

- Good:
  - model ownership with `std::unique_ptr` or `std::shared_ptr`.
  - pass scalar/simple values by value.
  - pass heavy read-only objects by `const T&`.
  - enforce Rule of Five or use `tools::non_copyable`.
  - move by-value constructor inputs into members.
- Bad:
  - owning raw pointers without explicit ownership semantics.
  - unnecessary copies of heavy objects in API boundaries.
  - public/protected inheritance exposing internal state by default.
  - non-templated implementation bodies left inline in headers.

## OOP Design Constraints

- Respect information hiding and encapsulation.
- Avoid inheritance exposure patterns that leak internals.
- Respect Liskov substitution principle in polymorphic APIs.

## Clean Code Policy

- Names must be meaningful and reveal intent: prefer `elapsed_time_ms` over `t`, `sensor_reading` over `val`.
- Avoid abbreviations unless they are universally understood in the domain (e.g., `buf`, `idx`, `ctx`).
- Comments must be brief and explain *why*, not *what* — the code itself must be readable enough to explain what.
- Do not add comments that merely restate the code (`// increment counter` above `++counter` is noise).
- Each class must have a single, clearly stated responsibility. Split classes that grow beyond one concern.
- Methods must be short and focused: a method that needs a comment to describe each of its phases should be split.
- Avoid deeply nested lambdas: extract named helper lambdas or free functions instead of nesting captures inside captures.
- Prefer named lambdas stored in a local variable over immediately-invoked anonymous lambdas when the logic is non-trivial.
- Avoid boolean parameter traps: prefer named enums or separate overloads over `bool` parameters that control behavior.

### Good vs Bad (Clean Code)

- Good:
  - names that make the reader's intent obvious without a comment.
  - short methods with one clear job.
  - a comment that explains a non-obvious constraint or algorithm decision.
  - lambdas kept to a single, obvious expression or extracted to a named variable.
- Bad:
  - single-letter or cryptic variable names outside tight mathematical loops.
  - methods longer than ~40 lines that mix concerns.
  - comments that duplicate what the code already says clearly.
  - lambdas nested three levels deep with multiple captures.

### Processed .clang-format Rules

- Base style: WebKit.
- C++ formatting target: C++20 syntax.
- Indentation: 4 spaces, no tabs (`UseTab: Never`).
- Maximum line length: 120 columns.
- Braces: Allman style.
- Template declarations: always break.
- Namespace indentation: All.
- Case labels are indented.
- Trailing comments should be aligned with one leading space.
- Space before assignment operators is required.
- Keep at most 2 consecutive empty lines.
- Do not compress short constructs onto one line:
  - short `if`
  - short loops
  - short functions

### Processed .clang-tidy Rules

- Enabled groups:
  - `clang-diagnostic-*`
  - `clang-analyzer-*`
  - `modernize-*`
  - `performance-*`
  - `readability-*`
  - `bugprone-*`
  - `fuchsia-*`
  - `cppcoreguidelines-*`
- Treat all warnings as errors (`WarningsAsErrors: *`).
- Header-focused analysis filter: `.hpp`.
- Explicitly disabled checks:
  - `modernize-use-trailing-return-type`
  - `modernize-type-traits`
  - `modernize-use-constraints`
  - `performance-unnecessary-copy-initialization`
  - `cppcoreguidelines-avoid-do-while`
  - `cppcoreguidelines-pro-type-vararg`
  - `cppcoreguidelines-pro-bounds-array-to-pointer-decay`
  - `cppcoreguidelines-pro-bounds-pointer-arithmetic`
  - `cppcoreguidelines-pro-type-reinterpret-cast`
  - `bugprone-suspicious-include`
  - `fuchsia-default-arguments-calls`
  - `fuchsia-overloaded-operator`
  - `readability-function-cognitive-complexity`

## Header and Documentation Conventions

- New `.cpp` files should include the project banner/header style used by files under `main/tools/`.
- Non-templated code should be split between `.hpp` (declarations only, no inline implementation bodies) and `.cpp` (definitions).
- Use Doxygen comments for:
  - classes
  - public methods
  - non-obvious behavior and constraints
- Mirror the tone and granularity already used in `main/tools/*`.

## Error Handling Requirements

- New classes and new APIs should be exception-free.
- Prefer `tools::expected` for operation results.
- Return structured error information instead of throwing.
- Favor explicit result checks in call sites and tests.

## Testing Requirements

- Tests use Google Test.
- For new/changed behavior:
  - add success-path tests
  - add failure-path tests
- For result-based APIs, assert both `.has_value()` and error payload semantics.

## Build Awareness

When suggesting or validating changes, consider both build families:

1. ESP32/FreeRTOS via `idf.py` and ESP32 CMake setup.
2. Desktop via PC CMake setup (`main/CMakeLists_PC.txt` workflow).

Do not introduce platform-specific code without guarding it appropriately.

## Preferred Contribution Pattern

1. Read nearby reference implementation in `main/tools/`.
2. Implement minimal, localized change.
3. Keep naming/style/doc conventions consistent.
4. Add/update Google Tests.
5. Verify no conflicts with `.clang-tidy` and `.clang-format`.
6. Update docs if API usage changed.
