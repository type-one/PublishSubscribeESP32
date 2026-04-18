# CLAUDE.md

This file defines repository-specific guidance for AI coding agents.

## Project Scope

- Core framework reference code is under `main/tools/`.
- Third-party code is under folders such as `main/cJSON/`, `main/cjsonpp/`, `main/CException/`, `main/uzlib/`, `main/bytepack/`, `main/cpptime/`.
- Prefer implementing features and fixes in first-party framework code, not in third-party folders, unless explicitly requested.

## Language and Standards

- Primary language: C++20.
- Keep compatibility with C++17 where applicable.
- C++23 is allowed only when a C++20/C++17 fallback is provided.
- Typical fallback pattern:
  - Use feature-test macros and/or `__cplusplus` / `_MSVC_LANG` checks.
  - Provide equivalent implementation for older standards.

## Build Targets

- ESP32 / FreeRTOS build path:
  - Root `CMakeLists.txt` and `main/CMakeLists.ESP32`.
  - Build/flash with `idf.py`.
- Linux / Windows desktop build path:
  - Use `main/CMakeLists_PC.txt` by renaming/copying as needed in local workflow.
  - Build through CMake/Ninja or Visual Studio generator.

## Formatting and Static Analysis

- Formatting source of truth: `.clang-format` at repository root.
- Static analysis source of truth: `.clang-tidy` at repository root.
- Respect all enabled tidy checks and warnings-as-errors behavior.
- For code changes, run formatting and static analysis on modified files before finalizing.

### Processed .clang-format Rules

- Base style: WebKit.
- C++ formatting target: C++20 syntax.
- Indentation: 4 spaces, no tabs (`UseTab: Never`).
- Line length: 120 columns.
- Brace style: Allman (`BreakBeforeBraces: Allman`).
- Template declarations: always break.
- Namespace indentation: indent all namespace contents.
- Case labels: indented.
- Trailing comments: aligned, one space before trailing comments.
- Assignment operators: enforce surrounding spacing.
- Keep at most 2 consecutive empty lines.
- Do not keep short statements/functions on a single line:
  - `if`
  - loop statements
  - short functions

### Processed .clang-tidy Rules

- Enabled check groups:
  - `clang-diagnostic-*`
  - `clang-analyzer-*`
  - `modernize-*`
  - `performance-*`
  - `readability-*`
  - `bugprone-*`
  - `fuchsia-*`
  - `cppcoreguidelines-*`
- Warnings are errors (`WarningsAsErrors: *`).
- Header analysis filter is `.hpp` (`HeaderFilterRegex: '.hpp'`).
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

## Coding Conventions

- Naming:
  - snake_case for functions, variables, and methods.
  - class member variables must use `m_` prefix.
- Class layout:
  - If class has private members, use an explicit `private:` section.
  - Avoid `protected:` sections unless explicitly required by the design.
- File structure:
  - New `.cpp` files should include the standard project banner/header style used in `main/tools/*`.
  - Non-templated code should use a split layout: declarations in `.hpp` (no inline implementation body), definitions in `.cpp`.
- Documentation:
  - Use Doxygen comments for classes and methods, consistent with `main/tools/*` examples.

## Template API Design

- In templated APIs, prefer the same pattern used in `main/tools/*.hpp`:
  - keep exact-type `T` overloads where needed for clarity and compatibility,
  - add perfect-forwarding template overloads (`U&&` / `Args&&...` with `std::forward`) to support conversion paths and avoid unnecessary copies.
- Constrain forwarding templates appropriately (`requires` in C++20, SFINAE fallback for C++17) to avoid over-broad matches.

## Object Lifetime and API Signatures

- Use RAII for resource management (memory, locks, file handles, timers, and other acquire/release lifecycles).
- For class ownership semantics:
  - Either implement the Rule of Five correctly,
  - or explicitly inherit from `tools::non_copyable` when copying/moving must be forbidden.
- Use smart pointers when applicable and model ownership explicitly (`std::unique_ptr` for unique ownership, `std::shared_ptr` for shared ownership).
- Avoid raw pointers for ownership; use raw pointers/references only for non-owning access when justified.
- Function/method argument policy:
  - use pass-by-value for simple/scalar types,
  - use pass-by-value for types with well-defined move operations when ownership transfer or local copy is intended,
  - use `const T&` for heavier non-scalar types where copying is unnecessary.
- Constructor policy:
  - when parameters are passed by value and the type supports move semantics, move into members using `std::move(...)`.

### Good vs Bad (Ownership and API Design)

- Good:
  - choose `std::unique_ptr`/`std::shared_ptr` according to ownership semantics.
  - pass small scalar parameters by value.
  - pass heavy read-only objects by `const T&`.
  - implement Rule of Five when a class manages resources.
  - inherit from `tools::non_copyable` when copying/moving must be disallowed.
  - move by-value constructor parameters into members.
- Bad:
  - owning raw pointers without clear lifetime contracts.
  - copying heavy objects in APIs without intent.
  - exposing implementation details through inheritance.
  - introducing `protected` members by default.
  - mixing non-templated declarations and implementations inline in headers.

## OOP Design Constraints

- Respect encapsulation and information hiding.
- Avoid exposing internals through inheritance hierarchies unless required.
- Respect Liskov substitution principle for all polymorphic interfaces and derived classes.
- Singleton pattern is not allowed unless explicitly justified by a narrow, concrete requirement.
- Global variables are not allowed unless explicitly justified by a narrow, concrete requirement.

## Error Handling Model

- Newly introduced classes should be exception-free by design.
- Prefer result-based APIs using `tools::expected`.
- Return explicit success/error results instead of throwing.
- Keep error payloads descriptive and machine-checkable where possible.

## Testing

- Unit tests are written with Google Test.
- Add or update tests with every functional change.
- Prefer behavior-focused tests (success + failure paths) for result-returning APIs.

## Change Strategy

- Keep changes minimal and localized.
- Preserve public behavior unless the task explicitly requires API changes.
- Avoid broad refactors in third-party code.
- Update docs when API behavior or usage patterns change.

## Practical Checklist for New Code

1. Is the code in first-party framework scope (`main/tools/*`) unless requested otherwise?
2. Does it compile for C++20 with C++17 fallback where needed?
3. Is it exception-free and based on `tools::expected` for new APIs?
4. Does naming follow snake_case and `m_` members?
5. Are Doxygen comments and file header style present?
6. Are tests added/updated in Google Test?
7. Do `.clang-format` and `.clang-tidy` expectations remain satisfied?
