# C++ Timeouts

A portable C++ timer component (local adaptation).

https://github.com/eglimi/cpptime

It manages a set of timeouts that, when expired, invoke a callback. It uses
standard C++ facilities in order to avoid platform-specific timer APIs.

It supports one-shot and periodic timeouts.

## Documentation

Please see [cpptime.hpp](./cpptime.hpp) for the public API and
[cpptime.cpp](./cpptime.cpp) for runtime implementation details.

## Implementation Status

We use this timer implementation in some of our products without issues.
Judging from the GitHub stars and forks, it seems to be used in other projects
as well. Since it was implemented in 2015 and has not seen many issue reports,
we assume it is quite stable.

But note that this is not a guarantee and if you find any issues, please report
them.

## Use Cases and Limitations

Naturally the implementation makes some trade-offs. This makes it useful for
some cases, and less so for others.

- The timer runs completely in user space. This makes it slightly less
  efficient than other solutions, such as `timer_create()` or
  `timerfd_create()`. However, in many cases, this overhead is acceptable.

- Given a C++11 capable compiler, the code is portable.

- The API to add or remove a timeout is arguably nicer than the platform
  specific alternatives. E.g.

```cpp
timer.add(seconds(2), [](CppTime::timer_id) { ... });
```

- The implementation is small and easy to understand. You can change or extend
  it to make it better suitable for your use-cases.

## Examples

A one shot timer.

~~~
using namespace std::chrono;
CppTime::Timer t;
t.add(seconds(2), [](CppTime::timer_id) { std::cout << "yes\n"; });
std::this_thread::sleep_for(seconds(3));
~~~

A periodic timer that is first executed after 2 seconds, and after this every
second. The timeout event is then removed after 10 seconds. When a timeout
event is removed, its attached handler is also freed to clean-up any attached
resources.

~~~
using namespace std::chrono;
CppTime::Timer t;
auto id = t.add(seconds(2), [](CppTime::timer_id) { std::cout << "yes\n"; }, seconds(1));
std::this_thread::sleep_for(seconds(10));
t.remove(id);
~~~

See the tests for more examples.

## Usage

Local integration in this repository uses:

- [cpptime.hpp](./cpptime.hpp) (declarations/public API)
- [cpptime.cpp](./cpptime.cpp) (implementation)

So this local copy is not header-only. Both files must be part of your build.

Minimal standalone build example (POSIX-like toolchain):

~~~
g++ -std=c++17 -Wall -Wextra -pthread -I main -c main/cpptime/cpptime.cpp -o cpptime.o
g++ -std=c++17 -Wall -Wextra -pthread -I main your_timer_app.cpp cpptime.o -o your_timer_app
~~~

Replace `your_timer_app.cpp` with your source file that includes `cpptime/cpptime.hpp`.

## Possible Features

While the current implementation serves us well, there are some features that
might potentially be interesting for other use cases. Contact us in case you
are interested.

- [x] Ability to have multiple timer components running.
- [x] Distribute it as a header only library.
- [ ] Optionally avoid locking.
- [ ] API to use client thread instead of creating its own.
- [ ] API to use client mutex instead of its own.

## API Summary (local)

Namespace: `CppTime`

- Type aliases:
  - `timer_id`
  - `handler_t`
  - `clock`
  - `timestamp`
  - `duration` (microseconds)
- Main class: `Timer`
  - `add(const timestamp&, handler_t, const duration&)`
  - `add(const timestamp&, handler_t)`
  - `add(const std::chrono::duration<...>&, handler_t, const duration&)`
  - `add(const std::chrono::duration<...>&, handler_t)`
  - `add(std::uint64_t when_us, handler_t, std::uint64_t period_us)`
  - `add(std::uint64_t when_us, handler_t)`
  - `remove(timer_id)`

## Known Issues

GCC up to version 10 (e.g. used in Ubuntu 20.04 LTS) has [an issue][gcc-clock] where `conditional_variable` doesn't use the monotonic clock. This leads to unreliable programs when the system clock is moved backwards. See #5 for more details. The fix is to update to a GCC version with the fix applied, e.g. version 10 or greater.

[gcc-clock]: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=41861

## Contributions

Contributions, suggestions, and feature requests are welcome. Please use the
GitHub issue tracker.
